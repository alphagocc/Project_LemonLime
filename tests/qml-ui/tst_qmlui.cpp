/*
 * SPDX-FileCopyrightText: 2026 Project LemonLime
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "base/LemonLog.hpp"
#include "base/LemonTranslator.hpp"
#include "base/compiler.h"
#include "base/settings.h"
#include "core/contest.h"
#include "core/contestant.h"
#include "core/task.h"
#include "core/testcase.h"
#include "qml/appcontroller.h"
#include "qml/contesttools.h"
#include "qml/controlstyle.h"
#include "qml/settingscontroller.h"
#include "qml/taskcontroller.h"
#include "spdlog/sinks/null_sink.h"

#include <QApplication>
#include <QFile>
#include <QFontDatabase>
#include <QFontInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLibrary>
#include <QMetaMethod>
#include <QOperatingSystemVersion>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlError>
#include <QQmlProperty>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QStyleHints>
#include <QTemporaryDir>
#include <QtTest>
#include <algorithm>
#include <cmath>
#include <memory>

#ifdef Q_OS_WIN
#include <dwmapi.h>
#include <qt_windows.h>
#endif

namespace {
#ifdef Q_OS_WIN
	class TransparentTestWindows final : public QObject {
	  protected:
		bool eventFilter(QObject *object, QEvent *event) override {
			if (event->type() == QEvent::Show) {
				if (auto *window = qobject_cast<QWindow *>(object))
					window->setOpacity(0.0);
			}
			return false;
		}
	};

	void prepareTestWindow(QQuickWindow *window) {
		// Changing native flags can temporarily reinterpret a hidden window's
		// geometry as a frame rectangle. Preserve the QML client size first.
		const QSize clientSize = window->size();
		window->setProperty("_testClientSize", clientSize);
		window->setOpacity(0.0);
		if (! window->isVisible())
			window->setFlags(window->flags() | Qt::WindowDoesNotAcceptFocus | Qt::WindowTransparentForInput);
		window->resize(clientSize);
	}
#endif

	void restoreTestClientSize(QQuickWindow *window) {
#ifdef Q_OS_WIN
		const QVariant savedSize = window->property("_testClientSize");
		if (savedSize.isValid()) {
			// Apply client dimensions once the HWND has its final flags and DPI.
			QCoreApplication::processEvents();
			window->resize(savedSize.toSize());
			QCoreApplication::processEvents();
		}
#else
		Q_UNUSED(window);
#endif
	}

	bool openTestWindow(QQuickWindow *window, const char *method = "open") {
		if (! window || ! QMetaObject::invokeMethod(window, method))
			return false;
		restoreTestClientSize(window);
		return true;
	}
	class TestSettings : public Settings {
	  public:
		TestSettings() {
			loadSettings();
			setUiLanguage("en_US");
			setPreventSleepWhileJudging(false);
			setSplashTime(0);
		}
		~TestSettings() {
			while (! getCompilerList().isEmpty())
				deleteCompiler(0);
			while (! getColorThemeList().isEmpty())
				deleteColorTheme(0);
		}
	};

	// Mirror main.cpp's controller connections without starting SingleApplication.
	struct Session {
		TestSettings settings;
		AppController app{&settings};
		TaskController tasks{&settings};
		SettingsController preferences{&settings};
		ContestTools tools;
		QStringList warnings;
		QQmlApplicationEngine engine;

		Session() {
			registerControlImages(&engine);
			QVariantMap initialProperties{{"automaticWelcome", false}};
#ifdef Q_OS_WIN
			initialProperties.insert("opacity", 0.0);
			initialProperties.insert(
			    "flags", int(Qt::Window | Qt::WindowDoesNotAcceptFocus | Qt::WindowTransparentForInput));
#endif
			engine.setInitialProperties(initialProperties);
			engine.setUiLanguage(settings.getUiLanguage());
			engine.rootContext()->setContextProperty("appController", &app);
			engine.rootContext()->setContextProperty("taskController", &tasks);
			engine.rootContext()->setContextProperty("settingsController", &preferences);
			engine.rootContext()->setContextProperty("contestTools", &tools);
			QObject::connect(&app, &AppController::contestChanged, &tasks, [this] {
				tasks.setContest(app.getContest());
				tools.setContest(app.getContest());
			});
			QObject::connect(&app, &AppController::judgingChanged, &tasks, [this] {
				tasks.setBusy(app.judging());
				tools.setBusy(app.judging());
			});
			QObject::connect(&app, &AppController::contentChanged, &tools, &ContestTools::refresh);
			QObject::connect(&app, &AppController::dataFilesChanged, &tasks, &TaskController::refresh);
			QObject::connect(&tasks, &TaskController::contestEdited, &app, &AppController::edited);
			QObject::connect(&tools, &ContestTools::filesChanged, &app, &AppController::refreshContestants);
			QObject::connect(&preferences, &SettingsController::settingsApplied, &app, [this] {
				LemonLimeTranslator->InstallTranslation(settings.getUiLanguage());
				engine.setUiLanguage(settings.getUiLanguage());
				app.settingsApplied();
				tasks.refresh();
				engine.retranslate();
			});
			QObject::connect(&engine, &QQmlEngine::warnings, &engine, [this](const QList<QQmlError> &errors) {
				for (const auto &error : errors)
					warnings.append(error.toString());
			});
		}
	};

	bool writeFile(const QString &name, const QByteArray &contents) {
		if (! QDir().mkpath(QFileInfo(name).absolutePath()))
			return false;
		QFile file(name);
		return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
	}

	QByteArray readFile(const QString &name) {
		QFile file(name);
		return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
	}

	QVariantMap subtaskValues(const QString &input, const QString &output, int score = 100,
	                          const QString &dependencies = {}) {
		return {{"input", input}, {"output", output}, {"score", score},
		        {"time", 1000},   {"memory", 256},    {"dependencies", dependencies}};
	}

	bool createContest(Session &session, const QString &directory,
	                   const QStringList &titles = {"sum", "graph"},
	                   const QStringList &names = {"Ada", "Grace"}) {
		if (! session.app.newContest("Autumn contest", QUrl::fromLocalFile(directory), "contest"))
			return false;
		for (const auto &title : titles) {
			if (! session.tasks.addTask(title))
				return false;
			session.tasks.selectTask(session.app.taskCount() - 1);
			if (! session.tasks.addSubtask(subtaskValues(title + ".in", title + ".out")))
				return false;
		}
		for (const auto &name : names) {
			if (! QDir().mkpath(QDir(directory).filePath("source/" + name)))
				return false;
		}
		session.app.refreshContestants();
		return true;
	}

	void setResult(Contestant *contestant, int task, int score, CompileState compile = CompileSuccessfully,
	               bool judged = true) {
		contestant->setCheckJudged(task, judged);
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

	int rowFor(ResultModel *model, const QString &name) {
		for (int row = 0; row < model->rowCount(); ++row) {
			if (model->nameAt(row) == name)
				return row;
		}
		return -1;
	}

	QQuickItem *pageItem(QObject *root, const QByteArray &type) {
		for (auto *item : root->findChildren<QQuickItem *>()) {
			if (QByteArray(item->metaObject()->className()).startsWith(type + "_QML"))
				return item;
		}
		return nullptr;
	}

	QObject *qmlObject(QObject *root, const QByteArray &type) {
		for (auto *object : root->findChildren<QObject *>()) {
			if (QByteArray(object->metaObject()->className()).startsWith(type + "_QML"))
				return object;
		}
		return nullptr;
	}
	bool insideDirectory(const QString &directory, const QString &fileName) {
		const QString relative = QDir(directory).relativeFilePath(QFileInfo(fileName).absoluteFilePath());
		return ! QDir::isAbsolutePath(relative) && relative != ".." && ! relative.startsWith("../");
	}

	QRect renderedRect(const QImage &image, QQuickItem *item, const QRectF &localRect) {
		const QRectF sceneRect = item->mapRectToScene(localRect);
		const QSize windowSize = item->window()->size();
		const qreal scaleX = qreal(image.width()) / windowSize.width();
		const qreal scaleY = qreal(image.height()) / windowSize.height();
		return QRectF(sceneRect.x() * scaleX, sceneRect.y() * scaleY, sceneRect.width() * scaleX,
		              sceneRect.height() * scaleY)
		    .toAlignedRect()
		    .intersected(image.rect());
	}

	double luminance(QRgb pixel) {
		const auto linear = [](int component) {
			const double value = component / 255.0;
			return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
		};
		return 0.2126 * linear(qRed(pixel)) + 0.7152 * linear(qGreen(pixel)) + 0.0722 * linear(qBlue(pixel));
	}

	double medianLuminance(const QImage &image, const QRect &area) {
		QList<double> values;
		values.reserve(area.width() * area.height());
		for (int y = area.top(); y <= area.bottom(); ++y)
			for (int x = area.left(); x <= area.right(); ++x)
				values.append(luminance(image.pixel(x, y)));
		if (values.isEmpty())
			return -1.0;
		auto middle = values.begin() + values.size() / 2;
		std::nth_element(values.begin(), middle, values.end());
		return *middle;
	}

	double contrastRatio(double first, double second) {
		return (qMax(first, second) + 0.05) / (qMin(first, second) + 0.05);
	}

	QString backgroundError(const QImage &image, QQuickItem *item, const QRectF &area, Qt::ColorScheme scheme,
	                        double *background = nullptr) {
		const double value = medianLuminance(image, renderedRect(image, item, area));
		if (background)
			*background = value;
		if (value < 0.0 || (scheme == Qt::ColorScheme::Dark ? value > 0.30 : value < 0.60))
			return QString("%1 background luminance %2 does not match %3 mode")
			    .arg(item->objectName().isEmpty() ? item->metaObject()->className() : item->objectName())
			    .arg(value, 0, 'f', 3)
			    .arg(scheme == Qt::ColorScheme::Dark ? "dark" : "light");
		return {};
	}

	QString textContrastError(const QImage &image, QQuickItem *item, const QRectF &area, double background,
	                          double minimumContrast) {
		const QRect pixels = renderedRect(image, item, area);
		int readablePixels = 0;
		for (int y = pixels.top(); y <= pixels.bottom(); ++y) {
			for (int x = pixels.left(); x <= pixels.right(); ++x) {
				if (contrastRatio(luminance(image.pixel(x, y)), background) >= minimumContrast)
					++readablePixels;
			}
		}
		// Inspect glyph interiors, allowing antialiased edge pixels to blend with
		// the background. A blank label or white-on-white text cannot pass.
		const int requiredPixels = qMax(6, pixels.width() * pixels.height() / 200);
		if (readablePixels < requiredPixels)
			return QString("%1 text '%2' has only %3 readable pixels, expected at least %4 at contrast %5")
			    .arg(item->metaObject()->className(), item->property("text").toString())
			    .arg(readablePixels)
			    .arg(requiredPixels)
			    .arg(minimumContrast);
		return {};
	}

	QQuickItem *renderedText(QQuickItem *item) {
		if (item->inherits("QQuickText") && item->isVisible() && item->width() > 0 &&
		    ! item->property("text").toString().isEmpty())
			return item;
		for (auto *child : item->childItems()) {
			if (auto *text = renderedText(child))
				return text;
		}
		return nullptr;
	}
} // namespace

class QmlUiTest : public QObject {
	Q_OBJECT

  private slots:
	void init();
	void cleanup();
	void contestRoundTripKeepsCurrentContestOnFailure();
	void selectionFollowsContestantsAfterSorting();
	void judgingFiltersAndCancellation();
	void answersOnlyJudgingCompletesAndSaves();
	void groupedImportAndDependencyReordering();
	void taskDraftCancelAndAccept();
	void crossSubtaskRowsDeleteAndCancel();
	void directoryTaskImportDistributesFullScore();
	void settingsCancelAndApply();
	void exportsEscapeUserText();
	void sourceOrganizationKeepsBackup();
	void qmlLoadsPagesAndCapturesScreenshots_data();
	void qmlLoadsPagesAndCapturesScreenshots();
	void colorSchemeRendering_data();
	void colorSchemeRendering();
	void compactSpinBoxButtonsChangeValue();

  private:
	QString originalDirectory;
	std::unique_ptr<QTemporaryDir> temporary;
	void capture(QQuickWindow *window, const QString &name);
	void verifyThemeRendering(QQuickWindow *window, QQuickWindow *settings, Qt::ColorScheme scheme,
	                          const QString &name);
};

void QmlUiTest::init() {
	originalDirectory = QDir::currentPath();
	temporary = std::make_unique<QTemporaryDir>();
	QVERIFY(temporary->isValid());
	const QString settingsDirectory = temporary->filePath("settings");
	QVERIFY(QDir().mkpath(settingsDirectory));
	QSettings::setDefaultFormat(QSettings::IniFormat);
	QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDirectory);
	QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, settingsDirectory);
	// Settings uses this explicit format constructor. Check the storage location
	// before constructing any controller that can persist application settings.
	QSettings storage(QSettings::defaultFormat(), QSettings::UserScope, "LemonLime", "lemon");
	QCOMPARE(storage.format(), QSettings::IniFormat);
	QVERIFY2(insideDirectory(settingsDirectory, storage.fileName()), qPrintable(storage.fileName()));
	QSettings applicationStorage;
	QCOMPARE(applicationStorage.format(), QSettings::IniFormat);
	QVERIFY2(insideDirectory(settingsDirectory, applicationStorage.fileName()),
	         qPrintable(applicationStorage.fileName()));
	QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Light);
}

void QmlUiTest::cleanup() {
	QDir::setCurrent(originalDirectory);
	temporary.reset();
}

void QmlUiTest::contestRoundTripKeepsCurrentContestOnFailure() {
	Session session;
	QVERIFY(createContest(session, temporary->filePath("contest")));
	const QString savedFile = session.app.contestFile();
	session.app.renameContest("Saved & named contest");
	QVERIFY(session.app.saveContest());
	const auto saved = QJsonDocument::fromJson(readFile(savedFile)).object();
	QCOMPARE(saved.value("contestTitle").toString(), "Saved & named contest");
	QCOMPARE(saved.value("tasks").toArray().size(), 2);
	QVERIFY(session.app.closeContest());
	QVERIFY(! session.app.hasContest());
	QVERIFY(session.app.openContest(QUrl::fromLocalFile(savedFile)));
	QCOMPARE(session.app.contestTitle(), "Saved & named contest");
	QCOMPARE(session.app.taskCount(), 2);
	QCOMPARE(session.app.results()->rowCount(), 2);
	Contest *current = session.app.getContest();
	const QString currentDirectory = QDir::currentPath();
	QSignalSpy errors(&session.app, &AppController::errorOccurred);
	QVERIFY(writeFile(temporary->filePath("broken.cdf"), "{ broken JSON"));
	QVERIFY(! session.app.openContest(QUrl::fromLocalFile(temporary->filePath("broken.cdf"))));
	QVERIFY(! session.app.openContest(QUrl::fromLocalFile(temporary->filePath("missing.cdf"))));
	QCOMPARE(session.app.getContest(), current);
	QCOMPARE(session.app.contestFile(), savedFile);
	QCOMPARE(QDir::currentPath(), currentDirectory);
	QCOMPARE(errors.size(), 2);
	QVERIFY(! session.app.newContest("Replacement", QUrl::fromLocalFile(currentDirectory), "contest"));
	QCOMPARE(session.app.getContest(), current);
}

void QmlUiTest::selectionFollowsContestantsAfterSorting() {
	Session session;
	QVERIFY(createContest(session, temporary->filePath("contest")));
	setResult(session.app.getContest()->getContestant("Ada"), 0, 100);
	setResult(session.app.getContest()->getContestant("Ada"), 1, 25);
	setResult(session.app.getContest()->getContestant("Grace"), 0, 40);
	setResult(session.app.getContest()->getContestant("Grace"), 1, 100);
	session.app.edited();
	auto *model = session.app.results();
	model->selectCell(rowFor(model, "Ada"), 4, false);
	QCOMPARE(model->selection().value("Ada"), QSet<int>{1});
	model->sort(1, Qt::DescendingOrder);
	QCOMPARE(model->nameAt(0), "Grace");
	QVERIFY(model->data(model->index(rowFor(model, "Ada"), 4), ResultModel::SelectedRole).toBool());
	QVERIFY(! model->data(model->index(rowFor(model, "Grace"), 4), ResultModel::SelectedRole).toBool());
	model->sort(2, Qt::AscendingOrder);
	QCOMPARE(model->nameAt(0), "Ada");
	QCOMPARE(model->selection().value("Ada"), QSet<int>{1});
	model->selectCell(rowFor(model, "Grace"), 1, true);
	QCOMPARE(model->selectedCount(), 2);
	QCOMPARE(model->selection().value("Grace"), QSet<int>{-1});
	model->refresh();
	QCOMPARE(model->selectedCount(), 2);
	model->selectRange(0, 3, 1, 4, false);
	QCOMPARE(model->selectedCount(), 2);
	QCOMPARE(model->selection().value("Ada"), (QSet<int>{0, 1}));
	QCOMPARE(model->selection().value("Grace"), (QSet<int>{0, 1}));
	model->sort(1, Qt::AscendingOrder);
	QCOMPARE(model->selection().value("Ada"), (QSet<int>{0, 1}));
}

void QmlUiTest::judgingFiltersAndCancellation() {
	Session session;
	QVERIFY(createContest(session, temporary->filePath("contest"), {"sum", "graph", "answers"}));
	session.app.getContest()->getTask(2)->setTaskType(Task::AnswersOnly);
	auto *ada = session.app.getContest()->getContestant("Ada");
	auto *grace = session.app.getContest()->getContestant("Grace");
	setResult(ada, 0, 100);
	setResult(ada, 1, 0, CompileError);
	setResult(ada, 2, 60, CompileError);
	setResult(grace, 0, 0, NoValidSourceFile);
	setResult(grace, 1, 0, NoValidGraderFile);
	setResult(grace, 2, 0, CompileSuccessfully, false);
	session.app.edited();
	session.app.results()->selectCell(rowFor(session.app.results(), "Ada"), 4, false);
	const QList<std::pair<QString, int>> cases{
	    {"selected", 1}, {"unjudged", 1}, {"missing", 1}, {"failed", 1}, {"all", 6}};
	for (const auto &entry : cases) {
		session.app.judge(entry.first);
		QVERIFY2(session.app.judging(), qPrintable(entry.first));
		const int scheduled = session.app.total();
		QVERIFY(session.tasks.busy());
		// Cancel before the queued start callback, avoiding compiler or submission processes.
		session.app.stop();
		QVERIFY(session.app.stopping());
		QTRY_VERIFY_WITH_TIMEOUT(! session.app.judging(), 3000);
		QCOMPARE(scheduled, entry.second);
		QCOMPARE(session.app.completed(), 0);
		QVERIFY(! session.tasks.busy());
	}
	QVERIFY(! grace->getCheckJudged(2));
	QCOMPARE(ada->getCompileState(1), CompileError);
}

void QmlUiTest::answersOnlyJudgingCompletesAndSaves() {
	Session session;
	QVERIFY(createContest(session, temporary->filePath("contest"), {"answers"}));
	QVERIFY(session.settings.getCompilerList().isEmpty());
	QVERIFY(session.tasks.updateTask({{"type", int(Task::AnswersOnly)},
	                                  {"comparison", int(Task::IgnoreSpacesMode)},
	                                  {"answerExtension", "ans"},
	                                  {"subfolder", false}}));
	QVERIFY(writeFile("data/answers.in", "5 8\n"));
	QVERIFY(writeFile("data/answers.out", "13\n"));
	QVERIFY(writeFile("source/Ada/answers.ans", "13  \n"));
	QVERIFY(writeFile("source/Grace/answers.ans", "14\n"));
	QVERIFY(session.app.saveContest());
	QSignalSpy stateChanges(&session.app, &AppController::judgingChanged);
	QSignalSpy errors(&session.app, &AppController::errorOccurred);
	session.app.judge("all");
	QVERIFY(session.app.judging());
	QCOMPARE(session.app.total(), 2);
	QTRY_VERIFY_WITH_TIMEOUT(! session.app.judging(), 10000);
	QCOMPARE(stateChanges.size(), 2);
	QCOMPARE(session.app.completed(), 2);
	QVERIFY(! session.tasks.busy());
	QVERIFY(errors.isEmpty());
	const auto *ada = session.app.getContest()->getContestant("Ada");
	const auto *grace = session.app.getContest()->getContestant("Grace");
	QVERIFY(ada->getCheckJudged(0));
	QVERIFY(grace->getCheckJudged(0));
	QCOMPARE(ada->getTaskScore(0), 100);
	QCOMPARE(grace->getTaskScore(0), 0);
	QCOMPARE(ada->getResult(0).value(0).size(), 1);
	QCOMPARE(grace->getResult(0).value(0).size(), 1);
	QCOMPARE(ada->getResult(0).value(0).value(0), CorrectAnswer);
	QCOMPARE(grace->getResult(0).value(0).value(0), WrongAnswer);
	QCOMPARE(session.app.results()->data(session.app.results()->index(0, 2), Qt::DisplayRole).toInt(), 100);

	// Read the saved file before closeContest(), which would otherwise save it again.
	const auto persisted = QJsonDocument::fromJson(readFile(session.app.contestFile())).object();
	const auto rows = persisted.value("contestants").toArray();
	QCOMPARE(rows.size(), 2);
	QMap<QString, int> savedScores;
	for (const auto &row : rows) {
		const auto object = row.toObject();
		QVERIFY(object.value("checkJudged").toArray().at(0).toBool());
		const auto score = object.value("score").toArray().at(0).toArray().at(0).toArray().at(0).toInt();
		savedScores.insert(object.value("contestantName").toString(), score);
	}
	QCOMPARE(savedScores.value("Ada", -1), 100);
	QCOMPARE(savedScores.value("Grace", -1), 0);
}

void QmlUiTest::groupedImportAndDependencyReordering() {
	Session session;
	QVERIFY(createContest(session, temporary->filePath("contest"), {}, {}));
	QVERIFY(session.tasks.addTask("groups"));
	session.tasks.selectTask(0);
	for (const auto &name : {"1-1", "1-2", "2-1", "3-1"}) {
		QVERIFY(writeFile("data/group/" + QString(name) + ".in", "1\n"));
		QVERIFY(writeFile("data/group/" + QString(name) + ".out", "2\n"));
	}
	const QVariantList arguments{QVariantMap{{"expression", "[0-9]+"}, {"group", true}},
	                             QVariantMap{{"expression", "[0-9]+"}, {"group", false}}};
	QVERIFY(session.tasks.previewImport("group/<1>-<2>.in", "group/<1>-<2>.out", arguments));
	QCOMPARE(session.tasks.importPreview().size(), 3);
	QCOMPARE(session.tasks.importPreview().first().toMap().value("count").toInt(), 2);
	QVERIFY(session.tasks.importTestCases({{"score", 30}, {"time", 1000}, {"memory", 256}}));
	auto *task = session.app.getContest()->getTask(0);
	QCOMPARE(task->getTestCaseList().size(), 3);
	QCOMPARE(task->getTestCase(0)->getInputFiles().size(), 2);
	QVERIFY(session.tasks.updateSubtasks({2}, {{"dependencies", "1"}}));
	QCOMPARE(task->getTestCase(2)->getDependenceSubtask(), QList<int>{1});
	QVERIFY(session.tasks.moveSubtasks({0}, 1));
	QCOMPARE(QDir::fromNativeSeparators(task->getTestCase(1)->getInputFiles().first()), "group/1-1.in");
	QCOMPARE(task->getTestCase(2)->getDependenceSubtask(), QList<int>{2});
	QVERIFY(session.tasks.removeSubtasks({0}));
	QCOMPARE(task->getTestCase(1)->getDependenceSubtask(), QList<int>{1});
	QVERIFY(! session.tasks.updateSubtasks({0}, {{"dependencies", "1"}}));
	QCOMPARE(task->getTestCaseList().size(), 2);
}

void QmlUiTest::taskDraftCancelAndAccept() {
	Session session;
	QVERIFY(createContest(session, temporary->filePath("contest"), {"sum", "graph"}, {}));
	session.tasks.selectTask(0);
	session.tasks.selectSubtask(0);
	auto *original = session.app.getContest()->getTask(0);
	const QStringList originalFiles = original->getTestCase(0)->getInputFiles();
	QSignalSpy edits(&session.tasks, &TaskController::contestEdited);

	QVERIFY(session.tasks.beginTaskEdit());
	QVERIFY(session.tasks.updateSubtasks({0}, {{"score", 35}, {"time", 2400}}));
	QVERIFY(session.tasks.setFilePair(0, "draft.in", "draft.out"));
	QVERIFY(session.tasks.addEmptySubtask());
	QCOMPARE(session.tasks.subtasks().size(), 2);
	QCOMPARE(original->getTestCaseList().size(), 1);
	QCOMPARE(original->getTestCase(0)->getFullScore(), 100);
	QCOMPARE(original->getTestCase(0)->getInputFiles(), originalFiles);
	QVERIFY(edits.isEmpty());
	session.tasks.finishTaskEdit(false);
	QCOMPARE(session.tasks.subtasks().size(), 1);
	QCOMPARE(original->getTestCase(0)->getFullScore(), 100);
	QCOMPARE(original->getTestCase(0)->getTimeLimit(), 1000);
	QCOMPARE(original->getTestCase(0)->getInputFiles(), originalFiles);
	QVERIFY(edits.isEmpty());

	session.tasks.selectSubtask(0);
	QVERIFY(session.tasks.beginTaskEdit());
	QVERIFY(session.tasks.updateSubtasks({0}, {{"score", 45}, {"memory", 128}}));
	QVERIFY(session.tasks.setFilePair(0, "accepted.in", "accepted.out"));
	session.tasks.finishTaskEdit(true);
	QCOMPARE(edits.size(), 1);
	QCOMPARE(original->getTestCase(0)->getFullScore(), 45);
	QCOMPARE(original->getTestCase(0)->getMemoryLimit(), 128);
	QCOMPARE(original->getTestCase(0)->getInputFiles(), QStringList{"accepted.in"});
	QCOMPARE(session.app.getContest()->getTask(1)->getTestCase(0)->getFullScore(), 100);
}
void QmlUiTest::crossSubtaskRowsDeleteAndCancel() {
	Session session;
	QVERIFY(createContest(session, temporary->filePath("contest"), {}, {}));
	QVERIFY(session.tasks.addTask("groups"));
	session.tasks.selectTask(0);
	for (int group = 0; group < 3; ++group) {
		const QString prefix = QString("group%1-").arg(group + 1);
		QVERIFY(session.tasks.addSubtask(subtaskValues(prefix + "0.in", prefix + "0.out")));
		session.tasks.selectSubtask(group);
		for (int file = 1; file < 3; ++file)
			QVERIFY(session.tasks.setFilePair(-1, prefix + QString::number(file) + ".in",
			                                  prefix + QString::number(file) + ".out"));
	}
	QVERIFY(session.tasks.updateSubtasks({2}, {{"dependencies", "1,2"}}));
	const QVariantList originalSubtasks = session.tasks.subtasks();
	auto *original = session.app.getContest()->getTask(0);
	QSignalSpy edits(&session.tasks, &TaskController::contestEdited);

	QVERIFY(session.tasks.beginTaskEdit());
	QVERIFY(session.tasks.removeTestCaseRows(1, 6));
	const QVariantList remaining = session.tasks.subtasks();
	QCOMPARE(remaining.size(), 2);
	const QVariantList firstFiles{QVariantMap{{"input", "group1-0.in"}, {"output", "group1-0.out"}}};
	const QVariantList lastFiles{QVariantMap{{"input", "group3-1.in"}, {"output", "group3-1.out"}},
	                             QVariantMap{{"input", "group3-2.in"}, {"output", "group3-2.out"}}};
	QCOMPARE(remaining.at(0).toMap().value("files").toList(), firstFiles);
	QCOMPARE(remaining.at(1).toMap().value("files").toList(), lastFiles);
	QCOMPARE(remaining.at(1).toMap().value("dependencies").toString(), "1");
	QCOMPARE(original->getTestCaseList().size(), 3);
	QCOMPARE(original->getTestCase(2)->getDependenceSubtask(), (QList<int>{1, 2}));
	QVERIFY(edits.isEmpty());
	session.tasks.finishTaskEdit(false);
	QCOMPARE(session.tasks.subtasks(), originalSubtasks);
	QVERIFY(edits.isEmpty());

	QVERIFY(session.tasks.beginTaskEdit());
	QVERIFY(session.tasks.removeTestCaseRows(1, 6));
	session.tasks.finishTaskEdit(true);
	QCOMPARE(edits.size(), 1);
	QCOMPARE(session.tasks.subtasks(), remaining);
	QCOMPARE(original->getTestCaseList().size(), 2);
	QCOMPARE(original->getTestCase(0)->getInputFiles(), QStringList{"group1-0.in"});
	QCOMPARE(original->getTestCase(1)->getInputFiles(), (QStringList{"group3-1.in", "group3-2.in"}));
	QCOMPARE(original->getTestCase(1)->getOutputFiles(), (QStringList{"group3-1.out", "group3-2.out"}));
	QCOMPARE(original->getTestCase(1)->getDependenceSubtask(), QList<int>{1});
}
void QmlUiTest::directoryTaskImportDistributesFullScore() {
	Session session;
	QVERIFY(createContest(session, temporary->filePath("contest"), {}, {}));
	for (const auto &name : {"sample1", "sample2", "sample10"}) {
		QVERIFY(writeFile("data/addition/" + QString(name) + ".in", "1 2\n"));
		QVERIFY(writeFile("data/addition/" + QString(name) + ".out", "3\n"));
	}
	QVERIFY(session.tasks.scanTasks());
	QCOMPARE(session.tasks.discoveredTasks().size(), 1);
	QVERIFY(session.tasks.importTasks(
	    {QVariantMap{{"index", 0}, {"selected", true}, {"score", 100}, {"time", 1500}, {"memory", 128}}}));
	auto *task = session.app.getContest()->getTask(0);
	QCOMPARE(task->getTotalScore(), 100);
	QCOMPARE(task->getTestCaseList().size(), 3);
	QCOMPARE(task->getTestCase(0)->getFullScore(), 34);
	QCOMPARE(task->getTestCase(1)->getFullScore(), 33);
	QCOMPARE(task->getTestCase(2)->getFullScore(), 33);
	QCOMPARE(task->getTestCase(2)->getTimeLimit(), 1500);
	QCOMPARE(QDir::fromNativeSeparators(task->getTestCase(2)->getInputFiles().first()),
	         "addition/sample10.in");
}

void QmlUiTest::settingsCancelAndApply() {
	// The organization/application-only constructor is deliberately inspected
	// read-only: Qt always uses NativeFormat for it, ignoring setDefaultFormat().
	const QSettings nativeStorage("LemonLime", "lemon");
	qInfo().noquote() << "Native QSettings location (read only):" << nativeStorage.fileName();
	Session session;
	const int originalLimit = session.settings.getDefaultTimeLimit();
	const bool originalPreventSleep = session.settings.getPreventSleepWhileJudging();
	session.preferences.setGeneral("defaultTimeLimit", 2400);
	session.preferences.setGeneral("preventSleepWhileJudging", ! originalPreventSleep);
	QVERIFY(session.preferences.dirty());
	QCOMPARE(session.settings.getDefaultTimeLimit(), originalLimit);
	session.preferences.cancel();
	QVERIFY(! session.preferences.dirty());
	QCOMPARE(session.preferences.general().value("defaultTimeLimit").toInt(), originalLimit);
	QCOMPARE(session.settings.getPreventSleepWhileJudging(), originalPreventSleep);
	session.preferences.setGeneral("defaultTimeLimit", -1);
	QVERIFY(! session.preferences.apply());
	QCOMPARE(session.settings.getDefaultTimeLimit(), originalLimit);
	session.preferences.setGeneral("defaultTimeLimit", 2400);
	session.preferences.setGeneral("preventSleepWhileJudging", ! originalPreventSleep);
	QSignalSpy applied(&session.preferences, &SettingsController::settingsApplied);
	QVERIFY2(session.preferences.apply(), qPrintable(session.preferences.error()));
	QCOMPARE(applied.size(), 1);
	QCOMPARE(session.settings.getDefaultTimeLimit(), 2400);
	QCOMPARE(session.settings.getPreventSleepWhileJudging(), ! originalPreventSleep);
	TestSettings reloaded;
	QCOMPARE(reloaded.getDefaultTimeLimit(), 2400);
	QSettings persisted(QSettings::defaultFormat(), QSettings::UserScope, "LemonLime", "lemon");
	qInfo().noquote() << "Isolated QSettings location:" << persisted.fileName();
	QCOMPARE(persisted.format(), QSettings::IniFormat);
	QVERIFY2(insideDirectory(temporary->filePath("settings"), persisted.fileName()),
	         qPrintable(persisted.fileName()));
	QCOMPARE(persisted.value("GeneralSettings/DefaultTimeLimit").toInt(), 2400);
}

void QmlUiTest::exportsEscapeUserText() {
	Session session;
	QVERIFY(createContest(session, temporary->filePath("contest"), {"sum"}, {"Ada"}));
	auto *contest = session.app.getContest();
	contest->setContestTitle("Contest <title> & friends");
	contest->getTask(0)->setProblemTitle("Task \"quoted\" & <em>");
	setResult(contest->getContestant("Ada"), 0, 75);
	contest->getContestant("Ada")->setMessage(0, {{"<script>alert(1)</script> & diagnostics"}});
	QJsonObject object;
	contest->writeToJson(object);
	auto contestants = object.value("contestants").toArray();
	auto contestant = contestants[0].toObject();
	const QString name = "A \"quoted\", <b> & team";
	contestant.insert("contestantName", name);
	contestants[0] = contestant;
	object.insert("contestants", contestants);
	const QString fixture = temporary->filePath("contest/export-fixture.cdf");
	QVERIFY(writeFile(fixture, QJsonDocument(object).toJson()));
	QVERIFY(session.app.openContest(QUrl::fromLocalFile(fixture)));
	for (int format = 0; format < 2; ++format) {
		const QString output = temporary->filePath(QString("report-%1.html").arg(format));
		QVERIFY(session.tools.exportResults(QUrl::fromLocalFile(output), format));
		const QString html = QString::fromUtf8(readFile(output));
		QVERIFY(html.contains(name.toHtmlEscaped()));
		QVERIFY(html.contains("Contest &lt;title&gt; &amp; friends"));
		QVERIFY(html.contains("&lt;script&gt;alert(1)&lt;/script&gt; &amp; diagnostics"));
		QVERIFY(! html.contains("<script>alert(1)</script>"));
	}
	const QString csvFile = temporary->filePath("results.csv");
	QVERIFY(session.tools.exportResults(QUrl::fromLocalFile(csvFile), 2));
	const auto csv = readFile(csvFile);
	QVERIFY(csv.contains("\"A \"\"quoted\"\", <b> & team\""));
	QVERIFY(csv.contains("\"Task \"\"quoted\"\" & <em>\""));
	QVERIFY(csv.endsWith("\"75\"\r\n"));
	const QString statistics = temporary->filePath("statistics.html");
	QVERIFY(session.tools.exportStatistics(QUrl::fromLocalFile(statistics)));
	QVERIFY(readFile(statistics).contains("Contest &lt;title&gt; &amp; friends"));
	QVERIFY(readFile(statistics).contains("75 / 100"));
}

void QmlUiTest::sourceOrganizationKeepsBackup() {
	Session session;
	const QString directory = temporary->filePath("contest");
	QVERIFY(createContest(session, directory, {"sum"}, {"Ada"}));
	QVERIFY(writeFile("source/Ada/sum.cpp", "root source\n"));
	QVERIFY(writeFile("source/Ada/sum/sum.cpp", "older nested source\n"));
	QVERIFY(writeFile("source/Ada/sum.exe", "executable\n"));
	QVERIFY(writeFile("source/Ada/notes.txt", "notes\n"));
	QSignalSpy changed(&session.tools, &ContestTools::filesChanged);
	QVERIFY(session.tools.organizeSources(true));
	QCOMPARE(changed.size(), 1);
	QCOMPARE(readFile("source/Ada/sum.cpp"), QByteArray("root source\n"));
	QCOMPARE(readFile("source/Ada/sum/sum.cpp"), QByteArray("root source\n"));
	QVERIFY(! QFileInfo::exists("source/Ada/sum.exe"));
	QVERIFY(! QFileInfo::exists("source/Ada/notes.txt"));
	QCOMPARE(readFile("source_bak_0/Ada/sum/sum.cpp"), QByteArray("older nested source\n"));
	QCOMPARE(readFile("source_bak_0/Ada/notes.txt"), QByteArray("notes\n"));
	session.app.getContest()->getTask(0)->setSourceFileName("../outside");
	QVERIFY(! session.tools.organizeSources(false));
	QCOMPARE(readFile("source/Ada/sum.cpp"), QByteArray("root source\n"));
	QVERIFY(! QFileInfo::exists(QDir(directory).filePath("outside")));
}

void QmlUiTest::capture(QQuickWindow *window, const QString &requestedName) {
	const QString name =
	    QByteArray(QTest::currentDataTag()) == "dark" ? "dark-" + requestedName : requestedName;
	QVERIFY(window);
	restoreTestClientSize(window);
	QVERIFY(window->isVisible());
#ifdef Q_OS_WIN
	QCOMPARE(window->opacity(), 0.0);
	QVERIFY(window->flags().testFlag(Qt::WindowDoesNotAcceptFocus));
	QVERIFY(window->flags().testFlag(Qt::WindowTransparentForInput));
#endif
	window->requestUpdate();
	QTest::qWait(120);
	const QImage image = window->grabWindow();
	QVERIFY2(! image.isNull(), qPrintable(name));
	QVERIFY(image.width() > 0);
	QVERIFY(image.height() > 0);
	const QString directory = qEnvironmentVariable("LEMON_QML_SCREENSHOT_DIR", QML_UI_TEST_OUTPUT_DIR);
	QVERIFY(QDir().mkpath(directory));
	QVERIFY(image.save(QDir(directory).filePath(name + ".png")));
	QJsonArray items;
	for (auto *item : window->findChildren<QQuickItem *>()) {
		if (item->window() != window)
			continue;
		const QPointF position = item->mapToScene(QPointF());
		QJsonObject entry{{"class", item->metaObject()->className()},
		                  {"name", item->objectName()},
		                  {"geometry", QJsonArray{position.x(), position.y(), item->width(), item->height()}},
		                  {"visible", item->isVisible()},
		                  {"enabled", item->isEnabled()}};
		for (const auto *property : {"text", "title", "currentIndex", "checked"}) {
			const QVariant value = item->property(property);
			if (value.isValid())
				entry[property] = QJsonValue::fromVariant(value);
		}
		if (item->property("font").isValid()) {
			const QFont font = item->property("font").value<QFont>();
#ifdef Q_OS_WIN
			if (item->isVisible() && item->inherits("QQuickLabel"))
				QCOMPARE(font.family(), QString("Microsoft YaHei"));
#endif
			entry["font"] = QJsonObject{{"family", font.family()},
			                            {"pointSize", font.pointSizeF()},
			                            {"pixelSize", QFontInfo(font).pixelSize()},
			                            {"bold", font.bold()}};
		}
		items.append(entry);
	}
	const QJsonObject geometry{{"name", name},
	                           {"size", QJsonArray{window->width(), window->height()}},
	                           {"devicePixelRatio", window->devicePixelRatio()},
	                           {"items", items}};
	QVERIFY(writeFile(QDir(directory).filePath(name + ".json"), QJsonDocument(geometry).toJson()));
}

void QmlUiTest::verifyThemeRendering(QQuickWindow *window, QQuickWindow *settings, Qt::ColorScheme scheme,
                                     const QString &name) {
	QVERIFY(scheme == Qt::ColorScheme::Light || scheme == Qt::ColorScheme::Dark);
	auto *scrollBar = window->findChild<QQuickItem *>("resultsVerticalScrollBar");
	QVERIFY(scrollBar);
	QVERIFY(scrollBar->setProperty("active", true));
	QVERIFY(scrollBar->property("size").toDouble() < 1.0);
	capture(window, name + "-results");
	const QImage image = window->grabWindow();
	QVERIFY(! image.isNull());
	capture(settings, name + "-settings");
	QCOMPARE(settings->size(), QSize(533, 437));
	const QImage settingsImage = settings->grabWindow();
	QVERIFY(! settingsImage.isNull());
#ifdef Q_OS_WIN
	if (QOperatingSystemVersion::current() >=
	    QOperatingSystemVersion(QOperatingSystemVersion::Windows, 10, 0, 19041)) {
		// grabWindow() captures the client area. Read DWM's frame attribute as a
		// separate check that title bars follow the same application color scheme.
		static QLibrary dwmApi("dwmapi");
		using ReadWindowAttribute = HRESULT(WINAPI *)(HWND, DWORD, PVOID, DWORD);
		const auto readAttribute =
		    reinterpret_cast<ReadWindowAttribute>(dwmApi.resolve("DwmGetWindowAttribute"));
		QVERIFY2(readAttribute, qPrintable(dwmApi.errorString()));
		for (auto *surface : {window, settings}) {
			BOOL darkFrame = FALSE;
			const HRESULT result =
			    readAttribute(reinterpret_cast<HWND>(surface->winId()), DWMWA_USE_IMMERSIVE_DARK_MODE,
			                  &darkFrame, sizeof(darkFrame));
			QCOMPARE(result, S_OK);
			QVERIFY2(bool(darkFrame) == (scheme == Qt::ColorScheme::Dark),
			         qPrintable(QString("%1 title bar dark-mode flag is %2 for color scheme %3")
			                        .arg(surface->objectName())
			                        .arg(darkFrame)
			                        .arg(int(scheme))));
		}
	}
#endif
	for (const auto *buttonName :
	     {"cleanupButton", "refreshButton", "judgeUnjudgedButton", "judgeButton", "judgeAllButton"}) {
		auto *button = window->findChild<QQuickItem *>(buttonName);
		QVERIFY2(button, buttonName);
		QVERIFY(button->isVisible());
#ifdef Q_OS_WIN
		QCOMPARE(QSizeF(button->width(), button->height()), QSizeF(120, 26));
		QCOMPARE(button->property("font").value<QFont>().pointSizeF(), 10.0);
#endif
		double background = 0.0;
		QString error = backgroundError(image, button, button->boundingRect().adjusted(3, 3, -3, -3), scheme,
		                                &background);
		QVERIFY2(error.isEmpty(), qPrintable(error));
		auto *label = renderedText(button);
		QVERIFY2(label, buttonName);
		error = textContrastError(image, label, label->boundingRect(), background,
		                          button->isEnabled() ? 3.0 : 1.3);
		QVERIFY2(error.isEmpty(), qPrintable(QString(buttonName) + ": " + error));
	}
	QVERIFY(! window->findChild<QQuickItem *>("judgeButton")->isEnabled());
	auto *menuBar = window->findChild<QQuickItem *>("mainMenuBar");
	QVERIFY(menuBar);
	double menuBackground = 0.0;
	QString error = backgroundError(image, menuBar, menuBar->boundingRect().adjusted(2, 2, -2, -2), scheme,
	                                &menuBackground);
	QVERIFY2(error.isEmpty(), qPrintable(error));
	int menuCount = 0;
	for (auto *item : menuBar->findChildren<QQuickItem *>()) {
		if (! item->inherits("QQuickMenuBarItem") || ! item->isVisible())
			continue;
		auto *label = renderedText(item);
		QVERIFY(label);
		error = textContrastError(image, label, label->boundingRect(), menuBackground, 3.0);
		QVERIFY2(error.isEmpty(), qPrintable(error));
		++menuCount;
	}
	QCOMPARE(menuCount, 4);

	auto *thumb = scrollBar->property("contentItem").value<QQuickItem *>();
	QVERIFY(thumb);
	QVERIFY(thumb->isVisible());
	QVERIFY(thumb->width() > 0.0);
	QVERIFY(thumb->height() > 0.0);
	const qreal trackTop = thumb->mapToItem(scrollBar, QPointF(0, thumb->height())).y() + 3;
	const QRectF track(2, trackTop, scrollBar->width() - 4, scrollBar->height() - trackTop - 2);
	QVERIFY(track.height() > 5);
	double trackBackground = 0.0;
	error = backgroundError(image, scrollBar, track, scheme, &trackBackground);
	QVERIFY2(error.isEmpty(), qPrintable(error));
	const QRectF thumbCenter(thumb->width() / 4, thumb->height() / 4, thumb->width() / 2,
	                         thumb->height() / 2);
	const double thumbColor = medianLuminance(image, renderedRect(image, thumb, thumbCenter));
	QVERIFY2(contrastRatio(thumbColor, trackBackground) >= 1.2,
	         qPrintable(QString("Scroll thumb luminance %1 is indistinguishable from track %2")
	                        .arg(thumbColor)
	                        .arg(trackBackground)));

	for (const auto *fieldName : {"defaultFullScore", "defaultTimeLimit", "inputFileExtensions"}) {
		auto *field = settings->findChild<QQuickItem *>(fieldName);
		QVERIFY2(field, fieldName);
		QVERIFY(field->isVisible());
#ifdef Q_OS_WIN
		QVERIFY(field->height() <= 26);
#endif
		double background = 0.0;
		error = backgroundError(settingsImage, field, field->boundingRect().adjusted(3, 3, -3, -3), scheme,
		                        &background);
		QVERIFY2(error.isEmpty(), qPrintable(error));
		const qreal left = qMax(3.0, field->property("leftPadding").toDouble());
		const qreal top = qMax(3.0, field->property("topPadding").toDouble());
		const qreal right = qMax(3.0, field->property("rightPadding").toDouble());
		const qreal bottom = qMax(3.0, field->property("bottomPadding").toDouble());
		error =
		    textContrastError(settingsImage, field,
		                      field->boundingRect().adjusted(left, top, -right, -bottom), background, 3.0);
		QVERIFY2(error.isEmpty(), qPrintable(QString(fieldName) + ": " + error));
	}
}

void QmlUiTest::colorSchemeRendering_data() {
	QTest::addColumn<int>("startupScheme");
	QTest::newRow("light-startup") << int(Qt::ColorScheme::Light);
	QTest::newRow("dark-startup") << int(Qt::ColorScheme::Dark);
}

void QmlUiTest::colorSchemeRendering() {
	QFETCH(int, startupScheme);
	auto *styleHints = QGuiApplication::styleHints();
	styleHints->setColorScheme(Qt::ColorScheme::Unknown);
	QTest::qWait(50);
	const Qt::ColorScheme systemScheme = styleHints->colorScheme();
	const auto initialScheme = Qt::ColorScheme(startupScheme);
	{
		TestSettings startupSettings;
		startupSettings.setColorScheme(initialScheme);
		startupSettings.saveSettings();
	}
	QVERIFY(LemonLimeTranslator->InstallTranslation("zh_CN"));
	Session session;
	session.settings.setUiLanguage("zh_CN");
	session.engine.setUiLanguage("zh_CN");
	QCOMPARE(session.settings.getColorScheme(), initialScheme);
	// Match application startup: restore the persisted mode before creating QML.
	styleHints->setColorScheme(session.settings.getColorScheme());
#ifdef Q_OS_WIN
	QTRY_COMPARE(styleHints->colorScheme(), initialScheme);
#else
	if (styleHints->colorScheme() != initialScheme)
		QSKIP("This platform plugin does not support the requested application color scheme.");
#endif
	QStringList names;
	for (int index = 0; index < 40; ++index)
		names.append(QString("Contestant %1").arg(index + 1, 2, 10, QLatin1Char('0')));
	QVERIFY(createContest(session, temporary->filePath("contest"), {"sum", "graph", "strings"}, names));
	for (auto *contestant : session.app.getContest()->getContestantList())
		for (int task = 0; task < 3; ++task)
			setResult(contestant, task, 100 - task * 25);
	session.app.edited();
	session.engine.loadFromModule("LemonLime", "Main");
	QVERIFY2(! session.engine.rootObjects().isEmpty(), qPrintable(session.warnings.join('\n')));
	auto *window = qobject_cast<QQuickWindow *>(session.engine.rootObjects().first());
	QVERIFY(window);
#ifdef Q_OS_WIN
	for (auto *child : window->findChildren<QQuickWindow *>())
		prepareTestWindow(child);
#endif
	QVERIFY(QTest::qWaitForWindowExposed(window));
	QVERIFY(window->setProperty("page", 1));
	auto *settings = window->property("settingsWindow").value<QQuickWindow *>();
	QVERIFY(openTestWindow(settings));
	QTRY_VERIFY(settings->isVisible());
	const QString prefix = QString("theme-%1").arg(QString::fromLatin1(QTest::currentDataTag()));
	verifyThemeRendering(window, settings, initialScheme, prefix);
	if (QTest::currentTestFailed())
		return;
	const auto alternateScheme =
	    initialScheme == Qt::ColorScheme::Light ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light;
	for (const auto scheme : {alternateScheme, initialScheme, Qt::ColorScheme::Unknown}) {
		session.preferences.startEditing();
		session.preferences.setGeneral("colorScheme", int(scheme));
		QVERIFY2(session.preferences.apply(), qPrintable(session.preferences.error()));
		QCOMPARE(session.settings.getColorScheme(), scheme);
		QSettings persisted(QSettings::defaultFormat(), QSettings::UserScope, "LemonLime", "lemon");
		QVERIFY(insideDirectory(temporary->filePath("settings"), persisted.fileName()));
		QCOMPARE(persisted.value("VisualSettings/ColorScheme").toInt(), int(scheme));
		const auto effectiveScheme = scheme == Qt::ColorScheme::Unknown ? systemScheme : scheme;
#ifndef Q_OS_WIN
		QTest::qWait(50);
		if (scheme != Qt::ColorScheme::Unknown && styleHints->colorScheme() != effectiveScheme)
			QSKIP("This platform plugin does not support changing the application color scheme.");
#endif
		QTRY_COMPARE(styleHints->colorScheme(), effectiveScheme);
		if (effectiveScheme == Qt::ColorScheme::Unknown)
			continue;
		const QString mode = scheme == Qt::ColorScheme::Unknown ? "system"
		                     : scheme == Qt::ColorScheme::Dark  ? "dark"
		                                                        : "light";
		verifyThemeRendering(window, settings, effectiveScheme, prefix + "-apply-" + mode);
		if (QTest::currentTestFailed())
			return;
	}
	session.preferences.startEditing();
	session.preferences.setGeneral("colorScheme", int(alternateScheme));
	QVERIFY(session.preferences.dirty());
	session.preferences.cancel();
	QVERIFY(! session.preferences.dirty());
	QCOMPARE(session.settings.getColorScheme(), Qt::ColorScheme::Unknown);
	QCOMPARE(session.preferences.general().value("colorScheme").toInt(), int(Qt::ColorScheme::Unknown));
	QCOMPARE(styleHints->colorScheme(), systemScheme);
	QSettings persisted(QSettings::defaultFormat(), QSettings::UserScope, "LemonLime", "lemon");
	QCOMPARE(persisted.value("VisualSettings/ColorScheme").toInt(), int(Qt::ColorScheme::Unknown));
	if (systemScheme != Qt::ColorScheme::Unknown)
		verifyThemeRendering(window, settings, systemScheme, prefix + "-cancel");
	QVERIFY2(session.warnings.isEmpty(), qPrintable(session.warnings.join('\n')));
	settings->close();
	window->setProperty("allowClose", true);
	window->close();
}

void QmlUiTest::compactSpinBoxButtonsChangeValue() {
	Session session;
	QQuickWindow window;
	window.setObjectName("spinBoxTestWindow");
	window.resize(180, 80);
	QQmlComponent component(&session.engine, QUrl("qrc:/qt/qml/LemonLime/controls/SpinBox.qml"));
	QVERIFY2(component.isReady(), qPrintable(component.errorString()));
	std::unique_ptr<QObject> object(component.create());
	auto *spin = qobject_cast<QQuickItem *>(object.get());
	QVERIFY(spin);
	spin->setParentItem(window.contentItem());
	QFont font = QApplication::font();
	font.setPointSize(10);
	QVERIFY(spin->setProperty("font", font));
	QVERIFY(spin->setProperty("from", 0));
	QVERIFY(spin->setProperty("to", 2));
	QVERIFY(spin->setProperty("value", 1));
	spin->setPosition(QPointF(16, 16));
	spin->setWidth(120);
#ifdef Q_OS_WIN
	prepareTestWindow(&window);
#endif
	window.show();
	restoreTestClientSize(&window);
	QVERIFY(QTest::qWaitForWindowExposed(&window));
	QCoreApplication::processEvents();
	auto *up = QQmlProperty::read(spin, "up.indicator").value<QQuickItem *>();
	auto *down = QQmlProperty::read(spin, "down.indicator").value<QQuickItem *>();
	QVERIFY(up);
	QVERIFY(down);
	QVERIFY(up->isVisible());
	QVERIFY(down->isVisible());
	QVERIFY(up->width() > 0 && up->height() > 0);
	QVERIFY(down->width() > 0 && down->height() > 0);
#ifdef Q_OS_WIN
	QCOMPARE(spin->height(), 27.0);
	const QRectF upRect = up->mapRectToItem(spin, up->boundingRect());
	const QRectF downRect = down->mapRectToItem(spin, down->boundingRect());
	QCOMPARE(upRect.left(), downRect.left());
	QCOMPARE(upRect.width(), downRect.width());
	QVERIFY(upRect.bottom() <= downRect.top());
	QVERIFY(upRect.right() <= spin->width());
	QVERIFY(downRect.bottom() <= spin->height());
#endif
	const auto click = [&window](QQuickItem *indicator) {
		QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier,
		                  indicator->mapToScene(indicator->boundingRect().center()).toPoint());
	};
	const int valueSignal = spin->metaObject()->indexOfSignal("valueModified()");
	QVERIFY(valueSignal >= 0);
	QSignalSpy valueChanges(spin, spin->metaObject()->method(valueSignal));
	QVERIFY(valueChanges.isValid());
	click(up);
	QCOMPARE(spin->property("value").toInt(), 2);
	QCOMPARE(valueChanges.size(), 1);
	QVERIFY(! up->isEnabled());
	click(up);
	QCOMPARE(spin->property("value").toInt(), 2);
	QCOMPARE(valueChanges.size(), 1);
	click(down);
	QCOMPARE(spin->property("value").toInt(), 1);
	click(down);
	QCOMPARE(spin->property("value").toInt(), 0);
	QCOMPARE(valueChanges.size(), 3);
	QVERIFY(! down->isEnabled());
	click(down);
	QCOMPARE(spin->property("value").toInt(), 0);
	QCOMPARE(valueChanges.size(), 3);
	click(up);
	QCOMPARE(spin->property("value").toInt(), 1);
	QCOMPARE(valueChanges.size(), 4);
	QVERIFY2(session.warnings.isEmpty(), qPrintable(session.warnings.join('\n')));
	window.close();
}

void QmlUiTest::qmlLoadsPagesAndCapturesScreenshots_data() {
	QTest::addColumn<int>("startupScheme");
	QTest::newRow("light") << int(Qt::ColorScheme::Light);
	QTest::newRow("dark") << int(Qt::ColorScheme::Dark);
}

void QmlUiTest::qmlLoadsPagesAndCapturesScreenshots() {
	QFETCH(int, startupScheme);
	const auto initialScheme = Qt::ColorScheme(startupScheme);
	QVERIFY(LemonLimeTranslator->InstallTranslation("zh_CN"));
	Session session;
	session.settings.setUiLanguage("zh_CN");
	session.settings.setPreventSleepWhileJudging(true);
	session.settings.setSplashTime(500);
	session.settings.setColorScheme(initialScheme);
	QGuiApplication::styleHints()->setColorScheme(initialScheme);
#ifdef Q_OS_WIN
	QTRY_COMPARE(QGuiApplication::styleHints()->colorScheme(), initialScheme);
#else
	if (initialScheme == Qt::ColorScheme::Dark &&
	    QGuiApplication::styleHints()->colorScheme() != initialScheme)
		QSKIP("This platform plugin does not support dark application color schemes.");
#endif
	session.engine.setUiLanguage("zh_CN");
	auto *compiler = new Compiler;
	compiler->setCompilerName("GNU C++");
	compiler->setSourceExtensions("cpp;cc;cxx");
	compiler->setCompilerLocation("g++");
	compiler->addConfiguration("C++17", "-std=c++17 -O2 %s -o %e", "");
	session.settings.addCompiler(compiler);
	session.preferences.startEditing();
	session.engine.loadFromModule("LemonLime", "Main");
	QVERIFY2(! session.engine.rootObjects().isEmpty(), qPrintable(session.warnings.join('\n')));
	auto *window = qobject_cast<QQuickWindow *>(session.engine.rootObjects().first());
	QVERIFY(window);
#ifdef Q_OS_WIN
	for (auto *child : window->findChildren<QQuickWindow *>())
		prepareTestWindow(child);
#endif
	QVERIFY(QTest::qWaitForWindowExposed(window));
	QCOMPARE(window->objectName(), "mainWindow");
	QCOMPARE(window->size(), QSize(800, 600));
	QCOMPARE(window->minimumSize(), QSize(725, 510));
	capture(window, "main-empty");

	auto *welcome = window->property("welcomeWindow").value<QQuickWindow *>();
	auto *newContest = window->property("newContestWindow").value<QQuickWindow *>();
	auto *openContest = window->property("openContestWindow").value<QQuickWindow *>();
	auto *settings = window->property("settingsWindow").value<QQuickWindow *>();
	auto *progress = window->property("judgingWindow").value<QQuickWindow *>();
	QVERIFY(welcome);
	QVERIFY(newContest);
	QVERIFY(openContest);
	QVERIFY(settings);
	QVERIFY(progress);
	QVERIFY(openTestWindow(welcome));
	QTRY_VERIFY(welcome->isVisible());
	QCOMPARE(welcome->size(), QSize(470, 350));
	capture(welcome, "welcome-open");
	QVERIFY(welcome->setProperty("currentIndex", 1));
	capture(welcome, "welcome-new");
	QVERIFY(QMetaObject::invokeMethod(welcome, "reject"));
	for (const auto &entry : {std::pair{newContest, "new-contest"}, {openContest, "open-contest"}}) {
		QVERIFY(openTestWindow(entry.first));
		QTRY_VERIFY(entry.first->isVisible());
		QCOMPARE(entry.first->size(), QSize(450, 320));
		capture(entry.first, entry.second);
		QVERIFY(QMetaObject::invokeMethod(entry.first, "reject"));
	}
	QVERIFY(! session.app.hasContest());

	QVERIFY(QMetaObject::invokeMethod(window, "settingsPage"));
	restoreTestClientSize(settings);
	QTRY_VERIFY(settings->isVisible());
	QCOMPARE(settings->size(), QSize(533, 437));
	auto *settingsTabs = settings->findChild<QObject *>("settingsTabs");
	QVERIFY(settingsTabs);
	for (int tab = 0; tab < 3; ++tab) {
		QVERIFY(settingsTabs->setProperty("currentIndex", tab));
		capture(settings, QString("options-%1").arg(QStringList{"general", "compiler", "visual"}[tab]));
	}
	QVERIFY(settingsTabs->setProperty("currentIndex", 1));
	session.preferences.setCompilerIndex(0);
	auto *advanced = settings->findChild<QQuickWindow *>("AdvancedCompilerSettingsDialog");
	QVERIFY(advanced);
	const QVariantMap savedCompiler = session.preferences.compiler();
	QVERIFY(openTestWindow(advanced));
	QTRY_VERIFY(advanced->isVisible());
	capture(advanced, "compiler-advanced");
	auto *environment = advanced->findChild<QQuickWindow *>("EnvironmentVariablesDialog");
	QVERIFY(environment);
	QVERIFY(environment->setProperty("environment",
	                                 QVariantList{QVariantMap{{"name", "LANG"}, {"value", "zh_CN.UTF-8"}}}));
	QVERIFY(openTestWindow(environment));
	QTRY_VERIFY(environment->isVisible());
	capture(environment, "environment-variables");
	QVERIFY(QMetaObject::invokeMethod(environment, "edit", Q_ARG(QVariant, 0)));
	auto *variable = environment->findChild<QQuickWindow *>("EditVariableDialog");
	QVERIFY(variable);
	QTRY_VERIFY(variable->isVisible());
	capture(variable, "environment-variable-edit");
	QVERIFY(QMetaObject::invokeMethod(variable, "reject"));
	QVERIFY(QMetaObject::invokeMethod(environment, "reject"));
	QVERIFY(QMetaObject::invokeMethod(advanced, "reject"));
	QCOMPARE(session.preferences.compiler(), savedCompiler);

	auto *wizard = settings->findChild<QQuickWindow *>("AddCompilerWizard");
	QVERIFY(wizard);
	QVERIFY(openTestWindow(wizard));
	QTRY_VERIFY(wizard->isVisible());
	capture(wizard, "compiler-wizard-intro");
	QVERIFY(wizard->setProperty("custom", true));
	QVERIFY(QMetaObject::invokeMethod(wizard, "advance"));
	QCOMPARE(wizard->property("pageIndex").toInt(), 1);
	capture(wizard, "compiler-wizard-custom");
	QVERIFY(wizard->setProperty("custom", false));
	QVERIFY(wizard->setProperty("pageIndex", 2));
	capture(wizard, "compiler-wizard-detected");
	QVERIFY(QMetaObject::invokeMethod(wizard, "reject"));
	QCOMPARE(session.preferences.compilerNames().size(), 1);

	QVERIFY(settingsTabs->setProperty("currentIndex", 2));
	auto *theme = settings->findChild<QQuickWindow *>("ThemeEditDialog");
	QVERIFY(theme);
	QVERIFY(openTestWindow(theme));
	QTRY_VERIFY(theme->isVisible());
	capture(theme, "theme-editor");
	QVERIFY(QMetaObject::invokeMethod(theme, "reject"));
	session.preferences.setGeneral("defaultTimeLimit", 3456);
	QVERIFY(session.preferences.dirty());
	settings->close();
	QTRY_VERIFY(! settings->isVisible());
	QCOMPARE(session.settings.getDefaultTimeLimit(), 1000);
	QVERIFY(! session.preferences.dirty());

	QVERIFY(createContest(session, temporary->filePath("contest"), {"sum", "graph", "strings"},
	                      {"Ada", "Babbage", "Grace", "Linus"}));
	const auto contestants = session.app.getContest()->getContestantList();
	for (int index = 0; index < contestants.size(); ++index)
		for (int task = 0; task < 3; ++task)
			setResult(contestants[index], task, qMax(0, 100 - index * 20 - task * 10));
	setResult(session.app.getContest()->getContestant("Linus"), 2, 0, CompileError);
	session.app.edited();
	auto *tasks = pageItem(window, "TasksPage");
	auto *results = pageItem(window, "ResultsPage");
	auto *statistics = pageItem(window, "StatisticsPage");
	QVERIFY(tasks);
	QVERIFY(results);
	QVERIFY(statistics);
	QVERIFY(window->setProperty("page", 0));
	QTRY_VERIFY(tasks->isVisible());
	QVERIFY(QMetaObject::invokeMethod(tasks, "selectTask", Q_ARG(QVariant, 0)));
	QVERIFY(tasks->setProperty("expanded", QVariantMap{{"0", true}, {"1", true}, {"2", true}}));
	capture(window, "main-task");
	QVERIFY(QMetaObject::invokeMethod(tasks, "selectCase", Q_ARG(QVariant, 0), Q_ARG(QVariant, 0)));
	QTRY_VERIFY(tasks->property("showingSubtask").toBool());
	capture(window, "main-testcase");

	auto *importWizard = qobject_cast<QQuickWindow *>(qmlObject(tasks, "TestCaseImportDialog"));
	QVERIFY(importWizard);
	QVERIFY(openTestWindow(importWizard, "begin"));
	QTRY_VERIFY(importWizard->isVisible());
	capture(importWizard, "testcase-wizard-limits");
	QVERIFY(QMetaObject::invokeMethod(importWizard, "next"));
	QCOMPARE(importWizard->property("page").toInt(), 1);
	capture(importWizard, "testcase-wizard-files");
	QVERIFY(QMetaObject::invokeMethod(importWizard, "reject"));

	auto *modifier = qobject_cast<QQuickWindow *>(qmlObject(tasks, "TestCaseModifierDialog"));
	QVERIFY(modifier);
	QVERIFY(openTestWindow(modifier, "begin"));
	QTRY_VERIFY(modifier->isVisible());
	capture(modifier, "testcase-batch-editor");
	auto *batch = qobject_cast<QQuickWindow *>(qmlObject(modifier, "TestCaseBatchDialog"));
	QVERIFY(batch);
	QVERIFY(
	    QMetaObject::invokeMethod(batch, "begin", Q_ARG(QVariant, true), Q_ARG(QVariant, QVariantList{})));
	QTRY_VERIFY(batch->isVisible());
	capture(batch, "testcase-batch-update");
	QVERIFY(QMetaObject::invokeMethod(batch, "reject"));
	QVERIFY(QMetaObject::invokeMethod(modifier, "reject"));
	QCOMPARE(session.app.getContest()->getTask(0)->getTestCaseList().size(), 1);

	QVERIFY(writeFile("data/imported/sample.in", "1 2\n"));
	QVERIFY(writeFile("data/imported/sample.out", "3\n"));
	auto *discovery = qobject_cast<QQuickWindow *>(qmlObject(tasks, "TaskDiscoveryDialog"));
	QVERIFY(discovery);
	QVERIFY(openTestWindow(discovery, "begin"));
	QTRY_VERIFY(discovery->isVisible());
	capture(discovery, "import-task");
	QVERIFY(QMetaObject::invokeMethod(discovery, "reject"));
	QCOMPARE(session.app.taskCount(), 3);

	QVERIFY(window->setProperty("page", 1));
	QTRY_VERIFY(results->isVisible());
	QVERIFY(! tasks->isVisible());
	QVERIFY(window->findChild<QQuickItem *>("resultsTable"));
	capture(window, "main-results");
	QVERIFY(QMetaObject::invokeMethod(results, "details", Q_ARG(QVariant, "Ada")));
	auto *detail = results->findChild<QQuickWindow *>("detailDialog");
	QVERIFY(detail);
	QTRY_VERIFY(detail->isVisible());
	capture(detail, "result-detail");
	QVERIFY(QMetaObject::invokeMethod(detail, "reject"));
	QVERIFY(window->setProperty("page", 2));
	QTRY_VERIFY(statistics->isVisible());
	session.tools.refresh();
	QVERIFY(session.tools.getStatisticsHtml().contains("175 / 300"));
	capture(window, "main-statistics");

	QVERIFY(window->setProperty("page", 1));
	QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Dark);
	QTest::qWait(150);
	capture(window, "main-results-dark-request");
	QGuiApplication::styleHints()->setColorScheme(initialScheme);
	QVERIFY(openTestWindow(progress));
	QTRY_VERIFY(progress->isVisible());
	capture(progress, "judging-progress");
	QVERIFY(QMetaObject::invokeMethod(progress, "reject"));
	QVERIFY(! session.app.judging());
	QVERIFY2(session.warnings.isEmpty(), qPrintable(session.warnings.join('\n')));
	window->setProperty("allowClose", true);
	window->close();
}
int main(int argc, char **argv) {
	if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
#ifdef Q_OS_WIN
		qputenv("QT_QPA_PLATFORM", "windows");
#else
		qputenv("QT_QPA_PLATFORM", "offscreen");
#endif
	}
#ifndef Q_OS_WIN
	if (qEnvironmentVariableIsEmpty("QT_QUICK_BACKEND"))
		qputenv("QT_QUICK_BACKEND", "software");
#endif
#ifdef Q_OS_WIN
	const bool offscreenFonts = qEnvironmentVariable("QT_QPA_PLATFORM").startsWith("offscreen");
	const QDir systemFonts(QDir(qEnvironmentVariable("WINDIR", "C:/Windows")).filePath("Fonts"));
	std::unique_ptr<QTemporaryDir> fontDirectory;
	if (offscreenFonts) {
		fontDirectory = std::make_unique<QTemporaryDir>();
		if (! fontDirectory->isValid())
			return 1;
		for (const auto *font : {"msyh.ttc", "msyhbd.ttc", "segoeui.ttf", "segoeuib.ttf", "seguisym.ttf"})
			QFile::copy(systemFonts.filePath(font), fontDirectory->filePath(font));
		qputenv("QT_QPA_FONTDIR", fontDirectory->path().toUtf8());
	}
#endif
	QApplication application(argc, argv);
	QCoreApplication::setOrganizationName("LemonLimeQmlTests");
	QCoreApplication::setApplicationName("qml-ui");
	QStandardPaths::setTestModeEnabled(true);
#ifdef Q_OS_WIN
	// Native handles provide Windows theme rendering. Zero opacity hides every
	// test surface; non-activating window flags preserve the user's keyboard focus.
	TransparentTestWindows transparentWindows;
	application.installEventFilter(&transparentWindows);
	if (offscreenFonts) {
		for (const auto *font : {"msyh.ttc", "msyhbd.ttc", "segoeui.ttf", "segoeuib.ttf", "seguisym.ttf"})
			QFontDatabase::addApplicationFont(systemFonts.filePath(font));
	}
	if (! QFontDatabase::hasFamily("Microsoft YaHei")) {
		qCritical("Cannot load Microsoft YaHei for the QML screenshot tests.");
		return 1;
	}
	QFont originalFont("Microsoft YaHei", 9);
	originalFont.setHintingPreference(QFont::PreferNoHinting);
	QApplication::setFont(originalFont);
#endif
	Q_INIT_RESOURCE(resource);
#ifdef Q_OS_WIN
	QQuickStyle::setStyle("FluentWinUI3");
#endif
	Lemon::base::logger =
	    std::make_shared<spdlog::logger>("qml-ui-tests", std::make_shared<spdlog::sinks::null_sink_mt>());
	LemonLimeTranslator = std::make_unique<Lemon::common::LemonTranslator>();
	QmlUiTest test;
	const int result = QTest::qExec(&test, argc, argv);
	LemonLimeTranslator.reset();
	Lemon::base::logger.reset();
	return result;
}

#include "tst_qmlui.moc"
