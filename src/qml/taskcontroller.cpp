/*
 * SPDX-FileCopyrightText: 2026 Project LemonLime
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "taskcontroller.h"

#include "base/compiler.h"
#include "base/settings.h"
#include "core/contest.h"
#include "core/task.h"
#include "core/testcase.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>
#include <functional>
#include <numeric>
#include <utility>

namespace {
	bool fileLess(const QString &left, const QString &right) {
		return left.size() < right.size() ||
		       (left.size() == right.size() && QString::localeAwareCompare(left, right) < 0);
	}

	QVariantList filePairs(const TestCase *testCase) {
		QVariantList result;
		for (int i = 0; i < testCase->getInputFiles().size(); ++i)
			result.append(QVariantMap{{"input", testCase->getInputFiles().at(i)},
			                          {"output", testCase->getOutputFiles().value(i)}});
		return result;
	}

	QVariantMap caseValues(const TestCase *testCase, int index) {
		QStringList dependencies;
		for (int dependency : testCase->getDependenceSubtask())
			dependencies.append(QString::number(dependency));
		return {{"index", index},
		        {"score", testCase->getFullScore()},
		        {"time", testCase->getTimeLimit()},
		        {"memory", testCase->getMemoryLimit()},
		        {"dependencies", dependencies.join(",")},
		        {"files", filePairs(testCase)},
		        {"count", testCase->getInputFiles().size()},
		        {"firstInput", testCase->getInputFiles().value(0)}};
	}

	QList<TestCase> snapshot(const Task *task) {
		QList<TestCase> result;
		for (auto *testCase : task->getTestCaseList())
			result.append(*testCase);
		return result;
	}

	// The mapping expands a former subtask to all of its replacements after a split.
	// Only dependencies preceding their owner remain valid in the judging engine.
	void replaceCases(Task *task, QList<TestCase> values, const QList<QList<int>> &mapping) {
		for (int i = 0; i < values.size(); ++i) {
			QSet<int> dependencies;
			for (int dependency : values[i].getDependenceSubtask()) {
				if (dependency <= 0 || dependency > mapping.size())
					continue;
				for (int replacement : mapping.at(dependency - 1)) {
					if (replacement >= 0 && replacement < i)
						dependencies.insert(replacement + 1);
				}
			}
			values[i].setDependenceSubtask(dependencies);
			values[i].setIndex(i + 1);
		}
		while (! task->getTestCaseList().isEmpty())
			task->deleteTestCase(0);
		for (const auto &value : values)
			task->addTestCase(new TestCase(value));
	}

	void reorderCases(Task *task, const QList<int> &order) {
		const auto old = snapshot(task);
		QList<TestCase> values;
		QList<QList<int>> mapping(old.size());
		for (int index : order) {
			mapping[index].append(values.size());
			values.append(old.at(index));
		}
		replaceCases(task, values, mapping);
	}

	bool safeName(const QString &name) {
		return ! name.isEmpty() && name != "." && name != ".." &&
		       ! name.contains(QRegularExpression(R"([\\/:*?"<>|\x00-\x1f])"));
	}

	QString matchKey(const QStringList &parts) {
		return QString::fromUtf8(
		    QJsonDocument(QJsonArray::fromStringList(parts)).toJson(QJsonDocument::Compact));
	}
} // namespace

TaskController::TaskController(Settings *settings, QObject *parent) : QObject(parent), settings(settings) {}

TaskController::~TaskController() {
	if (editDraft) {
		while (! editDraft->getTestCaseList().isEmpty())
			editDraft->deleteTestCase(0);
	}
}

void TaskController::setContest(Contest *value) {
	finishTaskEdit(false);
	contest = value;
	taskIndex = contest && ! contest->getTaskList().isEmpty() ? 0 : -1;
	subtaskIndex = -1;
	preview.clear();
	discovered.clear();
	emit previewChanged();
	refresh();
}

void TaskController::setBusy(bool value) {
	if (isBusy == value)
		return;
	isBusy = value;
	emit changed();
}

bool TaskController::available() const { return contest; }
bool TaskController::busy() const { return isBusy; }
int TaskController::selectedTask() const { return taskIndex; }
int TaskController::selectedSubtask() const { return subtaskIndex; }
QVariantList TaskController::importPreview() const { return preview; }
QVariantList TaskController::discoveredTasks() const { return discovered; }
QUrl TaskController::dataFolder() const {
	return QUrl::fromLocalFile(QDir(Settings::dataPath()).absolutePath());
}
Task *TaskController::currentTask() const {
	return editDraft ? editDraft.data() : contest ? contest->getTask(taskIndex) : nullptr;
}
TestCase *TaskController::currentSubtask() const {
	auto *value = currentTask();
	return value ? value->getTestCase(subtaskIndex) : nullptr;
}

QVariantList TaskController::tasks() const {
	QVariantList result;
	if (contest) {
		for (auto *value : contest->getTaskList())
			result.append(QVariantMap{{"title", value->getProblemTitle()},
			                          {"score", value->getTotalScore()},
			                          {"count", value->getTestCaseList().size()},
			                          {"source", value->getSourceFileName()}});
	}
	return result;
}

QVariantMap TaskController::task() const {
	auto *value = currentTask();
	if (! value)
		return {};
	QVariantList sources;
	QVariantList graders;
	for (int i = 0; i < value->getSourceFilesPath().size(); ++i)
		sources.append(QVariantMap{{"source", value->getSourceFilesPath().at(i)},
		                           {"name", value->getSourceFilesName().value(i)}});
	for (int i = 0; i < value->getGraderFilesPath().size(); ++i)
		graders.append(QVariantMap{{"source", value->getGraderFilesPath().at(i)},
		                           {"name", value->getGraderFilesName().value(i)}});
	return {{"title", value->getProblemTitle()},
	        {"source", value->getSourceFileName()},
	        {"input", value->getInputFileName()},
	        {"output", value->getOutputFileName()},
	        {"type", int(value->getTaskType())},
	        {"comparison", int(value->getComparisonMode())},
	        {"standardInput", value->getStandardInputCheck()},
	        {"standardOutput", value->getStandardOutputCheck()},
	        {"subfolder", value->getSubFolderCheck()},
	        {"diffArguments", value->getDiffArguments()},
	        {"precision", value->getRealPrecision()},
	        {"specialJudge", value->getSpecialJudge()},
	        {"interactor", value->getInteractor()},
	        {"interactorName", value->getInteractorName()},
	        {"grader", value->getGrader()},
	        {"answerExtension", value->getAnswerFileExtension()},
	        {"sources", sources},
	        {"graders", graders},
	        {"score", value->getTotalScore()},
	        {"count", value->getTestCaseList().size()}};
}

QVariantList TaskController::subtasks() const {
	QVariantList result;
	if (auto *value = currentTask()) {
		for (int i = 0; i < value->getTestCaseList().size(); ++i)
			result.append(caseValues(value->getTestCase(i), i));
	}
	return result;
}

QVariantMap TaskController::subtask() const {
	auto *value = currentSubtask();
	return value ? caseValues(value, subtaskIndex) : QVariantMap();
}

QVariantList TaskController::compilers() const {
	QVariantList result;
	auto *value = currentTask();
	if (! settings)
		return result;
	for (auto *compiler : settings->getCompilerList()) {
		QStringList names{"disable"};
		QStringList keys{"disable"};
		names.append(compiler->getConfigurationNames());
		keys.append(compiler->getConfigurationNames());
		const QString selected =
		    value ? value->getCompilerConfiguration(compiler->getCompilerName()) : "default";
		result.append(QVariantMap{{"name", compiler->getCompilerName()},
		                          {"names", names},
		                          {"keys", keys},
		                          {"selected", int(keys.indexOf(selected))}});
	}
	return result;
}

QVariantMap TaskController::defaults() const {
	return {{"score", settings->getDefaultFullScore()},
	        {"time", settings->getDefaultTimeLimit()},
	        {"memory", settings->getDefaultMemoryLimit()},
	        {"maxScore", Settings::upperBoundForFullScore()},
	        {"maxTime", Settings::upperBoundForTimeLimit()},
	        {"maxMemory", Settings::upperBoundForMemoryLimit()},
	        {"inputExtension", settings->getDefaultInputFileExtension()},
	        {"outputExtension", settings->getDefaultOutputFileExtension()}};
}

void TaskController::refresh() {
	if (! contest)
		taskIndex = -1;
	else if (taskIndex >= contest->getTaskList().size())
		taskIndex = contest->getTaskList().size() - 1;
	auto *value = currentTask();
	if (! value)
		subtaskIndex = -1;
	else if (subtaskIndex < 0 || subtaskIndex >= value->getTestCaseList().size())
		subtaskIndex = value->getTestCaseList().isEmpty() ? -1 : 0;
	emit changed();
}

void TaskController::selectTask(int index) {
	if (editDraft || index == taskIndex || ! contest || index < 0 || index >= contest->getTaskList().size())
		return;
	taskIndex = index;
	subtaskIndex = -1;
	preview.clear();
	emit previewChanged();
	refresh();
}

void TaskController::selectSubtask(int index) {
	if (index == subtaskIndex || ! currentTask() || index < 0 ||
	    index >= currentTask()->getTestCaseList().size())
		return;
	subtaskIndex = index;
	emit changed();
}

bool TaskController::fail(const QString &message) {
	emit errorOccurred(message);
	return false;
}

bool TaskController::writable() {
	return contest && ! isBusy ? true : fail(tr("Open a contest and finish judging before editing tasks."));
}

void TaskController::edited() {
	refresh();
	if (! editDraft)
		emit contestEdited();
}

bool TaskController::beginTaskEdit() {
	if (! writable() || ! currentTask() || editDraft)
		return false;
	auto *draft = new Task(this);
	currentTask()->copyTo(draft);
	editDraft = draft;
	refresh();
	return true;
}

void TaskController::finishTaskEdit(bool accept) {
	if (! editDraft)
		return;
	if (accept && contest && contest->getTask(taskIndex)) {
		auto *destination = contest->getTask(taskIndex);
		while (! destination->getTestCaseList().isEmpty())
			destination->deleteTestCase(0);
		editDraft->copyTo(destination);
	}
	while (! editDraft->getTestCaseList().isEmpty())
		editDraft->deleteTestCase(0);
	delete editDraft.data();
	editDraft = nullptr;
	refresh();
	if (accept)
		emit contestEdited();
}

bool TaskController::addEmptySubtask() {
	if (! writable() || ! currentTask())
		return false;
	auto *value = new TestCase;
	value->setFullScore(settings->getDefaultFullScore());
	value->setTimeLimit(settings->getDefaultTimeLimit());
	value->setMemoryLimit(settings->getDefaultMemoryLimit());
	value->setIndex(currentTask()->getTestCaseList().size() + 1);
	currentTask()->addTestCase(value);
	subtaskIndex = currentTask()->getTestCaseList().size() - 1;
	edited();
	return true;
}

Task *TaskController::createTask(const QString &title) {
	auto *value = new Task;
	value->setProblemTitle(title);
	value->setSourceFileName(title);
	value->setInputFileName(title + "." + settings->getDefaultInputFileExtension());
	value->setOutputFileName(title + "." + settings->getDefaultOutputFileExtension());
	value->setAnswerFileExtension(settings->getDefaultOutputFileExtension());
	value->refreshCompilerConfiguration(settings);
	contest->addTask(value);
	return value;
}

bool TaskController::addTask(const QString &title) {
	if (! writable())
		return false;
	if (! safeName(title.trimmed()))
		return fail(tr("Enter a task name without file-name separators or reserved characters."));
	createTask(title.trimmed());
	taskIndex = contest->getTaskList().size() - 1;
	subtaskIndex = -1;
	edited();
	return true;
}

bool TaskController::duplicateTask() {
	if (! writable() || ! currentTask())
		return false;
	auto *copy = new Task;
	currentTask()->copyTo(copy);
	copy->setProblemTitle(tr("%1 — copy").arg(copy->getProblemTitle()));
	contest->addTask(copy);
	taskIndex = contest->getTaskList().size() - 1;
	subtaskIndex = -1;
	edited();
	return true;
}

bool TaskController::removeTask() {
	if (! writable() || ! currentTask())
		return false;
	while (! currentTask()->getTestCaseList().isEmpty())
		currentTask()->deleteTestCase(0);
	contest->deleteTask(taskIndex);
	subtaskIndex = -1;
	edited();
	return true;
}

bool TaskController::moveTask(int offset) {
	if (! writable() || ! currentTask() || (offset != -1 && offset != 1))
		return false;
	const int destination = taskIndex + offset;
	if (destination < 0 || destination >= contest->getTaskList().size())
		return false;
	contest->swapTask(taskIndex, destination);
	taskIndex = destination;
	edited();
	return true;
}

bool TaskController::updateTask(const QVariantMap &values) {
	if (! writable() || ! currentTask())
		return false;
	auto *value = currentTask();
	for (const auto &key : {QString("source"), QString("input"), QString("output"), QString("interactorName"),
	                        QString("answerExtension")}) {
		if (values.contains(key) && ! values.value(key).toString().isEmpty() &&
		    ! safeName(values.value(key).toString()))
			return fail(tr("The %1 field must contain a valid, nonempty file name or title.").arg(key));
	}
	for (const auto &field : {std::pair<QString, int>{"type", 4}, {"comparison", 5}, {"precision", 18}}) {
		if (! values.contains(field.first))
			continue;
		bool ok = false;
		const int number = values.value(field.first).toInt(&ok);
		if (! ok || number < 0 || number > field.second)
			return fail(tr("The %1 value is outside the supported range.").arg(field.first));
	}
	if (values.contains("title"))
		value->setProblemTitle(values.value("title").toString());
	if (values.contains("source")) {
		const QString source = values.value("source").toString();
		value->setSourceFileName(source.isEmpty() ? value->getProblemTitle() : source);
		if (! value->getStandardInputCheck())
			value->setInputFileName(value->getSourceFileName() + "." +
			                        settings->getDefaultInputFileExtension());
		if (! value->getStandardOutputCheck())
			value->setOutputFileName(value->getSourceFileName() + "." +
			                         settings->getDefaultOutputFileExtension());
	}
	if (values.contains("input"))
		value->setInputFileName(values.value("input").toString());
	if (values.contains("output"))
		value->setOutputFileName(values.value("output").toString());
	if (values.contains("type"))
		value->setTaskType(static_cast<Task::TaskType>(values.value("type").toInt()));
	if (values.contains("comparison"))
		value->setComparisonMode(static_cast<Task::ComparisonMode>(values.value("comparison").toInt()));
	if (values.contains("standardInput"))
		value->setStandardInputCheck(values.value("standardInput").toBool());
	if (values.contains("standardOutput"))
		value->setStandardOutputCheck(values.value("standardOutput").toBool());
	if (values.contains("subfolder"))
		value->setSubFolderCheck(values.value("subfolder").toBool());
	if (values.contains("diffArguments"))
		value->setDiffArguments(values.value("diffArguments").toString());
	if (values.contains("precision"))
		value->setRealPrecision(values.value("precision").toInt());
	if (values.contains("specialJudge"))
		value->setSpecialJudge(values.value("specialJudge").toString());
	if (values.contains("interactor"))
		value->setInteractor(values.value("interactor").toString());
	if (values.contains("interactorName"))
		value->setInteractorName(values.value("interactorName").toString());
	if (values.contains("grader"))
		value->setGrader(values.value("grader").toString());
	if (values.contains("answerExtension"))
		value->setAnswerFileExtension(values.value("answerExtension").toString());
	edited();
	return true;
}

bool TaskController::setCompilerConfiguration(int index, const QString &configuration) {
	if (! writable() || ! currentTask() || index < 0 || index >= settings->getCompilerList().size())
		return false;
	auto *compiler = settings->getCompilerList().at(index);
	if (configuration != "default" && configuration != "disable" &&
	    ! compiler->getConfigurationNames().contains(configuration))
		return fail(tr("The selected compiler configuration no longer exists."));
	currentTask()->setCompilerConfiguration(compiler->getCompilerName(), configuration);
	edited();
	return true;
}

bool TaskController::setFileMapping(bool grader, int index, const QString &source, const QString &name) {
	if (! writable() || ! currentTask())
		return false;
	if (source.trimmed().isEmpty() || name.trimmed().isEmpty())
		return fail(tr("Both the source file and destination file are required."));
	auto *value = currentTask();
	auto paths = grader ? value->getGraderFilesPath() : value->getSourceFilesPath();
	auto names = grader ? value->getGraderFilesName() : value->getSourceFilesName();
	if (index < -1 || index >= paths.size())
		return false;
	if (index == -1) {
		paths.append(source);
		names.append(name);
	} else {
		paths[index] = source;
		while (names.size() <= index)
			names.append(QString());
		names[index] = name;
	}
	if (grader) {
		value->setGraderFilesPath(paths);
		value->setGraderFilesName(names);
	} else {
		value->setSourceFilesPath(paths);
		value->setSourceFilesName(names);
	}
	edited();
	return true;
}

bool TaskController::removeFileMapping(bool grader, int index) {
	if (! writable() || ! currentTask())
		return false;
	auto *value = currentTask();
	auto paths = grader ? value->getGraderFilesPath() : value->getSourceFilesPath();
	auto names = grader ? value->getGraderFilesName() : value->getSourceFilesName();
	if (index < 0 || index >= paths.size())
		return false;
	paths.removeAt(index);
	if (index < names.size())
		names.removeAt(index);
	if (grader) {
		value->setGraderFilesPath(paths);
		value->setGraderFilesName(names);
	} else {
		value->setSourceFilesPath(paths);
		value->setSourceFilesName(names);
	}
	edited();
	return true;
}
bool TaskController::validateLimits(const QVariantMap &values, bool allowPartial) {
	for (const auto &field : {std::pair<QString, int>{"score", Settings::upperBoundForFullScore()},
	                          {"time", Settings::upperBoundForTimeLimit()},
	                          {"memory", Settings::upperBoundForMemoryLimit()}}) {
		if (allowPartial && ! values.contains(field.first))
			continue;
		bool ok = false;
		const int number = values.value(field.first).toInt(&ok);
		if (! ok || number < (field.first == "score" ? 0 : 1) || number > field.second)
			return fail(tr("Enter a valid %1 value between %2 and %3.")
			                .arg(field.first)
			                .arg(field.first == "score" ? 0 : 1)
			                .arg(field.second));
	}
	return true;
}

bool TaskController::validateDependencies(const QString &text, int index, QList<int> &result) {
	result.clear();
	if (text.trimmed().isEmpty())
		return true;
	for (const auto &part : text.split(',')) {
		bool ok = false;
		const int number = part.trimmed().toInt(&ok);
		if (! ok || number < 1 || number > index || result.contains(number))
			return fail(tr("Dependencies must be distinct subtask numbers preceding this subtask."));
		result.append(number);
	}
	return true;
}

QList<int> TaskController::checkedIndices(const QVariantList &indices) const {
	QList<int> result;
	auto *value = currentTask();
	if (! value)
		return result;
	for (const auto &index : indices) {
		bool ok = false;
		const int number = index.toInt(&ok);
		if (! ok || number < 0 || number >= value->getTestCaseList().size())
			return {};
		if (! result.contains(number))
			result.append(number);
	}
	std::sort(result.begin(), result.end());
	return result;
}

bool TaskController::addSubtask(const QVariantMap &values) {
	if (! writable() || ! currentTask() || ! validateLimits(values, false))
		return false;
	const QString input = values.value("input").toString();
	const QString output = values.value("output").toString();
	if (input.isEmpty() || output.isEmpty())
		return fail(tr("Choose an input file and an output file."));
	const int index = currentTask()->getTestCaseList().size();
	QList<int> dependencies;
	if (! validateDependencies(values.value("dependencies").toString(), index, dependencies))
		return false;
	auto *value = new TestCase;
	value->setFullScore(values.value("score").toInt());
	value->setTimeLimit(values.value("time").toInt());
	value->setMemoryLimit(values.value("memory").toInt());
	value->setIndex(index + 1);
	value->setDependenceSubtask(dependencies);
	value->addSingleCase(input, output);
	currentTask()->addTestCase(value);
	subtaskIndex = index;
	edited();
	return true;
}

bool TaskController::updateSubtasks(const QVariantList &indices, const QVariantMap &values) {
	if (! writable() || ! validateLimits(values))
		return false;
	const auto selected = checkedIndices(indices);
	if (selected.isEmpty())
		return false;
	QList<int> dependencies;
	if (values.contains("dependencies") &&
	    ! validateDependencies(values.value("dependencies").toString(), selected.first(), dependencies))
		return false;
	for (int index : selected) {
		auto *value = currentTask()->getTestCase(index);
		if (values.contains("score"))
			value->setFullScore(values.value("score").toInt());
		if (values.contains("time"))
			value->setTimeLimit(values.value("time").toInt());
		if (values.contains("memory"))
			value->setMemoryLimit(values.value("memory").toInt());
		if (values.contains("dependencies"))
			value->setDependenceSubtask(dependencies);
	}
	edited();
	return true;
}

bool TaskController::removeSubtasks(const QVariantList &indices) {
	if (! writable())
		return false;
	const auto selected = checkedIndices(indices);
	if (selected.isEmpty())
		return false;
	QList<int> order;
	for (int i = 0; i < currentTask()->getTestCaseList().size(); ++i)
		if (! selected.contains(i))
			order.append(i);
	reorderCases(currentTask(), order);
	subtaskIndex = qMin(selected.first(), int(order.size()) - 1);
	edited();
	return true;
}

bool TaskController::removeTestCaseRows(int firstRow, int lastRow) {
	if (! writable() || ! currentTask() || firstRow < 0 || lastRow < firstRow)
		return false;
	const auto old = snapshot(currentTask());
	int totalRows = 0;
	for (const auto &value : old)
		totalRows += qMax(1, int(value.getInputFiles().size()));
	if (lastRow >= totalRows)
		return false;
	QList<TestCase> values;
	QList<QList<int>> mapping(old.size());
	int row = 0;
	int firstSubtask = -1;
	for (int index = 0; index < old.size(); ++index) {
		TestCase value = old.at(index);
		const int fileCount = value.getInputFiles().size();
		const int rowCount = qMax(1, fileCount);
		const int last = row + rowCount - 1;
		if (firstRow <= last && lastRow >= row && firstSubtask < 0)
			firstSubtask = index;
		if (! (firstRow <= row && last <= lastRow)) {
			for (int file = qMin(fileCount - 1, lastRow - row); file >= qMax(0, firstRow - row); --file)
				value.deleteSingleCase(file);
			mapping[index].append(values.size());
			values.append(value);
		}
		row += rowCount;
	}
	replaceCases(currentTask(), values, mapping);
	subtaskIndex = qMin(firstSubtask, int(values.size()) - 1);
	edited();
	return true;
}

bool TaskController::moveSubtasks(const QVariantList &indices, int offset) {
	if (! writable() || (offset != -1 && offset != 1))
		return false;
	const auto selected = checkedIndices(indices);
	if (selected.isEmpty())
		return false;
	const int count = currentTask()->getTestCaseList().size();
	if (selected.last() - selected.first() + 1 != selected.size())
		return fail(tr("Select consecutive subtasks to move them together."));
	if (selected.first() + offset < 0 || selected.last() + offset >= count)
		return false;
	QList<int> order(count);
	std::iota(order.begin(), order.end(), 0);
	if (offset < 0) {
		for (int index : selected)
			order.swapItemsAt(index, index - 1);
	} else {
		for (auto it = selected.crbegin(); it != selected.crend(); ++it)
			order.swapItemsAt(*it, *it + 1);
	}
	reorderCases(currentTask(), order);
	subtaskIndex = selected.first() + offset;
	edited();
	return true;
}

bool TaskController::mergeSubtasks(const QVariantList &indices) {
	if (! writable())
		return false;
	const auto selected = checkedIndices(indices);
	if (selected.size() < 2)
		return fail(tr("Select at least two consecutive subtasks to merge."));
	if (selected.last() - selected.first() + 1 != selected.size())
		return fail(tr("Select consecutive subtasks to merge."));
	auto old = snapshot(currentTask());
	TestCase merged = old.at(selected.first());
	QSet<int> dependencies;
	qint64 score = 0;
	for (int index : selected) {
		const auto &value = old.at(index);
		score += value.getFullScore();
		for (int dependency : value.getDependenceSubtask())
			dependencies.insert(dependency);
		if (index == selected.first())
			continue;
		for (int i = 0; i < value.getInputFiles().size(); ++i)
			merged.addSingleCase(value.getInputFiles().at(i), value.getOutputFiles().value(i));
	}
	if (score > Settings::upperBoundForFullScore())
		return fail(tr("The merged score exceeds the supported maximum."));
	merged.setFullScore(int(score));
	merged.setDependenceSubtask(dependencies);
	QList<TestCase> values;
	QList<QList<int>> mapping(old.size());
	for (int i = 0; i < old.size(); ++i) {
		if (selected.contains(i)) {
			mapping[i].append(selected.first());
			if (i == selected.first())
				values.append(merged);
		} else {
			mapping[i].append(values.size());
			values.append(old.at(i));
		}
	}
	replaceCases(currentTask(), values, mapping);
	subtaskIndex = selected.first();
	edited();
	return true;
}

bool TaskController::splitSubtasks(const QVariantList &indices) {
	if (! writable())
		return false;
	const auto selected = checkedIndices(indices);
	if (selected.isEmpty())
		return false;
	const auto old = snapshot(currentTask());
	QList<TestCase> values;
	QList<QList<int>> mapping(old.size());
	for (int i = 0; i < old.size(); ++i) {
		const auto &value = old.at(i);
		const int count = value.getInputFiles().size();
		if (! selected.contains(i) || count < 2) {
			mapping[i].append(values.size());
			values.append(value);
			continue;
		}
		for (int j = 0; j < count; ++j) {
			TestCase single;
			single.setFullScore(value.getFullScore() / count +
			                    int(j >= count - value.getFullScore() % count));
			single.setTimeLimit(value.getTimeLimit());
			single.setMemoryLimit(value.getMemoryLimit());
			single.setDependenceSubtask(value.getDependenceSubtask());
			single.addSingleCase(value.getInputFiles().at(j), value.getOutputFiles().value(j));
			mapping[i].append(values.size());
			values.append(single);
		}
	}
	replaceCases(currentTask(), values, mapping);
	subtaskIndex = mapping.at(selected.first()).first();
	edited();
	return true;
}

bool TaskController::sortSubtasks() {
	if (! writable() || ! currentTask())
		return false;
	QList<int> order(currentTask()->getTestCaseList().size());
	std::iota(order.begin(), order.end(), 0);
	std::stable_sort(order.begin(), order.end(), [this](int left, int right) {
		return fileLess(currentTask()->getTestCase(left)->getInputFiles().value(0),
		                currentTask()->getTestCase(right)->getInputFiles().value(0));
	});
	reorderCases(currentTask(), order);
	edited();
	return true;
}

bool TaskController::setFilePair(int index, const QString &input, const QString &output) {
	if (! writable() || ! currentSubtask())
		return false;
	if (input.isEmpty() || output.isEmpty())
		return fail(tr("Choose an input file and an output file."));
	auto *value = currentSubtask();
	if (index < -1 || index >= value->getInputFiles().size())
		return false;
	if (index == -1)
		value->addSingleCase(input, output);
	else {
		value->setInputFiles(index, input);
		value->setOutputFiles(index, output);
	}
	edited();
	return true;
}

bool TaskController::removeFilePair(int index) { return removeFilePairs({index}); }

bool TaskController::removeFilePairs(const QVariantList &indices) {
	if (! writable() || ! currentSubtask())
		return false;
	auto *value = currentSubtask();
	QList<int> selected;
	for (const auto &entry : indices) {
		bool ok = false;
		const int index = entry.toInt(&ok);
		if (! ok || index < 0 || index >= value->getInputFiles().size() ||
		    index >= value->getOutputFiles().size())
			return false;
		if (! selected.contains(index))
			selected.append(index);
	}
	if (selected.isEmpty())
		return false;
	std::sort(selected.begin(), selected.end(), std::greater<int>());
	for (int index : selected)
		value->deleteSingleCase(index);
	edited();
	return true;
}

bool TaskController::moveFilePair(int index, int offset) { return moveFilePairs({index}, offset); }

bool TaskController::moveFilePairs(const QVariantList &indices, int offset) {
	if (! writable() || ! currentSubtask() || (offset != -1 && offset != 1))
		return false;
	auto *value = currentSubtask();
	const int count = qMin(value->getInputFiles().size(), value->getOutputFiles().size());
	QList<int> selected;
	for (const auto &entry : indices) {
		bool ok = false;
		const int index = entry.toInt(&ok);
		if (! ok || index < 0 || index >= count)
			return false;
		if (! selected.contains(index))
			selected.append(index);
	}
	if (selected.isEmpty())
		return false;
	std::sort(selected.begin(), selected.end());
	if (selected.last() - selected.first() + 1 != selected.size())
		return fail(tr("Select consecutive file pairs to move them together."));
	if (selected.first() + offset < 0 || selected.last() + offset >= count)
		return false;
	if (offset < 0) {
		for (int index : selected)
			value->swapFiles(index, index - 1);
	} else {
		for (auto it = selected.crbegin(); it != selected.crend(); ++it)
			value->swapFiles(*it, *it + 1);
	}
	edited();
	return true;
}
bool TaskController::sortFilePairs() {
	if (! writable() || ! currentSubtask())
		return false;
	auto *value = currentSubtask();
	QList<std::pair<QString, QString>> pairs;
	for (int i = 0; i < value->getInputFiles().size(); ++i)
		pairs.append({value->getInputFiles().at(i), value->getOutputFiles().value(i)});
	std::stable_sort(pairs.begin(), pairs.end(),
	                 [](const auto &left, const auto &right) { return fileLess(left.first, right.first); });
	for (int i = 0; i < pairs.size(); ++i) {
		value->setInputFiles(i, pairs.at(i).first);
		value->setOutputFiles(i, pairs.at(i).second);
	}
	edited();
	return true;
}

QString TaskController::relativeDataFile(const QUrl &url) const {
	return url.isLocalFile()
	           ? QDir::toNativeSeparators(QDir(Settings::dataPath()).relativeFilePath(url.toLocalFile()))
	           : QString();
}

QStringList TaskController::dataFiles(const QString &prefix) const {
	QStringList result;
	const QDir directory(Settings::dataPath());
	QDirIterator iterator(directory.absolutePath(), QDir::Files, QDirIterator::Subdirectories);
	while (iterator.hasNext()) {
		const QString name = QDir::toNativeSeparators(directory.relativeFilePath(iterator.next()));
		if (name.startsWith(prefix, Qt::CaseInsensitive))
			result.append(name);
		if (result.size() >= 100)
			break;
	}
	return result;
}

bool TaskController::previewImport(const QString &inputPattern, const QString &outputPattern,
                                   const QVariantList &arguments) {
	preview.clear();
	emit previewChanged();
	if (! writable() || ! currentTask())
		return false;
	if (inputPattern.isEmpty() || outputPattern.isEmpty())
		return fail(tr("Enter both file-name patterns."));
	if (arguments.size() > 9)
		return fail(tr("A maximum of nine pattern arguments is supported."));
	auto expressionFor = [this, &arguments](const QString &pattern, QRegularExpression &result) {
		const QString normalized = QDir::fromNativeSeparators(pattern);
		const QRegularExpression token("<([0-9]+)>");
		auto matches = token.globalMatch(normalized);
		QString expression;
		qsizetype position = 0;
		QSet<int> used;
		while (matches.hasNext()) {
			const auto match = matches.next();
			const int index = match.captured(1).toInt() - 1;
			if (index < 0 || index >= arguments.size() || used.contains(index))
				return fail(tr("Each <n> argument must exist and appear at most once per pattern."));
			used.insert(index);
			const QString argument = arguments.at(index).toMap().value("expression").toString();
			if (! QRegularExpression(argument).isValid())
				return fail(tr("Argument <%1> contains an invalid regular expression.").arg(index + 1));
			expression +=
			    QRegularExpression::escape(normalized.mid(position, match.capturedStart() - position));
			expression += QString("(?<argument%1>%2)").arg(index + 1).arg(argument);
			position = match.capturedEnd();
		}
		expression += QRegularExpression::escape(normalized.mid(position));
		result = QRegularExpression(QRegularExpression::anchoredPattern(expression));
		return result.isValid() ? true : fail(tr("Invalid pattern: %1").arg(result.errorString()));
	};
	QRegularExpression inputExpression;
	QRegularExpression outputExpression;
	if (! expressionFor(inputPattern, inputExpression) || ! expressionFor(outputPattern, outputExpression))
		return false;
	const QDir directory(Settings::dataPath());
	if (! directory.exists())
		return fail(tr("The contest data directory does not exist."));
	QStringList files;
	QDirIterator iterator(directory.absolutePath(), QDir::Files, QDirIterator::Subdirectories);
	while (iterator.hasNext())
		files.append(QDir::fromNativeSeparators(directory.relativeFilePath(iterator.next())));
	std::sort(files.begin(), files.end(), fileLess);
	QMap<QString, QString> inputs;
	QMap<QString, QString> outputs;
	QMap<QString, QStringList> partsByKey;
	for (const auto &file : files) {
		for (int kind = 0; kind < 2; ++kind) {
			const auto match = (kind == 0 ? inputExpression : outputExpression).match(file);
			if (! match.hasMatch())
				continue;
			QStringList parts;
			for (int i = 0; i < arguments.size(); ++i) {
				const QString name = QString("argument%1").arg(i + 1);
				parts.append(match.regularExpression().namedCaptureGroups().contains(name)
				                 ? match.captured(name)
				                 : QString());
			}
			const QString key = matchKey(parts);
			auto &target = kind == 0 ? inputs : outputs;
			if (target.contains(key))
				return fail(tr("Several %1 files have the same argument values. Add an argument to identify "
				               "each pair.")
				                .arg(kind == 0 ? tr("input") : tr("output")));
			target.insert(key, file);
			partsByKey.insert(key, parts);
		}
	}
	QMap<QString, QVariantList> grouped;
	QMap<QString, QString> labels;
	for (auto it = inputs.cbegin(); it != inputs.cend(); ++it) {
		if (! outputs.contains(it.key()))
			continue;
		const auto parts = partsByKey.value(it.key());
		QStringList group;
		for (int i = 0; i < arguments.size(); ++i)
			if (arguments.at(i).toMap().value("group").toBool())
				group.append(parts.value(i));
		const QString key = matchKey(group);
		grouped[key].append(QVariantMap{{"input", QDir::toNativeSeparators(it.value())},
		                                {"output", QDir::toNativeSeparators(outputs.value(it.key()))}});
		labels.insert(key, group.join(" · "));
	}
	auto keys = grouped.keys();
	std::sort(keys.begin(), keys.end(), [&labels](const QString &left, const QString &right) {
		return fileLess(labels.value(left), labels.value(right));
	});
	for (const auto &key : keys) {
		auto pairs = grouped.value(key);
		std::sort(pairs.begin(), pairs.end(), [](const QVariant &left, const QVariant &right) {
			return fileLess(left.toMap().value("input").toString(), right.toMap().value("input").toString());
		});
		preview.append(QVariantMap{{"group", labels.value(key)}, {"files", pairs}, {"count", pairs.size()}});
	}
	emit previewChanged();
	if (preview.isEmpty())
		return fail(tr("No matching input and output pairs were found."));
	return true;
}

bool TaskController::importTestCases(const QVariantMap &values) {
	if (! writable() || ! currentTask() || ! validateLimits(values, false))
		return false;
	if (preview.isEmpty())
		return fail(tr("Preview matching files before importing."));
	for (const auto &entry : std::as_const(preview)) {
		auto *value = new TestCase;
		value->setFullScore(values.value("score").toInt());
		value->setTimeLimit(values.value("time").toInt());
		value->setMemoryLimit(values.value("memory").toInt());
		value->setIndex(currentTask()->getTestCaseList().size() + 1);
		for (const auto &pair : entry.toMap().value("files").toList()) {
			const auto files = pair.toMap();
			value->addSingleCase(files.value("input").toString(), files.value("output").toString());
		}
		currentTask()->addTestCase(value);
	}
	subtaskIndex = currentTask()->getTestCaseList().size() - 1;
	preview.clear();
	emit previewChanged();
	edited();
	return true;
}

bool TaskController::scanTasks() {
	discovered.clear();
	emit previewChanged();
	if (! writable())
		return false;
	const QDir data(Settings::dataPath());
	QSet<QString> existing;
	for (auto *value : contest->getTaskList())
		existing.insert(value->getSourceFileName());
	auto inputExtensions = settings->getInputFileExtensions();
	auto outputExtensions = settings->getOutputFileExtensions();
	if (inputExtensions.isEmpty())
		inputExtensions = {"in"};
	if (outputExtensions.isEmpty())
		outputExtensions = {"out", "ans"};
	QStringList inputFilters;
	QStringList outputFilters;
	for (const auto &extension : inputExtensions)
		inputFilters.append("*." + extension);
	for (const auto &extension : outputExtensions)
		outputFilters.append("*." + extension);
	const auto folders = data.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
	for (const auto &folder : folders) {
		if (existing.contains(folder))
			continue;
		const QDir directory(data.filePath(folder));
		QMap<QString, QString> inputs;
		QMap<QString, QString> outputs;
		for (const auto &file : directory.entryInfoList(inputFilters, QDir::Files))
			inputs.insert(file.completeBaseName(), file.fileName());
		for (const auto &file : directory.entryInfoList(outputFilters, QDir::Files))
			outputs.insert(file.completeBaseName(), file.fileName());
		QVariantList pairs;
		for (auto it = inputs.cbegin(); it != inputs.cend(); ++it) {
			if (outputs.contains(it.key()))
				pairs.append(QVariantMap{
				    {"input", QDir::toNativeSeparators(folder + "/" + it.value())},
				    {"output", QDir::toNativeSeparators(folder + "/" + outputs.value(it.key()))}});
		}
		std::sort(pairs.begin(), pairs.end(), [](const QVariant &left, const QVariant &right) {
			return fileLess(left.toMap().value("input").toString(), right.toMap().value("input").toString());
		});
		if (! pairs.isEmpty())
			discovered.append(QVariantMap{{"title", folder},
			                              {"score", qMax(100, int(pairs.size()))},
			                              {"time", settings->getDefaultTimeLimit()},
			                              {"memory", settings->getDefaultMemoryLimit()},
			                              {"files", pairs},
			                              {"count", pairs.size()},
			                              {"selected", true}});
	}
	emit previewChanged();
	return discovered.isEmpty()
	           ? fail(tr("No new task directories with matching input and output files were found."))
	           : true;
}

bool TaskController::importTasks(const QVariantList &selections) {
	if (! writable())
		return false;
	QList<std::pair<int, QVariantMap>> imports;
	QSet<int> used;
	QSet<QString> existing;
	for (auto *value : contest->getTaskList())
		existing.insert(value->getSourceFileName());
	for (const auto &selection : selections) {
		const auto values = selection.toMap();
		if (! values.value("selected").toBool())
			continue;
		bool ok = false;
		const int index = values.value("index").toInt(&ok);
		if (! ok || index < 0 || index >= discovered.size() || used.contains(index))
			return fail(tr("Scan the task directories again before importing."));
		if (! validateLimits(values, false))
			return false;
		const QString title = discovered.at(index).toMap().value("title").toString();
		if (existing.contains(title))
			return fail(tr("Task %1 already exists.").arg(title));
		existing.insert(title);
		used.insert(index);
		imports.append({index, values});
	}
	if (imports.isEmpty())
		return fail(tr("Select at least one task to import."));
	for (const auto &entry : imports) {
		const auto candidate = discovered.at(entry.first).toMap();
		const auto pairs = candidate.value("files").toList();
		if (pairs.isEmpty())
			continue;
		auto *value = createTask(candidate.value("title").toString());
		const int total = entry.second.value("score").toInt();
		for (int i = 0; i < pairs.size(); ++i) {
			auto *testCase = new TestCase;
			testCase->setIndex(i + 1);
			testCase->setFullScore(total / pairs.size() + int(i < total % pairs.size()));
			testCase->setTimeLimit(entry.second.value("time").toInt());
			testCase->setMemoryLimit(entry.second.value("memory").toInt());
			const auto files = pairs.at(i).toMap();
			testCase->addSingleCase(files.value("input").toString(), files.value("output").toString());
			value->addTestCase(testCase);
		}
	}
	discovered.clear();
	emit previewChanged();
	taskIndex = contest->getTaskList().isEmpty() ? -1 : 0;
	subtaskIndex = -1;
	edited();
	return true;
}
