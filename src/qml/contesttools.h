/*
 * SPDX-FileCopyrightText: 2011-2018 Project Lemon, Zhipeng Jia
 * SPDX-FileCopyrightText: 2018-2019 Project LemonPlus, Dust1404
 * SPDX-FileCopyrightText: 2019-2026 Project LemonLime
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#pragma once

#include "base/LemonType.hpp"
#include <QMap>
#include <QObject>
#include <QPointer>
#include <QUrl>

class Contest;
class Contestant;
class Task;
class TestCase;

class ContestTools : public QObject {
	Q_OBJECT
	Q_PROPERTY(QString statisticsHtml READ getStatisticsHtml NOTIFY statisticsChanged)
	Q_PROPERTY(bool xlsAvailable READ getXlsAvailable CONSTANT)

  public:
	explicit ContestTools(QObject *parent = nullptr);
	void setContest(Contest *contest);
	void setBusy(bool busy);
	QString getStatisticsHtml() const;
	bool getXlsAvailable() const;

	Q_INVOKABLE void refresh();
	Q_INVOKABLE bool exportResults(const QUrl &destination, int format);
	Q_INVOKABLE bool exportStatistics(const QUrl &destination);
	Q_INVOKABLE bool organizeSources(bool backup);
	Q_INVOKABLE QString backupDirectoryName() const;

  signals:
	void statisticsChanged();
	void errorOccurred(const QString &message);
	void notification(const QString &message);
	void filesChanged();

  private:
	QPointer<Contest> contest;
	QString contestDirectory;
	QString statisticsHtml;
	bool busy = false;

	bool checkReady();
	bool saveText(const QUrl &destination, const QString &text);
	QString buildStatisticsHtml() const;
	static QString getContestantHtmlCode(Contest *contest, Contestant *contestant, int number);
	static QString getSmallerContestantHtmlCode(Contest *contest, Contestant *contestant);
	static QString resultHtml(Contest *contest);
	static QString resultSmallerHtml(Contest *contest);
	static QString resultCsv(Contest *contest);
	static QString getScoreNormalChart(const QMap<int, int> &scoreCount, int listSize, int totalScore);
	static QString getTestcaseScoreChart(QList<TestCase *> testCases, QList<QList<QList<int>>> scores,
	                                     QList<QList<QList<ResultState>>> results);
	static bool checkValid(QList<Task *> tasks, const QList<Contestant *> &contestants);
#ifdef ENABLE_XLS_EXPORT
	bool exportXls(const QUrl &destination);
#endif
};
