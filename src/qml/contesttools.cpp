/*
 * SPDX-FileCopyrightText: 2011-2018 Project Lemon, Zhipeng Jia
 * SPDX-FileCopyrightText: 2018-2019 Project LemonPlus, Dust1404
 * SPDX-FileCopyrightText: 2019-2026 Project LemonLime
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include "contesttools.h"
#include "base/settings.h"
#include "core/contest.h"
#include "core/contestant.h"
#include "core/subtaskdependencelib.h"
#include "core/task.h"
#include "core/testcase.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QTextStream>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <tuple>
#ifdef ENABLE_XLS_EXPORT
#include <QAxObject>
#include <QScopeGuard>
#include <memory>
#endif

namespace {
	bool isWithin(const QString &directory, const QString &fileName) {
		const QString relative = QDir(directory).relativeFilePath(fileName);
		return ! QDir::isAbsolutePath(relative) && relative != ".." && ! relative.startsWith("../");
	}

	bool safeDirectoryName(const QString &name) {
		return ! name.isEmpty() && name != "." && name != ".." && ! name.contains('/') &&
		       ! name.contains('\\') && ! name.contains(':') && ! name.endsWith('.') && ! name.endsWith(' ');
	}

	bool inspectTree(const QString &directory, const QString &root, QString &error) {
		const QFileInfo current(directory);
		if (current.isSymLink() || current.isJunction() || current.canonicalFilePath().isEmpty() ||
		    ! isWithin(root, current.canonicalFilePath())) {
			error = ContestTools::tr(
			            "The source tree contains a link or a file outside the contest directory: %1")
			            .arg(directory);
			return false;
		}
		if (current.isFile())
			return true;
		if (! current.isDir()) {
			error = ContestTools::tr("Unsupported source entry: %1").arg(directory);
			return false;
		}
		const auto entries = QDir(directory).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot |
		                                                   QDir::Hidden | QDir::System);
		for (const QFileInfo &entry : entries) {
			if (! inspectTree(entry.absoluteFilePath(), root, error))
				return false;
		}
		return true;
	}

	bool copyTree(const QString &from, const QString &to, QString &error) {
		if (QFileInfo(from).isFile()) {
			if (QFile::copy(from, to))
				return true;
			error = ContestTools::tr("Cannot copy %1 to %2").arg(from, to);
			return false;
		}
		if (! QDir().mkpath(to)) {
			error = ContestTools::tr("Cannot create directory %1").arg(to);
			return false;
		}
		const auto entries =
		    QDir(from).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
		for (const QFileInfo &entry : entries) {
			if (! copyTree(entry.absoluteFilePath(), QDir(to).filePath(entry.fileName()), error))
				return false;
		}
		return true;
	}

	QList<std::pair<int, QString>> rankedContestants(Contest *contest) {
		QList<std::pair<int, QString>> result;
		for (const auto *contestant : contest->getContestantList()) {
			const int score = contestant->getTotalScore();
			result.append({score < 0 ? 1 : -score, contestant->getContestantName()});
		}
		std::sort(result.begin(), result.end());
		return result;
	}

	auto resultStateMap() {
		QMap<ResultState, std::tuple<QString, QString, QString>> result;
		for (int state = CorrectAnswer; state < LastResultState; ++state) {
			QString text, foreground, background;
			Settings::setTextAndColor(static_cast<ResultState>(state), text, foreground, background);
			result[static_cast<ResultState>(state)] = {text, foreground, background};
		}
		return result;
	}
} // namespace

ContestTools::ContestTools(QObject *parent) : QObject(parent) { refresh(); }

void ContestTools::setContest(Contest *value) {
	if (contest)
		disconnect(contest, nullptr, this, nullptr);
	contest = value;
	contestDirectory = value ? QFileInfo(QDir::currentPath()).canonicalFilePath() : QString();
	if (contest) {
		connect(contest, &Contest::taskJudgingFinished, this, &ContestTools::refresh);
		connect(contest, &Contest::taskAddedForViewer, this, &ContestTools::refresh);
		connect(contest, &Contest::taskDeletedForViewer, this, &ContestTools::refresh);
		connect(contest, &Contest::problemTitleChanged, this, &ContestTools::refresh);
		connect(contest, &QObject::destroyed, this, [this]() {
			contest = nullptr;
			contestDirectory.clear();
			refresh();
		});
	}
	refresh();
}

void ContestTools::setBusy(bool value) {
	busy = value;
	if (! busy)
		refresh();
}

QString ContestTools::getStatisticsHtml() const { return statisticsHtml; }

bool ContestTools::getXlsAvailable() const {
#ifdef ENABLE_XLS_EXPORT
	return true;
#else
	return false;
#endif
}

void ContestTools::refresh() {
	if (busy)
		return;
	statisticsHtml = buildStatisticsHtml();
	emit statisticsChanged();
}

bool ContestTools::checkReady() {
	if (busy) {
		emit errorOccurred(tr("Wait for judging to finish before using this action."));
		return false;
	}
	if (! contest) {
		emit errorOccurred(tr("No contest yet"));
		return false;
	}
	return true;
}

bool ContestTools::saveText(const QUrl &destination, const QString &text) {
	if (! destination.isLocalFile() || destination.toLocalFile().isEmpty()) {
		emit errorOccurred(tr("Select a local file for export."));
		return false;
	}
	QSaveFile file(destination.toLocalFile());
	const QByteArray bytes = text.toUtf8();
	if (! file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || ! file.commit()) {
		emit errorOccurred(tr("Cannot save %1: %2").arg(destination.toLocalFile(), file.errorString()));
		return false;
	}
	emit notification(tr("Export is done"));
	return true;
}

bool ContestTools::exportResults(const QUrl &destination, int format) {
	if (! checkReady())
		return false;
	if (contest->getTaskList().isEmpty() || contest->getContestantList().isEmpty()) {
		emit errorOccurred(tr("Add tasks and contestants before exporting results."));
		return false;
	}
	switch (format) {
		case 0:
			return saveText(destination, resultHtml(contest));
		case 1:
			return saveText(destination, resultSmallerHtml(contest));
		case 2:
			return saveText(destination, resultCsv(contest));
#ifdef ENABLE_XLS_EXPORT
		case 3:
			return exportXls(destination);
#endif
		default:
			emit errorOccurred(tr("The selected export format is unavailable."));
			return false;
	}
}

bool ContestTools::exportStatistics(const QUrl &destination) {
	if (! checkReady())
		return false;
	if (contest->getTaskList().isEmpty() || contest->getContestantList().isEmpty() ||
	    ! checkValid(contest->getTaskList(), contest->getContestantList())) {
		emit errorOccurred(
		    tr("Judge all contestants with the current test cases before exporting statistics."));
		return false;
	}
	refresh();
	return saveText(destination, statisticsHtml);
}

QString ContestTools::backupDirectoryName() const {
	QDir directory(contestDirectory);
	int index = 0;
	while (QFileInfo::exists(directory.filePath(QString("source_bak_%1").arg(index))))
		++index;
	return QString("source_bak_%1").arg(index);
}

QString ContestTools::resultCsv(Contest *contest) {
	const auto quoted = [](QString value) {
		value.replace('"', "\"\"");
		return '"' + value + '"';
	};
	QStringList header{tr("Rank"), tr("Name")};
	for (const auto *task : contest->getTaskList())
		header.append(task->getProblemTitle());
	header.append(tr("Total Score"));
	for (QString &value : header)
		value = quoted(value);
	QString output = header.join(',') + "\r\n";
	const auto sorted = rankedContestants(contest);
	int rank = 0;
	for (int index = 0; index < sorted.size(); ++index) {
		if (index == 0 || sorted[index].first != sorted[index - 1].first)
			rank = index + 1;
		const auto *contestant = contest->getContestant(sorted[index].second);
		QStringList row{QString::number(rank), contestant->getContestantName()};
		for (int task = 0; task < contest->getTaskList().size(); ++task) {
			const int score = contestant->getTaskScore(task);
			row.append(score < 0 ? tr("Invalid") : QString::number(score));
		}
		const int score = contestant->getTotalScore();
		row.append(score < 0 ? tr("Invalid") : QString::number(score));
		for (QString &value : row)
			value = quoted(value);
		output += row.join(',') + "\r\n";
	}
	return output;
}

#ifdef ENABLE_XLS_EXPORT
bool ContestTools::exportXls(const QUrl &destination) {
	if (! destination.isLocalFile() || destination.toLocalFile().isEmpty()) {
		emit errorOccurred(tr("Select a local file for export."));
		return false;
	}
	const QString fileName = QFileInfo(destination.toLocalFile()).absoluteFilePath();
	QTemporaryDir temporary(QFileInfo(fileName).absolutePath() + "/.lemon-export-XXXXXX");
	if (! temporary.isValid()) {
		emit errorOccurred(tr("Cannot create a temporary export file."));
		return false;
	}
	QAxObject excel("Excel.Application");
	if (excel.isNull()) {
		emit errorOccurred(tr("Microsoft Excel is required for XLS export."));
		return false;
	}
	QString exception;
	const auto watchExceptions = [this, &exception](QAxObject *object) {
		if (object)
			connect(object, &QAxObject::exception, this,
			        [&exception](int, const QString &, const QString &description, const QString &) {
				        exception = description;
			        });
	};
	watchExceptions(&excel);
	const auto quitExcel = qScopeGuard([&excel]() { excel.dynamicCall("Quit()"); });
	excel.setProperty("Visible", false);
	excel.setProperty("DisplayAlerts", false);
	std::unique_ptr<QAxObject> books(excel.querySubObject("Workbooks"));
	watchExceptions(books.get());
	std::unique_ptr<QAxObject> workbook(books ? books->querySubObject("Add") : nullptr);
	if (! workbook) {
		emit errorOccurred(tr("Cannot create an Excel workbook: %1").arg(exception));
		return false;
	}
	watchExceptions(workbook.get());
	const auto closeWorkbook = qScopeGuard([&workbook]() { workbook->dynamicCall("Close(bool)", false); });
	std::unique_ptr<QAxObject> sheet(workbook->querySubObject("ActiveSheet"));
	if (! sheet) {
		emit errorOccurred(tr("Cannot access the Excel worksheet: %1").arg(exception));
		return false;
	}
	watchExceptions(sheet.get());
	sheet->setProperty("Name", QDate::currentDate().toString("yyyy-MM-dd"));
	const auto writeCell = [&sheet, &exception, &watchExceptions](int row, int column, const QVariant &value,
	                                                              bool bold) {
		std::unique_ptr<QAxObject> cell(sheet->querySubObject("Cells(int, int)", row, column));
		if (! cell)
			return false;
		watchExceptions(cell.get());
		cell->setProperty("Value", value);
		if (bold) {
			std::unique_ptr<QAxObject> font(cell->querySubObject("Font"));
			if (font) {
				watchExceptions(font.get());
				font->setProperty("Bold", true);
			}
		}
		return exception.isEmpty();
	};
	QStringList header{tr("Rank"), tr("Name")};
	for (const auto *task : contest->getTaskList())
		header.append(task->getProblemTitle());
	header.append(tr("Total Score"));
	for (int column = 0; column < header.size(); ++column) {
		if (! writeCell(1, column + 1, header[column], true)) {
			emit errorOccurred(tr("Cannot write the Excel worksheet: %1").arg(exception));
			return false;
		}
	}
	const auto sorted = rankedContestants(contest);
	int rank = 0;
	for (int index = 0; index < sorted.size(); ++index) {
		if (index == 0 || sorted[index].first != sorted[index - 1].first)
			rank = index + 1;
		const auto *contestant = contest->getContestant(sorted[index].second);
		QVariantList row{rank, contestant->getContestantName()};
		for (int task = 0; task < contest->getTaskList().size(); ++task) {
			const int score = contestant->getTaskScore(task);
			row.append(score < 0 ? QVariant(tr("Invalid")) : QVariant(score));
		}
		const int score = contestant->getTotalScore();
		row.append(score < 0 ? QVariant(tr("Invalid")) : QVariant(score));
		for (int column = 0; column < row.size(); ++column) {
			if (! writeCell(index + 2, column + 1, row[column], false)) {
				emit errorOccurred(tr("Cannot write the Excel worksheet: %1").arg(exception));
				return false;
			}
		}
	}
	const QString stagedFile = temporary.filePath("results.xls");
	workbook->dynamicCall("SaveAs(const QString&, int)", QDir::toNativeSeparators(stagedFile), -4143);
	QFile input(stagedFile);
	if (! exception.isEmpty() || ! input.open(QIODevice::ReadOnly)) {
		emit errorOccurred(tr("Cannot save the Excel workbook: %1")
		                       .arg(exception.isEmpty() ? input.errorString() : exception));
		return false;
	}
	QSaveFile output(fileName);
	const QByteArray bytes = input.readAll();
	if (input.error() != QFileDevice::NoError || ! output.open(QIODevice::WriteOnly) ||
	    output.write(bytes) != bytes.size() || ! output.commit()) {
		emit errorOccurred(tr("Cannot save %1: %2").arg(fileName, output.errorString()));
		return false;
	}
	emit notification(tr("Export is done"));
	return true;
}
#endif

bool ContestTools::organizeSources(bool backup) {
	if (! checkReady())
		return false;
	const QDir root(contestDirectory);
	const QString source = root.filePath("source");
	QString error;
	const auto fail = [this](const QString &message) {
		emit errorOccurred(message);
		return false;
	};
	if (contestDirectory.isEmpty() || ! QFileInfo(source).isDir() ||
	    ! inspectTree(source, contestDirectory, error))
		return fail(error.isEmpty() ? tr("The contest source directory is unavailable.") : error);

	QMap<QString, Task *> tasks;
	QMap<QString, QString> sourceNames;
	for (auto *task : contest->getTaskList()) {
		const QString name = task->getSourceFileName();
		if (! safeDirectoryName(name))
			return fail(tr("A task source name must be a single directory name: %1").arg(name));
		tasks[name] = task;
		if (task->getTaskType() == Task::AnswersOnly) {
			for (const auto *test : task->getTestCaseList()) {
				for (const QString &input : test->getInputFiles())
					sourceNames[QFileInfo(input).completeBaseName()] = name;
			}
		} else if (task->getTaskType() == Task::Communication ||
		           task->getTaskType() == Task::CommunicationExec) {
			for (const QString &file : task->getSourceFilesPath())
				sourceNames[QFileInfo(file).completeBaseName()] = name;
		} else {
			sourceNames[name] = name;
		}
	}
	if (tasks.isEmpty())
		return fail(tr("Add tasks before organizing source files."));

	// Prepare a complete replacement before touching the original source tree.
	QTemporaryDir staging(root.filePath(".lemon-organize-XXXXXX"));
	if (! staging.isValid() || ! isWithin(contestDirectory, QFileInfo(staging.path()).canonicalFilePath()))
		return fail(tr("Cannot create a temporary directory inside the contest directory."));
	const QString replacement = staging.filePath("source");
	if (! QDir().mkpath(replacement))
		return fail(tr("Cannot create directory %1").arg(replacement));
	const auto entries =
	    QDir(source).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
	for (const QFileInfo &entry : entries) {
		const QString target = QDir(replacement).filePath(entry.fileName());
		// The legacy action processes visible contestant directories only.
		if (! entry.isDir() || entry.isHidden()) {
			if (! copyTree(entry.absoluteFilePath(), target, error))
				return fail(error);
			continue;
		}
		if (! QDir().mkpath(target))
			return fail(tr("Cannot create directory %1").arg(target));
		QMap<QString, QString> candidates;
		const QDir contestant(entry.absoluteFilePath());
		const auto files = contestant.entryInfoList(QDir::Files | QDir::Hidden);
		for (const QFileInfo &file : files) {
			if (! file.suffix().isEmpty() && file.suffix().compare("exe", Qt::CaseInsensitive) != 0)
				candidates[file.fileName()] = file.absoluteFilePath();
		}
		const auto directories = contestant.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);
		for (const QFileInfo &directory : directories) {
			if (directory.isHidden()) {
				if (! copyTree(directory.absoluteFilePath(), QDir(target).filePath(directory.fileName()),
				               error))
					return fail(error);
				continue;
			}
			if (! tasks.contains(directory.fileName()))
				continue;
			const auto taskFiles =
			    QDir(directory.absoluteFilePath()).entryInfoList(QDir::Files | QDir::Hidden);
			for (const QFileInfo &file : taskFiles) {
				if (! file.suffix().isEmpty() && file.suffix().compare("exe", Qt::CaseInsensitive) != 0 &&
				    ! candidates.contains(file.fileName()))
					candidates[file.fileName()] = file.absoluteFilePath();
			}
		}
		for (auto task = tasks.cbegin(); task != tasks.cend(); ++task) {
			if (! QDir(target).mkpath(task.key()))
				return fail(tr("Cannot create directory %1").arg(QDir(target).filePath(task.key())));
		}
		for (auto file = candidates.cbegin(); file != candidates.cend(); ++file) {
			const QFileInfo info(file.key());
			const auto mapping = sourceNames.constFind(info.completeBaseName());
			if (mapping == sourceNames.cend())
				continue;
			const auto *task = tasks.value(mapping.value());
			const bool accepted =
			    task->getTaskType() == Task::AnswersOnly
			        ? info.suffix() == task->getAnswerFileExtension()
			        : file.key() != task->getInputFileName() && file.key() != task->getOutputFileName();
			if (! accepted)
				continue;
			const QString nested = QDir(target).filePath(mapping.value() + '/' + file.key());
			if (! copyTree(file.value(), nested, error) ||
			    ! copyTree(file.value(), QDir(target).filePath(file.key()), error))
				return fail(error);
		}
	}

	QString previous = staging.filePath("previous");
	if (backup) {
		int index = 0;
		do {
			previous = root.filePath(QString("source_bak_%1").arg(index++));
		} while (QFileInfo::exists(previous));
	}
	if (! QDir().rename(source, previous))
		return fail(
		    tr("Cannot move the source directory. Close files opened by other programs and try again."));
	if (! QDir().rename(replacement, source)) {
		if (! QDir().rename(previous, source)) {
			staging.setAutoRemove(false);
			return fail(tr("Cannot restore the source directory. The original files are preserved in %1.")
			                .arg(previous));
		}
		return fail(tr("Cannot replace the source directory. The original files were restored."));
	}
	if (! backup && ! QDir(previous).removeRecursively()) {
		staging.setAutoRemove(false);
		emit notification(tr("Source files organized. Previous files remain in %1.").arg(previous));
	} else {
		emit notification(backup ? tr("Source files organized. Backup: %1").arg(previous)
		                         : tr("Source files organized."));
	}
	emit filesChanged();
	return true;
}

auto ContestTools::getContestantHtmlCode(Contest *contest, Contestant *contestant, int num) -> QString {
	QString htmlCode;
	QList<Task *> taskList = contest->getTaskList();

	for (int i = 0; i < taskList.size(); i++) {
		htmlCode += QString(R"(<div id="c%1p%2"><p><span>)").arg(num).arg(i);
		htmlCode +=
		    QString("%1 %2</span><br>").arg(tr("Task")).arg(taskList[i]->getProblemTitle().toHtmlEscaped());

		if (! contestant->getCheckJudged(i)) {
			htmlCode += QString("&nbsp;&nbsp;%1</p></div>").arg(tr("Not judged"));
			continue;
		}

		if (taskList[i]->getTaskType() == Task::Traditional ||
		    taskList[i]->getTaskType() == Task::Interaction ||
		    taskList[i]->getTaskType() == Task::Communication ||
		    taskList[i]->getTaskType() == Task::CommunicationExec) {
			if (contestant->getCompileState(i) != CompileSuccessfully) {
				switch (contestant->getCompileState(i)) {
					case NoValidGraderFile:
						htmlCode += QString("&nbsp;&nbsp;%1</p></div>")
						                .arg(tr("Main grader (grader.*) cannot be found"));
						break;

					case NoValidSourceFile:
						htmlCode +=
						    QString("&nbsp;&nbsp;%1</p></div>").arg(tr("Cannot find valid source file"));
						break;

					case CompileTimeLimitExceeded:
						htmlCode += QString("&nbsp;&nbsp;%1%2<br>")
						                .arg(tr("Source file: "))
						                .arg(contestant->getSourceFile(i).toHtmlEscaped());
						htmlCode +=
						    QString("&nbsp;&nbsp;%1</p></div>").arg(tr("Compile time limit exceeded"));
						break;

					case InvalidCompiler:
						htmlCode += QString("&nbsp;&nbsp;%1</p></div>").arg(tr("Cannot run given compiler"));
						break;

					case CompileError:
						htmlCode += QString("&nbsp;&nbsp;%1%2<br>")
						                .arg(tr("Source file: "))
						                .arg(contestant->getSourceFile(i).toHtmlEscaped());
						htmlCode += QString("&nbsp;&nbsp;%1").arg(tr("Compile error"));

						if (! contestant->getCompileMessage(i).isEmpty()) {
							QString compileMessage = contestant->getCompileMessage(i).toHtmlEscaped();
							compileMessage.replace("\r\n", "<br>");
							compileMessage.replace("\n", "<br>");
							compileMessage.replace("\r", "<br>");

							if (compileMessage.endsWith("<br>"))
								compileMessage.chop(4);

							htmlCode += R"(<table border="1"  cellpadding="1">)";
							htmlCode += "<tr><td style=\"padding: 0.5em; text-align: left;\"><code>";
							htmlCode += compileMessage;
							htmlCode += "</code></td></tr></table>";
						}

						htmlCode += "</p></div>";
						break;

					default:
						break;
				}

				continue;
			}

			htmlCode += QString("&nbsp;&nbsp;%1%2")
			                .arg(tr("Source file: "))
			                .arg(contestant->getSourceFile(i).toHtmlEscaped());
		}

		htmlCode += "<table><tr>";
		htmlCode += QString(R"(<th>%1</th>)").arg(tr("Test Case"));
		htmlCode += QString(R"(<th>%1</th>)").arg(tr("Input File"));
		htmlCode += QString(R"(<th>%1</th>)").arg(tr("Result"));
		htmlCode += QString(R"(<th>%1</th>)").arg(tr("Time Used"));
		htmlCode += QString(R"(<th>%1</th>)").arg(tr("Memory Used"));
		htmlCode += QString(R"(<th>%1</th></tr>)").arg(tr("Score"));
		QList<TestCase *> testCases = taskList[i]->getTestCaseList();
		QList<QStringList> inputFiles = contestant->getInputFiles(i);
		QList<QList<ResultState>> result = contestant->getResult(i);
		QList<QStringList> message = contestant->getMessage(i);
		QList<QList<int>> timeUsed = contestant->getTimeUsed(i);
		QList<QList<qint64>> memoryUsed = contestant->getMemoryUsed(i);
		QList<QList<int>> score = contestant->getScore(i);

		for (int j = 0; j < inputFiles.size(); j++) {
			for (int k = 0; k < inputFiles[j].size(); k++) {
				htmlCode += "<tr>";

				if (k == 0) {
					if (score.value(j).size() == inputFiles[j].size())
						htmlCode +=
						    QString(R"(<td rowspan="%1">#%2</td>)").arg(inputFiles[j].size()).arg(j + 1);
					else
						htmlCode += QString(R"(<td rowspan="%1">#%2<br>%3:%4</td>)")
						                .arg(inputFiles[j].size())
						                .arg(j + 1)
						                .arg(tr("Subtask Dependence Status"))
						                .arg(statusRankingText(
						                    (score.value(j).isEmpty() ? -1 : score.value(j).back())));
				}

				htmlCode += QString("<td>%1</td>").arg(inputFiles[j][k].toHtmlEscaped());
				QString text, bgColor, frColor;
				Settings::setTextAndColor(result.value(j).value(k, FileError), text, frColor, bgColor);
				htmlCode += QString("<td class=\"result%2\">%1")
				                .arg(text)
				                .arg(static_cast<int>(result.value(j).value(k, FileError)));

				if (! message.value(j).value(k).isEmpty()) {
					htmlCode += QString("<details><summary>%1</summary><pre>%2</pre></details>")
					                .arg(tr("Message"), message.value(j).value(k).toHtmlEscaped());
				}

				htmlCode += "</td>";
				htmlCode += "<td>";

				if (timeUsed.value(j).value(k, -1) != -1) {
					htmlCode +=
					    QString("").asprintf("%.3lf s", double(timeUsed.value(j).value(k, -1)) / 1000);
				} else {
					htmlCode += tr("Invalid");
				}

				htmlCode += "</td>";
				htmlCode += "<td>";

				if (memoryUsed.value(j).value(k, -1) != -1) {
					htmlCode += QString("").asprintf("%.3lf MiB",
					                                 double(memoryUsed.value(j).value(k, -1)) / 1024 / 1024);
				} else {
					htmlCode += tr("Invalid");
				}

				htmlCode += "</td>";

				if (k == 0) {
					int minv = 2147483647;
					int maxv = (j < testCases.size() ? testCases[j]->getFullScore() : 0);

					for (int t = 0; t < inputFiles[j].size(); t++)
						if (score.value(j).value(t, 0) < minv)
							minv = score.value(j).value(t, 0);

					QString bgClass = "zero-score";

					if (minv >= maxv)
						bgClass = "full-score";
					else if (minv > 0)
						bgClass = "partial-score";

					htmlCode += QString(R"(<td rowspan="%1" class="%2"><span class="c">%3</span> / %4</td>)")
					                .arg(inputFiles[j].size())
					                .arg(bgClass)
					                .arg(minv)
					                .arg(maxv);
				}

				htmlCode += "</tr>";
			}
		}

		htmlCode += "</table><br></p></div>";
	}

	htmlCode += QString("<p><a href=\"#top\">%1</a></p>").arg(tr("Return to top"));
	return htmlCode;
}
/*
 * Generate the HTML code for the summary page
 * Might be difficult to maintain
 * Use Javascript to shrink the filesize
 */
