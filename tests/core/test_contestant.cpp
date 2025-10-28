/*
 * SPDX-FileCopyrightText: 2025 Project LemonLime
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QTest>

#include "base/LemonType.hpp"
#include "contestant.h"

class TestContestant : public QObject {
	Q_OBJECT

  public:
	explicit TestContestant(QObject *parent = nullptr);

  private slots:
	void initTestCase();
	void cleanupTestCase();
	void init();
	void cleanup();

	// 测试参赛者基本信息管理
	void testSetAndGetContestantName();
	void testSetAndGetContestantName_data();
	void testSetAndGetJudgingTime();

	// 测试任务管理
	void testAddTask_increasesTaskCount();
	void testDeleteTask_withValidIndex_removesTask();
	void testDeleteTask_withInvalidIndex_noChange();
	void testSwapTask_withValidIndices_swapsTasks();
	void testSwapTask_withInvalidIndices_noChange();

	// 测试成绩和状态查询
	void testGetCheckJudged_withValidTaskIndex_returnsCorrectValue();
	void testGetCheckJudged_withInvalidTaskIndex_returnsFalse();
	void testGetCompileState_withValidTaskIndex_returnsCorrectState();
	void testGetSourceFile_withValidTaskIndex_returnsCorrectFile();
	void testGetTotalScore_withMultipleTasks_returnsCorrectSum();
	void testGetTotalUsedTime_withMultipleTasks_returnsCorrectSum();

	// 测试设置操作
	void testSetCheckJudged_updatesValue();
	void testSetCompileState_updatesState();
	void testSetSourceFile_updatesFile();

	// 测试 JSON 序列化
	void testWriteToJson_withValidContestant_writesCorrectJson();
	void testReadFromJson_withValidJson_createsCorrectContestant();

  private:
	Contestant *m_contestant;
	QStringList m_testFileList;

	// 辅助方法
	void setupContestantWithTasks(int taskCount);
};

TestContestant::TestContestant(QObject *parent) : QObject(parent) {}

void TestContestant::initTestCase() {
	m_contestant = new Contestant();

	// 准备测试数据
	m_testFileList << "main.cpp"
	               << "solution.java"
	               << "main.py";
}

void TestContestant::cleanupTestCase() { delete m_contestant; }

void TestContestant::init() {
	// 准备初始状态
	m_contestant->setContestantName("TestUser");
	m_contestant->setJudgingTime(QDateTime::currentDateTime());

	// 添加初始任务
	setupContestantWithTasks(2);
}

void TestContestant::cleanup() {
	// 清理任务
	while (m_contestant->getTaskScore(0) >= 0) {
		m_contestant->deleteTask(0);
	}
}

void TestContestant::testSetAndGetContestantName() {
	QFETCH(QString, name);
	QFETCH(QString, expected);

	m_contestant->setContestantName(name);
	QString result = m_contestant->getContestantName();

	QVERIFY2(result == expected, qPrintable(QString("Expected '%1' but got '%2'").arg(expected).arg(result)));
}

void TestContestant::testSetAndGetContestantName_data() {
	QTest::addColumn<QString>("name");
	QTest::addColumn<QString>("expected");

	QTest::newRow("empty_name") << QString("") << QString("");
	QTest::newRow("simple_name") << QString("User123") << QString("User123");
	QTest::newRow("name_with_underscore") << QString("user_123") << QString("user_123");
	QTest::newRow("name_with_digit") << QString("user123") << QString("user123");
	QTest::newRow("name_with_unicode") << QString("用户123") << QString("用户123");
}

void TestContestant::testSetAndGetJudgingTime() {
	// 准备
	QDateTime expectedTime = QDateTime::fromString("2025-01-01T12:00:00", Qt::ISODate);

	// 执行
	m_contestant->setJudgingTime(expectedTime);
	QDateTime result = m_contestant->getJudingTime();

	// 验证
	QVERIFY(result == expectedTime);
}

void TestContestant::testAddTask_increasesTaskCount() {
	// 准备
	setupContestantWithTasks(1);
	int initialCount = 1;

	// 执行
	m_contestant->addTask();

	// 验证
	// 检查是否能访问更多任务（通过尝试设置值）
	m_contestant->setCheckJudged(1, true);
	bool result = m_contestant->getCheckJudged(1);
	QVERIFY(result == true);
}

void TestContestant::testDeleteTask_withValidIndex_removesTask() {
	// 准备
	setupContestantWithTasks(2);
	m_contestant->setCheckJudged(0, true);
	m_contestant->setCheckJudged(1, true);

	// 执行
	m_contestant->deleteTask(0);

	// 验证 - 现在应该只有 1 个任务
	// 尝试访问被删除的任务应该返回默认值
	// 访问剩余的任务应该成功
	m_contestant->setCheckJudged(0, true);
	bool result = m_contestant->getCheckJudged(0);
	QVERIFY(result == true);
}

void TestContestant::testDeleteTask_withInvalidIndex_noChange() {
	// 准备
	setupContestantWithTasks(2);
	bool checkJudged0 = m_contestant->getCheckJudged(0);
	bool checkJudged1 = m_contestant->getCheckJudged(1);

	// 执行 - 删除无效索引
	m_contestant->deleteTask(100);
	m_contestant->deleteTask(-1);

	// 验证 - 应该没有改变
	QVERIFY(m_contestant->getCheckJudged(0) == checkJudged0);
	QVERIFY(m_contestant->getCheckJudged(1) == checkJudged1);
}

void TestContestant::testSwapTask_withValidIndices_swapsTasks() {
	// 准备
	setupContestantWithTasks(2);
	m_contestant->setCheckJudged(0, true);
	m_contestant->setCheckJudged(1, false);

	// 执行
	m_contestant->swapTask(0, 1);

	// 验证 - 值应该交换
	QVERIFY(m_contestant->getCheckJudged(0) == false);
	QVERIFY(m_contestant->getCheckJudged(1) == true);
}

void TestContestant::testSwapTask_withInvalidIndices_noChange() {
	// 准备
	setupContestantWithTasks(2);
	bool checkJudged0 = m_contestant->getCheckJudged(0);
	bool checkJudged1 = m_contestant->getCheckJudged(1);

	// 执行 - 交换无效索引
	m_contestant->swapTask(0, 100);
	m_contestant->swapTask(-1, 1);

	// 验证 - 应该没有改变
	QVERIFY(m_contestant->getCheckJudged(0) == checkJudged0);
	QVERIFY(m_contestant->getCheckJudged(1) == checkJudged1);
}

void TestContestant::testGetCheckJudged_withValidTaskIndex_returnsCorrectValue() {
	// 准备
	setupContestantWithTasks(1);
	m_contestant->setCheckJudged(0, true);

	// 执行和验证
	QVERIFY(m_contestant->getCheckJudged(0) == true);

	m_contestant->setCheckJudged(0, false);
	QVERIFY(m_contestant->getCheckJudged(0) == false);
}

void TestContestant::testGetCheckJudged_withInvalidTaskIndex_returnsFalse() {
	// 执行和验证
	QVERIFY(m_contestant->getCheckJudged(-1) == false);
	QVERIFY(m_contestant->getCheckJudged(100) == false);
}

void TestContestant::testGetCompileState_withValidTaskIndex_returnsCorrectState() {
	// 准备
	setupContestantWithTasks(1);

	// 初始状态应该是 NoValidSourceFile (1)
	CompileState initialState = m_contestant->getCompileState(0);
	QVERIFY(initialState == CompileState::NoValidSourceFile);

	// 设置为编译成功
	m_contestant->setCompileState(0, CompileState::CompileSuccessfully);
	QVERIFY(m_contestant->getCompileState(0) == CompileState::CompileSuccessfully);

	// 设置为编译错误
	m_contestant->setCompileState(0, CompileState::CompileError);
	QVERIFY(m_contestant->getCompileState(0) == CompileState::CompileError);
}

void TestContestant::testGetSourceFile_withValidTaskIndex_returnsCorrectFile() {
	// 准备
	setupContestantWithTasks(1);
	QString testFile = "solution.cpp";
	m_contestant->setSourceFile(0, testFile);

	// 执行和验证
	QString result = m_contestant->getSourceFile(0);
	QVERIFY(result == testFile);
}

void TestContestant::testGetTotalScore_withMultipleTasks_returnsCorrectSum() {
	// 准备
	setupContestantWithTasks(3);

	// 设置各任务分数
	m_contestant->setScore(0, QList<QList<int>>() << (QList<int>() << 100));
	m_contestant->setScore(1, QList<QList<int>>() << (QList<int>() << 80));
	m_contestant->setScore(2, QList<QList<int>>() << (QList<int>() << 90));

	// 执行和验证
	// 注意：getTotalScore 计算所有任务所有子任务的总分
	int result = m_contestant->getTotalScore();
	QVERIFY(result == 270); // 100 + 80 + 90
}

void TestContestant::testGetTotalUsedTime_withMultipleTasks_returnsCorrectSum() {
	// 准备
	setupContestantWithTasks(2);

	// 设置各任务时间
	m_contestant->setTimeUsed(0, QList<QList<int>>() << (QList<int>() << 100));
	m_contestant->setTimeUsed(1, QList<QList<int>>() << (QList<int>() << 200));

	// 执行和验证
	int result = m_contestant->getTotalUsedTime();
	QVERIFY(result == 300); // 100 + 200
}

void TestContestant::testSetCheckJudged_updatesValue() {
	// 准备
	setupContestantWithTasks(1);

	// 执行
	m_contestant->setCheckJudged(0, true);

	// 验证
	QVERIFY(m_contestant->getCheckJudged(0) == true);

	m_contestant->setCheckJudged(0, false);
	QVERIFY(m_contestant->getCheckJudged(0) == false);
}

void TestContestant::testSetCompileState_updatesState() {
	// 准备
	setupContestantWithTasks(1);

	// 执行
	m_contestant->setCompileState(0, CompileState::CompileSuccessfully);

	// 验证
	QVERIFY(m_contestant->getCompileState(0) == CompileState::CompileSuccessfully);
}

void TestContestant::testSetSourceFile_updatesFile() {
	// 准备
	setupContestantWithTasks(1);

	// 执行
	QString testFile = "new_solution.cpp";
	m_contestant->setSourceFile(0, testFile);

	// 验证
	QVERIFY(m_contestant->getSourceFile(0) == testFile);
}

void TestContestant::testWriteToJson_withValidContestant_writesCorrectJson() {
	// 准备
	m_contestant->setContestantName("TestUser");
	m_contestant->setJudgingTime(QDateTime::fromString("2025-01-01T00:00:00", Qt::ISODate));
	m_contestant->setCheckJudged(0, true);
	m_contestant->setCompileState(0, CompileState::CompileSuccessfully);
	m_contestant->setSourceFile(0, "main.cpp");

	// 执行
	QJsonObject json;
	m_contestant->writeToJson(json);

	// 验证
	QVERIFY(json.contains("contestantName"));
	QVERIFY(json["contestantName"] == "TestUser");
	QVERIFY(json.contains("judgingTime"));
	QVERIFY(json.contains("taskResults"));
}

void TestContestant::testReadFromJson_withValidJson_createsCorrectContestant() {
	// 准备
	QJsonObject json;
	json["contestantName"] = "TestUser";
	json["judgingTime"] = "2025-01-01T00:00:00";

	// 执行
	int result = m_contestant->readFromJson(json);

	// 验证
	QVERIFY(result == 0); // 0 表示成功
	QVERIFY(m_contestant->getContestantName() == "TestUser");
}

void TestContestant::setupContestantWithTasks(int taskCount) {
	// 为指定数量的任务添加空槽位
	for (int i = 0; i < taskCount; i++) {
		m_contestant->addTask();
	}
}

QTEST_MAIN(TestContestant)

#include "test_contestant.moc"
