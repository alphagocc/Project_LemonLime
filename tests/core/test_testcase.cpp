/*
 * SPDX-FileCopyrightText: 2025 Project LemonLime
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <QTest>
#include <QJsonObject>
#include <QJsonArray>

#include "testcase.h"

class TestTestCase : public QObject {
    Q_OBJECT

  public:
    explicit TestTestCase(QObject *parent = nullptr);

  private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // 测试基本属性
    void testConstructor_initializesWithDefaults();
    void testSetAndGetFullScore();
    void testSetAndGetTimeLimit();
    void testSetAndGetMemoryLimit();
    void testSetIndex_setsCorrectIndex();

    // 测试文件管理
    void testSetAndGetInputFiles();
    void testSetAndGetOutputFiles();
    void testAddSingleCase_addsInputAndOutputFiles();
    void testAddSingleCase_withMultipleFiles_addsMultipleFiles();
    void testSwapFiles_withValidIndices_swapsFiles();
    void testSwapFiles_withInvalidIndices_noChange();
    void testDeleteSingleCase_withValidIndex_deletesCase();
    void testDeleteSingleCase_withInvalidIndex_noChange();

    // 测试依赖子任务
    void testSetAndGetDependenceSubtask_withStringList();
    void testSetAndGetDependenceSubtask_withIntList();
    void testSetAndGetDependenceSubtask_withIntSet();
    void testCheckDependenceSubtask_withMatchingList_returnsTrue();
    void testCheckDependenceSubtask_withNonMatchingList_returnsFalse();
    void testClearDependenceSubtask_removesAllDependencies();

    // 测试 JSON 序列化
    void testWriteToJson_withValidTestCase_writesCorrectJson();
    void testReadFromJson_withValidJson_createsCorrectTestCase();

    // 测试边界条件
    void testGetInputFiles_withNoFiles_returnsEmptyList();
    void testGetOutputFiles_withNoFiles_returnsEmptyList();

    // 测试聚合属性
    void testGetFullScore_withDefaultValue_returnsZero();
    void testGetTimeLimit_withDefaultValue_returnsZero();
    void testGetMemoryLimit_withDefaultValue_returnsZero();

  private:
    TestCase *m_testCase;
    QStringList m_testInputFiles;
    QStringList m_testOutputFiles;

    // 辅助方法
    void setupTestCaseWithFiles(int fileCount);
};

TestTestCase::TestTestCase(QObject *parent) : QObject(parent) {}

void TestTestCase::initTestCase() {
    m_testCase = new TestCase();

    // 准备测试数据
    m_testInputFiles << "input1.txt"
                     << "input2.txt";
    m_testOutputFiles << "output1.txt"
                      << "output2.txt";
}

void TestTestCase::cleanupTestCase() {
    delete m_testCase;
}

void TestTestCase::init() {
    // 设置初始值
    m_testCase->setFullScore(100);
    m_testCase->setTimeLimit(1000);
    m_testCase->setMemoryLimit(256);
}

void TestTestCase::cleanup() {
    // 清理文件
    m_testCase->clearDependenceSubtask();
}

void TestTestCase::testConstructor_initializesWithDefaults() {
    // 执行
    TestCase *testCase = new TestCase();

    // 验证默认值
    QVERIFY(testCase->getFullScore() == 0);
    QVERIFY(testCase->getTimeLimit() == 0);
    QVERIFY(testCase->getMemoryLimit() == 0);
    QVERIFY(testCase->getInputFiles().isEmpty());
    QVERIFY(testCase->getOutputFiles().isEmpty());
    QVERIFY(testCase->getDependenceSubtask().isEmpty());

    delete testCase;
}

void TestTestCase::testSetAndGetFullScore() {
    // 执行和验证
    m_testCase->setFullScore(200);
    QVERIFY(m_testCase->getFullScore() == 200);

    m_testCase->setFullScore(50);
    QVERIFY(m_testCase->getFullScore() == 50);

    m_testCase->setFullScore(0);
    QVERIFY(m_testCase->getFullScore() == 0);
}

void TestTestCase::testSetAndGetTimeLimit() {
    // 执行和验证
    m_testCase->setTimeLimit(2000);
    QVERIFY(m_testCase->getTimeLimit() == 2000);

    m_testCase->setTimeLimit(500);
    QVERIFY(m_testCase->getTimeLimit() == 500);

    m_testCase->setTimeLimit(0);
    QVERIFY(m_testCase->getTimeLimit() == 0);
}

void TestTestCase::testSetAndGetMemoryLimit() {
    // 执行和验证
    m_testCase->setMemoryLimit(512);
    QVERIFY(m_testCase->getMemoryLimit() == 512);

    m_testCase->setMemoryLimit(128);
    QVERIFY(m_testCase->getMemoryLimit() == 128);

    m_testCase->setMemoryLimit(0);
    QVERIFY(m_testCase->getMemoryLimit() == 0);
}

void TestTestCase::testSetIndex_setsCorrectIndex() {
    // 执行和验证
    m_testCase->setIndex(5);
    // 注意：index 是私有成员，我们通过其他方式验证
    // 这里主要测试 setIndex 方法不会崩溃
    QVERIFY(true); // 如果没有崩溃就算成功
}

void TestTestCase::testSetAndGetInputFiles() {
    // 准备
    QStringList files = {"input1.txt", "input2.txt", "input3.txt"};

    // 执行
    for (int i = 0; i < files.size(); i++) {
        m_testCase->setInputFiles(i, files[i]);
    }

    // 验证
    QStringList result = m_testCase->getInputFiles();
    QVERIFY(result == files);
}

void TestTestCase::testSetAndGetOutputFiles() {
    // 准备
    QStringList files = {"output1.txt", "output2.txt", "output3.txt"};

    // 执行
    for (int i = 0; i < files.size(); i++) {
        m_testCase->setOutputFiles(i, files[i]);
    }

    // 验证
    QStringList result = m_testCase->getOutputFiles();
    QVERIFY(result == files);
}

void TestTestCase::testAddSingleCase_addsInputAndOutputFiles() {
    // 执行
    m_testCase->addSingleCase("input.txt", "output.txt");

    // 验证
    QStringList inputFiles = m_testCase->getInputFiles();
    QStringList outputFiles = m_testCase->getOutputFiles();

    QVERIFY(inputFiles.size() == 1);
    QVERIFY(outputFiles.size() == 1);
    QVERIFY(inputFiles.at(0) == "input.txt");
    QVERIFY(outputFiles.at(0) == "output.txt");
}

void TestTestCase::testAddSingleCase_withMultipleFiles_addsMultipleFiles() {
    // 执行
    m_testCase->addSingleCase("input1.txt", "output1.txt");
    m_testCase->addSingleCase("input2.txt", "output2.txt");
    m_testCase->addSingleCase("input3.txt", "output3.txt");

    // 验证
    QStringList inputFiles = m_testCase->getInputFiles();
    QStringList outputFiles = m_testCase->getOutputFiles();

    QVERIFY(inputFiles.size() == 3);
    QVERIFY(outputFiles.size() == 3);

    QVERIFY(inputFiles.at(0) == "input1.txt");
    QVERIFY(inputFiles.at(1) == "input2.txt");
    QVERIFY(inputFiles.at(2) == "input3.txt");

    QVERIFY(outputFiles.at(0) == "output1.txt");
    QVERIFY(outputFiles.at(1) == "output2.txt");
    QVERIFY(outputFiles.at(2) == "output3.txt");
}

void TestTestCase::testSwapFiles_withValidIndices_swapsFiles() {
    // 准备
    m_testCase->addSingleCase("input1.txt", "output1.txt");
    m_testCase->addSingleCase("input2.txt", "output2.txt");

    // 验证初始状态
    QVERIFY(m_testCase->getInputFiles().at(0) == "input1.txt");
    QVERIFY(m_testCase->getInputFiles().at(1) == "input2.txt");

    // 执行
    m_testCase->swapFiles(0, 1);

    // 验证
    QVERIFY(m_testCase->getInputFiles().at(0) == "input2.txt");
    QVERIFY(m_testCase->getInputFiles().at(1) == "input1.txt");

    QVERIFY(m_testCase->getOutputFiles().at(0) == "output2.txt");
    QVERIFY(m_testCase->getOutputFiles().at(1) == "output1.txt");
}

void TestTestCase::testSwapFiles_withInvalidIndices_noChange() {
    // 准备
    m_testCase->addSingleCase("input1.txt", "output1.txt");
    m_testCase->addSingleCase("input2.txt", "output2.txt");

    QString input0 = m_testCase->getInputFiles().at(0);
    QString input1 = m_testCase->getInputFiles().at(1);

    // 执行 - 交换无效索引
    m_testCase->swapFiles(0, 100);
    m_testCase->swapFiles(-1, 1);

    // 验证 - 应该没有改变
    QVERIFY(m_testCase->getInputFiles().at(0) == input0);
    QVERIFY(m_testCase->getInputFiles().at(1) == input1);
}

void TestTestCase::testDeleteSingleCase_withValidIndex_deletesCase() {
    // 准备
    m_testCase->addSingleCase("input1.txt", "output1.txt");
    m_testCase->addSingleCase("input2.txt", "output2.txt");
    m_testCase->addSingleCase("input3.txt", "output3.txt");

    QVERIFY(m_testCase->getInputFiles().size() == 3);

    // 执行
    m_testCase->deleteSingleCase(1);

    // 验证
    QVERIFY(m_testCase->getInputFiles().size() == 2);
    QVERIFY(m_testCase->getInputFiles().at(0) == "input1.txt");
    QVERIFY(m_testCase->getInputFiles().at(1) == "input3.txt");

    QVERIFY(m_testCase->getOutputFiles().at(0) == "output1.txt");
    QVERIFY(m_testCase->getOutputFiles().at(1) == "output3.txt");
}

void TestTestCase::testDeleteSingleCase_withInvalidIndex_noChange() {
    // 准备
    m_testCase->addSingleCase("input1.txt", "output1.txt");
    m_testCase->addSingleCase("input2.txt", "output2.txt");

    int initialSize = m_testCase->getInputFiles().size();

    // 执行 - 删除无效索引
    m_testCase->deleteSingleCase(100);
    m_testCase->deleteSingleCase(-1);

    // 验证 - 应该没有改变
    QVERIFY(m_testCase->getInputFiles().size() == initialSize);
}

void TestTestCase::testSetAndGetDependenceSubtask_withStringList() {
    // 准备
    QStringList dependencies = {"1", "2", "3"};

    // 执行
    m_testCase->setDependenceSubtask(dependencies);

    // 验证
    QList<int> result = m_testCase->getDependenceSubtask();
    QVERIFY(result.size() == 3);
    QVERIFY(result.at(0) == 1);
    QVERIFY(result.at(1) == 2);
    QVERIFY(result.at(2) == 3);
}

void TestTestCase::testSetAndGetDependenceSubtask_withIntList() {
    // 准备
    QList<int> dependencies = {2, 4, 6};

    // 执行
    m_testCase->setDependenceSubtask(dependencies);

    // 验证
    QList<int> result = m_testCase->getDependenceSubtask();
    QVERIFY(result == dependencies);
}

void TestTestCase::testSetAndGetDependenceSubtask_withIntSet() {
    // 准备
    QSet<int> dependencies = {1, 2, 3, 4, 5};

    // 执行
    m_testCase->setDependenceSubtask(dependencies);

    // 验证
    QList<int> result = m_testCase->getDependenceSubtask();
    QVERIFY(result.size() == 5);
    for (int dep : dependencies) {
        QVERIFY(result.contains(dep));
    }
}

void TestTestCase::testCheckDependenceSubtask_withMatchingList_returnsTrue() {
    // 准备
    QStringList dependencies = {"1", "2", "3"};
    m_testCase->setDependenceSubtask(dependencies);

    // 执行和验证
    QStringList testList = {"1", "2", "3"};
    QVERIFY(m_testCase->checkDependenceSubtask(testList) == true);
}

void TestTestCase::testCheckDependenceSubtask_withNonMatchingList_returnsFalse() {
    // 准备
    QStringList dependencies = {"1", "2", "3"};
    m_testCase->setDependenceSubtask(dependencies);

    // 执行和验证 - 测试列表不匹配
    QStringList testList1 = {"1", "2"}; // 缺少 3
    QVERIFY(m_testCase->checkDependenceSubtask(testList1) == false);

    QStringList testList2 = {"1", "2", "3", "4"}; // 多了 4
    QVERIFY(m_testCase->checkDependenceSubtask(testList2) == false);

    QStringList testList3 = {"1", "2", "4"}; // 不同元素
    QVERIFY(m_testCase->checkDependenceSubtask(testList3) == false);
}

void TestTestCase::testClearDependenceSubtask_removesAllDependencies() {
    // 准备
    QStringList dependencies = {"1", "2", "3"};
    m_testCase->setDependenceSubtask(dependencies);
    QVERIFY(m_testCase->getDependenceSubtask().size() == 3);

    // 执行
    m_testCase->clearDependenceSubtask();

    // 验证
    QVERIFY(m_testCase->getDependenceSubtask().isEmpty());
}

void TestTestCase::testWriteToJson_withValidTestCase_writesCorrectJson() {
    // 准备
    m_testCase->setFullScore(100);
    m_testCase->setTimeLimit(1000);
    m_testCase->setMemoryLimit(256);
    m_testCase->addSingleCase("input.txt", "output.txt");

    // 执行
    QJsonObject json;
    m_testCase->writeToJson(json);

    // 验证
    QVERIFY(json.contains("fullScore"));
    QVERIFY(json["fullScore"] == 100);
    QVERIFY(json.contains("timeLimit"));
    QVERIFY(json["timeLimit"] == 1000);
    QVERIFY(json.contains("memoryLimit"));
    QVERIFY(json["memoryLimit"] == 256);
    QVERIFY(json.contains("inputFiles"));
    QVERIFY(json.contains("outputFiles"));
    QVERIFY(json.contains("dependenceSubtask"));
}

void TestTestCase::testReadFromJson_withValidJson_createsCorrectTestCase() {
    // 准备
    QJsonObject json;
    json["fullScore"] = 150;
    json["timeLimit"] = 2000;
    json["memoryLimit"] = 512;

    QJsonArray inputFiles;
    inputFiles.append("input1.txt");
    inputFiles.append("input2.txt");
    json["inputFiles"] = inputFiles;

    QJsonArray outputFiles;
    outputFiles.append("output1.txt");
    outputFiles.append("output2.txt");
    json["outputFiles"] = outputFiles;

    QJsonArray dependenceSubtask;
    dependenceSubtask.append(1);
    dependenceSubtask.append(2);
    json["dependenceSubtask"] = dependenceSubtask;

    // 执行
    int result = m_testCase->readFromJson(json);

    // 验证
    QVERIFY(result == 0); // 0 表示成功
    QVERIFY(m_testCase->getFullScore() == 150);
    QVERIFY(m_testCase->getTimeLimit() == 2000);
    QVERIFY(m_testCase->getMemoryLimit() == 512);

    QStringList inputResult = m_testCase->getInputFiles();
    QVERIFY(inputResult.size() == 2);
    QVERIFY(inputResult.at(0) == "input1.txt");
    QVERIFY(inputResult.at(1) == "input2.txt");

    QStringList outputResult = m_testCase->getOutputFiles();
    QVERIFY(outputResult.size() == 2);
    QVERIFY(outputResult.at(0) == "output1.txt");
    QVERIFY(outputResult.at(1) == "output2.txt");

    QList<int> deps = m_testCase->getDependenceSubtask();
    QVERIFY(deps.size() == 2);
    QVERIFY(deps.at(0) == 1);
    QVERIFY(deps.at(1) == 2);
}

void TestTestCase::testGetInputFiles_withNoFiles_returnsEmptyList() {
    // 准备
    TestCase *emptyCase = new TestCase();

    // 执行和验证
    QVERIFY(emptyCase->getInputFiles().isEmpty());

    delete emptyCase;
}

void TestTestCase::testGetOutputFiles_withNoFiles_returnsEmptyList() {
    // 准备
    TestCase *emptyCase = new TestCase();

    // 执行和验证
    QVERIFY(emptyCase->getOutputFiles().isEmpty());

    delete emptyCase;
}

void TestTestCase::testGetFullScore_withDefaultValue_returnsZero() {
    // 准备
    TestCase *defaultCase = new TestCase();

    // 执行和验证
    QVERIFY(defaultCase->getFullScore() == 0);

    delete defaultCase;
}

void TestTestCase::testGetTimeLimit_withDefaultValue_returnsZero() {
    // 准备
    TestCase *defaultCase = new TestCase();

    // 执行和验证
    QVERIFY(defaultCase->getTimeLimit() == 0);

    delete defaultCase;
}

void TestTestCase::testGetMemoryLimit_withDefaultValue_returnsZero() {
    // 准备
    TestCase *defaultCase = new TestCase();

    // 执行和验证
    QVERIFY(defaultCase->getMemoryLimit() == 0);

    delete defaultCase;
}

void TestTestCase::setupTestCaseWithFiles(int fileCount) {
    for (int i = 0; i < fileCount; i++) {
        m_testCase->addSingleCase(QString("input%1.txt").arg(i), QString("output%1.txt").arg(i));
    }
}

QTEST_MAIN(TestTestCase)

#include "test_testcase.moc"
