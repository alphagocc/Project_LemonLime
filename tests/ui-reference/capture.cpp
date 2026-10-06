/*
 * SPDX-FileCopyrightText: 2026 Project LemonLime
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <QtCore>
#include <QtGui>
#include <QtWidgets>

#include "lemon.h"

#include "addcompilerwizard.h"
#include "addtaskdialog.h"
#include "addtestcaseswizard.h"
#include "advancedcompilersettingsdialog.h"
#include "base/LemonLog.hpp"
#include "base/LemonTranslator.hpp"
#include "base/compiler.h"
#include "base/settings.h"
#include "core/contest.h"
#include "core/contestant.h"
#include "core/task.h"
#include "core/testcase.h"
#include "detaildialog.h"
#include "editvariabledialog.h"
#include "environmentvariablesdialog.h"
#include "exttestcasemodifierdialog.h"
#include "exttestcaseupdaterdialog.h"
#include "judgingdialog.h"
#include "newcontestdialog.h"
#include "opencontestdialog.h"
#include "optionsdialog.h"
#include "resultviewer.h"
#include "spdlog/sinks/null_sink.h"
#include "summarytree.h"
#include "taskeditwidget.h"
#include "testcaseeditwidget.h"
#include "themeeditdialog.h"
#include "welcomedialog.h"

namespace {
	QJsonArray rectJson(const QRect &rect) { return {rect.x(), rect.y(), rect.width(), rect.height()}; }

	QJsonObject fontJson(const QFont &font) {
		const QFontInfo resolved(font);
		return {{"family", font.family()},
		        {"resolvedFamily", resolved.family()},
		        {"pointSize", font.pointSizeF()},
		        {"pixelSize", font.pixelSize()},
		        {"resolvedPixelSize", resolved.pixelSize()},
		        {"weight", int(font.weight())},
		        {"bold", font.bold()},
		        {"italic", font.italic()}};
	}

	QJsonObject layoutJson(QLayout *layout) {
		QJsonObject result;
		if (! layout)
			return result;
		const QMargins margins = layout->contentsMargins();
		result = {{"class", layout->metaObject()->className()},
		          {"name", layout->objectName()},
		          {"geometry", rectJson(layout->geometry())},
		          {"spacing", layout->spacing()},
		          {"margins", QJsonArray{margins.left(), margins.top(), margins.right(), margins.bottom()}}};
		QJsonArray items;
		for (int index = 0; index < layout->count(); ++index) {
			auto *item = layout->itemAt(index);
			QJsonObject entry{{"geometry", rectJson(item->geometry())},
			                  {"alignment", int(item->alignment())}};
			if (item->widget())
				entry["widget"] = item->widget()->objectName();
			if (item->layout())
				entry["layout"] = layoutJson(item->layout());
			if (auto *box = qobject_cast<QBoxLayout *>(layout))
				entry["stretch"] = box->stretch(index);
			if (auto *grid = qobject_cast<QGridLayout *>(layout)) {
				int row, column, rowSpan, columnSpan;
				grid->getItemPosition(index, &row, &column, &rowSpan, &columnSpan);
				entry["gridPosition"] = QJsonArray{row, column, rowSpan, columnSpan};
			}
			items.append(entry);
		}
		result["items"] = items;
		return result;
	}

	QJsonObject widgetJson(QWidget *widget, QWidget *root) {
		const QPoint origin = widget == root ? QPoint() : widget->mapTo(root, QPoint());
		const auto sizePolicy = widget->sizePolicy();
		QJsonObject result{
		    {"class", widget->metaObject()->className()},
		    {"name", widget->objectName()},
		    {"parent", widget->parent() ? widget->parent()->objectName() : QString()},
		    {"geometry", rectJson(widget->geometry())},
		    {"rootGeometry", rectJson(QRect(origin, widget->size()))},
		    {"minimumSize", QJsonArray{widget->minimumWidth(), widget->minimumHeight()}},
		    {"sizeHint", QJsonArray{widget->sizeHint().width(), widget->sizeHint().height()}},
		    {"visible", widget->isVisibleTo(root)},
		    {"enabled", widget->isEnabled()},
		    {"font", fontJson(widget->font())},
		    {"sizePolicy", QJsonArray{int(sizePolicy.horizontalPolicy()), int(sizePolicy.verticalPolicy()),
		                              sizePolicy.horizontalStretch(), sizePolicy.verticalStretch()}},
		    {"layout", layoutJson(widget->layout())}};
		for (const auto *property :
		     {"text", "title", "windowTitle", "placeholderText", "checked", "currentIndex", "currentText",
		      "value", "minimum", "maximum", "toolTip", "statusTip", "wordWrap", "alignment"}) {
			const QVariant value = widget->property(property);
			if (value.isValid())
				result[property] = QJsonValue::fromVariant(value);
		}
		if (auto *tabs = qobject_cast<QTabBar *>(widget)) {
			QJsonArray values;
			for (int index = 0; index < tabs->count(); ++index)
				values.append(QJsonObject{{"text", tabs->tabText(index)},
				                          {"geometry", rectJson(tabs->tabRect(index))}});
			result["tabs"] = values;
		}
		if (auto *combo = qobject_cast<QComboBox *>(widget)) {
			QJsonArray values;
			for (int index = 0; index < combo->count(); ++index)
				values.append(combo->itemText(index));
			result["items"] = values;
		}
		if (auto *label = qobject_cast<QLabel *>(widget))
			result["margin"] = label->margin();
		if (auto *table = qobject_cast<QTableWidget *>(widget)) {
			QJsonArray columns;
			for (int index = 0; index < table->columnCount(); ++index)
				columns.append(QJsonObject{{"width", table->columnWidth(index)},
				                           {"text", table->horizontalHeaderItem(index)
				                                        ? table->horizontalHeaderItem(index)->text()
				                                        : QString()}});
			result["columns"] = columns;
			result["rowCount"] = table->rowCount();
			result["rowHeight"] =
			    table->rowCount() ? table->rowHeight(0) : table->verticalHeader()->defaultSectionSize();
		}
		if (auto *menu = qobject_cast<QMenu *>(widget)) {
			QJsonArray actions;
			for (auto *action : menu->actions())
				actions.append(QJsonObject{{"text", action->text()},
				                           {"separator", action->isSeparator()},
				                           {"shortcut", action->shortcut().toString()},
				                           {"enabled", action->isEnabled()}});
			result["actions"] = actions;
		}
		return result;
	}

	class Capture {
	  public:
		explicit Capture(QString directory) : directory(std::move(directory)) {
			if (! QDir().mkpath(this->directory))
				qFatal("Cannot create reference output directory");
		}

		void take(QWidget &widget, const QString &name) {
			if (QGuiApplication::platformName() == QStringLiteral("windows"))
				widget.setAttribute(Qt::WA_DontShowOnScreen, true);
			widget.show();
			widget.ensurePolished();
			QCoreApplication::processEvents();
			if (widget.layout())
				widget.layout()->activate();
			QCoreApplication::sendPostedEvents();
			QCoreApplication::processEvents();
			const QPixmap picture = widget.grab();
			if (picture.isNull() || ! picture.save(QDir(directory).filePath(name + ".png")))
				qFatal("Cannot save reference capture");
			QJsonArray widgets;
			widgets.append(widgetJson(&widget, &widget));
			for (auto *child : widget.findChildren<QWidget *>()) {
				// Child dialogs have their own independent capture.
				if (child->window() == widget.window())
					widgets.append(widgetJson(child, &widget));
			}
			QJsonObject document{{"name", name},
			                     {"windowTitle", widget.windowTitle()},
			                     {"size", QJsonArray{widget.width(), widget.height()}},
			                     {"devicePixelRatio", picture.devicePixelRatio()},
			                     {"widgets", widgets}};
			QFile file(QDir(directory).filePath(name + ".json"));
			if (! file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(document).toJson()) < 0)
				qFatal("Cannot save reference geometry");
			captures.append(
			    QJsonObject{{"name", name}, {"width", widget.width()}, {"height", widget.height()}});
			qInfo().noquote() << name << widget.size();
		}

		QString directory;
		QJsonArray captures;
	};

	void sampleResult(Contestant *contestant, int task, int score,
	                  CompileState compile = CompileSuccessfully) {
		contestant->setCheckJudged(task, true);
		contestant->setCompileState(task, compile);
		contestant->setSourceFile(task, "solution.cpp");
		contestant->setInputFiles(task, {{"case.in"}});
		contestant->setResult(task, {{score == 100 ? CorrectAnswer
		                              : score > 0  ? PartlyCorrect
		                                           : WrongAnswer}});
		contestant->setMessage(task, {{QString()}});
		contestant->setTimeUsed(task, {{12}});
		contestant->setMemoryUsed(task, {{4 * 1024 * 1024}});
		contestant->setScore(task, {{score}});
		contestant->setJudgingTime(QDateTime(QDate(2026, 10, 6), QTime(8, 30)));
	}

	void setUpContest(LemonLime &window, const QString &directory) {
		window.newContest("Autumn contest", "contest", directory);
		if (! window.curContest)
			qFatal("Cannot create isolated reference contest");
		for (const auto *name : {"sum", "graph", "strings"}) {
			auto *task = new Task(window.curContest);
			task->setProblemTitle(name);
			task->setSourceFileName(name);
			task->setInputFileName(QString(name) + ".in");
			task->setOutputFileName(QString(name) + ".out");
			task->refreshCompilerConfiguration(window.settings);
			auto *testCase = new TestCase;
			testCase->setIndex(0);
			testCase->setFullScore(100);
			testCase->setTimeLimit(1000);
			testCase->setMemoryLimit(256);
			testCase->addSingleCase(QString(name) + ".in", QString(name) + ".out");
			task->addTestCase(testCase);
			window.curContest->addTask(task);
		}
		for (const auto *name : {"Ada", "Babbage", "Grace", "Linus"})
			if (! QDir().mkpath(Settings::sourcePath() + QDir::separator() + name))
				qFatal("Cannot create isolated contestant directory");
		window.curContest->refreshContestantList();
		const auto contestants = window.curContest->getContestantList();
		for (int index = 0; index < contestants.size(); ++index)
			for (int task = 0; task < 3; ++task)
				sampleResult(contestants[index], task, qMax(0, 100 - index * 20 - task * 10));
		sampleResult(window.curContest->getContestant("Linus"), 2, 0, CompileError);
		window.refreshSummary();
		window.findChild<ResultViewer *>("resultViewer")->refreshViewer();
	}
} // namespace

int main(int argc, char **argv) {
	QTemporaryDir isolation(QDir::tempPath() + "/lemon-ui-reference-XXXXXX");
	if (! isolation.isValid())
		return 1;
	const QString originalDirectory = QDir::currentPath();
	const QString settingsDirectory = isolation.filePath("settings");
	QDir().mkpath(settingsDirectory);
	QSettings::setDefaultFormat(QSettings::IniFormat);
	QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDirectory);
	QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, settingsDirectory);
	QStandardPaths::setTestModeEnabled(true);
	QDir::setCurrent(isolation.path());
	if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
		qputenv("QT_QPA_PLATFORM", "offscreen");
#ifdef Q_OS_WIN
	const QDir systemFonts(QDir(qEnvironmentVariable("WINDIR", "C:/Windows")).filePath("Fonts"));
	const bool offscreenFonts = qEnvironmentVariable("QT_QPA_PLATFORM").startsWith("offscreen");
	if (offscreenFonts) {
		const QString fontsDirectory = isolation.filePath("fonts");
		QDir().mkpath(fontsDirectory);
		for (const auto *font : {"msyh.ttc", "msyhbd.ttc", "segoeui.ttf", "segoeuib.ttf", "seguisym.ttf"})
			QFile::copy(systemFonts.filePath(font), QDir(fontsDirectory).filePath(font));
		qputenv("QT_QPA_FONTDIR", fontsDirectory.toUtf8());
	}
#endif
	QApplication application(argc, argv);
	QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Light);
	application.setQuitOnLastWindowClosed(false);
	QCoreApplication::setOrganizationName("LemonLimeUiReference");
	QCoreApplication::setApplicationName("ui-reference");
	QSettings probe(QSettings::IniFormat, QSettings::UserScope, "LemonLime", "lemon");
	if (QSettings::defaultFormat() != QSettings::IniFormat || probe.format() != QSettings::IniFormat ||
	    ! QDir::cleanPath(probe.fileName()).startsWith(QDir::cleanPath(settingsDirectory) + '/'))
		qFatal("Reference settings are not isolated; refusing to create any original UI");
	probe.setFallbacksEnabled(false);
	probe.setValue("UiLanguage", "zh_CN");
	probe.sync();
	if (probe.status() != QSettings::NoError)
		qFatal("Cannot write isolated settings");
	const QFont platformDefaultFont = QApplication::font();
#ifdef Q_OS_WIN
	if (offscreenFonts) {
		for (const auto *font : {"msyh.ttc", "msyhbd.ttc", "segoeui.ttf", "segoeuib.ttf", "seguisym.ttf"})
			QFontDatabase::addApplicationFont(systemFonts.filePath(font));
	}
	if (! QFontDatabase::hasFamily("Microsoft YaHei"))
		qFatal("Original Microsoft YaHei font is unavailable");
	QFont fonts;
	fonts.setFamily("Microsoft YaHei");
	fonts.setHintingPreference(QFont::PreferNoHinting);
	QApplication::setFont(fonts);
#endif
	Q_INIT_RESOURCE(resource);
	Lemon::base::logger =
	    std::make_shared<spdlog::logger>("ui-reference", std::make_shared<spdlog::sinks::null_sink_mt>());
	LemonLimeTranslator = std::make_unique<Lemon::common::LemonTranslator>();
	QTranslator translation;
	const bool translationLoaded = translation.load(QString::fromUtf8(UI_REFERENCE_TRANSLATION));
	if (! translationLoaded)
		qFatal("Cannot load original Chinese translation");
	application.installTranslator(&translation);

	QString output =
	    qEnvironmentVariable("LEMON_UI_REFERENCE_OUTPUT", QString::fromUtf8(UI_REFERENCE_OUTPUT_DIR));
	output = QDir(originalDirectory).absoluteFilePath(output);
	if (argc > 1)
		output = QDir(originalDirectory).absoluteFilePath(QString::fromLocal8Bit(argv[1]));
	Capture capture(output);
	QJsonObject manifest{
	    {"source", QString::fromUtf8(UI_REFERENCE_SOURCE_DIR)},
	    {"platform", QGuiApplication::platformName()},
	    {"dontShowOnScreen", QGuiApplication::platformName() == QStringLiteral("windows")},
	    {"style", QApplication::style()->objectName()},
	    {"styleClass", QApplication::style()->metaObject()->className()},
	    {"qtVersion", qVersion()},
	    {"platformDefaultFont", fontJson(platformDefaultFont)},
	    {"applicationFont", fontJson(QApplication::font())},
	    {"settingsFormat", "IniFormat"},
	    {"settingsFile", probe.fileName()},
	    {"translationLoaded", translationLoaded},
	    {"note", "Default QApplication platform style; no forced Fusion style. Original layouts and UI "
	             "logic, with only QSettings isolation in generated lemon.cpp."}};
	QJsonObject palette;
	for (const auto &role : {std::pair{"window", QPalette::Window},
	                         {"windowText", QPalette::WindowText},
	                         {"base", QPalette::Base},
	                         {"button", QPalette::Button},
	                         {"text", QPalette::Text},
	                         {"highlight", QPalette::Highlight}})
		palette[role.first] = QApplication::palette().color(role.second).name();
	manifest["palette"] = palette;

	{
		LemonLime window;
		window.autoSaveTimer.stop();
		capture.take(window, "main-empty");
		{
			WelcomeDialog dialog(&window);
			dialog.setRecentContest({});
			capture.take(dialog, "welcome-open");
			dialog.findChild<QTabWidget *>("tabWidget")->setCurrentIndex(1);
			capture.take(dialog, "welcome-new");
		}
		{
			NewContestDialog dialog(&window);
			capture.take(dialog, "new-contest");
		}
		{
			OpenContestDialog dialog(&window);
			dialog.setRecentContest({});
			capture.take(dialog, "open-contest");
		}
		{
			AddCompilerWizard wizard(&window);
			capture.take(wizard, "compiler-wizard-intro");
			wizard.findChild<QRadioButton *>("customRadioButton")->setChecked(true);
			wizard.next();
			capture.take(wizard, "compiler-wizard-custom");
			wizard.restart();
			wizard.findChild<QRadioButton *>("builtinRadioButton")->setChecked(true);
			wizard.next();
			capture.take(wizard, "compiler-wizard-detected");
		}
		auto *compiler = new Compiler;
		compiler->setCompilerName("GNU C++");
		compiler->setSourceExtensions("cpp;cc;cxx");
		compiler->setCompilerLocation("g++");
		compiler->addConfiguration("C++17", "-std=c++17 -O2 %s -o %e", "");
		window.settings->addCompiler(compiler);
		{
			OptionsDialog dialog(&window);
			dialog.resetEditSettings(window.settings);
			auto *tabs = dialog.findChild<QTabWidget *>("tabWidget");
			for (int index = 0; index < 3; ++index) {
				tabs->setCurrentIndex(index);
				capture.take(dialog,
				             QString("options-%1").arg(QStringList{"general", "compiler", "visual"}[index]));
			}
		}
		{
			AdvancedCompilerSettingsDialog dialog(&window);
			dialog.resetEditCompiler(compiler);
			capture.take(dialog, "compiler-advanced");
		}
		{
			ThemeEditDialog dialog(&window);
			ColorTheme theme = window.settings->getCurrentColorTheme();
			dialog.resetEditTheme(&theme);
			capture.take(dialog, "theme-editor");
		}
		{
			EnvironmentVariablesDialog dialog(&window);
			QProcessEnvironment environment;
			environment.insert("LANG", "zh_CN.UTF-8");
			dialog.setProcessEnvironment(environment);
			capture.take(dialog, "environment-variables");
		}
		{
			EditVariableDialog dialog(&window);
			dialog.setVariableName("LANG");
			dialog.setVariableValue("zh_CN.UTF-8");
			capture.take(dialog, "environment-variable-edit");
		}

		setUpContest(window, isolation.filePath("contest"));
		auto *tabs = window.findChild<QTabWidget *>("tabWidget");
		auto *summary = window.findChild<SummaryTree *>("summary");
		summary->expandAll();
		summary->setCurrentItem(nullptr);
		capture.take(window, "main-tasks-empty");
		summary->setCurrentItem(summary->topLevelItem(0));
		capture.take(window, "main-task");
		capture.take(*window.findChild<TaskEditWidget *>("taskEdit"), "task-editor");
		summary->setCurrentItem(summary->topLevelItem(0)->child(0));
		capture.take(window, "main-testcase");
		capture.take(*window.findChild<TestCaseEditWidget *>("testCaseEdit"), "testcase-editor");
		tabs->setCurrentIndex(1);
		capture.take(window, "main-results");
		tabs->setCurrentIndex(2);
		capture.take(window, "main-statistics");
		{
			DetailDialog dialog(&window);
			dialog.refreshViewer(window.curContest, window.curContest->getContestant("Ada"));
			capture.take(dialog, "result-detail");
		}
		{
			AddTestCasesWizard wizard(&window);
			wizard.setSettings(window.settings, true);
			capture.take(wizard, "testcase-wizard-limits");
			wizard.next();
			capture.take(wizard, "testcase-wizard-files");
		}
		{
			AddTaskDialog dialog(&window);
			dialog.addTask("sum", 100, 1000, 256);
			dialog.addTask("graph", 100, 1000, 256);
			capture.take(dialog, "import-task");
		}
		{
			ExtTestCaseModifierDialog dialog(&window);
			dialog.init(window.curContest->getTask(0), window.settings);
			capture.take(dialog, "testcase-batch-editor");
		}
		{
			ExtTestCaseUpdaterDialog dialog(&window, window.curContest->getTask(0), window.settings, 1,
			                                MAY_EDIT, MAY_EDIT, MAY_EDIT, MAY_EDIT, MAY_EDIT);
			capture.take(dialog, "testcase-batch-update");
		}
		{
			JudgingDialog dialog(&window);
			dialog.setContest(window.curContest);
			capture.take(dialog, "judging-progress");
		}
		// closeEvent is safe here: the generated window source explicitly selects
		// IniFormat, Settings honors defaultFormat(), and cwd is the temporary contest.
		window.close();
	}
	manifest["captures"] = capture.captures;
	QFile manifestFile(QDir(output).filePath("manifest.json"));
	if (! manifestFile.open(QIODevice::WriteOnly) || manifestFile.write(QJsonDocument(manifest).toJson()) < 0)
		return 1;
	QDir::setCurrent(originalDirectory);
	LemonLimeTranslator.reset();
	Lemon::base::logger.reset();
	return 0;
}