QString ContestTools::resultHtml(Contest *contest) {
	Settings settings;
	contest->copySettings(settings);
	ColorTheme colors = settings.getCurrentColorTheme();

	QString output;
	QTextStream out(&output);
	QList<Contestant *> contestantList = contest->getContestantList();
	QList<Task *> taskList = contest->getTaskList();
	out << "<html><head>";
	out << R"(<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />)";

	// Style sheet
	out << "<style type=\"text/css\">"
	       "th, td {padding-left: 1em; padding-right: 1em; white-space: nowrap; "
	       "text-align: center; verticle-align: middle;}"
	       ".td-0 {border-style: none solid solid none; border-width: 1px 3px; border-color: #ccc;}"
	       ".th-0 {border-style: none solid solid none; border-width: 3px 3px; border-color: #000;}"
	       ".th-1 {border-style: none solid solid none; border-width: 3px 2px; border-color: #000;}"
	       ".a-0 {color: black; text-decoration: none;} .c {font-weight: bold; font-size: large;}"
	       ".td-2 {border-radius: 5px; font-weight: bold;}"
	       ".td-3 {border-radius: 5px;}"
	       ".full-score {background: rgb(192, 255, 192)}"
	       ".partial-score {background: rgb(192, 255, 255)}"
	       ".zero-score {background: rgb(255, 192, 192)}";
	for (auto [k, v] : resultStateMap().toStdMap()) {
		out << ".result" << static_cast<int>(k);
		out << QString(" {color: %1;background: %2;}").arg(std::get<1>(v)).arg(std::get<2>(v));
	}
	out << "</style>";

	out << "<title>" << contest->getContestTitle().toHtmlEscaped() << " : " << tr("Contest Result")
	    << "</title>";
	out << "</head><body>";
	QList<std::pair<int, QString>> sortList;

	for (auto &i : contestantList) {
		int totalScore = i->getTotalScore();

		if (totalScore != -1) {
			sortList.append(std::make_pair(-totalScore, i->getContestantName()));
		} else {
			sortList.append(std::make_pair(1, i->getContestantName()));
		}
	}

	std::sort(sortList.begin(), sortList.end());
	QMap<QString, int> rankList;

	for (int i = 0; i < sortList.size(); i++) {
		if (i > 0 && sortList[i].first == sortList[i - 1].first) {
			rankList.insert(sortList[i].second, rankList[sortList[i - 1].second]);
		} else {
			rankList.insert(sortList[i].second, i);
		}
	}

	QHash<Contestant *, int> loc;

	for (int i = 0; i < contestantList.size(); i++) {
		loc.insert(contestantList[i], i);
	}

	out << "<p><span class=\"d\">";
	out << "<a name=\"top\"></a>" << contest->getContestTitle().toHtmlEscaped() << " : " << tr("Rank List")
	    << "</span></p>";
	out << "<p>" << tr("Click names or task scores to jump to details. Judged By LemonLime") << "</p>";
	out << R"(<p><table cellpadding="1" style="border-style: solid;"><tr>)";
	out << QString(R"(<th class="th-0" scope="col">%1</th>)").arg(tr("Rank"));
	out << QString(R"(<th class="th-0" scope="col">%1</th>)").arg(tr("Name"));
	out << QString(R"(<th class="th-1" scope="col">%1</th>)").arg(tr("Total Score"));

	for (auto &i : taskList)
		out << QString(R"(<th class="th-1" scope="col">%1</th>)").arg(i->getProblemTitle().toHtmlEscaped());

	out << "</tr>";
	QList<int> fullScore;
	int sfullScore = contest->getTotalScore();

	for (auto &i : taskList) {
		fullScore.append(i->getTotalScore());
	}

	for (auto &i : sortList) {
		Contestant *contestant = contest->getContestant(i.second);
		out << "<tr>";
		out << QString("<td class=\"td-0\">%1</td>").arg(rankList[contestant->getContestantName()] + 1);
		out << QString(R"(<td class="td-0"><a href="#c%1" class="a-0">%2</a></td>)")
		           .arg(loc[contestant])
		           .arg(i.second.toHtmlEscaped());
		int allScore = contestant->getTotalScore();

		if (allScore >= 0) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
			float h = NAN;
			float s = NAN;
			float l = NAN;
#else
			double h = NAN;
			double s = NAN;
			double l = NAN;
#endif
			colors.getColorGrand(allScore, sfullScore).getHslF(&h, &s, &l);
			h *= 360, s *= 100, l *= 100;
			out << QString("<td class=\"td-2\" style=\"background: hsl(%2,%3%,%4%); border: 2px solid "
			               "hsl(%2,%3%,%5%);\">%1</td>")
			           .arg(allScore)
			           .arg(h)
			           .arg(s)
			           .arg(l)
			           .arg(qMax(l - 20, 0.00));
		} else {
			out << QString("<td class=\"td-2\">%1</td>").arg(tr("Invalid"));
		}

		for (int j = 0; j < taskList.size(); j++) {
			int score = contestant->getTaskScore(j);

			if (score != -1) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
				float h = NAN;
				float s = NAN;
				float l = NAN;
#else
				double h = NAN;
				double s = NAN;
				double l = NAN;
#endif
				QColor col = colors.getColorPer(score, fullScore[j]);
				col.getHslF(&h, &s, &l);

				if (taskList[j]->getTaskType() != Task::AnswersOnly &&
				    contestant->getCompileState(j) != CompileSuccessfully) {
					if (contestant->getCompileState(j) == NoValidSourceFile) {
						colors.getColorNf().getHslF(&h, &s, &l);
					} else {
						colors.getColorCe().getHslF(&h, &s, &l);
					}
				}

				h *= 360, s *= 100, l *= 100;
				out << QString(
				           R"(<td class="td-3" style="background: hsl(%2,%3%,%4%);"><a href="#c%5p%6" class="a-0">%1</a></td>)")
				           .arg(score)
				           .arg(h)
				           .arg(s)
				           .arg(l)
				           .arg(loc[contestant])
				           .arg(j);
			} else {
				out << QString(R"(<td class="td-3"><a href="#c%2p%3" class="a-0">%1</a></td>)")
				           .arg(tr("Invalid"))
				           .arg(loc[contestant])
				           .arg(j);
			}
		}
		out << "</tr>";
	}

	out << "</table></p>";

	for (int i = 0; i < contestantList.size(); i++) {
		out << QString("<a name=\"c%1\"><hr></a>").arg(i) << "<span class=\"d\">";
		out << tr("Contestant: %1").arg(contestantList[i]->getContestantName().toHtmlEscaped()) << "</span>";
		out << getContestantHtmlCode(contest, contestantList[i], i);
	}

	out << QString(R"(<footer><p><i>Lemonlime Version %1:%2</i></p></footer>)")
	           .arg(LEMON_VERSION_STRING)
	           .arg(LEMON_VERSION_BUILD);

	out << R"(
	<script>
		document.querySelectorAll("div[id^='c'] th").forEach(e=>e.classList.add("td-0"));
		document.querySelectorAll("div[id^='c'] td").forEach(e=>e.classList.add("td-0"));
		document.querySelectorAll("div[id^='c']>p>span").forEach(e=>{e.style.fontWeight="bold";e.style.fontSize="large";});
		document.querySelectorAll("div[id^='c']>p>table").forEach(e=>e.style.border="solid");
		document.querySelectorAll("div[id^='c']>p>table th").forEach(e=>e.setAttribute("scope","col"));
	</script>
	)";
	out << "</body>";
	out << "</html>";
	return output;
}

