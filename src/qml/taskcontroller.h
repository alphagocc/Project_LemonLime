/*
 * SPDX-FileCopyrightText: 2026 Project LemonLime
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QObject>
#include <QPointer>
#include <QUrl>
#include <QVariant>

class Contest;
class Settings;
class Task;
class TestCase;

class TaskController : public QObject {
	Q_OBJECT
	Q_PROPERTY(bool available READ available NOTIFY changed)
	Q_PROPERTY(bool busy READ busy NOTIFY changed)
	Q_PROPERTY(QVariantList tasks READ tasks NOTIFY changed)
	Q_PROPERTY(int selectedTask READ selectedTask WRITE selectTask NOTIFY changed)
	Q_PROPERTY(QVariantMap task READ task NOTIFY changed)
	Q_PROPERTY(QVariantList subtasks READ subtasks NOTIFY changed)
	Q_PROPERTY(int selectedSubtask READ selectedSubtask WRITE selectSubtask NOTIFY changed)
	Q_PROPERTY(QVariantMap subtask READ subtask NOTIFY changed)
	Q_PROPERTY(QVariantList compilers READ compilers NOTIFY changed)
	Q_PROPERTY(QVariantMap defaults READ defaults NOTIFY changed)
	Q_PROPERTY(QUrl dataFolder READ dataFolder NOTIFY changed)
	Q_PROPERTY(QVariantList importPreview READ importPreview NOTIFY previewChanged)
	Q_PROPERTY(QVariantList discoveredTasks READ discoveredTasks NOTIFY previewChanged)

  public:
	explicit TaskController(Settings *settings, QObject *parent = nullptr);
	~TaskController() override;
	void setContest(Contest *contest);
	void setBusy(bool busy);
	bool available() const;
	bool busy() const;
	QVariantList tasks() const;
	int selectedTask() const;
	QVariantMap task() const;
	QVariantList subtasks() const;
	int selectedSubtask() const;
	QVariantMap subtask() const;
	QVariantList compilers() const;
	QVariantMap defaults() const;
	QUrl dataFolder() const;
	QVariantList importPreview() const;
	QVariantList discoveredTasks() const;

	Q_INVOKABLE void refresh();
	Q_INVOKABLE void selectTask(int index);
	Q_INVOKABLE void selectSubtask(int index);
	Q_INVOKABLE bool addTask(const QString &title);
	Q_INVOKABLE bool duplicateTask();
	Q_INVOKABLE bool removeTask();
	Q_INVOKABLE bool moveTask(int offset);
	Q_INVOKABLE bool updateTask(const QVariantMap &values);
	Q_INVOKABLE bool setCompilerConfiguration(int index, const QString &configuration);
	Q_INVOKABLE bool setFileMapping(bool grader, int index, const QString &source, const QString &name);
	Q_INVOKABLE bool removeFileMapping(bool grader, int index);
	Q_INVOKABLE bool addSubtask(const QVariantMap &values);
	Q_INVOKABLE bool addEmptySubtask();
	Q_INVOKABLE bool beginTaskEdit();
	Q_INVOKABLE void finishTaskEdit(bool accept);
	Q_INVOKABLE bool updateSubtasks(const QVariantList &indices, const QVariantMap &values);
	Q_INVOKABLE bool removeSubtasks(const QVariantList &indices);
	Q_INVOKABLE bool removeTestCaseRows(int firstRow, int lastRow);
	Q_INVOKABLE bool moveSubtasks(const QVariantList &indices, int offset);
	Q_INVOKABLE bool mergeSubtasks(const QVariantList &indices);
	Q_INVOKABLE bool splitSubtasks(const QVariantList &indices);
	Q_INVOKABLE bool sortSubtasks();
	Q_INVOKABLE bool setFilePair(int index, const QString &input, const QString &output);
	Q_INVOKABLE bool removeFilePair(int index);
	Q_INVOKABLE bool removeFilePairs(const QVariantList &indices);
	Q_INVOKABLE bool moveFilePair(int index, int offset);
	Q_INVOKABLE bool moveFilePairs(const QVariantList &indices, int offset);
	Q_INVOKABLE bool sortFilePairs();
	Q_INVOKABLE QString relativeDataFile(const QUrl &url) const;
	Q_INVOKABLE QStringList dataFiles(const QString &prefix) const;
	Q_INVOKABLE bool previewImport(const QString &inputPattern, const QString &outputPattern,
	                               const QVariantList &arguments);
	Q_INVOKABLE bool importTestCases(const QVariantMap &values);
	Q_INVOKABLE bool scanTasks();
	Q_INVOKABLE bool importTasks(const QVariantList &selections);

  signals:
	void changed();
	void previewChanged();
	void contestEdited();
	void errorOccurred(const QString &message);

  private:
	Settings *settings;
	QPointer<Contest> contest;
	QPointer<Task> editDraft;
	bool isBusy = false;
	int taskIndex = -1;
	int subtaskIndex = -1;
	QVariantList preview;
	QVariantList discovered;
	Task *currentTask() const;
	TestCase *currentSubtask() const;
	bool writable();
	bool fail(const QString &message);
	void edited();
	bool validateLimits(const QVariantMap &values, bool allowPartial = true);
	bool validateDependencies(const QString &text, int index, QList<int> &result);
	QList<int> checkedIndices(const QVariantList &indices) const;
	Task *createTask(const QString &title);
};
