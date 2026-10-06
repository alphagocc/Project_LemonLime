/*
 * SPDX-FileCopyrightText: 2026 Project LemonLime
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "resultmodel.h"
#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QVariantList>

class Contest;
class Settings;
class QFileSystemWatcher;

class AppController : public QObject {
	Q_OBJECT
	Q_PROPERTY(bool hasContest READ hasContest NOTIFY contestChanged)
	Q_PROPERTY(QString contestTitle READ contestTitle NOTIFY contentChanged)
	Q_PROPERTY(QString contestFile READ contestFile NOTIFY contestChanged)
	Q_PROPERTY(QStringList recentContests READ recentContests NOTIFY recentChanged)
	Q_PROPERTY(QVariantList recentEntries READ recentEntries NOTIFY recentChanged)
	Q_PROPERTY(bool judging READ judging NOTIFY judgingChanged)
	Q_PROPERTY(bool stopping READ stopping NOTIFY judgingChanged)
	Q_PROPERTY(int taskCount READ taskCount NOTIFY contentChanged)
	Q_PROPERTY(int fullScore READ fullScore NOTIFY contentChanged)
	Q_PROPERTY(int completed READ completed NOTIFY progressChanged)
	Q_PROPERTY(int total READ total NOTIFY progressChanged)
	Q_PROPERTY(QStringList progressLog READ progressLog NOTIFY progressChanged)
	Q_PROPERTY(QString progressHtml READ progressHtml NOTIFY progressChanged)
	Q_PROPERTY(int progressValue READ progressValue NOTIFY progressChanged)
	Q_PROPERTY(int progressMaximum READ progressMaximum NOTIFY progressChanged)
	Q_PROPERTY(QString detailHtml READ detailHtml NOTIFY detailChanged)
	Q_PROPERTY(ResultModel *results READ results CONSTANT)
	Q_PROPERTY(QString version READ version CONSTANT)
	Q_PROPERTY(int splashDuration READ splashDuration CONSTANT)
	Q_PROPERTY(QSize windowSize READ windowSize CONSTANT)
  public:
	explicit AppController(Settings *settings, QObject *parent = nullptr);
	~AppController() override;
	Contest *getContest() const;
	bool hasContest() const;
	QString contestTitle() const;
	QString contestFile() const;
	QStringList recentContests() const;
	QVariantList recentEntries() const;
	Q_INVOKABLE bool addRecent(const QUrl &file);
	Q_INVOKABLE QString defaultContestPath(const QString &name) const;
	Q_INVOKABLE QString localFile(const QUrl &url) const;
	bool judging() const;
	bool stopping() const;
	int taskCount() const;
	int fullScore() const;
	int completed() const;
	int total() const;
	QStringList progressLog() const;
	QString progressHtml() const;
	int progressValue() const;
	int progressMaximum() const;
	Q_INVOKABLE void detailLink(const QString &link);
	QString detailHtml() const;
	ResultModel *results();
	QString version() const;
	int splashDuration() const;
	QSize windowSize() const;
	Q_INVOKABLE void saveWindowSize(int width, int height);
	Q_INVOKABLE bool newContest(const QString &title, const QUrl &directory, const QString &fileName);
	Q_INVOKABLE bool openContest(const QUrl &file);
	Q_INVOKABLE bool saveContest();
	Q_INVOKABLE bool closeContest();
	Q_INVOKABLE void renameContest(const QString &title);
	Q_INVOKABLE void removeRecent(int index);
	Q_INVOKABLE void refreshContestants();
	Q_INVOKABLE void judge(const QString &mode);
	Q_INVOKABLE void rejudge(const QString &name, int task);
	Q_INVOKABLE void stop();
	Q_INVOKABLE void showDetails(const QString &name, int task = -1);
	Q_INVOKABLE void deleteSelected(bool removeFiles);
	Q_INVOKABLE void openFolder();
	Q_INVOKABLE bool exportManual(const QUrl &destination);
	Q_INVOKABLE void edited();
	Q_INVOKABLE void settingsApplied();
	Q_INVOKABLE QUrl localUrl(const QString &file) const;
	Q_INVOKABLE bool prepareExit();
  signals:
	void contestChanged();
	void contentChanged();
	void recentChanged();
	void judgingChanged();
	void progressChanged();
	void detailChanged();
	void errorOccurred(const QString &message);
	void messageRequested(const QString &title, const QString &message);
	void notification(const QString &message);
	void dataFilesChanged();

  private:
	void installContest(Contest *value, const QString &file);
	void watchDirectories();
	void addLog(const QString &message);
	void runJudge(const QList<std::pair<QString, QVector<int>>> &queue);
	Settings *settings;
	Contest *contest = nullptr;
	ResultModel model;
	QString currentFile;
	QString initialDirectory;
	QString details;
	QString detailName;
	int detailTask = -1;
	bool isJudging = false;
	bool isStopping = false;
	bool judgeStarted = false;
	int done = 0;
	int scheduled = 0;
	int currentProgress = 0;
	int maximumProgress = 0;
	QStringList log;
	QTimer autosave;
	QTimer watcherTimer;
	QFileSystemWatcher *watcher;
};
