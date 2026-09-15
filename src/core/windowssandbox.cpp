/*
 * SPDX-FileCopyrightText: 2026 Project LemonLime
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include "windowssandbox.h"
#ifdef Q_OS_WIN
#include "base/LemonLog.hpp"
#include "windowsprocessutils.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QMutex>
#include <QMutexLocker>
#include <QProcess>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QThread>
#include <QUuid>
#include <aclapi.h>
#include <sddl.h>
#include <userenv.h>
#include <vector>

#define LEMON_MODULE_NAME "WindowsSandbox"

using namespace Lemon::Windows;

namespace {
	QMutex permissionsMutex;
	QMutex sessionMutex;
	std::weak_ptr<WindowsSandboxSession> activeSession;

	Handle openAclFile(const QString &path, bool privateFile) {
		const auto nativePath = QDir::toNativeSeparators(path);
		const DWORD attributes = GetFileAttributesW(wide(nativePath));
		const bool directory =
		    attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY);
		const DWORD flags = FILE_FLAG_BACKUP_SEMANTICS | (privateFile ? FILE_FLAG_OPEN_REPARSE_POINT : 0);
		HANDLE file = CreateFileW(
		    wide(nativePath), directory ? MAXIMUM_ALLOWED : READ_CONTROL | WRITE_DAC | FILE_READ_ATTRIBUTES,
		    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, flags, nullptr);
		if (file == INVALID_HANDLE_VALUE && ! directory && ! privateFile &&
		    GetLastError() == ERROR_ACCESS_DENIED)
			file = CreateFileW(wide(nativePath), READ_CONTROL | FILE_READ_ATTRIBUTES,
			                   FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
			                   flags, nullptr);
		return Handle(file);
	}

	void revokeSid(const QString &path, PSID sid, bool privateFile = false) {
		auto file = openAclFile(path, privateFile);
		if (! file) {
			const DWORD code = GetLastError();
			if (code != ERROR_FILE_NOT_FOUND && code != ERROR_PATH_NOT_FOUND)
				WARN("Cannot open file to revoke sandbox permissions.", path, errorText(code));
			return;
		}
		PSECURITY_DESCRIPTOR descriptor = nullptr;
		PACL acl = nullptr;
		DWORD status = GetSecurityInfo(file.get(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr,
		                               nullptr, &acl, nullptr, &descriptor);
		auto cleanup = qScopeGuard([&] { LocalFree(descriptor); });
		if (status != ERROR_SUCCESS) {
			WARN("Cannot read sandbox permissions for cleanup.", path, errorText(status));
			return;
		}
		bool changed = false;
		for (DWORD i = acl ? acl->AceCount : 0; i > 0; --i) {
			void *entry = nullptr;
			if (! GetAce(acl, i - 1, &entry))
				continue;
			const auto *ace = static_cast<ACCESS_ALLOWED_ACE *>(entry);
			if (ace->Header.AceType == ACCESS_ALLOWED_ACE_TYPE &&
			    EqualSid(const_cast<DWORD *>(&ace->SidStart), sid)) {
				DeleteAce(acl, i - 1);
				changed = true;
			}
		}
		if (changed) {
			status = SetSecurityInfo(file.get(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr,
			                         acl, nullptr);
			if (status != ERROR_SUCCESS)
				WARN("Cannot revoke sandbox permissions.", path, errorText(status));
		}
	}

	struct RuntimeAcl {
		QString path;
		QByteArray sid;
	};

	// QDir("") 表示当前工作目录；此处保留空值，以表示未配置的目录或运行工具。
	QString canonical(const QString &path) { return path.isEmpty() ? QString() : QDir(path).canonicalPath(); }
	bool within(const QString &path, const QString &root) {
		return path.compare(root, Qt::CaseInsensitive) == 0 ||
		       path.startsWith(root + '/', Qt::CaseInsensitive);
	}
	QString hash(const QString &text) {
		return QString::fromLatin1(
		    QCryptographicHash::hash(text.toUtf8(), QCryptographicHash::Sha256).toHex());
	}
	QString stamp(const QString &path) {
		const QFileInfo info(path);
		return QString("%1:%2").arg(info.size()).arg(info.lastModified().toMSecsSinceEpoch());
	}
	bool hasGrant(PACL acl, PSID sid, DWORD rights) {
		if (! acl)
			return true;
		DWORD allowed = 0;
		for (DWORD i = 0; i < acl->AceCount; ++i) {
			void *entry = nullptr;
			if (! GetAce(acl, i, &entry))
				return false;
			const auto *header = static_cast<ACE_HEADER *>(entry);
			if (header->AceFlags & INHERIT_ONLY_ACE)
				continue;
			if (header->AceType != ACCESS_ALLOWED_ACE_TYPE && header->AceType != ACCESS_DENIED_ACE_TYPE)
				continue;
			const auto *ace = static_cast<ACCESS_ALLOWED_ACE *>(entry);
			if (! EqualSid(const_cast<DWORD *>(&ace->SidStart), sid))
				continue;
			DWORD mask = ace->Mask;
			GENERIC_MAPPING mapping{FILE_GENERIC_READ, FILE_GENERIC_WRITE, FILE_GENERIC_EXECUTE,
			                        FILE_ALL_ACCESS};
			MapGenericMask(&mask, &mapping);
			if (header->AceType == ACCESS_DENIED_ACE_TYPE && (mask & rights))
				return false;
			allowed |= mask;
		}
		return (allowed & rights) == rights;
	}
} // namespace

class WindowsSandboxSession {
  public:
	QString id = QUuid::createUuid().toString(QUuid::Id128);
	QHash<QString, QString> runtimes;
	QHash<QString, QStringList> pythonRoots;
	std::vector<RuntimeAcl> grants;

	~WindowsSandboxSession() {
		QMutexLocker lock(&permissionsMutex);
		for (auto it = grants.rbegin(); it != grants.rend(); ++it)
			revokeSid(it->path, it->sid.data());
	}
};

struct WindowsSandbox::Data {
	const ProcessRunnerConfig &config;
	const std::atomic<bool> &stop;
	QElapsedTimer timer;
	qint64 lastProgress = -500;
	QString profileName;
	PSID packageSid = nullptr;
	QString privateSddl;
	QString workingDirectory;
	std::shared_ptr<WindowsSandboxSession> session;
	QProcessEnvironment childEnvironment;
	SandboxSettings::Runtime runtime = SandboxSettings::Native;
	std::vector<SID_AND_ATTRIBUTES> capabilityList;
	Handle job;
	std::vector<BYTE> attributeBuffer;
	LPPROC_THREAD_ATTRIBUTE_LIST attributeList = nullptr;
	SECURITY_CAPABILITIES processCapabilities{};
	HANDLE inheritedHandles[3]{};
	HANDLE jobs[1]{};
	int updates = 0;
	bool hit = true;
	QString error;

	Data(const ProcessRunnerConfig &config, const std::atomic<bool> &stop)
	    : config(config), stop(stop),
	      session(config.sandboxSession ? config.sandboxSession : WindowsSandbox::createSession()) {}
	~Data() {
		stopProcesses();
		if (! workingDirectory.isEmpty()) {
			QMutexLocker lock(&permissionsMutex);
			revokeSid(workingDirectory, packageSid, true);
			QDirIterator files(workingDirectory,
			                   QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
			                   QDirIterator::Subdirectories);
			while (files.hasNext())
				revokeSid(files.next(), packageSid, true);
		}
		if (attributeList)
			DeleteProcThreadAttributeList(attributeList);
		for (const auto &capability : capabilityList)
			LocalFree(capability.Sid);
		if (packageSid) {
			DeleteAppContainerProfile(wide(profileName));
			FreeSid(packageSid);
		}
	}

	bool checkBudget() {
		if (stop)
			return fail("Windows sandbox preparation cancelled.");
		if (timer.elapsed() >= config.sandboxSettings.preparationTimeLimit)
			return fail(
			    QObject::tr(
			        "Windows sandbox preparation exceeded %1 seconds. Increase the preparation limit in "
			        "compiler settings.")
			        .arg(config.sandboxSettings.preparationTimeLimit / 1000));
		if (config.preparationProgress && timer.elapsed() - lastProgress >= 500) {
			lastProgress = timer.elapsed();
			config.preparationProgress(QObject::tr("Preparing Windows sandbox..."));
		}
		return true;
	}

	bool fail(const QString &text) {
		error = text;
		return false;
	}

	bool fail(const QString &operation, const QString &path, DWORD code = GetLastError()) {
		WARN(operation, path, errorText(code));
		error = QObject::tr("Windows sandbox preparation failed. See the log for details.");
		return false;
	}

	bool processFailure(const QString &operation, DWORD code = GetLastError()) {
		WARN(operation, errorText(code));
		error = QObject::tr("Internal error (See log for further information)");
		return false;
	}

	bool prepareProcess(STARTUPINFOEXW &startup) {
		DWORD handleCount = 0;
		for (HANDLE handle :
		     {startup.StartupInfo.hStdInput, startup.StartupInfo.hStdOutput, startup.StartupInfo.hStdError})
			if (handle)
				inheritedHandles[handleCount++] = handle;
		for (DWORD i = 0; i < handleCount; ++i)
			if (! inheritedHandles[i] || inheritedHandles[i] == INVALID_HANDLE_VALUE)
				return processFailure("Cannot open sandbox standard streams.", ERROR_INVALID_HANDLE);
		job.reset(CreateJobObjectW(nullptr, nullptr));
		if (! job)
			return processFailure("Cannot create sandbox job.");
		JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
		limits.BasicLimitInformation.LimitFlags =
		    JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_ACTIVE_PROCESS;
		// The native policy uses the job limit because the child-process creation
		// attribute can fail AppContainer DLL initialization on Windows.
		limits.BasicLimitInformation.ActiveProcessLimit = runtime == SandboxSettings::Native ? 1 : 16;
		if (! SetInformationJobObject(job.get(), JobObjectExtendedLimitInformation, &limits, sizeof(limits)))
			return processFailure("Cannot set sandbox process limits.");
		JOBOBJECT_BASIC_UI_RESTRICTIONS uiLimits{};
		uiLimits.UIRestrictionsClass =
		    JOB_OBJECT_UILIMIT_DESKTOP | JOB_OBJECT_UILIMIT_DISPLAYSETTINGS | JOB_OBJECT_UILIMIT_EXITWINDOWS |
		    JOB_OBJECT_UILIMIT_GLOBALATOMS | JOB_OBJECT_UILIMIT_HANDLES | JOB_OBJECT_UILIMIT_READCLIPBOARD |
		    JOB_OBJECT_UILIMIT_SYSTEMPARAMETERS | JOB_OBJECT_UILIMIT_WRITECLIPBOARD;
		if (! SetInformationJobObject(job.get(), JobObjectBasicUIRestrictions, &uiLimits, sizeof(uiLimits)))
			return processFailure("Cannot restrict sandbox desktop access.");
		SIZE_T attributeSize = 0;
		InitializeProcThreadAttributeList(nullptr, 3, 0, &attributeSize);
		attributeBuffer.resize(attributeSize);
		auto *list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributeBuffer.data());
		if (! InitializeProcThreadAttributeList(list, 3, 0, &attributeSize))
			return processFailure("Cannot initialize sandbox process attributes.");
		attributeList = list;
		startup.lpAttributeList = attributeList;
		processCapabilities = {packageSid, capabilityList.data(), DWORD(capabilityList.size()), 0};
		jobs[0] = job.get();
		if (! UpdateProcThreadAttribute(attributeList, 0, PROC_THREAD_ATTRIBUTE_SECURITY_CAPABILITIES,
		                                &processCapabilities, sizeof(processCapabilities), nullptr,
		                                nullptr) ||
		    ! UpdateProcThreadAttribute(attributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inheritedHandles,
		                                handleCount * sizeof(HANDLE), nullptr, nullptr) ||
		    ! UpdateProcThreadAttribute(attributeList, 0, PROC_THREAD_ATTRIBUTE_JOB_LIST, jobs, sizeof(jobs),
		                                nullptr, nullptr))
			return processFailure("Cannot configure sandbox process attributes.");
		return true;
	}

	bool stopProcesses() {
		if (! job)
			return true;
		TerminateJobObject(job.get(), 0);
		JOBOBJECT_BASIC_ACCOUNTING_INFORMATION accounting{};
		QElapsedTimer cleanup;
		cleanup.start();
		do {
			if (! QueryInformationJobObject(job.get(), JobObjectBasicAccountingInformation, &accounting,
			                                sizeof(accounting), nullptr))
				return processFailure("Cannot confirm sandbox shutdown.");
			if (accounting.ActiveProcesses == 0) {
				job.reset();
				return true;
			}
			QThread::msleep(10);
		} while (cleanup.elapsed() < 5000);
		return processFailure("Sandbox processes did not stop within the cleanup deadline.", WAIT_TIMEOUT);
	}

	// Directory handles use MAXIMUM_ALLOWED to suppress recursive ACL propagation.
	// Check every existing file ourselves so the preparation budget remains effective.
	bool grantFile(const QString &path, PSID sid, DWORD access = FILE_GENERIC_READ | FILE_GENERIC_EXECUTE) {
		if (! checkBudget())
			return false;
		const bool privateFile = sid == packageSid;
		auto file = openAclFile(path, privateFile);
		if (! file)
			return fail("Cannot open sandbox resource.", path);
		BY_HANDLE_FILE_INFORMATION info{};
		if (! GetFileInformationByHandle(file.get(), &info))
			return fail("Cannot inspect sandbox resource.", path);
		if (privateFile &&
		    ((info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) || info.nNumberOfLinks > 1))
			return fail(QObject::tr("Sandbox files must not be reparse points or hard links: %1").arg(path));
		PSECURITY_DESCRIPTOR descriptor = nullptr;
		PACL acl = nullptr, updated = nullptr;
		auto cleanup = qScopeGuard([&] {
			LocalFree(descriptor);
			LocalFree(updated);
		});
		if (privateFile) {
			if (! ConvertStringSecurityDescriptorToSecurityDescriptorW(wide(privateSddl.arg(access, 0, 16)),
			                                                           SDDL_REVISION_1, &descriptor, nullptr))
				return fail("Cannot construct sandbox permissions.", path);
			BOOL present = FALSE, defaulted = FALSE;
			GetSecurityDescriptorDacl(descriptor, &present, &acl, &defaulted);
		} else {
			DWORD status = GetSecurityInfo(file.get(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr,
			                               nullptr, &acl, nullptr, &descriptor);
			if (status != ERROR_SUCCESS)
				return fail("Cannot read runtime permissions.", path, status);
			BYTE allPackages[SECURITY_MAX_SID_SIZE];
			DWORD sidSize = sizeof(allPackages);
			CreateWellKnownSid(WinBuiltinAnyPackageSid, nullptr, allPackages, &sidSize);
			if (hasGrant(acl, sid, access) || hasGrant(acl, allPackages, access))
				return true;
			EXPLICIT_ACCESSW entry{};
			entry.grfAccessPermissions = access;
			entry.grfAccessMode = GRANT_ACCESS;
			// Existing runtime files are granted individually; new files must not inherit temporary grants.
			entry.grfInheritance = NO_INHERITANCE;
			BuildTrusteeWithSidW(&entry.Trustee, sid);
			status = SetEntriesInAclW(1, &entry, acl, &updated);
			if (status != ERROR_SUCCESS)
				return fail("Cannot construct runtime permissions.", path, status);
			acl = updated;
		}
		const DWORD status = SetSecurityInfo(file.get(), SE_FILE_OBJECT,
		                                     DACL_SECURITY_INFORMATION |
		                                         (privateFile ? PROTECTED_DACL_SECURITY_INFORMATION : 0),
		                                     nullptr, nullptr, acl, nullptr);
		if (status != ERROR_SUCCESS)
			return fail("Cannot set sandbox permissions. Use a user-owned runtime installation or have its "
			            "owner prepare read and execute access.",
			            path, status);
		if (! privateFile) {
			QByteArray savedSid(GetLengthSid(sid), Qt::Uninitialized);
			CopySid(DWORD(savedSid.size()), savedSid.data(), sid);
			session->grants.push_back({path, std::move(savedSid)});
			++updates;
		}
		return true;
	}
	PSID addCapability(const QString &name) {
		const auto derive = reinterpret_cast<decltype(&DeriveCapabilitySidsFromName)>(
		    GetProcAddress(GetModuleHandleW(L"kernelbase.dll"), "DeriveCapabilitySidsFromName"));
		if (! derive) {
			fail("Cannot load named sandbox capability API.", {});
			return nullptr;
		}
		PSID *groups = nullptr, *sids = nullptr;
		DWORD groupCount = 0, count = 0;
		if (! derive(wide(name), &groups, &groupCount, &sids, &count)) {
			fail("Cannot derive sandbox capability.", name);
			return nullptr;
		}
		PSID result = count ? sids[0] : nullptr;
		for (DWORD i = 0; i < groupCount; ++i)
			LocalFree(groups[i]);
		for (DWORD i = 1; i < count; ++i)
			LocalFree(sids[i]);
		LocalFree(groups);
		LocalFree(sids);
		if (result)
			capabilityList.push_back({result, SE_GROUP_ENABLED});
		return result;
	}

	bool prepareRuntime(const QString &root, const QString &tool, bool dllsOnly) {
		const auto key = hash("lemonlime.runtime.v1:" + root.toLower() + (dllsOnly ? ":dlls" : ":tree"));
		PSID sid = addCapability("lemonlime.runtime." + session->id + '.' + key);
		if (! sid)
			return false;
		const auto cacheKey = key + '/' + hash(tool.toLower());
		const auto expected = stamp(tool) + ':' + stamp(root);
		const int previous = updates;
		if (! grantFile(root, sid))
			return false;
		if (session->runtimes.value(cacheKey) == expected && updates == previous)
			return true;
		hit = false;
		QDirIterator iterator(root, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
		                      dllsOnly ? QDirIterator::NoIteratorFlags : QDirIterator::Subdirectories);
		while (iterator.hasNext()) {
			if (! checkBudget())
				return false;
			iterator.next();
			const auto info = iterator.fileInfo();
			if (dllsOnly && (info.isDir() || info.suffix().compare("dll", Qt::CaseInsensitive) != 0))
				continue;
			if (! grantFile(info.filePath(), sid))
				return false;
		}
		session->runtimes[cacheKey] = expected;
		return true;
	}

	bool discoverJava(const QString &tool, QStringList &roots) {
		QDir home(QFileInfo(tool).absolutePath());
		if (home.dirName().compare("bin", Qt::CaseInsensitive) == 0)
			home.cdUp();
		if (! QFileInfo::exists(home.filePath("lib")))
			return fail(QObject::tr("Select java.exe inside a Java runtime installation: %1").arg(tool));
		roots.append(home.absolutePath());
		return true;
	}

	bool discoverPython(const QString &tool, QStringList &roots) {
		const auto key = tool.toLower() + ':' + stamp(tool);
		roots = session->pythonRoots.value(key);
		if (roots.isEmpty()) {
			// Only probe the explicitly configured, trusted interpreter. Never execute a submission here.
			QTemporaryDir discoveryDirectory;
			if (! discoveryDirectory.isValid())
				return fail("Cannot create Python discovery directory.", {});
			QProcess probe;
			probe.setWorkingDirectory(discoveryDirectory.path());
			probe.setProcessEnvironment(QProcessEnvironment::systemEnvironment());
			probe.start(tool,
			            {"-I", "-S", "-c",
			             "import sys,json;print(json.dumps([sys.base_prefix,sys.prefix,sys.executable]))"});
			if (! probe.waitForStarted(1000))
				return fail("Cannot inspect the selected Python interpreter.", tool, ERROR_FILE_NOT_FOUND);
			while (! probe.waitForFinished(25)) {
				if (! checkBudget())
					return false;
				if (probe.bytesAvailable() > 65536)
					return fail(QObject::tr("Python runtime discovery produced excessive output."));
			}
			const auto values = QJsonDocument::fromJson(probe.readAllStandardOutput()).array();
			if (probe.exitStatus() != QProcess::NormalExit || probe.exitCode() != 0 || values.size() != 3)
				return fail(QObject::tr("Cannot discover the Python runtime: %1\n%2")
				                .arg(tool, QString::fromUtf8(probe.readAllStandardError().left(2048))));
			roots = {values[0].toString(), values[1].toString(),
			         QFileInfo(values[2].toString()).absolutePath()};
			session->pythonRoots[key] = roots;
		}
		QDir venv(QFileInfo(tool).absolutePath());
		if (venv.dirName().compare("Scripts", Qt::CaseInsensitive) == 0 && venv.cdUp() &&
		    QFileInfo::exists(venv.filePath("pyvenv.cfg")))
			roots.append(venv.absolutePath());
		return true;
	}

	bool discover(const QString &tool, QStringList &roots) {
		if (runtime == SandboxSettings::Java)
			return discoverJava(tool, roots);
		if (runtime == SandboxSettings::Python)
			return discoverPython(tool, roots);
		if (! tool.isEmpty())
			roots.append(QFileInfo(tool).absolutePath());
		return true;
	}

	bool prepare() {
		timer.start();
		if (! checkBudget())
			return false;
		QString tool = config.runtimeExecutable;
		if (! tool.isEmpty() && QFileInfo(tool).isRelative())
			tool = QStandardPaths::findExecutable(
			    tool, config.environment.value("PATH").split(';', Qt::SkipEmptyParts));
		tool = canonical(tool);
		if (! config.runtimeExecutable.isEmpty() && tool.isEmpty())
			return fail(QObject::tr("The configured sandbox runtime executable does not exist: %1")
			                .arg(config.runtimeExecutable));
		runtime = config.sandboxSettings.runtime;
		if (runtime == SandboxSettings::Automatic) {
			const auto name = QFileInfo(tool).baseName().toLower();
			runtime = name.startsWith("python") || name.startsWith("pypy") ? SandboxSettings::Python
			          : name == "java"                                     ? SandboxSettings::Java
			                                                               : SandboxSettings::Native;
		}
		while (! permissionsMutex.tryLock(25)) {
			if (! checkBudget())
				return false;
		}
		auto unlock = qScopeGuard([&] { permissionsMutex.unlock(); });
		const auto firstGrant = session->grants.size();
		const auto previousRuntimes = session->runtimes;
		bool runtimeReady = false;
		auto rollback = qScopeGuard([&] {
			if (! runtimeReady) {
				while (session->grants.size() > firstGrant) {
					auto &grant = session->grants.back();
					revokeSid(grant.path, grant.sid.data());
					session->grants.pop_back();
				}
				session->runtimes = previousRuntimes;
			}
		});
		QStringList roots;
		if (! discover(tool, roots))
			return false;
		QStringList seen;
		for (const auto &root : roots) {
			const auto resolved = canonical(root);
			if (seen.contains(resolved, Qt::CaseInsensitive))
				continue;
			seen.append(resolved);
			if (! prepareRuntime(resolved, tool, runtime == SandboxSettings::Native))
				return false;
		}
		for (const auto &root : config.sandboxSettings.readOnlyDirectories) {
			const auto resolved = canonical(root);
			if (! prepareRuntime(resolved, tool, false))
				return false;
			seen.append(resolved);
		}
		if (! tool.isEmpty() && runtime != SandboxSettings::Native) {
			bool covered = false;
			for (const auto &root : seen)
				covered = covered || within(tool, root);
			if (! covered) {
				// A launcher outside the runtime (for example uv) needs access to its own file.
				PSID sid = addCapability("lemonlime.launcher." + session->id + '.' + hash(tool.toLower()));
				if (! sid || ! grantFile(tool, sid))
					return false;
			}
		}

		runtimeReady = true;
		unlock.dismiss();
		permissionsMutex.unlock();
		HANDLE rawToken = nullptr;
		if (! OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &rawToken))
			return fail("Cannot read the current user identity.", {});
		Handle token(rawToken);
		DWORD size = 0;
		GetTokenInformation(token.get(), TokenUser, nullptr, 0, &size);
		std::vector<BYTE> tokenBuffer(size);
		if (! GetTokenInformation(token.get(), TokenUser, tokenBuffer.data(), size, &size))
			return fail("Cannot read the current user identity.", {});
		wchar_t *userText = nullptr;
		if (! ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER *>(tokenBuffer.data())->User.Sid, &userText))
			return fail("Cannot encode the current user identity.", {});
		const auto userSid = QString::fromWCharArray(userText);
		LocalFree(userText);
		profileName = "LemonLime.Run." + QUuid::createUuid().toString(QUuid::Id128);
		const HRESULT status = CreateAppContainerProfile(wide(profileName), wide(profileName),
		                                                 L"LemonLime judging", nullptr, 0, &packageSid);
		if (FAILED(status))
			return fail("Cannot create AppContainer.", {}, DWORD(status));
		wchar_t *packageText = nullptr;
		if (! ConvertSidToStringSidW(packageSid, &packageText))
			return fail("Cannot encode sandbox identity.", {});
		privateSddl = QString("D:P(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)(A;OICI;FA;;;%1)(A;OICI;0x%3;;;%2)")
		                  .arg(userSid, QString::fromWCharArray(packageText));
		LocalFree(packageText);
		const auto work = canonical(config.workingDirectory);
		if (work.size() <= 3 || work.startsWith("//"))
			return fail(
			    QObject::tr("A private local working directory is required for the Windows sandbox."));
		if (! QDir(work).mkpath(".sandbox-tmp") || ! QDir(work).mkpath(".sandbox-home"))
			return fail("Cannot create private sandbox directories.", work);
		workingDirectory = work;
		constexpr DWORD writable = FILE_GENERIC_READ | FILE_GENERIC_WRITE | FILE_GENERIC_EXECUTE | DELETE;
		if (! grantFile(work, packageSid, writable))
			return false;
		const auto namedInput = canonical(QDir(work).filePath(config.inputFileName));
		QDirIterator files(work, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
		                   QDirIterator::Subdirectories);
		while (files.hasNext()) {
			const auto path = files.next();
			const bool inputOnly = ! config.standardInputCheck && ! config.inputFileName.isEmpty() &&
			                       path.compare(namedInput, Qt::CaseInsensitive) == 0;
			if (! grantFile(path, packageSid, inputOnly ? FILE_GENERIC_READ : writable))
				return false;
		}
		const auto system = QProcessEnvironment::systemEnvironment();
		// Preserve explicitly configured variables, not the host's entire environment.
		childEnvironment = config.runtimeEnvironment;
		for (const auto &name :
		     {"SystemRoot", "WINDIR", "SystemDrive", "NUMBER_OF_PROCESSORS", "PROCESSOR_ARCHITECTURE"})
			if (system.contains(name))
				childEnvironment.insert(name, system.value(name));
		QStringList path;
		if (! tool.isEmpty())
			path.append(QFileInfo(tool).absolutePath());
		for (const auto &root : seen) {
			path.append(root);
			if (QFileInfo::exists(root + "/bin"))
				path.append(root + "/bin");
		}
		path.append(system.value("SystemRoot") + "/System32");
		path.removeDuplicates();
		childEnvironment.insert("PATH", QDir::toNativeSeparators(path.join(';')));
		for (const auto &name : {"TEMP", "TMP"})
			childEnvironment.insert(name, QDir::toNativeSeparators(work + "/.sandbox-tmp"));
		for (const auto &name : {"USERPROFILE", "LOCALAPPDATA", "APPDATA"})
			childEnvironment.insert(name, QDir::toNativeSeparators(work + "/.sandbox-home"));
		return checkBudget();
	}
};

std::shared_ptr<WindowsSandboxSession> WindowsSandbox::createSession() {
	QMutexLocker lock(&sessionMutex);
	auto session = activeSession.lock();
	if (! session) {
		session = std::make_shared<WindowsSandboxSession>();
		activeSession = session;
	}
	return session;
}

WindowsSandbox::WindowsSandbox(const ProcessRunnerConfig &config, const std::atomic<bool> &stop)
    : data(std::make_unique<Data>(config, stop)) {}
WindowsSandbox::~WindowsSandbox() = default;
bool WindowsSandbox::prepare(QString &error) {
	const bool result = data->prepare();
	error = data->error;
	return result;
}
bool WindowsSandbox::prepareProcess(STARTUPINFOEXW &startup, QString &error) {
	const bool result = data->prepareProcess(startup);
	error = data->error;
	return result;
}
bool WindowsSandbox::stopProcesses(QString &error) {
	const bool result = data->stopProcesses();
	error = data->error;
	return result;
}
const QProcessEnvironment &WindowsSandbox::environment() const { return data->childEnvironment; }
int WindowsSandbox::aclUpdates() const { return data->updates; }
bool WindowsSandbox::cacheHit() const { return data->hit; }
#endif
