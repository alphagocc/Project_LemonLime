/*
 * SPDX-FileCopyrightText: 2011-2018 Project Lemon, Zhipeng Jia
 * SPDX-FileCopyrightText: 2018-2019 Project LemonPlus, Dust1404
 * SPDX-FileCopyrightText: 2019-2022 Project LemonLime
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include "qml/appcontroller.h"
#include "qml/contesttools.h"
#include "qml/controlstyle.h"
#include "qml/settingscontroller.h"
#include "qml/taskcontroller.h"
#include "spdlog/sinks/stdout_color_sinks.h"
//
#include "base/LemonBase.hpp"
#include "base/LemonBaseApplication.hpp"
#include "base/LemonLog.hpp"
#include "base/LemonTranslator.hpp"
#include "base/settings.h"
#include "spdlog/sinks/daily_file_sink.h"
//
#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <chrono>

#define LEMON_MODULE_NAME "Main"

void initLogger() {
	auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
	console_sink->set_level(spdlog::level::warn);
	QDir logDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QDir::separator() +
	            "logs");
	logDir.mkpath(".");
	// retain last 30 days logs
	auto file_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(
	    (logDir.path() + QDir::separator() + "lemonlime-log.txt").toStdString(), 0, 0, false, 30);
	file_sink->set_level(spdlog::level::trace);
	Lemon::base::logger =
	    std::make_shared<spdlog::logger>(spdlog::logger("lemonlime", {console_sink, file_sink}));
	spdlog::flush_every(std::chrono::seconds(5));
}

int main(int argc, char *argv[]) {

	QCoreApplication::setApplicationName("Lemonlime");

	initLogger();

	Lemon::LemonBaseApplication app(argc, argv);

	app.Initialize();

	if (app.sendMessage("")) {
		app.activeWindow();
		return 0;
	}

#ifdef Q_OS_LINUX
	// fonts.setFamily("Noto Sans CJK SC");
#endif
#ifdef Q_OS_WIN32
	QFont fonts;
	fonts.setFamily("Microsoft YaHei");
	fonts.setHintingPreference(QFont::PreferNoHinting);
	SingleApplication::setFont(fonts);
#endif
#ifdef Q_OS_MAC
	// fonts.setFamily("PingFangSC-Regular");
#endif
	Q_INIT_RESOURCE(resource);
	app.setWindowIcon(QIcon(":/icon/icon.png"));
#ifdef Q_OS_WIN
	QQuickStyle::setStyle("FluentWinUI3");
#endif
	Settings settings;
	settings.loadSettings();
	AppController controller(&settings);
	TaskController taskController(&settings);
	SettingsController settingsController(&settings);
	ContestTools contestTools;
	QQmlApplicationEngine engine;
	registerControlImages(&engine);
	engine.setUiLanguage(settings.getUiLanguage());
	engine.rootContext()->setContextProperty("appController", &controller);
	engine.rootContext()->setContextProperty("taskController", &taskController);
	engine.rootContext()->setContextProperty("settingsController", &settingsController);
	engine.rootContext()->setContextProperty("contestTools", &contestTools);
	QObject::connect(&controller, &AppController::contestChanged, &taskController, [&] {
		taskController.setContest(controller.getContest());
		contestTools.setContest(controller.getContest());
	});
	QObject::connect(&controller, &AppController::judgingChanged, &taskController, [&] {
		taskController.setBusy(controller.judging());
		contestTools.setBusy(controller.judging());
	});
	QObject::connect(&controller, &AppController::contentChanged, &contestTools, &ContestTools::refresh);
	QObject::connect(&controller, &AppController::dataFilesChanged, &taskController,
	                 &TaskController::refresh);
	QObject::connect(&taskController, &TaskController::contestEdited, &controller, &AppController::edited);
	QObject::connect(&contestTools, &ContestTools::filesChanged, &controller,
	                 &AppController::refreshContestants);
	QObject::connect(&settingsController, &SettingsController::settingsApplied, &controller, [&] {
		LemonLimeTranslator->InstallTranslation(settings.getUiLanguage());
		engine.setUiLanguage(settings.getUiLanguage());
		controller.settingsApplied();
		taskController.refresh();
		engine.retranslate();
	});
	QObject::connect(&app, &SingleApplication::receivedMessage, &engine, [&](quint32, const QByteArray &) {
		if (engine.rootObjects().isEmpty())
			return;
		if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first())) {
			window->showNormal();
			window->raise();
			window->requestActivate();
		}
	});
	engine.loadFromModule("LemonLime", "Main");
	if (engine.rootObjects().isEmpty())
		return 1;
	return app.exec();
}