auto ContestTools::getSmallerContestantHtmlCode(Contest *contest, Contestant *contestant) -> QString {
	QString htmlCode;
	QList<Task *> taskList = contest->getTaskList();

	for (int i = 0; i < taskList.size(); i++) {
		htmlCode += "<p><span style=\"font-weight:bold; font-size:large;\">";
		htmlCode +=
		    QString("%1 %2</span><br>").arg(tr("Task")).arg(taskList[i]->getProblemTitle().toHtmlEscaped());

		if (! contestant->getCheckJudged(i)) {
			htmlCode += QString("&nbsp;&nbsp;%1</p>").arg(tr("Not judged"));
			continue;
		}

		if (taskList[i]->getTaskType() == Task::Traditional ||
		    taskList[i]->getTaskType() == Task::Interaction ||
		    taskList[i]->getTaskType() == Task::Communication ||
		    taskList[i]->getTaskType() == Task::CommunicationExec) {
			if (contestant->getCompileState(i) != CompileSuccessfully) {
				switch (contestant->getCompileState(i)) {
					case NoValidGraderFile:
						htmlCode +=
						    QString("&nbsp;&nbsp;%1</p>").arg(tr("Main grader (grader.*) cannot be found"));
						break;

					case NoValidSourceFile:
						htmlCode += QString("&nbsp;&nbsp;%1</p>").arg(tr("Cannot find valid source file"));
						break;

					case CompileTimeLimitExceeded:
						htmlCode += QString("&nbsp;&nbsp;%1%2<br>")
						                .arg(tr("Source file: "))
						                .arg(contestant->getSourceFile(i).toHtmlEscaped());
						htmlCode += QString("&nbsp;&nbsp;%1</p>").arg(tr("Compile time limit exceeded"));
						break;

					case InvalidCompiler:
						htmlCode += QString("&nbsp;&nbsp;%1</p>").arg(tr("Cannot run given compiler"));
						break;

					case CompileError:
						htmlCode += QString("&nbsp;&nbsp;%1%2<br>")
						                .arg(tr("Source file: "))
						                .arg(contestant->getSourceFile(i).toHtmlEscaped());
						htmlCode += QString("&nbsp;&nbsp;%1").arg(tr("Compile error"));

						if (! contestant->getCompileMessage(i).isEmpty()) {
							QString compileMessage = contestant->getCompileMessage(i).toHtmlEscaped();
							compileMessage.replace("\r\n", "<br>");
							compileMessage.replace("\n", "<br>");
							compileMessage.replace("\r", "<br>");

							if (compileMessage.endsWith("<br>"))
								compileMessage.chop(4);

							htmlCode += R"(<table border="1" cellpadding="1">)";
							htmlCode += "<tr><td style=\"padding: 0.5em; text-align: left;\"><code>";
							htmlCode += compileMessage;
							htmlCode += "</code></td></tr></table>";
						}

						htmlCode += "</p>";
						break;

					default:
						break;
				}

				continue;
			}

			htmlCode += QString("&nbsp;&nbsp;%1%2")
			                .arg(tr("Source file: "))
			                .arg(contestant->getSourceFile(i).toHtmlEscaped());
		}

		htmlCode += R"(<table border="1"  cellpadding="1"><tr>)";
		htmlCode += QString("<th scope=\"col\">%1</th>").arg(tr("Test Case"));
		htmlCode += QString("<th scope=\"col\">%1</th>").arg(tr("Input File"));
		htmlCode += QString("<th scope=\"col\">%1</th>").arg(tr("Result"));
		htmlCode += QString("<th scope=\"col\">%1</th>").arg(tr("Time Used"));
		htmlCode += QString("<th scope=\"col\">%1</th>").arg(tr("Memory Used"));
		htmlCode += QString("<th scope=\"col\">%1</th></tr>").arg(tr("Score"));
		QList<TestCase *> testCases = taskList[i]->getTestCaseList();
		QList<QStringList> inputFiles = contestant->getInputFiles(i);
		QList<QList<ResultState>> result = contestant->getResult(i);
		QList<QStringList> message = contestant->getMessage(i);
		QList<QList<int>> timeUsed = contestant->getTimeUsed(i);
		QList<QList<qint64>> memoryUsed = contestant->getMemoryUsed(i);
		QList<QList<int>> score = contestant->getScore(i);

		for (int j = 0; j < inputFiles.size(); j++) {
			for (int k = 0; k < inputFiles[j].size(); k++) {
				htmlCode += "<tr>";

				if (k == 0) {
					if (score.value(j).size() == inputFiles[j].size())
						htmlCode +=
						    QString("<td rowspan=\"%1\">#%2</td>").arg(inputFiles[j].size()).arg(j + 1);
					else
						htmlCode += QString("<td rowspan=\"%1\">#%2<br>%3:%4</td>")
						                .arg(inputFiles[j].size())
						                .arg(j + 1)
						                .arg(tr("Subtask Dependence Status"))
						                .arg(statusRankingText(
						                    (score.value(j).isEmpty() ? -1 : score.value(j).back())));
				}

				htmlCode += QString("<td>%1</td>").arg(inputFiles[j][k].toHtmlEscaped());
				QString text;
				QString bgColor;
				QString frColor;
				Settings::setTextAndColor(result.value(j).value(k, FileError), text, frColor, bgColor);
				htmlCode += QString("<td>%1").arg(text);

				if (! message.value(j).value(k).isEmpty()) {
					htmlCode += QString("<details><summary>%1</summary><pre>%2</pre></details>")
					                .arg(tr("Message"), message.value(j).value(k).toHtmlEscaped());
				}

				htmlCode += "</td>";
				htmlCode += "<td>";

				if (timeUsed.value(j).value(k, -1) != -1) {
					htmlCode +=
					    QString("").asprintf("%.3lf s", double(timeUsed.value(j).value(k, -1)) / 1000);
				} else {
					htmlCode += tr("Invalid");
				}

				htmlCode += "</td>";
				htmlCode += "<td>";

				if (memoryUsed.value(j).value(k, -1) != -1) {
					htmlCode += QString("").asprintf("%.3lf MiB",
					                                 double(memoryUsed.value(j).value(k, -1)) / 1024 / 1024);
				} else {
					htmlCode += tr("Invalid");
				}

				htmlCode += "</td>";

				if (k == 0) {
					int minv = 2147483647;
					int maxv = (j < testCases.size() ? testCases[j]->getFullScore() : 0);

					for (int t = 0; t < inputFiles[j].size(); t++)
						if (score.value(j).value(t, 0) < minv)
							minv = score.value(j).value(t, 0);

					htmlCode += QString(R"(<td rowspan="%1"><span class="c">%2</span> / %3</td>)")
					                .arg(inputFiles[j].size())
					                .arg(minv)
					                .arg(maxv);
				}

				htmlCode += "</tr>";
			}
		}

		htmlCode += "</table><br></p>";
	}

	htmlCode += QString("<p><a href=\"#top\">%1</a></p>").arg(tr("Return to top"));
	return htmlCode;
}

