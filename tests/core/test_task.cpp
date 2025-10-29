/*
 * SPDX-FileCopyrightText: 2025 Project LemonLime
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <QJsonArray>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>

#include "settings.h"
#include "task.h"
#include "testcase.h"

class TestTask : public QObject {
	Q_OBJECT

  public:
	explicit TestTask(QObject *parent = nullptr);

  private slots:
	void initTestCase();
	void cleanupTestCase();
	void init();
	void cleanup();

	// 测试基本属性
	void testConstructor_withDefaultParameters_initializesCorrectly();
	void testSetAndGetProblemTitle();
	void testSetAndGetProblemTitle_data();
	void testSetProblemTitle_emitsSignal();

	// 测试题目类型和比较模式
	void testTaskType_enumValues();
	void testComparisonMode_enumValues();
	void testSetAndGetTaskType();
	void testSetAndGetComparisonMode();

	// 测试文件名设置
	void testSetAndGetSourceFileName();
	void testSetAndGetInputFileName();
	void testSetAndGetOutputFileName();

	// 测试检查选项
	void testSetAndGetStandardInputCheck();
	void testSetAndGetStandardOutputCheck();
	void testSetAndGetSubFolderCheck();

	// 测试比较参数
	void testSetAndGetDiffArguments();
	void testSetAndGetRealPrecision();

	// 测试特判和交互
	void testSetAndGetSpecialJudge();
	void testSetAndGetInteractor();
	void testSetAndGetInteractorName();
	void testSetAndGetGrader();

	// 测试编译器配置
	void testSetAndGetCompilerConfiguration();
	void testSetCompilerConfiguration_multipleCompilers();
	void testGetCompilerConfiguration_withNonExistingCompiler_returnsEmpty();

	// 测试文件列表管理
	void testSourceFilesPathAndName_management();
	void testGraderFilesPathAndName_management();
	void testAppendSourceFiles_addsFiles();
	void testAppendGraderFiles_addsFiles();
	void testRemoveSourceFilesAt_removesFile();
	void testRemoveGraderFilesAt_removesFile();

	// 测试用例管理
	void testAddTestCase_addsCaseToEnd();
	void testAddTestCaseWithLocation_insertsCase();
	void testGetTestCase_withValidIndex_returnsCase();
	void testGetTestCase_withInvalidIndex_returnsNull();
	void testDeleteTestCase_withValidIndex_deletesCase();
	void testDeleteTestCase_withInvalidIndex_noChange();
	void testSwapTestCase_withValidIndices_swapsCases();
	void testSwapTestCase_withInvalidIndices_noChange();

	// 测试聚合计算
	void testGetTotalTimeLimit_withNoTestCases_returnsZero();
	void testGetTotalTimeLimit_withMultipleTestCases_returnsCorrectSum();
	void testGetTotalScore_withNoTestCases_returnsZero();
	void testGetTotalScore_withMultipleTestCases_returnsCorrectSum();

	// 测试 JSON 序列化
	void testWriteToJson_withValidTask_writesCorrectJson();
	void testReadFromJson_withValidJson_createsCorrectTask();

	// 测试复制功能
	void testCopyTo_copiesTaskToAnother();

	// 测试答案文件扩展名
	void testSetAndGetAnswerFileExtension();

  private:
	Task *m_task;
	TestCase *m_testCase;

	// 辅助方法
	void createTestTaskWithCases(int caseCount);
};

TestTask::TestTask(QObject *parent) : QObject(parent) {}

void TestTask::initTestCase() { m_task = new Task(); }

void TestTask::cleanupTestCase() { delete m_task; }

void TestTask::init() {
	// 准备初始状态
	m_task->setProblemTitle("Test Problem");
	m_task->setSourceFileName("main.cpp");
	m_task->setInputFileName("input.txt");
	m_task->setOutputFileName("output.txt");
}

void TestTask::cleanup() {
	// 清理测试用例
	while (m_task->getTotalScore() >= 0) {
		m_task->deleteTestCase(0);
	}
}

void TestTask::testConstructor_withDefaultParameters_initializesCorrectly() {
	// 执行
	Task *task = new Task();

	// 验证默认值
	QVERIFY(task->getTaskType() == Task::TaskType::Traditional);
	QVERIFY(task->getComparisonMode() == Task::ComparisonMode::IgnoreSpacesMode);
	QVERIFY(task->getDiffArguments() == "--ignore-space-change --text --brief");
	QVERIFY(task->getRealPrecision() == 3);
	QVERIFY(task->getStandardInputCheck() == false);
	QVERIFY(task->getStandardOutputCheck() == false);
	QVERIFY(task->getSubFolderCheck() == false);

	delete task;
}

void TestTask::testSetAndGetProblemTitle() {
	QFETCH(QString, title);
	QFETCH(QString, expected);

	m_task->setProblemTitle(title);
	QString result = m_task->getProblemTitle();

	QVERIFY2(result == expected, qPrintable(QString("Expected '%1' but got '%2'").arg(expected).arg(result)));
}

void TestTask::testSetAndGetProblemTitle_data() {
	QTest::addColumn<QString>("title");
	QTest::addColumn<QString>("expected");

	QTest::newRow("empty_title") << QString("") << QString("");
	QTest::newRow("simple_title") << QString("A+B Problem") << QString("A+B Problem");
	QTest::newRow("title_with_special_chars") << QString("Problem 1: Test!") << QString("Problem 1: Test!");
	QTest::newRow("title_with_unicode") << QString("题目1：测试") << QString("题目1：测试");
}

void TestTask::testSetProblemTitle_emitsSignal() {
	// 准备
	QSignalSpy spy(m_task, &Task::problemTitleChanged);

	// 执行
	m_task->setProblemTitle("New Title");

	// 验证
	QVERIFY(spy.count() == 1);
	QVERIFY(spy.at(0).at(0).toString() == "New Title");
}

void TestTask::testTaskType_enumValues() {
	// 验证所有枚举值存在
	QVERIFY(Task::TaskType::Traditional >= 0);
	QVERIFY(Task::TaskType::AnswersOnly >= 0);
	QVERIFY(Task::TaskType::Interaction >= 0);
	QVERIFY(Task::TaskType::Communication >= 0);
	QVERIFY(Task::TaskType::CommunicationExec >= 0);
}

void TestTask::testComparisonMode_enumValues() {
	// 验证所有比较模式存在
	QVERIFY(Task::ComparisonMode::LineByLineMode >= 0);
	QVERIFY(Task::ComparisonMode::IgnoreSpacesMode >= 0);
	QVERIFY(Task::ComparisonMode::ExternalToolMode >= 0);
	QVERIFY(Task::ComparisonMode::RealNumberMode >= 0);
	QVERIFY(Task::ComparisonMode::LemonSpecialJudgeMode >= 0);
}

void TestTask::testSetAndGetTaskType() {
	// 执行
	m_task->setTaskType(Task::TaskType::AnswersOnly);

	// 验证
	QVERIFY(m_task->getTaskType() == Task::TaskType::AnswersOnly);

	m_task->setTaskType(Task::TaskType::Interaction);
	QVERIFY(m_task->getTaskType() == Task::TaskType::Interaction);
}

void TestTask::testSetAndGetComparisonMode() {
	// 执行
	m_task->setComparisonMode(Task::ComparisonMode::LineByLineMode);

	// 验证
	QVERIFY(m_task->getComparisonMode() == Task::ComparisonMode::LineByLineMode);

	m_task->setComparisonMode(Task::ComparisonMode::RealNumberMode);
	QVERIFY(m_task->getComparisonMode() == Task::ComparisonMode::RealNumberMode);
}

void TestTask::testSetAndGetSourceFileName() {
	QString testFile = "solution.cpp";
	m_task->setSourceFileName(testFile);
	QVERIFY(m_task->getSourceFileName() == testFile);
}

void TestTask::testSetAndGetInputFileName() {
	QString testFile = "data.in";
	m_task->setInputFileName(testFile);
	QVERIFY(m_task->getInputFileName() == testFile);
}

void TestTask::testSetAndGetOutputFileName() {
	QString testFile = "data.out";
	m_task->setOutputFileName(testFile);
	QVERIFY(m_task->getOutputFileName() == testFile);
}

void TestTask::testSetAndGetStandardInputCheck() {
	// 执行和验证
	m_task->setStandardInputCheck(true);
	QVERIFY(m_task->getStandardInputCheck() == true);

	m_task->setStandardInputCheck(false);
	QVERIFY(m_task->getStandardInputCheck() == false);
}

void TestTask::testSetAndGetStandardOutputCheck() {
	// 执行和验证
	m_task->setStandardOutputCheck(true);
	QVERIFY(m_task->getStandardOutputCheck() == true);

	m_task->setStandardOutputCheck(false);
	QVERIFY(m_task->getStandardOutputCheck() == false);
}

void TestTask::testSetAndGetSubFolderCheck() {
	// 执行和验证
	m_task->setSubFolderCheck(true);
	QVERIFY(m_task->getSubFolderCheck() == true);

	m_task->setSubFolderCheck(false);
	QVERIFY(m_task->getSubFolderCheck() == false);
}

void TestTask::testSetAndGetDiffArguments() {
	QString args = "--ignore-case --brief";
	m_task->setDiffArguments(args);
	QVERIFY(m_task->getDiffArguments() == args);
}

void TestTask::testSetAndGetRealPrecision() {
	// 执行和验证
	m_task->setRealPrecision(5);
	QVERIFY(m_task->getRealPrecision() == 5);

	m_task->setRealPrecision(10);
	QVERIFY(m_task->getRealPrecision() == 10);
}

void TestTask::testSetAndGetSpecialJudge() {
	QString judgeFile = "special_judge.py";
	m_task->setSpecialJudge(judgeFile);
	QVERIFY(m_task->getSpecialJudge() == judgeFile);
}

void TestTask::testSetAndGetInteractor() {
	QString interactorFile = "interactor.cpp";
	m_task->setInteractor(interactorFile);
	QVERIFY(m_task->getInteractor() == interactorFile);
}

void TestTask::testSetAndGetInteractorName() {
	QString name = "Interactive Checker";
	m_task->setInteractorName(name);
	QVERIFY(m_task->getInteractorName() == name);
}

void TestTask::testSetAndGetGrader() {
	QString graderFile = "grader.cpp";
	m_task->setGrader(graderFile);
	QVERIFY(m_task->getGrader() == graderFile);
}

void TestTask::testSetAndGetCompilerConfiguration() {
	// 执行
	m_task->setCompilerConfiguration("gcc", "gcc -std=c++17 -O2");
	m_task->setCompilerConfiguration("g++", "g++ -std=c++17 -O2");

	// 验证
	QVERIFY(m_task->getCompilerConfiguration("gcc") == "gcc -std=c++17 -O2");
	QVERIFY(m_task->getCompilerConfiguration("g++") == "g++ -std=c++17 -O2");
}

void TestTask::testSetCompilerConfiguration_multipleCompilers() {
	// 添加多个编译器配置
	m_task->setCompilerConfiguration("gcc", "gcc -std=c11 -O2");
	m_task->setCompilerConfiguration("g++", "g++ -std=c++17 -O2");
	m_task->setCompilerConfiguration("java", "javac");
	m_task->setCompilerConfiguration("python", "python3");

	// 验证所有配置都正确保存
	QVERIFY(m_task->getCompilerConfiguration("gcc") == "gcc -std=c11 -O2");
	QVERIFY(m_task->getCompilerConfiguration("g++") == "g++ -std=c++17 -O2");
	QVERIFY(m_task->getCompilerConfiguration("java") == "javac");
	QVERIFY(m_task->getCompilerConfiguration("python") == "python3");
}

void TestTask::testGetCompilerConfiguration_withNonExistingCompiler_returnsEmpty() {
	// 执行和验证
	QString result = m_task->getCompilerConfiguration("non_existing_compiler");
	QVERIFY(result == "");
}

void TestTask::testSourceFilesPathAndName_management() {
	// 准备
	QStringList paths = {"/path/to/file1.cpp", "/path/to/file2.cpp"};
	QStringList names = {"file1.cpp", "file2.cpp"};

	// 执行
	m_task->setSourceFilesPath(paths);
	m_task->setSourceFilesName(names);

	// 验证
	QVERIFY(m_task->getSourceFilesPath() == paths);
	QVERIFY(m_task->getSourceFilesName() == names);
}

void TestTask::testGraderFilesPathAndName_management() {
	// 准备
	QStringList paths = {"/path/to/grader.cpp"};
	QStringList names = {"grader.cpp"};

	// 执行
	m_task->setGraderFilesPath(paths);
	m_task->setGraderFilesName(names);

	// 验证
	QVERIFY(m_task->getGraderFilesPath() == paths);
	QVERIFY(m_task->getGraderFilesName() == names);
}

void TestTask::testAppendSourceFiles_addsFiles() {
	// 准备
	QString path1 = "/path/to/file1.cpp";
	QString name1 = "file1.cpp";

	// 执行
	m_task->appendSourceFiles(path1, name1);

	// 验证
	QVERIFY(m_task->getSourceFilesPath().size() == 1);
	QVERIFY(m_task->getSourceFilesName().size() == 1);
	QVERIFY(m_task->getSourceFilesPath().at(0) == path1);
	QVERIFY(m_task->getSourceFilesName().at(0) == name1);

	// 添加第二个文件
	QString path2 = "/path/to/file2.cpp";
	QString name2 = "file2.cpp";
	m_task->appendSourceFiles(path2, name2);

	QVERIFY(m_task->getSourceFilesPath().size() == 2);
	QVERIFY(m_task->getSourceFilesName().size() == 2);
}

void TestTask::testAppendGraderFiles_addsFiles() {
	// 准备
	QString path = "/path/to/grader.cpp";
	QString name = "grader.cpp";

	// 执行
	m_task->appendGraderFiles(path, name);

	// 验证
	QVERIFY(m_task->getGraderFilesPath().size() == 1);
	QVERIFY(m_task->getGraderFilesName().size() == 1);
}

void TestTask::testRemoveSourceFilesAt_removesFile() {
	// 准备
	m_task->appendSourceFiles("/path1/file1.cpp", "file1.cpp");
	m_task->appendSourceFiles("/path2/file2.cpp", "file2.cpp");
	m_task->appendSourceFiles("/path3/file3.cpp", "file3.cpp");

	QVERIFY(m_task->getSourceFilesPath().size() == 3);

	// 执行
	m_task->removeSourceFilesAt(1);

	// 验证 - 第二个文件被删除
	QVERIFY(m_task->getSourceFilesPath().size() == 2);
	QVERIFY(m_task->getSourceFilesPath().at(0) == "/path1/file1.cpp");
	QVERIFY(m_task->getSourceFilesPath().at(1) == "/path3/file3.cpp");
}

void TestTask::testRemoveGraderFilesAt_removesFile() {
	// 准备
	m_task->appendGraderFiles("/path1/grader1.cpp", "grader1.cpp");
	m_task->appendGraderFiles("/path2/grader2.cpp", "grader2.cpp");

	QVERIFY(m_task->getGraderFilesPath().size() == 2);

	// 执行
	m_task->removeGraderFilesAt(0);

	// 验证
	QVERIFY(m_task->getGraderFilesPath().size() == 1);
	QVERIFY(m_task->getGraderFilesName().at(0) == "grader2.cpp");
}

void TestTask::testAddTestCase_addsCaseToEnd() {
	// 准备
	TestCase *testCase1 = new TestCase();
	testCase1->setFullScore(100);

	TestCase *testCase2 = new TestCase();
	testCase2->setFullScore(50);

	// 执行
	m_task->addTestCase(testCase1);
	m_task->addTestCase(testCase2);

	// 验证
	QVERIFY(m_task->getTestCaseList().size() == 2);
	QVERIFY(m_task->getTestCase(0) == testCase1);
	QVERIFY(m_task->getTestCase(1) == testCase2);
}

void TestTask::testAddTestCaseWithLocation_insertsCase() {
	// 准备
	TestCase *testCase1 = new TestCase();
	testCase1->setFullScore(100);

	TestCase *testCase2 = new TestCase();
	testCase2->setFullScore(50);

	TestCase *testCase3 = new TestCase();
	testCase3->setFullScore(75);

	m_task->addTestCase(testCase1);
	m_task->addTestCase(testCase2);

	// 执行 - 在位置 1 插入
	m_task->addTestCase(testCase3, 1);

	// 验证 - testCase3 应该在位置 1，testCase2 在位置 2
	QVERIFY(m_task->getTestCaseList().size() == 3);
	QVERIFY(m_task->getTestCase(0) == testCase1);
	QVERIFY(m_task->getTestCase(1) == testCase3);
	QVERIFY(m_task->getTestCase(2) == testCase2);
}

void TestTask::testGetTestCase_withValidIndex_returnsCase() {
	// 准备
	TestCase *testCase = new TestCase();
	testCase->setFullScore(100);
	m_task->addTestCase(testCase);

	// 执行和验证
	TestCase *result = m_task->getTestCase(0);
	QVERIFY(result == testCase);
}

void TestTask::testGetTestCase_withInvalidIndex_returnsNull() {
	// 执行和验证
	QVERIFY(m_task->getTestCase(-1) == nullptr);
	QVERIFY(m_task->getTestCase(100) == nullptr);
}

void TestTask::testDeleteTestCase_withValidIndex_deletesCase() {
	// 准备
	TestCase *testCase1 = new TestCase();
	testCase1->setFullScore(100);
	m_task->addTestCase(testCase1);

	TestCase *testCase2 = new TestCase();
	testCase2->setFullScore(50);
	m_task->addTestCase(testCase2);

	QVERIFY(m_task->getTestCaseList().size() == 2);

	// 执行
	m_task->deleteTestCase(0);

	// 验证
	QVERIFY(m_task->getTestCaseList().size() == 1);
	QVERIFY(m_task->getTestCase(0) == testCase2);
}

void TestTask::testDeleteTestCase_withInvalidIndex_noChange() {
	// 准备
	TestCase *testCase = new TestCase();
	testCase->setFullScore(100);
	m_task->addTestCase(testCase);

	int initialSize = m_task->getTestCaseList().size();

	// 执行 - 删除无效索引
	m_task->deleteTestCase(100);
	m_task->deleteTestCase(-1);

	// 验证 - 应该没有改变
	QVERIFY(m_task->getTestCaseList().size() == initialSize);
}

void TestTask::testSwapTestCase_withValidIndices_swapsCases() {
	// 准备
	TestCase *testCase1 = new TestCase();
	testCase1->setFullScore(100);
	m_task->addTestCase(testCase1);

	TestCase *testCase2 = new TestCase();
	testCase2->setFullScore(50);
	m_task->addTestCase(testCase2);

	// 执行
	m_task->swapTestCase(0, 1);

	// 验证
	QVERIFY(m_task->getTestCase(0) == testCase2);
	QVERIFY(m_task->getTestCase(1) == testCase1);
}

void TestTask::testSwapTestCase_withInvalidIndices_noChange() {
	// 准备
	TestCase *testCase1 = new TestCase();
	testCase1->setFullScore(100);
	m_task->addTestCase(testCase1);

	TestCase *testCase2 = new TestCase();
	testCase2->setFullScore(50);
	m_task->addTestCase(testCase2);

	// 执行 - 交换无效索引
	m_task->swapTestCase(0, 100);
	m_task->swapTestCase(-1, 1);

	// 验证 - 应该没有改变
	QVERIFY(m_task->getTestCase(0) == testCase1);
	QVERIFY(m_task->getTestCase(1) == testCase2);
}

void TestTask::testGetTotalTimeLimit_withNoTestCases_returnsZero() {
	// 验证没有测试用例时返回 0
	int result = m_task->getTotalTimeLimit();
	QVERIFY(result == 0);
}

void TestTask::testGetTotalTimeLimit_withMultipleTestCases_returnsCorrectSum() {
	// 准备
	createTestTaskWithCases(3);

	// 执行和验证
	// 总时间 = 1000 + 2000 + 1500 = 4500
	int result = m_task->getTotalTimeLimit();
	QVERIFY(result == 4500);
}

void TestTask::testGetTotalScore_withNoTestCases_returnsZero() {
	// 验证没有测试用例时返回 0
	int result = m_task->getTotalScore();
	QVERIFY(result == 0);
}

void TestTask::testGetTotalScore_withMultipleTestCases_returnsCorrectSum() {
	// 准备
	createTestTaskWithCases(3);

	// 执行和验证
	// 总分 = 100 + 50 + 75 = 225
	int result = m_task->getTotalScore();
	QVERIFY(result == 225);
}

void TestTask::testWriteToJson_withValidTask_writesCorrectJson() {
	// 准备
	m_task->setProblemTitle("Test Problem");
	m_task->setTaskType(Task::TaskType::AnswersOnly);
	m_task->setSourceFileName("main.cpp");

	TestCase *testCase = new TestCase();
	testCase->setFullScore(100);
	testCase->setTimeLimit(1000);
	testCase->addSingleCase("input.txt", "output.txt");
	m_task->addTestCase(testCase);

	// 执行
	QJsonObject json;
	m_task->writeToJson(json);

	// 验证
	QVERIFY(json.contains("problemTitle"));
	QVERIFY(json.contains("taskType"));
	QVERIFY(json.contains("sourceFileName"));
	QVERIFY(json.contains("testCaseList"));
}

void TestTask::testReadFromJson_withValidJson_createsCorrectTask() {
	// 准备
	QJsonObject json;
	json["problemTitle"] = "Test Problem";
	json["taskType"] = 0; // Traditional
	json["sourceFileName"] = "main.cpp";
	json["inputFileName"] = "input.txt";
	json["outputFileName"] = "output.txt";
	json["comparisonMode"] = 1; // IgnoreSpacesMode
	json["diffArguments"] = "--ignore-space-change --text --brief";
	json["realPrecision"] = 3;
	json["standardInputCheck"] = false;
	json["standardOutputCheck"] = false;
	json["subFolderCheck"] = false;
	json["specialJudge"] = "";
	json["interactor"] = "";
	json["interactorName"] = "";
	json["grader"] = "";
	json["answerFileExtension"] = ".out";
	json["sourceFilesPath"] = QJsonArray();
	json["sourceFilesName"] = QJsonArray();
	json["graderFilesPath"] = QJsonArray();
	json["graderFilesName"] = QJsonArray();

	QJsonArray testCaseList;
	QJsonObject testCaseObj;
	testCaseObj["index"] = 0;
	testCaseObj["fullScore"] = 100;
	testCaseObj["timeLimit"] = 1000;
	testCaseObj["memoryLimit"] = 256;
	testCaseObj["inputFiles"] = QJsonArray() << "input.txt";
	testCaseObj["outputFiles"] = QJsonArray() << "output.txt";
	testCaseObj["dependenceSubtask"] = QJsonArray();
	testCaseList.append(testCaseObj);
	json["testCaseList"] = testCaseList;

	// 执行
	int result = m_task->readFromJson(json);

	// 验证
	QVERIFY(result == 0); // 0 表示成功
	QVERIFY(m_task->getProblemTitle() == "Test Problem");
	QVERIFY(m_task->getTaskType() == Task::TaskType::Traditional);
	QVERIFY(m_task->getTestCaseList().size() == 1);
}

void TestTask::testCopyTo_copiesTaskToAnother() {
	// 准备
	m_task->setProblemTitle("Original Problem");
	m_task->setSourceFileName("original.cpp");

	TestCase *testCase = new TestCase();
	testCase->setFullScore(100);
	m_task->addTestCase(testCase);

	// 执行
	Task *newTask = new Task();
	m_task->copyTo(newTask);

	// 验证
	QVERIFY(newTask->getProblemTitle() == "Original Problem");
	QVERIFY(newTask->getSourceFileName() == "original.cpp");
	QVERIFY(newTask->getTestCaseList().size() == 1);

	delete newTask;
}

void TestTask::testSetAndGetAnswerFileExtension() {
	QString extension = ".ans";
	m_task->setAnswerFileExtension(extension);
	QVERIFY(m_task->getAnswerFileExtension() == extension);
}

void TestTask::createTestTaskWithCases(int caseCount) {
	for (int i = 0; i < caseCount; i++) {
		TestCase *testCase = new TestCase();
		testCase->setFullScore(25 * (i + 1));  // 25, 50, 75, ...
		testCase->setTimeLimit(500 * (i + 1)); // 500, 1000, 1500, ...
		testCase->setMemoryLimit(256);
		testCase->addSingleCase(QString("input%1.txt").arg(i), QString("output%1.txt").arg(i));
		m_task->addTestCase(testCase);
	}
}

QTEST_MAIN(TestTask)

#include "test_task.moc"
