/*
 * SPDX-FileCopyrightText: 2011-2018 Project Lemon, Zhipeng Jia
 * SPDX-FileCopyrightText: 2018-2019
 * Project LemonPlus, Dust1404
 * SPDX-FileCopyrightText: 2019-2026 Project LemonLime
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "resultdetails.h"
#include "base/settings.h"
#include "core/contest.h"
#include "core/contestant.h"
#include "core/subtaskdependencelib.h"
#include "core/task.h"
#include "core/testcase.h"
#include <QCoreApplication>

QString resultDetailsHtml(Contest *contest, Contestant *contestant) {
	QString htmlCode;
	htmlCode += "<html><head>";
	htmlCode += "<style type=\"text/css\">th, td {padding-left: 1em; padding-right: 1em;}</style>";
	htmlCode += "</head><body>";
	QList<Task *> taskList = contest->getTaskList();

	for (int i = 0; i < taskList.size(); i++) {
		htmlCode += "<p><span style=\"font-weight:bold; font-size:large;\">";
		htmlCode +=
		    QString(R"(%1 %2 (%5 / %6) <a href="Rejudge %3" style="text-decoration: none">(%4)</span>)")
		        .arg(QCoreApplication::translate("DetailDialog", "Task"))
		        .arg(taskList[i]->getProblemTitle().toHtmlEscaped())
		        .arg(i)
		        .arg(QCoreApplication::translate("DetailDialog", "Rejudge"))
		        .arg(contestant->getTaskScore(i))
		        .arg(taskList[i]->getTotalScore());
	}

	htmlCode += "<HR>";

	for (int i = 0; i < taskList.size(); i++) {
		htmlCode += "<p><span style=\"font-weight:bold; font-size:large;\">";
		htmlCode +=
		    QString(R"(%1 %2 (%5 / %6) <a href="Rejudge %3" style="text-decoration: none">(%4)</span><br>)")
		        .arg(QCoreApplication::translate("DetailDialog", "Task"))
		        .arg(taskList[i]->getProblemTitle().toHtmlEscaped())
		        .arg(i)
		        .arg(QCoreApplication::translate("DetailDialog", "Rejudge"))
		        .arg(contestant->getTaskScore(i))
		        .arg(taskList[i]->getTotalScore());

		if (! contestant->getCheckJudged(i)) {
			htmlCode +=
			    QString("&nbsp;&nbsp;%1</p>").arg(QCoreApplication::translate("DetailDialog", "Not judged"));
			continue;
		}

		if (taskList[i]->getTaskType() == Task::Traditional ||
		    taskList[i]->getTaskType() == Task::Interaction ||
		    taskList[i]->getTaskType() == Task::Communication ||
		    taskList[i]->getTaskType() == Task::CommunicationExec) {
			if (contestant->getCompileState(i) != CompileSuccessfully) {
				switch (contestant->getCompileState(i)) {
					case NoValidGraderFile:
						htmlCode += QString("&nbsp;&nbsp;%1</p>")
						                .arg(QCoreApplication::translate(
						                    "DetailDialog", "Main grader (grader.*) cannot be found"));
						break;

					case NoValidSourceFile:
						htmlCode += QString("&nbsp;&nbsp;%1</p>")
						                .arg(QCoreApplication::translate("DetailDialog",
						                                                 "Cannot find valid source file"));
						break;

					case CompileTimeLimitExceeded:
						htmlCode += QString("&nbsp;&nbsp;%1%2<br>")
						                .arg(QCoreApplication::translate("DetailDialog", "Source file: "))
						                .arg(contestant->getSourceFile(i).toHtmlEscaped());
						htmlCode += QString("&nbsp;&nbsp;%1</p>")
						                .arg(QCoreApplication::translate("DetailDialog",
						                                                 "Compile time limit exceeded"));
						break;

					case InvalidCompiler:
						htmlCode += QString("&nbsp;&nbsp;%1</p>")
						                .arg(QCoreApplication::translate("DetailDialog",
						                                                 "Cannot run given compiler"));
						break;

					case CompileError:
						htmlCode += QString("&nbsp;&nbsp;%1%2<br>")
						                .arg(QCoreApplication::translate("DetailDialog", "Source file: "))
						                .arg(contestant->getSourceFile(i).toHtmlEscaped());
						htmlCode += QString("&nbsp;&nbsp;%1")
						                .arg(QCoreApplication::translate("DetailDialog", "Compile error"));

						if (! contestant->getCompileMessage(i).isEmpty())
							htmlCode +=
							    QString(R"(<a href="CompileMessage %1" style="text-decoration: none"> (...))")
							        .arg(i);

						htmlCode += "</p>";
						break;

					case CompileSuccessfully:
						break;
				}

				continue;
			}

			htmlCode += QString("&nbsp;&nbsp;%1%2")
			                .arg(QCoreApplication::translate("DetailDialog", "Source file: "))
			                .arg(contestant->getSourceFile(i).toHtmlEscaped());
		}

		htmlCode += R"(<table width="100%" cellpadding="1" border="1"><tr>)";
		htmlCode += QString(R"(<th scope="col" nowrap="nowrap">%1</th>)")
		                .arg(QCoreApplication::translate("DetailDialog", "Test Case"));
		htmlCode += QString(R"(<th scope="col" nowrap="nowrap">%1</th>)")
		                .arg(QCoreApplication::translate("DetailDialog", "Input File"));
		htmlCode +=
		    QString("<th scope=\"col\">%1</th>").arg(QCoreApplication::translate("DetailDialog", "Result"));
		htmlCode += QString(R"(<th scope="col" nowrap="nowrap">%1</th>)")
		                .arg(QCoreApplication::translate("DetailDialog", "Time Used"));
		htmlCode += QString(R"(<th scope="col" nowrap="nowrap">%1</th>)")
		                .arg(QCoreApplication::translate("DetailDialog", "Memory Used"));
		htmlCode += QString(R"(<th scope="col" nowrap="nowrap">%1</th></tr>)")
		                .arg(QCoreApplication::translate("DetailDialog", "Score"));
		QList<TestCase *> testCases = taskList[i]->getTestCaseList();
		QList<QStringList> inputFiles = contestant->getInputFiles(i);
		QList<QList<ResultState>> result = contestant->getResult(i);
		QList<QStringList> message = contestant->getMessage(i);
		QList<QList<int>> timeUsed = contestant->getTimeUsed(i);
		QList<QList<qint64>> memoryUsed = contestant->getMemoryUsed(i);
		QList<QList<int>> score = contestant->getScore(i);

		for (int j = 0; j < inputFiles.size(); j++) {
			const int count = inputFiles[j].size();
			if (j >= testCases.size() || result.value(j).size() < count || message.value(j).size() < count ||
			    timeUsed.value(j).size() < count || memoryUsed.value(j).size() < count ||
			    score.value(j).size() < count)
				continue;
			for (int k = 0; k < inputFiles[j].size(); k++) {
				htmlCode += "<tr>";

				if (k == 0) {
					if (score[j].size() == inputFiles[j].size())
						htmlCode +=
						    QString(
						        R"(<td nowrap="nowrap" rowspan="%1" align="center" valign="middle">#%2</td>)")
						        .arg(inputFiles[j].size())
						        .arg(j + 1);
					else
						htmlCode +=
						    QString(
						        R"(<td nowrap="nowrap" rowspan="%1" align="center" valign="middle">#%2<br>%3:%4</td>)")
						        .arg(inputFiles[j].size())
						        .arg(j + 1)
						        .arg(QCoreApplication::translate("DetailDialog", "Subtask Dependence Status"))
						        .arg(statusRankingText(score[j].back()));
				}

				htmlCode += QString(R"(<td nowrap="nowrap" align="center" valign="middle">%1</td>)")
				                .arg(inputFiles[j][k].toHtmlEscaped());
				QString text;
				QString bgColor = "rgb(255, 255, 255)";
				QString frColor = "rgb(0, 0, 0)";
				Settings::setTextAndColor(result[j][k], text, frColor, bgColor);
				htmlCode +=
				    QString(
				        R"(<td align="center" valign="middle" style="background-color: %2; color: %3;">%1)")
				        .arg(text)
				        .arg(bgColor)
				        .arg(frColor);

				if (! message[j][k].isEmpty()) {
					htmlCode +=
					    QString(R"(<a href="Message %1 %2 %3" style="text-decoration: none"> (...)</a>)")
					        .arg(i)
					        .arg(j)
					        .arg(k);
				}

				htmlCode += "</td>";
				htmlCode += R"(<td nowrap="nowrap" align="center" valign="middle">)";

				if (timeUsed[j][k] != -1) {
					htmlCode += QString("").asprintf("%.3lf s", double(timeUsed[j][k]) / 1000);
				} else {
					htmlCode += QCoreApplication::translate("DetailDialog", "Invalid");
				}

				htmlCode += "</td>";
				htmlCode += R"(<td nowrap="nowrap" align="center" valign="middle">)";

				if (memoryUsed[j][k] != -1) {
					htmlCode += QString("").asprintf("%.3lf MiB", double(memoryUsed[j][k]) / 1024 / 1024);
				} else {
					htmlCode += QCoreApplication::translate("DetailDialog", "Invalid");
				}

				htmlCode += "</td>";

				if (k == 0) {
					int minv = 2147483647;
					int maxv = testCases[j]->getFullScore();

					for (int t = 0; t < inputFiles[j].size(); t++)
						if (score[j][t] < minv)
							minv = score[j][t];

					int tempStatus = maxDependValue + 1;

					if (! testCases[j]->getDependenceSubtask().empty()) {
						tempStatus = score[j].back();
						minv = qMin(minv, statusToScore(score[j].back(), maxv));
					}

					QString bgColor = "rgb(192, 255, 192)";

					for (int t = 0; t < inputFiles[j].size(); t++)
						if (result[j][t] != CorrectAnswer)
							bgColor = "rgb(192, 255, 255)";

					if (tempStatus < maxDependValue)
						bgColor = "rgb(192, 255, 255)";

					for (int t = 0; t < inputFiles[j].size(); t++)
						if (result[j][t] != CorrectAnswer && result[j][t] != PartlyCorrect)
							bgColor = "rgb(255, 192, 192)";

					if (tempStatus < 0)
						bgColor = "rgb(255, 192, 192)";

					htmlCode +=
					    QString(
					        R"(<td rowspan="%1" align="center" valign="middle" style="background-color: %2;"><a style="font-weight: bold; font-size: large;">%3</a> / %4</td>)")
					        .arg(inputFiles[j].size())
					        .arg(bgColor)
					        .arg(minv)
					        .arg(maxv);
				}

				htmlCode += "</tr>";
			}
		}

		htmlCode += "</table><br></p>";
	}

	htmlCode += "</body></html>";

	return htmlCode;
}