QString ContestTools::resultSmallerHtml(Contest *contest) {
	QString output;
	QTextStream out(&output);
	QList<Contestant *> contestantList = contest->getContestantList();
	QList<Task *> taskList = contest->getTaskList();
	out << "<html><head>";
	out << R"(<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />)";
	out << "<style type=\"text/css\">th, td {padding-left: 1em; padding-right: 1em; white-space: nowrap; "
	       "text-align: center; verticle-align: middle;} .a {border-style: none solid solid none; "
	       "border-width: 1px 3px; border-color: #ccc;} .b {border-style: none solid solid none; "
	       "border-width: 3px 3px; border-color: #000;} .c {font-weight: bold; font-size: large;} "
	       ".d {font-size:x-large; font-weight:bold;} .e {font-size: small;} .f {font-weight: bold;}"
	       ".g {border-style: none solid solid none; border-width: 3px 2px; border-color: #000;}</style>";
	out << "<title>" << contest->getContestTitle().toHtmlEscaped() << " : " << tr("Contest Result")
	    << "</title>";
	out << "</head><body>";
	QList<std::pair<int, QString>> sortList;

	for (auto &i : contestantList) {
		int totalScore = i->getTotalScore();

		if (totalScore != -1) {
			sortList.append(std::make_pair(-totalScore, i->getContestantName()));
		} else {
			sortList.append(std::make_pair(1, i->getContestantName()));
		}
	}

	std::sort(sortList.begin(), sortList.end());
	QMap<QString, int> rankList;

	for (int i = 0; i < sortList.size(); i++) {
		if (i > 0 && sortList[i].first == sortList[i - 1].first) {
			rankList.insert(sortList[i].second, rankList[sortList[i - 1].second]);
		} else {
			rankList.insert(sortList[i].second, i);
		}
	}

	QHash<Contestant *, int> loc;

	for (int i = 0; i < contestantList.size(); i++) {
		loc.insert(contestantList[i], i);
	}

	out << "<p><span class=\"d\">";
	out << "<a name=\"top\"></a>" << contest->getContestTitle().toHtmlEscaped() << " : " << tr("Rank List")
	    << "</span></p>";
	out << "<p class=\"e\">" << tr("Judged By LemonLime") << "</p>";
	out << R"(<p><table border="1" cellpadding="1"><tr>)";
	out << QString("<th scope=\"col\">%1</th>").arg(tr("Rank"));
	out << QString("<th scope=\"col\">%1</th>").arg(tr("Name"));
	out << QString("<th scope=\"col\">%1</th>").arg(tr("Total Score"));

	for (auto &i : taskList)
		out << QString("<th scope=\"col\">%1</th>").arg(i->getProblemTitle().toHtmlEscaped());

	out << QString("</tr>");
	QList<int> fullScore;

	for (auto &i : taskList) {
		int a = i->getTotalScore();
		fullScore.append(a);
	}

	for (auto &i : sortList) {
		Contestant *contestant = contest->getContestant(i.second);
		out << QString("<tr><td>%1</td>").arg(rankList[contestant->getContestantName()] + 1);
		out << QString("<td><a href=\"#c%1\">%2</a></td>").arg(loc[contestant]).arg(i.second.toHtmlEscaped());
		int allScore = contestant->getTotalScore();

		if (allScore != -1) {
			out << QString("<td class=\"f\">%1</td>").arg(allScore);
		} else {
			out << QString("<td class=\"f\">%1</td>").arg(tr("Invalid"));
		}

		for (int j = 0; j < taskList.size(); j++) {
			int score = contestant->getTaskScore(j);

			if (score != -1) {
				out << QString("<td>%1</td>").arg(score);
			} else {
				out << QString("<td>%1</td>").arg(tr("Invalid"));
			}
		}
		out << "</tr>";
	}

	out << "</table></p>";

	for (int i = 0; i < contestantList.size(); i++) {
		out << QString("<a name=\"c%1\"></a><hr>").arg(i) << "<span class=\"d\">";
		out << tr("Contestant: %1").arg(contestantList[i]->getContestantName().toHtmlEscaped()) << "</span>";
		out << getSmallerContestantHtmlCode(contest, contestantList[i]);
	}

	out << "</body></html>";
	return output;
}

