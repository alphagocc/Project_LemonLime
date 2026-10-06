/*
 * SPDX-FileCopyrightText: 2026 Project LemonLime
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include "settingscontroller.h"
#include "base/LemonTranslator.hpp"
#include "base/compiler.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QStyleHints>
#include <QThread>
#include <cmath>

namespace {
	void clearSettings(Settings &settings) {
		while (! settings.getCompilerList().isEmpty())
			settings.deleteCompiler(0);
		while (! settings.getColorThemeList().isEmpty())
			settings.deleteColorTheme(0);
	}

	bool numberInRange(const QVariant &value, double minimum, double maximum, bool integer = false) {
		bool ok = false;
		const double number = value.toDouble(&ok);
		return ok && std::isfinite(number) && number >= minimum && number <= maximum &&
		       (! integer || std::floor(number) == number);
	}

	bool validExtensions(const QString &value) {
		static const QRegularExpression expression(QStringLiteral("^(\\w+;)*\\w+$"));
		return expression.match(value).hasMatch();
	}

	QVariantMap compilerMap(const Compiler &compiler) {
		QVariantList configurations;
		for (int i = 0; i < compiler.getConfigurationNames().size(); ++i) {
			configurations.append(
			    QVariantMap{{"name", compiler.getConfigurationNames().at(i)},
			                {"compilerArguments", compiler.getCompilerArguments().value(i)},
			                {"interpreterArguments", compiler.getInterpreterArguments().value(i)}});
		}
		QVariantList environment;
		auto names = compiler.getEnvironment().keys();
		names.sort();
		for (const auto &name : names)
			environment.append(QVariantMap{{"name", name}, {"value", compiler.getEnvironment().value(name)}});
		const auto &sandbox = compiler.getSandboxSettings();
		return {{"name", compiler.getCompilerName()},
		        {"type", int(compiler.getCompilerType())},
		        {"sourceExtensions", compiler.getSourceExtensions().join(';')},
		        {"compilerLocation", compiler.getCompilerLocation()},
		        {"interpreterLocation", compiler.getInterpreterLocation()},
		        {"bytecodeExtensions", compiler.getBytecodeExtensions().join(';')},
		        {"timeLimitRatio", compiler.getTimeLimitRatio()},
		        {"memoryLimitRatio", compiler.getMemoryLimitRatio()},
		        {"disableMemoryLimitCheck", compiler.getDisableMemoryLimitCheck()},
		        {"interpreterAsWatcher", compiler.getInterpreterAsWatcher()},
		        {"sandboxEnabled", sandbox.enabled},
		        {"sandboxRuntime", int(sandbox.runtime)},
		        {"sandboxDirectories", sandbox.readOnlyDirectories.join('\n')},
		        {"sandboxTimeLimit", sandbox.preparationTimeLimit / 1000.0},
		        {"configurations", configurations},
		        {"environment", environment}};
	}

	std::unique_ptr<Compiler> makeCompiler(const QVariantMap &value) {
		auto compiler = std::make_unique<Compiler>();
		compiler->setCompilerName(value.value("name").toString());
		compiler->setCompilerType(Compiler::CompilerType(value.value("type").toInt()));
		compiler->setSourceExtensions(value.value("sourceExtensions").toString());
		compiler->setCompilerLocation(value.value("compilerLocation").toString());
		compiler->setInterpreterLocation(value.value("interpreterLocation").toString());
		compiler->setBytecodeExtensions(value.value("bytecodeExtensions").toString());
		compiler->setTimeLimitRatio(value.value("timeLimitRatio").toDouble());
		compiler->setMemoryLimitRatio(value.value("memoryLimitRatio").toDouble());
		compiler->setDisableMemoryLimitCheck(value.value("disableMemoryLimitCheck").toBool());
		compiler->setInterpreterAsWatcher(value.value("interpreterAsWatcher").toBool());
		for (const auto &item : value.value("configurations").toList()) {
			const auto configuration = item.toMap();
			compiler->addConfiguration(configuration.value("name").toString(),
			                           configuration.value("compilerArguments").toString(),
			                           configuration.value("interpreterArguments").toString());
		}
		QProcessEnvironment environment;
		for (const auto &item : value.value("environment").toList()) {
			const auto variable = item.toMap();
			environment.insert(variable.value("name").toString(), variable.value("value").toString());
		}
		compiler->setEnvironment(environment);
		SandboxSettings sandbox;
		sandbox.enabled = value.value("sandboxEnabled").toBool();
		sandbox.runtime = SandboxSettings::Runtime(value.value("sandboxRuntime").toInt());
		sandbox.preparationTimeLimit = qRound(value.value("sandboxTimeLimit").toDouble() * 1000);
		for (const auto &line : value.value("sandboxDirectories").toString().split('\n')) {
			if (! line.trimmed().isEmpty())
				sandbox.readOnlyDirectories.append(line.trimmed());
		}
		compiler->setSandboxSettings(sandbox);
		return compiler;
	}

	QVariantMap themeMap(const ColorTheme &theme) {
		QVariantMap result{{"name", theme.getName()}};
		const QList<QPair<QString, hslTuple>> colors{{"maximum", theme.getMxColor()},
		                                             {"minimum", theme.getMiColor()},
		                                             {"noFile", theme.getNfColor()},
		                                             {"compileError", theme.getCeColor()}};
		for (const auto &color : colors) {
			result.insert(color.first + "H", color.second.h);
			result.insert(color.first + "S", color.second.s);
			result.insert(color.first + "L", color.second.l);
		}
		const auto compensation = theme.getGrandComp();
		const auto rate = theme.getGrandRate();
		result.insert("compensationH", compensation.h);
		result.insert("compensationS", compensation.s);
		result.insert("compensationL", compensation.l);
		result.insert("rateH", rate.h);
		result.insert("rateS", rate.s);
		result.insert("rateL", rate.l);
		return result;
	}

	ColorTheme makeTheme(const QVariantMap &value) {
		auto color = [&value](const QString &name) {
			return hslTuple(value.value(name + "H").toInt(), value.value(name + "S").toDouble(),
			                value.value(name + "L").toDouble());
		};
		auto adjustment = [&value](const QString &name) {
			return dddTuple(value.value(name + "H").toDouble(), value.value(name + "S").toDouble(),
			                value.value(name + "L").toDouble());
		};
		ColorTheme theme;
		theme.setName(value.value("name").toString());
		theme.setColor(color("maximum"), color("minimum"), color("noFile"), color("compileError"),
		               adjustment("compensation"), adjustment("rate"));
		return theme;
	}

	QString uniqueName(const QString &base, const QStringList &names) {
		QString name = base;
		for (int suffix = 2; names.contains(name); ++suffix)
			name = QStringLiteral("%1 (%2)").arg(base).arg(suffix);
		return name;
	}
} // namespace

SettingsController::SettingsController(Settings *settings, QObject *parent)
    : QObject(parent), originalSettings(settings) {
	startEditing();
}

SettingsController::~SettingsController() { clearSettings(editSettings); }

Settings *SettingsController::settings() const { return originalSettings; }
QVariantMap SettingsController::general() const { return generalDraft; }
bool SettingsController::dirty() const { return modified; }
QString SettingsController::error() const { return errorMessage; }
int SettingsController::validationPage() const { return errorPage; }
int SettingsController::compilerIndex() const { return selectedCompiler; }
int SettingsController::themeIndex() const { return selectedTheme; }
QVariantMap SettingsController::compiler() const { return compilerDrafts.value(selectedCompiler).toMap(); }
QVariantMap SettingsController::theme() const { return themeDrafts.value(selectedTheme).toMap(); }

bool SettingsController::windows() const {
#ifdef Q_OS_WIN
	return true;
#else
	return false;
#endif
}

QVariantMap SettingsController::limits() const {
	return {{"fullScore", Settings::upperBoundForFullScore()},
	        {"time", Settings::upperBoundForTimeLimit()},
	        {"memory", Settings::upperBoundForMemoryLimit()},
	        {"fileSize", Settings::upperBoundForFileSizeLimit()},
	        {"rejudge", Settings::upperBoundForRejudgeTimes()},
	        {"threads", qMax(1, QThread::idealThreadCount() * 2)},
	        {"extraTime", Settings::upperBoundForExtraTimeRatio()}};
}

QStringList SettingsController::languages() const {
	return LemonLimeTranslator ? LemonLimeTranslator->GetAvailableLanguages() : QStringList();
}

QStringList SettingsController::compilerNames() const {
	QStringList result;
	for (const auto &value : compilerDrafts)
		result.append(value.toMap().value("name").toString());
	return result;
}

QStringList SettingsController::themeNames() const {
	QStringList result;
	for (const auto &value : themeDrafts)
		result.append(value.toMap().value("name").toString());
	return result;
}

void SettingsController::startEditing() {
	editSettings.copyFrom(originalSettings);
	generalDraft = {{"defaultFullScore", editSettings.getDefaultFullScore()},
	                {"defaultTimeLimit", editSettings.getDefaultTimeLimit()},
	                {"defaultExtraTimeRatio", editSettings.getDefaultExtraTimeRatio()},
	                {"defaultMemoryLimit", editSettings.getDefaultMemoryLimit()},
	                {"compileTimeLimit", editSettings.getCompileTimeLimit()},
	                {"specialJudgeTimeLimit", editSettings.getSpecialJudgeTimeLimit()},
	                {"fileSizeLimit", editSettings.getFileSizeLimit()},
	                {"rejudgeTimes", editSettings.getRejudgeTimes()},
	                {"maxJudgingThreads", editSettings.getMaxJudgingThreads()},
	                {"preventSleepWhileJudging", editSettings.getPreventSleepWhileJudging()},
	                {"defaultInputFileExtension", editSettings.getDefaultInputFileExtension()},
	                {"defaultOutputFileExtension", editSettings.getDefaultOutputFileExtension()},
	                {"inputFileExtensions", editSettings.getInputFileExtensions().join(';')},
	                {"outputFileExtensions", editSettings.getOutputFileExtensions().join(';')},
	                {"language", editSettings.getUiLanguage()},
	                {"splashTime", editSettings.getSplashTime()},
	                {"colorScheme", int(editSettings.getColorScheme())}};
	compilerDrafts.clear();
	for (const auto *item : editSettings.getCompilerList())
		compilerDrafts.append(compilerMap(*item));
	themeDrafts.clear();
	for (const auto *item : editSettings.getColorThemeList())
		themeDrafts.append(themeMap(*item));
	selectedCompiler = compilerDrafts.isEmpty() ? -1 : 0;
	selectedTheme = themeDrafts.isEmpty()
	                    ? -1
	                    : qBound(0, editSettings.getCurrentColorThemeIndex(), int(themeDrafts.size()) - 1);
	modified = false;
	errorMessage.clear();
	emit generalChanged();
	emit compilerNamesChanged();
	emit compilerChanged();
	emit themeNamesChanged();
	emit themeChanged();
	emit dirtyChanged();
	emit errorChanged();
}

void SettingsController::cancel() {
	startEditing();
	emit editingCancelled();
}

void SettingsController::markDirty() {
	if (! modified) {
		modified = true;
		emit dirtyChanged();
	}
	if (! errorMessage.isEmpty()) {
		errorMessage.clear();
		emit errorChanged();
	}
}

bool SettingsController::fail(const QString &message) {
	errorMessage = message;
	emit errorChanged();
	return false;
}

void SettingsController::setGeneral(const QString &key, const QVariant &value) {
	if (! generalDraft.contains(key) || generalDraft.value(key) == value)
		return;
	generalDraft.insert(key, value);
	markDirty();
	emit generalChanged();
}

void SettingsController::setCompilerIndex(int index) {
	if (index < 0 || index >= compilerDrafts.size() || selectedCompiler == index)
		return;
	selectedCompiler = index;
	emit compilerChanged();
}

void SettingsController::setCompilerValue(const QString &key, const QVariant &value) {
	auto current = compiler();
	if (! current.contains(key) || current.value(key) == value)
		return;
	current.insert(key, value);
	compilerDrafts[selectedCompiler] = current;
	markDirty();
	if (key == "name")
		emit compilerNamesChanged();
	emit compilerChanged();
}

QVariantMap SettingsController::compilerPreset(const QString &preset) const {
	Compiler added;
	added.setCompilerName(preset.isEmpty() ? tr("New compiler") : preset);
	QString stackArgument;
#ifdef Q_OS_WIN
	stackArgument = QStringLiteral(" -Wl,--stack=2147483647");
#endif
	if (preset == "gcc" || preset == "g++") {
		added.setSourceExtensions(preset == "gcc" ? "c" : "cpp;cc;cxx");
		added.setCompilerLocation(QStandardPaths::findExecutable(preset));
		const auto arguments = QStringLiteral("-o %s %s.* -lm") + stackArgument;
		added.addConfiguration("default", arguments, "");
		const QStringList standards = preset == "gcc"
		                                  ? QStringList{"c89", "c99", "c11", "c17"}
		                                  : QStringList{"c++98", "c++03", "c++11", "c++14", "c++17", "c++20"};
		for (const auto &standard : standards) {
			added.addConfiguration(standard.toUpper(), arguments + " -std=" + standard, "");
			added.addConfiguration(standard.toUpper() + " O2", arguments + " -std=" + standard + " -O2", "");
			if (standard == "c++17")
				added.addConfiguration("C++17 O3", arguments + " -std=c++17 -O3", "");
#ifdef Q_OS_LINUX
			added.addConfiguration(standard.toUpper() + " UB Catching",
			                       arguments + " -std=" + standard + " -fsanitize=undefined", "");
#endif
		}
		const auto newest = standards.last();
		added.addConfiguration(newest.toUpper() + " O3", arguments + " -std=" + newest + " -O3", "");
#ifdef Q_OS_WIN
		if (! added.getCompilerLocation().isEmpty()) {
			QProcessEnvironment environment;
			environment.insert(
			    "PATH", QDir::toNativeSeparators(QFileInfo(added.getCompilerLocation()).absolutePath()));
			added.setEnvironment(environment);
		}
#endif
	} else if (preset == "fpc" || preset == "fbc") {
		added.setSourceExtensions(preset == "fpc" ? "pas;pp;inc" : "bas");
		added.setCompilerLocation(QStandardPaths::findExecutable(preset));
		added.addConfiguration("default", "%s.*", "");
		if (preset == "fpc")
			added.addConfiguration("O2", "%s.* -O2", "");
	} else if (preset == "Java") {
		added.setCompilerName("jdk");
		added.setCompilerType(Compiler::InterpretiveWithByteCode);
		added.setSourceExtensions("java");
		added.setBytecodeExtensions("class");
		added.setCompilerLocation(QStandardPaths::findExecutable("javac"));
		added.setInterpreterLocation(QStandardPaths::findExecutable("java"));
		added.setTimeLimitRatio(5);
		added.setDisableMemoryLimitCheck(true);
		added.addConfiguration("default", "%s.*", "-Xmx1024m %s");
	} else if (preset == "Python") {
		added.setCompilerName("python");
		added.setCompilerType(Compiler::InterpretiveWithoutByteCode);
		added.setSourceExtensions("py");
		added.setInterpreterLocation(QStandardPaths::findExecutable("python"));
		added.setTimeLimitRatio(10);
		added.setMemoryLimitRatio(5);
		added.addConfiguration("default", "", "%s.*");
	} else {
		added.addConfiguration("default", "", "");
	}
	return compilerMap(added);
}

void SettingsController::addCompiler(const QString &preset) {
	auto added = compilerPreset(preset);
	added.insert("name", uniqueName(added.value("name").toString(), compilerNames()));
	compilerDrafts.append(added);
	selectedCompiler = compilerDrafts.size() - 1;
	markDirty();
	emit compilerNamesChanged();
	emit compilerChanged();
}

bool SettingsController::addCompilerDefinitions(const QVariantList &definitions) {
	if (definitions.isEmpty())
		return true;
	for (const auto &definition : definitions) {
		if (! validateCompiler(definition.toMap()))
			return false;
	}
	for (const auto &definition : definitions) {
		auto value = definition.toMap();
		value.insert("name", uniqueName(value.value("name").toString(), compilerNames()));
		compilerDrafts.append(value);
	}
	selectedCompiler = compilerDrafts.size() - 1;
	markDirty();
	emit compilerNamesChanged();
	emit compilerChanged();
	return true;
}

bool SettingsController::replaceCompiler(const QVariantMap &definition) {
	if (selectedCompiler < 0)
		return false;
	const auto translated = [](const char *text) {
		return QCoreApplication::translate("AdvancedCompilerSettingsDialog", text);
	};
	const auto type = Compiler::CompilerType(definition.value("type").toInt());
	if (type != Compiler::InterpretiveWithoutByteCode &&
	    definition.value("compilerLocation").toString().isEmpty())
		return fail(translated("Empty compiler's Location!"));
	if (type != Compiler::Typical && definition.value("interpreterLocation").toString().isEmpty())
		return fail(translated("Empty interpreter's Location!"));
	if (type == Compiler::InterpretiveWithByteCode &&
	    definition.value("bytecodeExtensions").toString().isEmpty())
		return fail(translated("Empty Byte-code Extensions!"));
	QSet<QString> configurationNames;
	for (const auto &item : definition.value("configurations").toList()) {
		const auto name = item.toMap().value("name").toString();
		if (name.isEmpty())
			return fail(translated("Empty configuration name!"));
		if (configurationNames.contains(name))
			return fail(translated("Configuration %1 appears more than once!").arg(name));
		if (name == "disable")
			return fail(translated("Invalid configuration name \"disable\"!"));
		configurationNames.insert(name);
	}
	compilerDrafts[selectedCompiler] = definition;
	markDirty();
	emit compilerNamesChanged();
	emit compilerChanged();
	return true;
}

bool SettingsController::replaceTheme(const QVariantMap &definition) {
	if (selectedTheme < 0 || ! validateTheme(definition))
		return false;
	themeDrafts[selectedTheme] = definition;
	markDirty();
	emit themeNamesChanged();
	emit themeChanged();
	return true;
}

void SettingsController::duplicateCompiler() {
	if (selectedCompiler < 0)
		return;
	auto value = compiler();
	value.insert("name", uniqueName(value.value("name").toString(), compilerNames()));
	compilerDrafts.append(value);
	selectedCompiler = compilerDrafts.size() - 1;
	markDirty();
	emit compilerNamesChanged();
	emit compilerChanged();
}

void SettingsController::deleteCompiler() {
	if (selectedCompiler < 0)
		return;
	compilerDrafts.removeAt(selectedCompiler);
	selectedCompiler = qMin(selectedCompiler, int(compilerDrafts.size()) - 1);
	markDirty();
	emit compilerNamesChanged();
	emit compilerChanged();
}

void SettingsController::moveCompiler(int offset) {
	const int next = selectedCompiler + offset;
	if (selectedCompiler < 0 || next < 0 || next >= compilerDrafts.size())
		return;
	compilerDrafts.move(selectedCompiler, next);
	selectedCompiler = next;
	markDirty();
	emit compilerNamesChanged();
	emit compilerChanged();
}

void SettingsController::addConfiguration() {
	if (selectedCompiler < 0)
		return;
	auto configurations = compiler().value("configurations").toList();
	QStringList names;
	for (const auto &value : configurations)
		names.append(value.toMap().value("name").toString());
	configurations.append(QVariantMap{{"name", uniqueName(tr("New configuration"), names)},
	                                  {"compilerArguments", ""},
	                                  {"interpreterArguments", ""}});
	setCompilerValue("configurations", configurations);
}

void SettingsController::deleteConfiguration(int index) {
	auto configurations = compiler().value("configurations").toList();
	if (index <= 0 || index >= configurations.size())
		return;
	configurations.removeAt(index);
	setCompilerValue("configurations", configurations);
}

void SettingsController::setConfiguration(int index, const QString &key, const QVariant &value) {
	auto configurations = compiler().value("configurations").toList();
	if (index < 0 || index >= configurations.size() || (index == 0 && key == "name"))
		return;
	auto configuration = configurations.at(index).toMap();
	if (! configuration.contains(key))
		return;
	configuration.insert(key, value);
	configurations[index] = configuration;
	setCompilerValue("configurations", configurations);
}

bool SettingsController::setEnvironmentVariable(const QString &oldName, const QString &name,
                                                const QString &value) {
	if (selectedCompiler < 0)
		return false;
	if (name.trimmed().isEmpty() || name.contains('=') || name.contains(QChar::Null) ||
	    value.contains(QChar::Null))
		return fail(tr("An environment variable needs a name without '=' or null characters."));
	auto environment = compiler().value("environment").toList();
	int oldIndex = -1;
	for (int i = 0; i < environment.size(); ++i) {
		const auto currentName = environment.at(i).toMap().value("name").toString();
		const auto sensitivity = windows() ? Qt::CaseInsensitive : Qt::CaseSensitive;
		if (currentName == oldName)
			oldIndex = i;
		else if (currentName.compare(name, sensitivity) == 0)
			return fail(tr("Environment variable %1 already exists.").arg(name));
	}
	const QVariantMap entry{{"name", name}, {"value", value}};
	if (oldIndex < 0)
		environment.append(entry);
	else
		environment[oldIndex] = entry;
	setCompilerValue("environment", environment);
	return true;
}

void SettingsController::removeEnvironmentVariable(const QString &name) {
	auto environment = compiler().value("environment").toList();
	for (int i = 0; i < environment.size(); ++i) {
		if (environment.at(i).toMap().value("name").toString() == name) {
			environment.removeAt(i);
			setCompilerValue("environment", environment);
			return;
		}
	}
}

void SettingsController::setThemeIndex(int index) {
	if (index < 0 || index >= themeDrafts.size() || selectedTheme == index)
		return;
	selectedTheme = index;
	markDirty();
	emit themeChanged();
}

void SettingsController::addTheme() {
	ColorTheme added;
	added.setName(QCoreApplication::translate("VisualMainSettings", "New Theme"));
	auto value = themeMap(added);
	themeDrafts.append(value);
	selectedTheme = themeDrafts.size() - 1;
	markDirty();
	emit themeNamesChanged();
	emit themeChanged();
}

void SettingsController::deleteTheme() {
	if (themeDrafts.size() <= 1 || selectedTheme < 0)
		return;
	themeDrafts.removeAt(selectedTheme);
	selectedTheme = qMin(selectedTheme, int(themeDrafts.size()) - 1);
	markDirty();
	emit themeNamesChanged();
	emit themeChanged();
}

void SettingsController::setThemeValue(const QString &key, const QVariant &value) {
	auto current = theme();
	if (! current.contains(key) || current.value(key) == value)
		return;
	current.insert(key, value);
	themeDrafts[selectedTheme] = current;
	markDirty();
	if (key == "name")
		emit themeNamesChanged();
	emit themeChanged();
}

QVariantList SettingsController::themePreview() const {
	QVariantList result;
	if (selectedTheme < 0)
		return result;
	// Keep the preview bounded while numeric fields contain partially entered values.
	auto draft = theme();
	for (auto iterator = draft.begin(); iterator != draft.end(); ++iterator) {
		if (iterator.key() != "name" && ! numberInRange(iterator.value(), -720, 720))
			return result;
	}
	const auto colorTheme = makeTheme(draft);
	for (int percentage = 0; percentage <= 100; percentage += 25) {
		result.append(QVariantMap{{"label", QString::number(percentage) + "%"},
		                          {"color", colorTheme.getColorPer(percentage / 100.0)},
		                          {"totalColor", colorTheme.getColorGrand(percentage / 100.0)}});
	}
	result.append(QVariantMap{{"label", tr("No file")},
	                          {"color", colorTheme.getColorNf()},
	                          {"totalColor", colorTheme.getColorNf()}});
	result.append(QVariantMap{{"label", tr("Compile error")},
	                          {"color", colorTheme.getColorCe()},
	                          {"totalColor", colorTheme.getColorCe()}});
	return result;
}

bool SettingsController::validateCompiler(const QVariantMap &value) {
	const auto name = value.value("name").toString();
	if (name.trimmed().isEmpty())
		return fail(tr("Enter a compiler name."));
	if (! validExtensions(value.value("sourceExtensions").toString()))
		return fail(tr("%1: enter source extensions separated by semicolons.").arg(name));
	if (! numberInRange(value.value("type"), 0, 2, true))
		return fail(tr("%1: select a compiler type.").arg(name));
	const int type = value.value("type").toInt();
	if (type != Compiler::InterpretiveWithoutByteCode &&
	    value.value("compilerLocation").toString().trimmed().isEmpty())
		return fail(tr("%1: enter the compiler executable.").arg(name));
	if (type != Compiler::Typical && value.value("interpreterLocation").toString().trimmed().isEmpty())
		return fail(tr("%1: enter the interpreter executable.").arg(name));
	if (type == Compiler::InterpretiveWithByteCode &&
	    ! validExtensions(value.value("bytecodeExtensions").toString()))
		return fail(tr("%1: enter bytecode extensions separated by semicolons.").arg(name));
	if (! numberInRange(value.value("timeLimitRatio"), 0.1, 999.9) ||
	    ! numberInRange(value.value("memoryLimitRatio"), 0.1, 999.9))
		return fail(tr("%1: time and memory ratios must be between 0.1 and 999.9.").arg(name));
	if (! numberInRange(value.value("sandboxRuntime"), 0, 3, true) ||
	    ! numberInRange(value.value("sandboxTimeLimit"), 1, 120))
		return fail(
		    tr("%1: select a sandbox runtime and a preparation limit from 1 to 120 seconds.").arg(name));
	const auto configurations = value.value("configurations").toList();
	if (configurations.isEmpty() || configurations.first().toMap().value("name").toString() != "default")
		return fail(tr("%1: the first configuration must be named 'default'.").arg(name));
	QSet<QString> names;
	for (const auto &item : configurations) {
		const auto configurationName = item.toMap().value("name").toString();
		if (configurationName.trimmed().isEmpty() || configurationName == "disable" ||
		    names.contains(configurationName))
			return fail(tr("%1: configuration names must be unique, nonempty, and different from 'disable'.")
			                .arg(name));
		names.insert(configurationName);
	}
	for (const auto &item : value.value("environment").toList()) {
		const auto variable = item.toMap();
		const auto variableName = variable.value("name").toString();
		if (variableName.trimmed().isEmpty() || variableName.contains('=') ||
		    variableName.contains(QChar::Null) || variable.value("value").toString().contains(QChar::Null))
			return fail(tr("%1: check environment variable names and values.").arg(name));
	}
	return true;
}

bool SettingsController::validateTheme(const QVariantMap &value) {
	for (const auto &prefix : {QStringLiteral("maximum"), QStringLiteral("minimum"), QStringLiteral("noFile"),
	                           QStringLiteral("compileError")}) {
		const int hueLimit = prefix == "maximum" ? 720 : prefix == "minimum" ? 719 : 359;
		if (! numberInRange(value.value(prefix + "H"), 0, hueLimit, true) ||
		    ! numberInRange(value.value(prefix + "S"), 0, 100) ||
		    ! numberInRange(value.value(prefix + "L"), 0, 100))
			return fail(
			    tr("%1: check hue, saturation and lightness values.").arg(value.value("name").toString()));
	}
	for (const auto &suffix : {QStringLiteral("H"), QStringLiteral("S"), QStringLiteral("L")}) {
		const int limit = suffix == "H" ? 360 : 100;
		if (! numberInRange(value.value("compensation" + suffix), -limit, limit) ||
		    ! numberInRange(value.value("rate" + suffix), -10, 10))
			return fail(tr("%1: check total-score compensation and rate values.")
			                .arg(value.value("name").toString()));
	}
	return true;
}

bool SettingsController::apply() {
	errorPage = 0;
	const QMap<QString, QString> labels{{"defaultFullScore", tr("Default full score")},
	                                    {"defaultTimeLimit", tr("Default time limit")},
	                                    {"defaultMemoryLimit", tr("Default memory limit")},
	                                    {"compileTimeLimit", tr("Compilation limit")},
	                                    {"specialJudgeTimeLimit", tr("Special judge limit")},
	                                    {"fileSizeLimit", tr("Source size limit")},
	                                    {"rejudgeTimes", tr("Maximum rejudge attempts")},
	                                    {"maxJudgingThreads", tr("Concurrent judging threads")},
	                                    {"splashTime", tr("Splash duration")}};
	const QVariantMap integerLimits{{"defaultFullScore", Settings::upperBoundForFullScore()},
	                                {"defaultTimeLimit", Settings::upperBoundForTimeLimit()},
	                                {"defaultMemoryLimit", Settings::upperBoundForMemoryLimit()},
	                                {"compileTimeLimit", Settings::upperBoundForTimeLimit()},
	                                {"specialJudgeTimeLimit", Settings::upperBoundForTimeLimit()},
	                                {"fileSizeLimit", Settings::upperBoundForFileSizeLimit()},
	                                {"rejudgeTimes", Settings::upperBoundForRejudgeTimes()},
	                                {"maxJudgingThreads", limits().value("threads")},
	                                {"splashTime", 3000}};
	for (auto iterator = integerLimits.begin(); iterator != integerLimits.end(); ++iterator) {
		const int minimum = iterator.key() == "rejudgeTimes" || iterator.key() == "splashTime" ? 0 : 1;
		if (! numberInRange(generalDraft.value(iterator.key()), minimum, iterator.value().toInt(), true))
			return fail(tr("%1 must be an integer between %2 and %3.")
			                .arg(labels.value(iterator.key()))
			                .arg(minimum)
			                .arg(iterator.value().toInt()));
	}
	if (! numberInRange(generalDraft.value("defaultExtraTimeRatio"), 0,
	                    Settings::upperBoundForExtraTimeRatio()))
		return fail(tr("The extra time ratio must be between 0 and %1.")
		                .arg(Settings::upperBoundForExtraTimeRatio()));
	for (const auto &key :
	     {QStringLiteral("inputFileExtensions"), QStringLiteral("outputFileExtensions"),
	      QStringLiteral("defaultInputFileExtension"), QStringLiteral("defaultOutputFileExtension")}) {
		if (! generalDraft.value(key).toString().isEmpty() &&
		    ! validExtensions(generalDraft.value(key).toString()))
			return fail(tr("Enter file extensions separated by semicolons."));
	}
	if (! numberInRange(generalDraft.value("colorScheme"), 0, 2, true))
		return fail(tr("Select an appearance mode."));
	errorPage = 1;
	QSet<QString> names;
	for (int i = 0; i < compilerDrafts.size(); ++i) {
		const auto value = compilerDrafts.at(i).toMap();
		const auto name = value.value("name").toString();
		if (names.contains(name)) {
			setCompilerIndex(i);
			return fail(QCoreApplication::translate("CompilerSettings", "Compiler %1 appears more than once!")
			                .arg(name));
		}
		names.insert(name);
	}
	for (int i = 0; i < compilerDrafts.size(); ++i) {
		const auto value = compilerDrafts.at(i).toMap();
		if (value.value("name").toString().isEmpty()) {
			setCompilerIndex(i);
			return fail(QCoreApplication::translate("CompilerSettings", "Empty compiler name!"));
		}
		if (value.value("sourceExtensions").toString().isEmpty()) {
			setCompilerIndex(i);
			return fail(QCoreApplication::translate("CompilerSettings", "Empty source file extensions!"));
		}
	}
	if (themeDrafts.isEmpty())
		return fail(tr("Keep at least one result color theme."));
	for (const auto &value : themeDrafts) {
		if (! validateTheme(value.toMap()))
			return false;
	}
	editSettings.setDefaultFullScore(generalDraft.value("defaultFullScore").toInt());
	editSettings.setDefaultTimeLimit(generalDraft.value("defaultTimeLimit").toInt());
	editSettings.setDefaultExtraTimeRatio(generalDraft.value("defaultExtraTimeRatio").toDouble());
	editSettings.setDefaultMemoryLimit(generalDraft.value("defaultMemoryLimit").toInt());
	editSettings.setCompileTimeLimit(generalDraft.value("compileTimeLimit").toInt());
	editSettings.setSpecialJudgeTimeLimit(generalDraft.value("specialJudgeTimeLimit").toInt());
	editSettings.setFileSizeLimit(generalDraft.value("fileSizeLimit").toInt());
	editSettings.setRejudgeTimes(generalDraft.value("rejudgeTimes").toInt());
	editSettings.setMaxJudgingThreads(generalDraft.value("maxJudgingThreads").toInt());
	editSettings.setPreventSleepWhileJudging(generalDraft.value("preventSleepWhileJudging").toBool());
	editSettings.setDefaultInputFileExtension(generalDraft.value("defaultInputFileExtension").toString());
	editSettings.setDefaultOutputFileExtension(generalDraft.value("defaultOutputFileExtension").toString());
	editSettings.setInputFileExtensions(generalDraft.value("inputFileExtensions").toString());
	editSettings.setOutputFileExtensions(generalDraft.value("outputFileExtensions").toString());
	editSettings.setUiLanguage(generalDraft.value("language").toString());
	editSettings.setSplashTime(generalDraft.value("splashTime").toInt());
	editSettings.setColorScheme(Qt::ColorScheme(generalDraft.value("colorScheme").toInt()));
	clearSettings(editSettings);
	for (const auto &value : compilerDrafts)
		editSettings.addCompiler(makeCompiler(value.toMap()).release());
	for (const auto &value : themeDrafts)
		editSettings.addColorTheme(new ColorTheme(makeTheme(value.toMap())));
	editSettings.setCurrentColorThemeIndex(selectedTheme);
	originalSettings->copyFrom(&editSettings);
	originalSettings->saveSettings();
	QGuiApplication::styleHints()->setColorScheme(originalSettings->getColorScheme());
	modified = false;
	errorMessage.clear();
	emit dirtyChanged();
	emit errorChanged();
	emit settingsApplied();
	return true;
}

QString SettingsController::localFile(const QUrl &url) const {
	return QDir::toNativeSeparators(url.isLocalFile() ? url.toLocalFile() : url.toString());
}

QString SettingsController::executableDirectory(const QString &file) const {
	return QDir::toNativeSeparators(QFileInfo(file).absolutePath());
}

bool SettingsController::importCompilers(const QUrl &url) {
	QFile file(localFile(url));
	if (! file.open(QIODevice::ReadOnly))
		return fail(tr("Cannot open compiler settings: %1").arg(file.errorString()));
	QJsonParseError parseError;
	const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
	if (parseError.error != QJsonParseError::NoError)
		return fail(tr("Invalid compiler JSON: %1").arg(parseError.errorString()));
	QJsonArray input;
	if (document.isArray())
		input = document.array();
	else if (document.object().value("compilers").isArray())
		input = document.object().value("compilers").toArray();
	else if (document.object().contains("compilerName"))
		input.append(document.object());
	if (input.isEmpty())
		return fail(tr("The file contains no compiler definitions."));
	QVariantList imported;
	auto names = compilerNames();
	for (const auto &item : input) {
		if (! item.isObject())
			return fail(tr("Each compiler definition must be a JSON object."));
		auto object = item.toObject();
		if (! object.contains("environment") && object.contains("environment.toStringList()"))
			object.insert("environment", object.value("environment.toStringList()"));
		Compiler importedCompiler;
		importedCompiler.read(object);
		if (importedCompiler.getConfigurationNames().size() !=
		        importedCompiler.getCompilerArguments().size() ||
		    importedCompiler.getConfigurationNames().size() !=
		        importedCompiler.getInterpreterArguments().size())
			return fail(tr("A compiler has mismatched configuration and argument lists."));
		auto value = compilerMap(importedCompiler);
		if (! validateCompiler(value))
			return false;
		const auto name = uniqueName(value.value("name").toString(), names);
		value.insert("name", name);
		names.append(name);
		imported.append(value);
	}
	compilerDrafts.append(imported);
	selectedCompiler = compilerDrafts.size() - imported.size();
	markDirty();
	emit compilerNamesChanged();
	emit compilerChanged();
	return true;
}

bool SettingsController::exportCompilers(const QUrl &url) {
	QJsonArray output;
	for (const auto &value : compilerDrafts) {
		if (! validateCompiler(value.toMap()))
			return false;
		const auto compiler = makeCompiler(value.toMap());
		QJsonObject object;
		compiler->write(object);
		object.remove("environment.toStringList()");
		object.insert("environment", QJsonArray::fromStringList(compiler->getEnvironment().toStringList()));
		output.append(object);
	}
	QSaveFile file(localFile(url));
	if (! file.open(QIODevice::WriteOnly))
		return fail(tr("Cannot export compiler settings: %1").arg(file.errorString()));
	const auto data = QJsonDocument(QJsonObject{{"compilers", output}}).toJson();
	if (file.write(data) != data.size() || ! file.commit())
		return fail(tr("Cannot save compiler settings: %1").arg(file.errorString()));
	errorMessage.clear();
	emit errorChanged();
	return true;
}
