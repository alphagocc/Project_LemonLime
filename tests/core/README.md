# LemonLime Core 模块单元测试

本目录包含 Project LemonLime 核心模块的单元测试，使用 Qt Test 框架编写。

## 测试类

### Contest 类测试 (`test_contest.cpp`)
- 竞赛标题管理
- 题目管理（添加、删除、交换）
- 参赛者管理
- 评测流程控制
- JSON 序列化/反序列化
- 聚合计算（总时间、总分数）

### Contestant 类测试 (`test_contestant.cpp`)
- 参赛者信息管理
- 多题目成绩跟踪
- 编译状态和消息
- 源码文件管理
- 测试结果查询
- 依赖题目检查

### Task 类测试 (`test_task.cpp`)
- 题目类型（Traditional、AnswersOnly、Interaction、Communication）
- 比较模式（逐行、忽略空格、外部工具、实数、特判）
- 测试用例管理
- 编译器配置
- 文件名设置
- 时间限制和分数计算

### TestCase 类测试 (`test_testcase.cpp`)
- 输入/输出文件管理
- 依赖子任务检查
- 分数、时间限制、内存限制
- 文件交换操作

### JudgingController 类测试 (`test_judgingcontroller.cpp`)
- 任务队列管理
- 并发线程控制
- 评测任务启动/停止
- 信号槽机制验证

## 运行测试

### 构建测试
```bash
# 在项目根目录
mkdir build
cd build
cmake ..
make core_tests
```

### 运行所有测试
```bash
# 方式 1：直接运行测试可执行文件
./tests/core/core_tests

# 方式 2：使用 CTest（推荐）
ctest --output-on-failure

# 方式 3：运行特定测试套件
./tests/core/core_tests contest
./tests/core/core_tests contestant
./tests/core/core_tests task
./tests/core/core_tests testcase
./tests/core/core_tests judgingcontroller
```

### 运行特定测试函数
```bash
./tests/core/core_tests testSetAndGetContestTitle
./tests/core/core_tests testTaskManagement_whenAddTask_thenTaskAdded
```

### 使用命令行选项
```bash
# 详细输出
./tests/core/core_tests -v2

# 仅运行匹配模式的测试
./tests/core/core_tests -contest

# 生成 XML 报告
./tests/core/core_tests -xml test_results.xml

# 跳过已知失败的测试
./tests/core/core_tests -skipblacklisted
```

## 测试数据

测试使用的数据文件位于 `testdata/` 目录：
- `valid_contest.json` - 有效的竞赛配置文件
- `invalid_contest.json` - 无效的竞赛配置文件（用于测试错误处理）

## 测试最佳实践

### 命名规范
- 测试类：`test_ClassName.cpp`
- 测试函数：`testFunctionName_whenExpectedBehavior_thenExpectedResult`

### 断言使用
- 使用 `QVERIFY(condition)` 验证条件
- 使用 `QCOMPARE(actual, expected)` 比较值
- 使用 `QFAIL(message)` 强制失败
- 使用 `QSKIP(message, Abort)` 跳过测试

### 测试生命周期
- `initTestCase()` - 在第一个测试前执行
- `init()` - 在每个测试前执行
- `cleanup()` - 在每个测试后执行
- `cleanupTestCase()` - 在最后一个测试后执行

## 代码覆盖率

可以使用 gcov 或其他工具分析测试覆盖率：
```bash
# 使用 gcov（需要重新编译并启用覆盖率）
cmake -DCMAKE_CXX_FLAGS="-fprofile-arcs -ftest-coverage" ..
make clean
make core_tests
gcov lemon-core source_files
```

## 故障排除

### 测试失败
1. 检查失败消息和堆栈跟踪
2. 验证测试数据和配置
3. 确保依赖库正确链接
4. 查看测试日志获取详细信息

### 常见问题
- **链接错误**：确保 lemon-core 和 lemon-base 库已构建
- **数据文件未找到**：检查测试数据目录是否正确复制
- **Qt 版本不匹配**：确保使用与主项目相同的 Qt 版本

## 贡献指南

编写新测试时请：
1. 遵循现有命名规范
2. 提供清晰的测试描述
3. 使用数据驱动测试测试边界条件
4. 确保测试自包含
5. 添加适当的错误处理
6. 更新此 README 文档

## 许可证

单元测试代码遵循项目主许可证 GPL-3.0-or-later。