auto ContestTools::getScoreNormalChart(const QMap<int, int> &scoreCount, int listSize, int totalScore)
    -> QString {
	QString buffer = "";
	long long overallScoreSum = 0;
	double scoreDiscrim = 0;
	double scoreStandardDevia = 0;
	int scoreTierPrefix = 0;
	int lastScoreTier = -1;
	int lastScoreTierNum = -1;

	for (auto i = scoreCount.constEnd(); i != scoreCount.constBegin();) {
		i--;
		int curScoreTier = i.key();
		int curScoreTierNum = i.value();

		if (curScoreTier < 0)
			continue;

		overallScoreSum += 1LL * curScoreTier * curScoreTierNum;

		if (lastScoreTier >= 0 && totalScore > 0)
			scoreDiscrim += qLn(1 + 10.00 * (lastScoreTier - curScoreTier) / totalScore) *
			                (1.00 - 1.00 * lastScoreTierNum * curScoreTierNum / listSize / listSize);

		lastScoreTier = curScoreTier;
		lastScoreTierNum = curScoreTierNum;
	}

	double scoreAverage = 1.00 * overallScoreSum / listSize;
	buffer += "<table border=\"0\" cellspacing=\"0\" cellpadding=\"5\">";
	buffer +=
	    QString(R"(<tr><th>%1</th><th>%2</th><th>%3</th><th colspan="2">%4</th><th colspan="2">%5</th></tr>)")
	        .arg(tr("Score"))
	        .arg(tr("Count"))
	        .arg(tr("Ratio"))
	        .arg(tr("Prefix"))
	        .arg(tr("Suffix"));

	for (auto i = scoreCount.constEnd(); i != scoreCount.constBegin();) {
		i--;
		int curScoreTier = i.key();
		int curScoreTierNum = i.value();
		scoreStandardDevia += qPow(curScoreTier - scoreAverage, 2) * curScoreTierNum;
		buffer += "<tr>";
		buffer += QString("<td align=\"right\"><nobr>%1 Pt</nobr></td>")
		              .arg(curScoreTier < 0 ? QString("N/A") : QString::number(curScoreTier));
		buffer += QString("<td align=\"right\"><nobr>%1</nobr></td>").arg(curScoreTierNum);
		buffer += QString("<td align=\"right\"><nobr>%1%</nobr></td>")
		              .arg(QString::number(100.00 * curScoreTierNum / listSize, 'f', 3));
		buffer += QString("<td align=\"right\"><nobr>%1</nobr></td>").arg(listSize - scoreTierPrefix);
		buffer += QString("<td align=\"right\"><nobr>%1%</nobr></td>")
		              .arg(QString::number(100.00 - 100.00 * scoreTierPrefix / listSize, 'f', 3));
		scoreTierPrefix += curScoreTierNum;
		buffer += QString("<td align=\"right\"><nobr>%1</nobr></td>").arg(scoreTierPrefix);
		buffer += QString("<td align=\"right\"><nobr>%1%</nobr></td>")
		              .arg(QString::number(100.00 * scoreTierPrefix / listSize, 'f', 3));
		buffer += "</tr>";
	}

	buffer += "</table>";
	scoreStandardDevia = qSqrt(scoreStandardDevia / listSize);
	scoreDiscrim = scoreDiscrim * scoreDiscrim;
	buffer += "<p>" + tr("Average") + " : " + QString::number(scoreAverage) + " / " +
	          QString::number(totalScore) + "</p>";
	buffer += "<p>" + tr("Standard Deviation") + " : " + QString::number(scoreStandardDevia) + "</p>";
	buffer += "<p>" + tr("Score Discrimination Power") + " : " + QString::number(scoreDiscrim) + "</p>";
	return buffer;
}

