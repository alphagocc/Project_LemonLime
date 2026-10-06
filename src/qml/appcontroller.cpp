/*
 * SPDX-FileCopyrightText: 2026 Project LemonLime
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "appcontroller.h"
#include "base/settings.h"
#include "core/contest.h"
#include "core/contestant.h"
#include "core/task.h"
#include "core/testcase.h"
#include "resultdetails.h"
#include <QByteArrayView>
#include <QDesktopServices>
#include <QDirIterator>
#include <QFileSystemWatcher>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSettings>
#include <algorithm>
#include <memory>

namespace {
	void destroyContest(Contest *contest) {
		if (! contest)
			return;
		for (auto *task : contest->getTaskList())
			qDeleteAll(task->getTestCaseList());
		qDeleteAll(contest->getTaskList());
		qDeleteAll(contest->getContestantList());
		delete contest;
	}
	QString resultText(int state) {
		QString text, color, background;
		Settings::setTextAndColor(static_cast<ResultState>(state), text, color, background);
		return text;
	}
	bool validContest(const QJsonObject &object) {
		if (! object.value("tasks").isArray() || ! object.value("contestants").isArray())
			return false;
		const int count = object.value("tasks").toArray().size();
		QSet<QString> names;
		for (const auto &value : object.value("contestants").toArray()) {
			const auto c = value.toObject();
			const auto name = c.value("contestantName").toString();
			if (name.isEmpty() || names.contains(name))
				return false;
			names.insert(name);
			for (const auto *key : {"checkJudged", "compileState", "sourceFile", "compileMesaage",
			                        "inputFiles", "result", "message", "score", "timeUsed", "memoryUsed"})
				if (! c.value(key).isArray() || c.value(key).toArray().size() != count)
					return false;
		}
		return true;
	}
} // namespace

AppController::AppController(Settings *settings, QObject *parent)
    : QObject(parent), settings(settings), model(settings, this), initialDirectory(QDir::currentPath()),
      watcher(new QFileSystemWatcher(this)) {
	autosave.setInterval(30000);
	connect(&autosave, &QTimer::timeout, this, [this] {
		if (contest && ! isJudging)
			saveContest();
	});
	autosave.start();
	watcherTimer.setSingleShot(true);
	watcherTimer.setInterval(300);
	connect(watcher, &QFileSystemWatcher::directoryChanged, this, [this] { watcherTimer.start(); });
	connect(&watcherTimer, &QTimer::timeout, this, [this] {
		if (! isJudging) {
			watchDirectories();
			emit dataFilesChanged();
		}
	});
	connect(qApp, &QGuiApplication::paletteChanged, &model, [this] {
		if (! isJudging)
			model.refresh();
	});
}
AppController::~AppController() { destroyContest(contest); }
Contest *AppController::getContest() const { return contest; }
bool AppController::hasContest() const { return contest != nullptr; }
QString AppController::contestTitle() const { return contest ? contest->getContestTitle() : QString(); }
QString AppController::contestFile() const { return currentFile; }
QStringList AppController::recentContests() const { return settings->getRecentContest(); }
QVariantList AppController::recentEntries() const {
	QVariantList entries;
	const auto files = settings->getRecentContest();
	for (int i = 0; i < files.size(); ++i) {
		QFile file(files[i]);
		if (! file.open(QIODevice::ReadOnly))
			continue;
		QByteArray bytes = file.readAll();
		QString title;
		if (bytes.trimmed().startsWith('{')) {
			const auto document = QJsonDocument::fromJson(bytes);
			if (! document.isObject())
				continue;
			title = document.object().value("contestTitle").toString();
		} else {
			QDataStream stream(bytes);
			quint32 magic = 0;
			quint16 checksum = 0;
			qint32 length = 0;
			stream >> magic >> checksum >> length;
			if (magic != MagicNumber || length <= 0 || length > bytes.size() - 10)
				continue;
			const auto compressed = bytes.mid(10, length);
			if (qChecksum(QByteArrayView(compressed)) != checksum)
				continue;
			QDataStream data(qUncompress(compressed));
			data >> title;
		}
		entries.append(
		    QVariantMap{{"title", title}, {"file", QDir::toNativeSeparators(files[i])}, {"index", i}});
	}
	return entries;
}
bool AppController::addRecent(const QUrl &url) {
	if (! url.isLocalFile())
		return false;
	QFile file(url.toLocalFile());
	if (! file.open(QIODevice::ReadOnly)) {
		emit errorOccurred(tr("Cannot open selected file"));
		return false;
	}
	const auto bytes = file.readAll();
	if (! QJsonDocument::fromJson(bytes).isObject() && ! bytes.startsWith(QByteArray::fromHex("20111127"))) {
		emit errorOccurred(tr("Broken contest data file"));
		return false;
	}
	auto files = settings->getRecentContest();
	const auto name = QFileInfo(file).absoluteFilePath();
	files.removeAll(name);
	files.prepend(name);
	settings->setRecentContest(files);
	settings->saveSettings();
	emit recentChanged();
	return true;
}
QString AppController::defaultContestPath(const QString &name) const {
	return QDir::toNativeSeparators(QDir::home().filePath(name));
}
QString AppController::localFile(const QUrl &url) const {
	return QDir::toNativeSeparators(url.toLocalFile());
}
bool AppController::judging() const { return isJudging; }
bool AppController::stopping() const { return isStopping; }
int AppController::taskCount() const { return contest ? contest->getTaskList().size() : 0; }
int AppController::fullScore() const { return contest ? contest->getTotalScore() : 0; }
int AppController::completed() const { return done; }
int AppController::total() const { return scheduled; }
QStringList AppController::progressLog() const { return log; }
QString AppController::progressHtml() const {
	QString html;
	for (const auto &line : log)
		html += "<p style=\"margin-left:15px;font-size:10pt\">" + line.toHtmlEscaped() + "</p>";
	return html;
}
int AppController::progressValue() const { return currentProgress; }
int AppController::progressMaximum() const { return maximumProgress; }
QString AppController::detailHtml() const { return details; }
ResultModel *AppController::results() { return &model; }
QString AppController::version() const { return QStringLiteral(LEMON_VERSION_STRING); }
int AppController::splashDuration() const { return settings->getSplashTime(); }
QSize AppController::windowSize() const {
	QSettings values(QSettings::defaultFormat(), QSettings::UserScope, "LemonLime", "lemon");
	return values.value("WindowSize", QSize(800, 600)).toSize();
}
void AppController::saveWindowSize(int width, int height) {
	QSettings values(QSettings::defaultFormat(), QSettings::UserScope, "LemonLime", "lemon");
	values.setValue("WindowSize", QSize(width, height));
}
QUrl AppController::localUrl(const QString &file) const { return QUrl::fromLocalFile(file); }

bool AppController::newContest(const QString &title, const QUrl &directory, const QString &fileName) {
	if (isJudging)
		return false;
	QString name = fileName.trimmed();
	if (! directory.isLocalFile() || title.trimmed().isEmpty() || name.isEmpty() || name == "." ||
	    name == ".." || name.contains('/') || name.contains('\\') || name.contains(':')) {
		emit errorOccurred(tr("Enter a contest title, a local folder and a valid file name."));
		return false;
	}
	if (! name.endsWith(".cdf", Qt::CaseInsensitive))
		name += ".cdf";
	const QDir dir(directory.toLocalFile());
	const auto file = dir.absoluteFilePath(name);
	if (QFileInfo::exists(file)) {
		emit errorOccurred(tr("The contest file already exists."));
		return false;
	}
	if (! QDir().mkpath(dir.absolutePath()) || ! QDir().mkpath(dir.filePath("data")) ||
	    ! QDir().mkpath(dir.filePath("source"))) {
		emit errorOccurred(tr("Cannot create the contest folders."));
		return false;
	}
	if (! closeContest())
		return false;
	auto *created = new Contest;
	created->setSettings(settings);
	created->setContestTitle(title.trimmed());
	installContest(created, file);
	return saveContest();
}

bool AppController::openContest(const QUrl &url) {
	if (isJudging || ! url.isLocalFile())
		return false;
	const QString fileName = QFileInfo(url.toLocalFile()).absoluteFilePath();
	QFile file(fileName);
	if (! file.open(QIODevice::ReadOnly)) {
		emit errorOccurred(tr("Cannot open %1: %2").arg(fileName, file.errorString()));
		return false;
	}
	std::unique_ptr<Contest, decltype(&destroyContest)> loaded(new Contest, destroyContest);
	loaded->setSettings(settings);
	QByteArray bytes = file.readAll();
	if (bytes.startsWith("\xEF\xBB\xBF"))
		bytes.remove(0, 3);
	if (bytes.trimmed().startsWith('{')) {
		QJsonParseError error;
		const auto doc = QJsonDocument::fromJson(bytes, &error);
		if (error.error != QJsonParseError::NoError || ! doc.isObject() || ! validContest(doc.object()) ||
		    loaded->readFromJson(doc.object()) < 0) {
			emit errorOccurred(tr("The contest file is invalid or contains inconsistent result data."));
			return false;
		}
	} else {
		QDataStream input(bytes);
		quint32 magic = 0;
		quint16 checksum = 0;
		qint32 length = 0;
		input >> magic >> checksum >> length;
		if (magic != MagicNumber || length <= 0 || length > bytes.size() - 10) {
			emit errorOccurred(tr("The legacy contest file header is invalid."));
			return false;
		}
		QByteArray compressed(length, Qt::Uninitialized);
		if (input.readRawData(compressed.data(), length) != length ||
		    qChecksum(QByteArrayView(compressed)) != checksum) {
			emit errorOccurred(tr("The legacy contest checksum is invalid."));
			return false;
		}
		auto data = qUncompress(compressed);
		if (data.isEmpty()) {
			emit errorOccurred(tr("Cannot decompress the contest."));
			return false;
		}
		QDataStream stream(data);
		loaded->readFromStream(stream);
		QJsonObject check;
		loaded->writeToJson(check);
		if (stream.status() != QDataStream::Ok || ! validContest(check)) {
			emit errorOccurred(tr("The legacy contest data is invalid."));
			return false;
		}
	}
	if (! closeContest())
		return false;
	installContest(loaded.release(), fileName);
	return true;
}

void AppController::installContest(Contest *value, const QString &file) {
	contest = value;
	currentFile = file;
	QDir::setCurrent(QFileInfo(file).absolutePath());
	QDir().mkpath(Settings::dataPath());
	QDir().mkpath(Settings::sourcePath());
	QStringList recent = settings->getRecentContest();
	recent.removeAll(file);
	recent.prepend(file);
	settings->setRecentContest(recent);
	settings->saveSettings();
	connect(contest, &Contest::dialogAlert, this, &AppController::errorOccurred);
	connect(contest, &Contest::taskJudgingStarted, this, [this](const QString &message) {
		addLog(QCoreApplication::translate("JudgingDialog", "Start judging task %1").arg(message));
	});
	connect(contest, &Contest::taskJudgingFinished, this, [this] {
		++done;
		emit progressChanged();
	});
	connect(contest, &Contest::singleCaseFinished, this,
	        [this](const QString &name, int progress, int subtask, int test, int state, int score, int time,
	               qint64 memory) {
		        currentProgress += progress;
		        addLog(tr("%1 · %2.%3 · %4 · %5 pts · %6 ms · %7 MiB")
		                   .arg(name)
		                   .arg(subtask + 1)
		                   .arg(test + 1)
		                   .arg(resultText(state))
		                   .arg(score)
		                   .arg(time)
		                   .arg(memory / 1048576.0, 0, 'f', 2));
	        });
	connect(contest, &Contest::compileError, this, [this](int progress, int state) {
		currentProgress += progress;
		const char *text = state == NoValidSourceFile          ? "Cannot find valid source file"
		                   : state == NoValidGraderFile        ? "Main grader (grader.*) cannot be found"
		                   : state == CompileTimeLimitExceeded ? "Compile time limit exceeded"
		                   : state == InvalidCompiler          ? "Cannot run given compiler"
		                                                       : "Compile error";
		addLog(QCoreApplication::translate("JudgingDialog", text));
	});
	model.setContest(contest);
	watchDirectories();
	emit contestChanged();
	emit contentChanged();
	emit recentChanged();
}

bool AppController::saveContest() {
	if (! contest)
		return true;
	if (isJudging)
		return false;
	QJsonObject data;
	contest->writeToJson(data);
	QSaveFile file(currentFile);
	const auto bytes = QJsonDocument(data).toJson(QJsonDocument::Compact);
	if (! file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || ! file.commit()) {
		emit errorOccurred(tr("Cannot save %1: %2").arg(currentFile, file.errorString()));
		return false;
	}
	emit notification(tr("Contest saved"));
	return true;
}

bool AppController::closeContest() {
	if (isJudging || ! saveContest())
		return false;
	model.setContest(nullptr);
	auto *previous = contest;
	contest = nullptr;
	currentFile.clear();
	details.clear();
	detailName.clear();
	emit contestChanged();
	emit contentChanged();
	emit detailChanged();
	destroyContest(previous);
	QDir::setCurrent(initialDirectory);
	watchDirectories();
	return true;
}

void AppController::renameContest(const QString &title) {
	if (contest && ! isJudging && ! title.trimmed().isEmpty()) {
		contest->setContestTitle(title.trimmed());
		edited();
	}
}
void AppController::removeRecent(int index) {
	auto recent = settings->getRecentContest();
	if (index < 0 || index >= recent.size())
		return;
	recent.removeAt(index);
	settings->setRecentContest(recent);
	settings->saveSettings();
	emit recentChanged();
}
void AppController::refreshContestants() {
	if (! contest || isJudging)
		return;
	contest->refreshContestantList();
	edited();
}
void AppController::edited() {
	if (isJudging)
		return;
	model.refresh();
	emit contentChanged();
	if (! detailName.isEmpty())
		showDetails(detailName, detailTask);
}
void AppController::settingsApplied() {
	if (isJudging)
		return;
	if (contest)
		for (auto *task : contest->getTaskList())
			task->refreshCompilerConfiguration(settings);
	edited();
}
void AppController::watchDirectories() {
	if (! watcher->directories().isEmpty())
		watcher->removePaths(watcher->directories());
	if (! contest)
		return;
	QStringList dirs{QFileInfo(Settings::dataPath()).absoluteFilePath()};
	QDirIterator it(Settings::dataPath(), QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks,
	                QDirIterator::Subdirectories);
	while (it.hasNext())
		dirs.append(it.next());
	watcher->addPaths(dirs);
}

void AppController::judge(const QString &mode) {
	if (! contest || isJudging)
		return;
	QList<std::pair<QString, QVector<int>>> queue;
	const auto selected = model.selection();
	for (auto *c : contest->getContestantList()) {
		QVector<int> tasks;
		const auto cells = selected.value(c->getContestantName());
		for (int i = 0; i < taskCount(); ++i) {
			bool include =
			    mode == "all" || (mode == "selected" && (cells.contains(-1) || cells.contains(i))) ||
			    (mode == "unjudged" && ! c->getCheckJudged(i)) ||
			    (mode == "missing" && c->getCompileState(i) == NoValidSourceFile) ||
			    (mode == "failed" && contest->getTask(i)->getTaskType() != Task::AnswersOnly &&
			     c->getCompileState(i) != CompileSuccessfully && c->getCompileState(i) != NoValidSourceFile &&
			     c->getCompileState(i) != NoValidGraderFile);
			if (include)
				tasks.append(i);
		}
		if (! tasks.isEmpty())
			queue.append({c->getContestantName(), tasks});
	}
	runJudge(queue);
}
void AppController::rejudge(const QString &name, int task) {
	if (! contest || isJudging || ! contest->getContestant(name))
		return;
	QVector<int> tasks;
	if (task < 0)
		for (int i = 0; i < taskCount(); ++i)
			tasks.append(i);
	else if (task < taskCount())
		tasks.append(task);
	runJudge({{name, tasks}});
}
void AppController::runJudge(const QList<std::pair<QString, QVector<int>>> &queue) {
	scheduled = 0;
	currentProgress = 0;
	maximumProgress = 0;
	for (const auto &entry : queue)
		for (int task : entry.second)
			maximumProgress += contest->getTask(task)->getTotalTimeLimit();
	for (const auto &entry : queue)
		scheduled += entry.second.size();
	if (! scheduled) {
		emit notification(tr("No matching submissions to judge."));
		return;
	}
	done = 0;
	log.clear();
	isJudging = true;
	isStopping = false;
	judgeStarted = false;
	model.setBusy(true);
	emit judgingChanged();
	emit progressChanged();
	// Return to QML before entering the core's nested event loop, so progress controls can render.
	QTimer::singleShot(0, this, [this, queue] {
		if (! isStopping) {
			judgeStarted = true;
			contest->judge(queue);
			judgeStarted = false;
		}
		addLog(isStopping ? tr("Judging stopped") : tr("Judging finished"));
		isJudging = false;
		isStopping = false;
		model.setBusy(false);
		emit judgingChanged();
		edited();
		saveContest();
		watchDirectories();
	});
}
void AppController::stop() {
	if (! isJudging || isStopping)
		return;
	isStopping = true;
	if (judgeStarted)
		contest->stopJudgingSlot();
	emit judgingChanged();
}
void AppController::addLog(const QString &message) {
	log.append(message);
	if (log.size() > 2000)
		log.removeFirst();
	emit progressChanged();
}

void AppController::showDetails(const QString &name, int taskIndex) {
	if (! contest || isJudging)
		return;
	auto *contestant = contest->getContestant(name);
	if (! contestant) {
		details.clear();
		emit detailChanged();
		return;
	}
	detailName = name;
	detailTask = taskIndex;
	details = resultDetailsHtml(contest, contestant);
	emit detailChanged();
}

void AppController::detailLink(const QString &link) {
	if (! contest || isJudging)
		return;
	auto *contestant = contest->getContestant(detailName);
	if (! contestant)
		return;
	const auto parts = QUrl::fromPercentEncoding(link.toUtf8()).split(' ', Qt::SkipEmptyParts);
	if (parts.size() < 2)
		return;
	bool valid = false;
	const int task = parts[1].toInt(&valid);
	if (! valid || task < 0 || task >= taskCount())
		return;
	if (parts[0] == "Rejudge")
		rejudge(detailName, task);
	else if (parts[0] == "CompileMessage")
		emit messageRequested(QCoreApplication::translate("DetailDialog", "Compile Message"),
		                      contestant->getCompileMessage(task));
	else if (parts[0] == "Message" && parts.size() == 4)
		emit messageRequested(QCoreApplication::translate("DetailDialog", "Message"),
		                      contestant->getMessage(task).value(parts[2].toInt()).value(parts[3].toInt()));
}
void AppController::deleteSelected(bool removeFiles) {
	if (! contest || isJudging)
		return;
	const QDir source(Settings::sourcePath());
	for (const auto &name : model.selection().keys()) {
		if (removeFiles) {
			const QFileInfo target(source.filePath(name));
			if (name != target.fileName() || target.isSymLink() || target.canonicalFilePath().isEmpty() ||
			    QDir(target.canonicalFilePath()).absolutePath() !=
			        QDir(source.canonicalPath()).absoluteFilePath(name)) {
				edited();
				emit errorOccurred(tr("Cannot delete a source folder outside the contest."));
				return;
			}
			if (! QDir(target.absoluteFilePath()).removeRecursively()) {
				edited();
				emit errorOccurred(tr("Cannot delete %1").arg(name));
				return;
			}
		}
		contest->deleteContestant(name);
	}
	model.clearSelection();
	edited();
}
void AppController::openFolder() {
	if (contest)
		QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(currentFile).absolutePath()));
}
bool AppController::exportManual(const QUrl &destination) {
	if (! destination.isLocalFile())
		return false;
	QFile source(":/manual/llmanual.pdf");
	if (! source.exists())
		source.setFileName(QCoreApplication::applicationDirPath() + "/manual/llmanual.pdf");
	QSaveFile target(destination.toLocalFile());
	if (! source.open(QIODevice::ReadOnly) || ! target.open(QIODevice::WriteOnly)) {
		emit errorOccurred(tr("Cannot open the manual or destination file."));
		return false;
	}
	auto bytes = source.readAll();
	if (target.write(bytes) != bytes.size() || ! target.commit()) {
		emit errorOccurred(tr("Cannot export the manual."));
		return false;
	}
	emit notification(tr("Manual exported"));
	return true;
}
bool AppController::prepareExit() {
	if (isJudging)
		return false;
	if (! saveContest())
		return false;
	settings->saveSettings();
	return true;
}
