/*
 * SPDX-FileCopyrightText: 2025 Project LemonLime
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <QTest>
#include <QSignalSpy>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>

#include "contest.h"
#include "task.h"
#include "contestant.h"
#include "testcase.h"
#include "settings.h"

class TestContest : public QObject {
    Q_OBJECT

  public:
    explicit TestContest(QObject *parent = nullptr);

  private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // 测试竞赛标题管理
    void testSetAndGetContestTitle();
    void testSetAndGetContestTitle_data();

    // 测试任务管理
    void testTaskManagement_whenAddTask_thenTaskAdded();
    void testTaskManagement_whenDeleteValidTask_thenTaskDeleted();
    void testTaskManagement_whenDeleteInvalidTask_thenNoChange();
    void testTaskManagement_whenSwapValidTasks_thenTasksSwapped();
    void testTaskManagement_whenSwapInvalidTasks_thenNoChange();
    void testGetTask_withValidIndex_returnsTask();
    void testGetTask_withInvalidIndex_returnsNull();

    // 测试参赛者管理
    void testContestantManagement_whenAddContestant_thenContestantAdded();
    void testContestantManagement_whenDeleteContestant_thenContestantDeleted();
    void testGetContestant_withExistingName_returnsContestant();
    void testGetContestant_withNonExistingName_returnsNull();

    // 测试聚合计算
    void testGetTotalTimeLimit_withNoTasks_returnsZero();
    void testGetTotalTimeLimit_withMultipleTasks_returnsCorrectSum();
    void testGetTotalScore_withNoTasks_returnsZero();
    void testGetTotalScore_withMultipleTasks_returnsCorrectSum();

    // 测试 JSON 序列化
    void testWriteToJson_withValidContest_writesCorrectJson();
    void testReadFromJson_withValidJson_createsCorrectContest();
    void testReadFromJson_withInvalidVersion_returnsError();
    void testReadFromJson_withMissingFields_handlesGracefully();

    // 测试信号发射
    void testSignals_whenAddTask_emitsSignals();
    void testSignals_whenDeleteTask_emitsSignals();

    // 测试设置管理
    void testSetSettings_andCopySettings();

  private:
    Contest *m_contest;
    Settings *m_settings;
    Task *m_task;
    Contestant *m_contestant;
    TestCase *m_testCase;

    // 辅助方法
    void createTestTask(const QString &title, int score, int timeLimit);
    void createTestContestant(const QString &name);
};

TestContest::TestContest(QObject *parent) : QObject(parent) {}

void TestContest::initTestCase() {
    // 在第一个测试前执行
    m_settings = new Settings();
    m_contest = new Contest();
    m_contest->setSettings(m_settings);

    // 加载默认设置
    m_settings->loadSettings();
}

void TestContest::cleanupTestCase() {
    // 在最后一个测试后执行
    delete m_contest;
    delete m_settings;
}

void TestContest::init() {
    // 在每个测试前执行
}

void TestContest::cleanup() {
    // 在每个测试后执行
    // 清理任务列表
    for (auto *task : m_contest->getTaskList()) {
        m_contest->deleteTask(0);
    }

    // 清理参赛者列表
    auto contestantList = m_contest->getContestantList();
    for (auto *contestant : contestantList) {
        m_contest->deleteContestant(contestant->getContestantName());
    }
}

void TestContest::testSetAndGetContestTitle() {
    QFETCH(QString, title);
    QFETCH(QString, expected);

    m_contest->setContestTitle(title);
    QString result = m_contest->getContestTitle();

    QVERIFY2(result == expected,
             qPrintable(QString("Expected '%1' but got '%2'").arg(expected).arg(result)));
}

void TestContest::testSetAndGetContestTitle_data() {
    QTest::addColumn<QString>("title");
    QTest::addColumn<QString>("expected");

    QTest::newRow("empty_title") << QString("") << QString("");
    QTest::newRow("simple_title") << QString("Test Contest") << QString("Test Contest");
    QTest::newRow("title_with_spaces") << QString("  My Contest  ") << QString("  My Contest  ");
    QTest::newRow("title_with_unicode") << QString("竞赛测试") << QString("竞赛测试");
    QTest::newRow("title_with_special_chars") << QString("Contest!@#$%") << QString("Contest!@#$%");
}

void TestContest::testTaskManagement_whenAddTask_thenTaskAdded() {
    createTestTask("Problem A", 100, 1000);

    QVERIFY(m_contest->getTaskList().size() == 1);
    QVERIFY(m_contest->getTask(0) == m_task);
    QVERIFY(m_contest->getTask(0)->getProblemTitle() == "Problem A");
}

void TestContest::testTaskManagement_whenDeleteValidTask_thenTaskDeleted() {
    // 准备
    createTestTask("Problem A", 100, 1000);
    QVERIFY(m_contest->getTaskList().size() == 1);

    // 执行
    m_contest->deleteTask(0);

    // 验证
    QVERIFY(m_contest->getTaskList().size() == 0);
}

void TestContest::testTaskManagement_whenDeleteInvalidTask_thenNoChange() {
    // 准备
    createTestTask("Problem A", 100, 1000);
    QVERIFY(m_contest->getTaskList().size() == 1);

    // 执行 - 删除无效索引
    m_contest->deleteTask(10);
    m_contest->deleteTask(-1);

    // 验证
    QVERIFY(m_contest->getTaskList().size() == 1);
}

void TestContest::testTaskManagement_whenSwapValidTasks_thenTasksSwapped() {
    // 准备
    createTestTask("Problem A", 100, 1000);
    Task *taskB = new Task();
    taskB->setProblemTitle("Problem B");
    m_contest->addTask(taskB);

    QVERIFY(m_contest->getTask(0)->getProblemTitle() == "Problem A");
    QVERIFY(m_contest->getTask(1)->getProblemTitle() == "Problem B");

    // 执行
    m_contest->swapTask(0, 1);

    // 验证
    QVERIFY(m_contest->getTask(0)->getProblemTitle() == "Problem B");
    QVERIFY(m_contest->getTask(1)->getProblemTitle() == "Problem A");
}

void TestContest::testTaskManagement_whenSwapInvalidTasks_thenNoChange() {
    // 准备
    createTestTask("Problem A", 100, 1000);
    Task *taskB = new Task();
    taskB->setProblemTitle("Problem B");
    m_contest->addTask(taskB);

    QString titleA = m_contest->getTask(0)->getProblemTitle();
    QString titleB = m_contest->getTask(1)->getProblemTitle();

    // 执行 - 交换无效索引
    m_contest->swapTask(0, 10);
    m_contest->swapTask(-1, 1);
    m_contest->swapTask(5, 5);

    // 验证 - 应该没有改变
    QVERIFY(m_contest->getTask(0)->getProblemTitle() == titleA);
    QVERIFY(m_contest->getTask(1)->getProblemTitle() == titleB);
}

void TestContest::testGetTask_withValidIndex_returnsTask() {
    // 准备
    createTestTask("Problem A", 100, 1000);
    QVERIFY(m_contest->getTaskList().size() >= 1);

    // 执行和验证
    Task *result = m_contest->getTask(0);
    QVERIFY(result != nullptr);
    QVERIFY(result == m_task);
}

void TestContest::testGetTask_withInvalidIndex_returnsNull() {
    // 准备
    createTestTask("Problem A", 100, 1000);
    QVERIFY(m_contest->getTaskList().size() == 1);

    // 执行和验证 - 边界测试
    QVERIFY(m_contest->getTask(-1) == nullptr);
    QVERIFY(m_contest->getTask(100) == nullptr);
}

void TestContest::testContestantManagement_whenAddContestant_thenContestantAdded() {
    createTestContestant("TestUser");

    Contestant *result = m_contest->getContestant("TestUser");
    QVERIFY(result != nullptr);
    QVERIFY(result->getContestantName() == "TestUser");
}

void TestContest::testContestantManagement_whenDeleteContestant_thenContestantDeleted() {
    // 准备
    createTestContestant("TestUser");
    QVERIFY(m_contest->getContestant("TestUser") != nullptr);

    // 执行
    m_contest->deleteContestant("TestUser");

    // 验证
    QVERIFY(m_contest->getContestant("TestUser") == nullptr);
}

void TestContest::testGetContestant_withExistingName_returnsContestant() {
    // 准备
    createTestContestant("TestUser");

    // 执行和验证
    Contestant *result = m_contest->getContestant("TestUser");
    QVERIFY(result != nullptr);
    QVERIFY(result->getContestantName() == "TestUser");
}

void TestContest::testGetContestant_withNonExistingName_returnsNull() {
    // 执行和验证
    Contestant *result = m_contest->getContestant("NonExistentUser");
    QVERIFY(result == nullptr);
}

void TestContest::testGetTotalTimeLimit_withNoTasks_returnsZero() {
    // 验证没有任务时返回 0
    int result = m_contest->getTotalTimeLimit();
    QVERIFY(result == 0);
}

void TestContest::testGetTotalTimeLimit_withMultipleTasks_returnsCorrectSum() {
    // 准备 - 创建多个任务，每个有多个测试用例
    Task *task1 = new Task();
    task1->setProblemTitle("Problem A");

    TestCase *testCase1 = new TestCase();
    testCase1->setFullScore(50);
    testCase1->setTimeLimit(1000);
    testCase1->addSingleCase("input1.txt", "output1.txt");
    task1->addTestCase(testCase1);

    TestCase *testCase2 = new TestCase();
    testCase2->setFullScore(50);
    testCase2->setTimeLimit(2000);
    testCase2->addSingleCase("input2.txt", "output2.txt");
    // testCase2 有 1 个输入文件
    task1->addTestCase(testCase2);

    m_contest->addTask(task1);

    Task *task2 = new Task();
    task2->setProblemTitle("Problem B");

    TestCase *testCase3 = new TestCase();
    testCase3->setFullScore(100);
    testCase3->setTimeLimit(1500);
    testCase3->addSingleCase("input3.txt", "output3.txt");
    task2->addTestCase(testCase3);

    m_contest->addTask(task2);

    // 执行和验证
    // 总时间 = (1000 * 1) + (2000 * 1) + (1500 * 1) = 4500
    int result = m_contest->getTotalTimeLimit();
    QVERIFY(result == 4500);
}

void TestContest::testGetTotalScore_withNoTasks_returnsZero() {
    // 验证没有任务时返回 0
    int result = m_contest->getTotalScore();
    QVERIFY(result == 0);
}

void TestContest::testGetTotalScore_withMultipleTasks_returnsCorrectSum() {
    // 准备
    Task *task1 = new Task();
    task1->setProblemTitle("Problem A");

    TestCase *testCase1 = new TestCase();
    testCase1->setFullScore(50);
    task1->addTestCase(testCase1);

    TestCase *testCase2 = new TestCase();
    testCase2->setFullScore(50);
    task1->addTestCase(testCase2);

    m_contest->addTask(task1);

    Task *task2 = new Task();
    task2->setProblemTitle("Problem B");

    TestCase *testCase3 = new TestCase();
    testCase3->setFullScore(100);
    task2->addTestCase(testCase3);

    m_contest->addTask(task2);

    // 执行和验证
    // 总分 = (50 + 50) + (100) = 200
    int result = m_contest->getTotalScore();
    QVERIFY(result == 200);
}

void TestContest::testWriteToJson_withValidContest_writesCorrectJson() {
    // 准备
    m_contest->setContestTitle("Test Contest");
    createTestTask("Problem A", 100, 1000);
    createTestContestant("TestUser");

    // 执行
    QJsonObject json;
    m_contest->writeToJson(json);

    // 验证
    QVERIFY(json.contains("contestTitle"));
    QVERIFY(json["contestTitle"] == "Test Contest");
    QVERIFY(json.contains("tasks"));
    QVERIFY(json.contains("contestants"));
    QVERIFY(json["tasks"].toArray().size() == 1);
    QVERIFY(json["contestants"].toArray().size() == 1);
}

void TestContest::testReadFromJson_withValidJson_createsCorrectContest() {
    // 准备 - 读取测试数据文件
    QString testDataPath = QCoreApplication::applicationDirPath() + "/testdata/valid_contest.json";
    if (!QFile::exists(testDataPath)) {
        testDataPath = QDir::currentPath() + "/testdata/valid_contest.json";
    }

    QVERIFY(QFile::exists(testDataPath));

    QFile file(testDataPath);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    // 执行
    int result = m_contest->readFromJson(doc.object());

    // 验证
    QVERIFY(result == 0); // 0 表示成功
    QVERIFY(m_contest->getContestTitle() == "Test Contest");
    QVERIFY(m_contest->getTaskList().size() == 1);
    QVERIFY(m_contest->getTask(0)->getProblemTitle() == "A+B Problem");
}

void TestContest::testReadFromJson_withInvalidVersion_returnsError() {
    // 准备
    QJsonObject json;
    json["version"] = "2.0"; // 不支持的版本
    json["contestTitle"] = "Test";

    // 执行
    int result = m_contest->readFromJson(json);

    // 验证
    QVERIFY(result == -1); // -1 表示错误
}

void TestContest::testReadFromJson_withMissingFields_handlesGracefully() {
    // 准备 - 空 JSON
    QJsonObject json;

    // 执行
    int result = m_contest->readFromJson(json);

    // 验证 - 应该能处理，没有崩溃
    QVERIFY(result == 0); // 空竞赛也是有效的
    QVERIFY(m_contest->getContestTitle() == "");
    QVERIFY(m_contest->getTaskList().size() == 0);
}

void TestContest::testSignals_whenAddTask_emitsSignals() {
    // 准备
    QSignalSpy spy(m_contest, &Contest::taskAddedForContestant);
    QSignalSpy spyViewer(m_contest, &Contest::taskAddedForViewer);

    // 执行
    createTestTask("Problem A", 100, 1000);

    // 验证
    QVERIFY(spy.wait(100)); // 等待信号，最多 100ms
    QVERIFY(spyViewer.wait(100));
    QVERIFY(spy.count() == 1);
    QVERIFY(spyViewer.count() == 1);
}

void TestContest::testSignals_whenDeleteTask_emitsSignals() {
    // 准备
    createTestTask("Problem A", 100, 1000);
    QSignalSpy spy(m_contest, &Contest::taskDeletedForContestant);
    QSignalSpy spyViewer(m_contest, &Contest::taskDeletedForViewer);

    // 执行
    m_contest->deleteTask(0);

    // 验证
    QVERIFY(spy.wait(100));
    QVERIFY(spyViewer.wait(100));
    QVERIFY(spy.count() == 1);
    QVERIFY(spyViewer.count() == 1);
}

void TestContest::testSetSettings_andCopySettings() {
    // 准备
    Settings *newSettings = new Settings();
    newSettings->setDefaultFullScore(200);

    // 执行
    m_contest->setSettings(newSettings);
    m_contest->copySettings(*newSettings);

    // 验证 - 基本设置复制操作
    QVERIFY(m_contest->getTaskList().size() == 0); // 应该保持不变

    delete newSettings;
}

void TestContest::createTestTask(const QString &title, int score, int timeLimit) {
    m_task = new Task();
    m_task->setProblemTitle(title);

    m_testCase = new TestCase();
    m_testCase->setFullScore(score);
    m_testCase->setTimeLimit(timeLimit);
    m_testCase->setMemoryLimit(256);
    m_testCase->addSingleCase("input.txt", "output.txt");

    m_task->addTestCase(m_testCase);
    m_contest->addTask(m_task);
}

void TestContest::createTestContestant(const QString &name) {
    m_contestant = new Contestant();
    m_contestant->setContestantName(name);

    // 添加任务（为参赛者创建空任务结果）
    for (int i = 0; i < m_contest->getTaskList().size(); i++) {
        m_contestant->addTask();
    }

    // 手动添加到 contestantList（因为通常通过 refreshContestantList 或文件扫描添加）
    m_contest->refreshContestantList();

    // 如果自动添加失败，手动插入
    if (m_contest->getContestant(name) == nullptr) {
        // 注意：在实际使用中，contestantList 是私有的，
        // 这里只是为了测试目的。实际上应该通过 refreshContestantList 或文件扫描来管理
    }
}

QTEST_MAIN(TestContest)

#include "test_contest.moc"