auto ContestTools::getTestcaseScoreChart(QList<TestCase *> testCaseList, QList<QList<QList<int>>> scoreList,
                                         QList<QList<QList<ResultState>>> resultList) -> QString {
	QString buffer = "";
	buffer += "<table border=\"0\" cellspacing=\"0\" cellpadding=\"5\">";
	buffer +=
	    QString(
	        R"(<tr><th>%1</th><th>%2</th><th>%3</th><th colspan="2">%4</th><th colspan="2">%5</th><th colspan="2">%6</th><th>%7</th></tr>)")
	        .arg(tr("No."))
	        .arg(tr("Input"))
	        .arg(tr("Output"))
	        .arg(tr("Pure"))
	        .arg(tr("Far"))
	        .arg(tr("Lost"))
	        .arg(tr("Average"));

	for (int i = 0; i < testCaseList.length(); i++) {
		QStringList inFileList = testCaseList[i]->getInputFiles();
		QStringList outFileList = testCaseList[i]->getOutputFiles();
		int mxScore = testCaseList[i]->getFullScore();
		QList<int> miScoreRecord;
		QList<int> miStatRecord;

		for (int j = 0; j < scoreList.length(); j++) {
			miScoreRecord.append(mxScore);
			miStatRecord.append(2);
		}

		for (int j = 0; j < inFileList.length(); j++) {
			int cntFail = 0;
			int cntPati = 0;
			int cntSucc = 0;
			long long sumscore = 0;

			for (int k = 0; k < scoreList.length(); k++) {
				int score = 0;
				int statVal = 2;
				ResultState stat = WrongAnswer;

				if (scoreList[k].length() > i && scoreList[k][i].length() > j) {
					score = scoreList[k][i][j];
					stat = resultList[k][i][j];
				}

				if (stat == CorrectAnswer)
					cntSucc++, statVal = 2;
				else if (stat == PartlyCorrect)
					cntPati++, statVal = 1;
				else
					cntFail++, statVal = 0;

				sumscore += score;
				miScoreRecord[k] = qMin(miScoreRecord[k], score);
				miStatRecord[k] = qMin(miStatRecord[k], statVal);
			}

			buffer += "<tr>";
			buffer += "<td align=\"left\">" + QString("%1.%2").arg(i + 1).arg(j + 1) + "</td>";
			buffer += "<td align=\"left\">" + QString("%1").arg(inFileList[j].toHtmlEscaped()) + "</td>";
			buffer += "<td align=\"left\">" + QString("%1").arg(outFileList[j].toHtmlEscaped()) + "</td>";
			buffer += "<td align=\"right\"><nobr>" + QString("%1").arg(cntSucc) + "</nobr></td>";
			buffer += "<td align=\"right\"><nobr>" +
			          QString("%1%").arg(QString::number(100.00 * cntSucc / scoreList.length(), 'f', 3)) +
			          "</nobr></td>";
			buffer += "<td align=\"right\"><nobr>" + QString("%1").arg(cntPati) + "</nobr></td>";
			buffer += "<td align=\"right\"><nobr>" +
			          QString("%1%").arg(QString::number(100.00 * cntPati / scoreList.length(), 'f', 3)) +
			          "</nobr></td>";
			buffer += "<td align=\"right\"><nobr>" + QString("%1").arg(cntFail) + "</nobr></td>";
			buffer += "<td align=\"right\"><nobr>" +
			          QString("%1%").arg(QString::number(100.00 * cntFail / scoreList.length(), 'f', 3)) +
			          "</nobr></td>";
			buffer += "<td align=\"right\"><nobr>" +
			          QString("%1 / %2")
			              .arg(QString::number(1.00 * sumscore / scoreList.length(), 'f', 3))
			              .arg(mxScore) +
			          "</nobr></td>";
			buffer += "</tr>";
		}

		if (inFileList.length() > 1) {
			int sumCntFail = 0;
			int sumCntPati = 0;
			int sumCntSucc = 0;
			long long sumSumScore = 0;

			for (int j = 0; j < scoreList.length(); j++) {
				sumSumScore += miScoreRecord[j];

				if (miStatRecord[j] >= 2)
					sumCntSucc++;
				else if (miStatRecord[j] == 1)
					sumCntPati++;
				else
					sumCntFail++;
			}

			buffer += "<tr>";
			buffer += "<td align=\"left\">" + QString("%1 %2").arg(i + 1).arg(tr("Overall")) + "</td>";
			buffer +=
			    "<td align=\"left\">" + QString("%1 %2").arg(inFileList.length()).arg(tr("Files")) + "</td>";
			buffer +=
			    "<td align=\"left\">" + QString("%1 %2").arg(outFileList.length()).arg(tr("Files")) + "</td>";
			buffer += "<td align=\"right\"><nobr>" + QString("%1").arg(sumCntSucc) + "</nobr></td>";
			buffer += "<td align=\"right\"><nobr>" +
			          QString("%1%").arg(QString::number(100.00 * sumCntSucc / scoreList.length(), 'f', 3)) +
			          "</nobr></td>";
			buffer += "<td align=\"right\"><nobr>" + QString("%1").arg(sumCntPati) + "</nobr></td>";
			buffer += "<td align=\"right\"><nobr>" +
			          QString("%1%").arg(QString::number(100.00 * sumCntPati / scoreList.length(), 'f', 3)) +
			          "</nobr></td>";
			buffer += "<td align=\"right\"><nobr>" + QString("%1").arg(sumCntFail) + "</nobr></td>";
			buffer += "<td align=\"right\"><nobr>" +
			          QString("%1%").arg(QString::number(100.00 * sumCntFail / scoreList.length(), 'f', 3)) +
			          "</nobr></td>";
			buffer += "<td align=\"right\"><nobr>" +
			          QString("%1 / %2")
			              .arg(QString::number(1.00 * sumSumScore / scoreList.length(), 'f', 3))
			              .arg(mxScore) +
			          "</nobr></td>";
			buffer += "</tr>";
		}
	}

	buffer += "</table>";
	return buffer;
}

auto ContestTools::checkValid(QList<Task *> taskList, const QList<Contestant *> &contestantList) -> bool {
	for (auto *i : taskList) {
		for (auto *j : i->getTestCaseList()) {
			if (j->getInputFiles().length() != j->getOutputFiles().length())
				return false;
		}
	}

	for (auto *i : contestantList) {
		for (int j = 0; j < taskList.length(); j++) {
			QList<QList<int>> scoreList;
			QList<QList<ResultState>> resultList;
			QList<TestCase *> testCaseList;
			bool isJudged = false;

			try {
				scoreList = i->getScore(j);
				resultList = i->getResult(j);
				testCaseList = taskList[j]->getTestCaseList();
				isJudged = i->getCheckJudged(j);
			} catch (...) {
				return false;
			}

			if (! isJudged)
				return false;

			if (scoreList.length() != resultList.length())
				return false;

			if (scoreList.length() > 0 && resultList.length() > 0 && testCaseList.length() > 0) {
				if (scoreList.length() != testCaseList.length())
					return false;

				if (resultList.length() != testCaseList.length())
					return false;

				for (int k = 0; k < testCaseList.length(); k++) {

					// 如果有子任务依赖，就会比一般的题目多一个 score 存依赖

					if (scoreList[k].length() - (! testCaseList[k]->getDependenceSubtask().empty()) !=
					    testCaseList[k]->getInputFiles().length())
						return false;

					if (resultList[k].length() != testCaseList[k]->getInputFiles().length())
						return false;
				}
			}
		}
	}

	return true;
}

QString ContestTools::buildStatisticsHtml() const {
	QString buffer;

	if (! contest) {
		buffer = tr("No contest yet");
		return buffer;
	}

	QList<Task *> taskList = contest->getTaskList();
	QList<Contestant *> contestantList = contest->getContestantList();

	if (taskList.empty()) {
		buffer = tr("No task yet");
		return buffer;
	}

	if (contestantList.empty()) {
		buffer = tr("No contestant yet");
		return buffer;
	}

	if (! checkValid(contest->getTaskList(), contest->getContestantList())) {
		buffer = tr("Some unhandled situation happened. May not all contestants are well judged, or not "
		            "rejudged after changing testcases. Please refresh and rejudge.");
		return buffer;
	}

	int totalScore = contest->getTotalScore();
	buffer += "<html><head><meta charset=\"utf-8\">";
	buffer += "<style type=\"text/css\">th, td {padding-left: 1em; padding-right: 1em;}</style>";
	buffer += "</head><body>";
	buffer += "<h1>" + QString("%1 %2").arg(tr("Contest")).arg(contest->getContestTitle().toHtmlEscaped()) +
	          "</h1>";
	buffer += "<h2>" + tr("Overall") + "</h2>";
	bool haveError = false;
	QMap<int, int> scoreCount;

	for (auto &i : contestantList) {
		int contestantTotalScore = 0;
		bool loss = false;

		for (int j = 0; j < taskList.size(); j++) {
			contestantTotalScore += i->getTaskScore(j);

			if (i->getTaskScore(j) < 0)
				haveError = true, loss = true;
		}

		if (! loss)
			scoreCount[contestantTotalScore]++;
		else
			scoreCount[-1]++;
	}

	if (haveError) {
		buffer += "<p style=\"font-size: large; color: red;\">" + tr("Warning: Judgement is not finished.") +
		          "</p><br>";
	}

	buffer += getScoreNormalChart(scoreCount, contestantList.size(), totalScore);
	buffer += "<br>";
	buffer += "<br>";
	buffer += "<h2>" + tr("Problems") + "</h2>";

	for (int i = 0; i < taskList.size(); i++) {
		buffer += "<h3>";
		buffer += QString("%1 %2: %3")
		              .arg(tr("Task"))
		              .arg(i + 1)
		              .arg(taskList[i]->getProblemTitle().toHtmlEscaped());
		buffer += "</h3>";
		int numberSubmitted = 0;
		QMap<int, int> cnts;
		QList<QList<QList<int>>> TestcaseScoreList;
		QList<QList<QList<ResultState>>> resultList;

		for (auto &j : contestantList) {
			cnts[j->getTaskScore(i)]++;

			if (j->getCompileState(i) != NoValidSourceFile && j->getCompileState(i) != NoValidGraderFile)
				numberSubmitted++;

			TestcaseScoreList.append(j->getScore(i));
			resultList.append(j->getResult(i));
		}

		buffer += getScoreNormalChart(cnts, contestantList.size(), taskList[i]->getTotalScore());
		buffer += "<p>" + tr("Number of answer submitted") + " : " + QString::number(numberSubmitted) +
		          " / " + QString::number(contestantList.size()) + " (" +
		          QString::number(100.00 * numberSubmitted / contestantList.size()) + "%)</p>";
		buffer += getTestcaseScoreChart(taskList[i]->getTestCaseList(), TestcaseScoreList, resultList);
		buffer += "<br>";
		buffer += "<br>";
	}

	buffer += "</body></html>";
	return buffer;
}
