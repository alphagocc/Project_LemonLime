# AGENTS.md

## Project Overview

Project LemonLime 是一个面向 OI（信息学奥林匹克）竞赛的轻量级评测系统，基于 Lemon + LemonPlus 开发。支持 Linux、Windows、macOS 三平台。

- **Qt 版本**: Qt 6.8 或更高（可通过 `-DLEMON_QT_MAJOR_VERSION=<6|7>` 指定主版本）
- **Qt 模块**: Core, Gui, Widgets（核心），LinguistTools（翻译），AxContainer（仅 Windows XLS 导出）
- **C++ 标准**: C++17（`CMAKE_CXX_STANDARD 17`，无扩展）
- **第三方依赖**: SingleApplication（单实例保护），spdlog（日志系统），均作为 git submodule 在 `3rdparty/` 下
- **许可证**: GPL-3.0-or-later

## Build & Run

### 依赖

- CMake ≥ 3.16
- Qt 6.8+（需要 Core, Gui, Widgets, LinguistTools 模块）
- C++17 兼容的编译器（MSVC / GCC / Clang）
- Ninja（推荐）或 Make

### 克隆

```bash
git clone https://github.com/Project-LemonLime/Project_LemonLime.git --recursive
```

### 构建（通用）

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -GNinja
cmake --build build --parallel
```

### CMake 选项

| 选项                     | 默认值 | 说明                       |
| ------------------------ | ------ | -------------------------- |
| `EMBED_TRANSLATIONS`     | `ON`   | 嵌入翻译文件到二进制       |
| `EMBED_DOCS`             | `ON`   | 嵌入手册文档               |
| `ENABLE_XLS_EXPORT`      | `OFF`  | XLS 导出支持（仅 Windows） |
| `ENABLE_LTO`             | `ON`   | 链接时优化                 |
| `ENABLE_CCACHE`          | `OFF`  | ccache 加速编译            |
| `BUILD_DEB`              | `OFF`  | 构建 DEB 包                |
| `BUILD_RPM`              | `OFF`  | 构建 RPM 包                |
| `LEMON_QT_MAJOR_VERSION` | `6`    | Qt 主版本号                |
| `LEMON_QT_MIN_VERSION`   | `6.8`  | 最低 Qt 版本               |
| `LEMON_BUILD_INFO`       | 空     | 自定义构建信息字符串       |
| `LEMON_CONFIG_DIR`       | 空     | 自定义配置目录             |

### 版本信息

版本号存储在 `makespec/VERSION`（如 `0.3.6`），后缀存储在 `makespec/VERSIONSUFFIX`（如 `.1`）。构建版本自动取 git short hash。

## Architecture

### 目录结构

```
Project_LemonLime/
├── src/
│   ├── base/           # 基础设施层（静态库 lemon-base）
│   ├── core/           # 核心业务逻辑（静态库 lemon-core，依赖 lemon-base）
│   ├── component/      # 组件（exportutil 导出工具）
│   ├── forms/          # Qt Designer .ui 文件（26 个）
│   ├── main.cpp        # 入口
│   ├── pch.h           # 预编译头（QtCore + QtGui）
│   ├── lemon.h/.cpp    # 主窗口 LemonLime : QMainWindow
│   └── *.h/*.cpp       # UI 层各对话框和控件
├── 3rdparty/
│   ├── SingleApplication/  # 单实例应用
│   └── spdlog/             # 日志库
├── cmake/              # CMake 模块文件
│   ├── lemon-base.cmake
│   ├── lemon-core.cmake
│   ├── lemon-ui.cmake
│   ├── LemonDocs.cmake
│   ├── LemonTranslations.cmake
│   ├── deployment.cmake
│   └── platforms/      # 平台安装配置
├── translations/       # 翻译文件（en_US, zh_CN, zh_TW）
├── assets/             # 图标、图片资源
├── manual/             # 用户手册
├── unix/               # Unix 平台 watcher 程序
├── makespec/           # 版本号、打包 spec 文件
└── .github/workflows/  # CI 工作流
```

### 三层架构

```
┌─────────────────────────────┐
│       UI Layer (exe)        │  主窗口、对话框、自定义控件
│  LemonLime, JudgingDialog,  │  直接链接 lemon-core, lemon-base
│  ResultViewer, SummaryTree  │
├─────────────────────────────┤
│     Core Layer (lemon-core) │  评测逻辑、比赛/选手/题目管理
│  Contest, Task, Contestant, │
│  JudgingThread, TaskJudger, │
│  JudgingController          │
├─────────────────────────────┤
│    Base Layer (lemon-base)  │  编译器抽象、全局设置、工具函数
│  Settings, Compiler,        │  日志、配置、翻译、JSON 工具
│  LemonLog, LemonConfig      │
└─────────────────────────────┘
```

### 核心数据模型

```
Contest (QObject)
├── Settings*
├── QList<Task*>
│   └── Task (QObject)
│       ├── QList<TestCase*>
│       │   └── TestCase (非 QObject)
│       └── TaskType: Traditional | AnswersOnly | Interaction | Communication
└── QMap<QString, Contestant*>
    └── Contestant (QObject)
        └── 存储每题的编译状态、评测结果、得分、时间、内存
```

### 评测流程

1. `Contest::judge()` 创建 `TaskJudger` 并提交给 `JudgingController`
2. `JudgingController` 管理线程池（`QThread` + `QQueue`），按 `maxJudgingThreads` 并行评测
3. `TaskJudger` 编译源代码，然后为每个测试点创建 `JudgingThread`
4. `JudgingThread` 继承 `QThread`，负责运行程序并对比输出
5. 结果通过信号链逐级回传至 UI

### Unix Watcher

在 Linux/macOS 平台，会编译一个单独的 `watcher_unix` 可执行文件（包含 `watcher_unix.cpp` + 平台相关的 `watcher_linux.cpp` 或 `watcher_macos.mm`），用于监控被评测程序的资源使用（时间/内存），嵌入 `watcher.qrc` 资源到主程序。

### Windows Sandbox

Windows AppContainer 沙箱作为实验性配置，默认关闭，按编译器显式启用。`SandboxSettings.enabled` 默认为 `false`，旧配置缺少该字段时同样关闭。两种模式共用 `WinProcessRunner::run()` 中的进程启动、监控、计量和错误处理。关闭时沿用原有进程环境；启用时由 `WindowsSandbox` 准备权限、AppContainer 属性和 Job，Job 仅允许一个活动进程，禁止提交程序创建子进程。

启用时，`WinProcessRunner` 在 AppContainer 中执行提交程序，使用独立 Package SID、私有工作目录、标准流句柄白名单和 Job Object。`WindowsSandbox` 根据 `SandboxSettings` 发现 C、C++、Java、Python 运行环境，使用 `QDir::canonicalPath()` 规范化配置的目录名，运行环境中的目录连接由 Windows 文件接口跟随。运行目录与比赛数据、工作目录可以重叠，程序按配置准备只读权限。运行环境授权及发现结果仅在当前活动评测会话的内存中复用。`TaskJudger::judge()` 持有会话，结束或取消后释放；并发任务共用会话，最后一个使用者退出时撤销本会话新增的运行环境授权。会话使用独立命名 capability SID，避免影响其他进程的授权。禁止对整个宿主 PATH 重复设置继承 ACL。

运行目录的现有文件逐项授权；运行环境目录句柄使用 `MAXIMUM_ALLOWED` 抑制 `SetSecurityInfo` 的递归传播；工作目录和文件句柄请求元数据及 DACL 权限，工作目录保留 Windows 的 ACL 继承行为，文件操作兼容正在使用的 DLL。运行环境与工作文件共用文件检查和权限合并实现，在原 DACL 上追加本次沙箱权限，保留原有用户及其他主体的权限项和保护状态。首次准备默认预算 15 秒，准备过程仅检查时间预算，停止标记由进程运行器处理。准备进度回调、信号转发及指向当前运行线程的取消界面连接移除；整体评测的取消入口按原有定义停止后续测试点。运行环境授权逐项设置为非继承 ACE，并在内存中记录，以便完整撤销。准备与撤销通过进程内互斥锁协调；准备失败时撤销该阶段新增的授权。工作文件准备与撤销共用权限互斥锁，准备按先子项后父目录的顺序追加授权；工作目录的 Package SID 权限允许新建对象继承，进程结束后撤销。具名输入使用工作目录中的独立副本，与其他工作文件统一授予读写、执行和删除权限，保留原 DACL 的继承与保护状态，省去输入文件安全描述符快照和继承标记恢复。标准输入仍通过原始输入文件的只读句柄提供。清理基于现行 DACL 删除本次 SID 的允许项，保留其他主体及其他活动任务后来追加的授权。沙箱以调用方预先创建工作目录为前提，沿用 `config.workingDirectory`；工作目录由 `TaskJudger` 现有的 `QTemporaryDir` 生命周期统一删除。运行环境和 Python 探测记录仅存在于会话内存中，禁止写入缓存文件或文件锁。Java 和 Python 的环境发现分别由独立函数实现，`discover()` 分派策略，将发现的目录规范化并保留大小写，再调用 `removeDuplicates()` 精确去重，授权阶段使用其返回结果。缓存键、能力标识及目录包含判断同样保留大小写差异。Python 探测使用普通 QProcess，准备超时返回时由其析构函数终止探测进程并等待退出；AppContainer、Job 和标准流句柄白名单仅用于提交程序。

`hasGrant()` 按 ACE 顺序检查剩余请求权限，允许项提供全部所需权限时结束检查。`ALL APPLICATION PACKAGES` SID 在会话内复用。工作文件授权省去当前用户 SID 查询、SDDL 模板及整份私有 DACL 构造。能力 SID 由独占所有权对象管理，进程属性数组仅引用它们。`quoteArgument()` 保存在测试源文件中，`errorText()` 保存在沙箱实现的匿名命名空间中。共享头文件 `windowsprocessutils.h` 提供 `Handle`、`LocalDeleter`、`LocalMemory` 和 `wide()`；生产代码与测试中要求 `LocalFree()` 释放的独立内存统一由 `LocalMemory` 管理，SID 数组由 `SidArray` 逐项释放，Package SID 使用 `FreeSid()`。`CreateProcessW()` 使用可写命令行缓冲区，失败后立即保存系统错误码。

Windows 资源包装采用 `LocalMemory<Pointer>`，模板参数使用 Windows 指针类型，声明中保留 `PSECURITY_DESCRIPTOR`、`PACL`、`PSID` 和 `LPWSTR` 等类型信息。通过 `put()` 接收 API 输出、`get()` 访问资源、`release()` 转移所有权；`Handle` 同样提供 `put()`。`SidArray` 将数组和计数保存在私有成员中，通过输出地址接口接收能力 SID。沙箱准备由 `prepare()` 创建 AppContainer 身份，再完成运行环境发现、授权和私有文件授权。`preparePrivateFiles()` 仅处理工作文件，按子项先于父目录的顺序使用范围 for 授权。运行环境授权阶段在函数作用域中管理互斥锁与回滚守卫，成功后解除回滚。

启用沙箱时，自动、本机程序、Java 和 Python 策略统一设置 Job 的 `ActiveProcessLimit = 1`，禁止提交程序创建子进程。运行器沿用 `TerminateProcess()` 终止主进程并等待退出；`stopProcesses()` 及进程树轮询、终止接口移除。Job 保留 `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`，在句柄关闭时终止仍在运行的进程。需要启动子进程的 Python 启动器和 Windows venv 会执行失败，应配置基础 Python 等单进程运行入口；执行时保留用户配置的启动程序，程序本身不会自动替换入口。可信 Python 环境探测仍使用宿主 QProcess。

两种执行模式均保留原有计量：返回主进程用户态时间与峰值工作集，内存限制检查主进程 `PrivateUsage` 与 `PeakWorkingSetSize` 的较大值。Job 不参与时间和内存计量，也不设置总内存配额。运行监控保留 10 毫秒等待间隔，输出结果判断保留在评测层。关闭标准流重定向时保留空句柄，继承白名单仅包含实际打开的标准流。AppContainer 和 Job 在创建进程时通过属性配置，沿用原有启动标志。沙箱使用当前测试点工作目录，临时目录和用户目录环境变量保留用户显式配置，省去额外辅助目录及对应的环境变量覆盖。`LOCALAPPDATA` 缺省时设为当前测试点工作目录，以满足本机验证中 Windows 创建 AppContainer 进程的要求。Python 输出编码、用户包加载和字节码缓存遵循解释器默认行为及用户显式配置。取消返回值、运行错误信息和启动优先级沿用原有定义。沙箱功能应保持上述评测行为，其他行为调整须取得用户明确授权。编译阶段和检查器当前仍使用宿主权限。

高级编译器设置提供默认未勾选的“实验性 Windows 沙箱”开关，启用后可以选择自动、本机程序、Java、Python 策略，额外只读目录和准备时间预算。配置通过 `Compiler` 的 JSON 字段 `windowsSandbox` 及 QSettings 的 `WindowsSandbox` 字段保存。

## Qt Conventions

### 信号槽风格

**100% 使用新式（C++11 函数指针）连接**，项目中无任何 `SIGNAL()`/`SLOT()` 宏的使用。

```cpp
// 典型连接方式（摘自 taskeditwidget.cpp）
connect(ui->problemTitle, &QLineEdit::textChanged, this, &TaskEditWidget::problemTitleChanged);
connect(ui->comparisonMode, qOverload<int>(&QComboBox::currentIndexChanged), this,
        &TaskEditWidget::comparisonModeChanged);
```

对于有重载的信号，使用 `qOverload<>()` 消歧。

### 内存管理

- **Qt 对象树机制**为主：大部分 QObject 派生类通过 `parent` 参数管理生命周期
- `JudgingController` 中手动 `new QThread` + `delete thread` / `delete taskJudger`
- `LemonTranslator` 使用 `std::unique_ptr<QTranslator>`
- `spdlog::logger` 使用 `std::shared_ptr`
- `TestCase` 不继承 QObject，存储在 `QList<TestCase*>` 中

```cpp
// 手动线程管理示例（摘自 judgingcontroller.cpp）
QThread *thread = new QThread;
taskJudger->moveToThread(thread);
// ...
thread->quit();
thread->wait();
delete thread;
delete taskJudger;
```

### 多线程

- `JudgingThread` 继承 `QThread`（重写 `run()`）
- `JudgingController` 使用 **moveToThread 模式**：创建 `QThread` → `moveToThread()` → `QMetaObject::invokeMethod()` 调用
- 通过 `maxJudgingThreads` 设置控制最大并发线程数

### 资源管理

- `resource.qrc`：图标、logo、版本文件等
- `translations/translations.qrc`：翻译文件（可嵌入或外置）
- `manual/manual.qrc`：用户手册（可嵌入）
- `unix/watcher.qrc`：watcher 二进制（仅 Unix）

### 国际化

- 面向 GUI 的文本使用 `tr()` 和 `QObject::tr()` 标记；仅供日志使用的内部诊断保持普通字符串
- 翻译文件：`translations/zh_CN.ts`、`translations/zh_TW.ts`、`translations/en_US.ts`
- `LemonTranslator` 类管理翻译加载，搜索多个路径（含 Snap/AppImage 支持）
- 非 QObject 类使用 `Q_DECLARE_TR_FUNCTIONS` 宏（如 `Settings`）
- CMake 通过 `qt_add_translation()` 编译 `.ts` → `.qm`

## C++ Code Style

### 格式化

使用 `.clang-format` 配置（CI 中强制检查）：

```yaml
BasedOnStyle: LLVM
BreakBeforeBraces: Attach
IndentWidth: 4
TabWidth: 4
UseTab: ForIndentation # 使用 Tab 缩进
ColumnLimit: 110
ContinuationIndentWidth: 4
IndentCaseLabels: true
NamespaceIndentation: All # 命名空间内容也缩进
SpaceAfterLogicalNot: true # "! expr" 而非 "!expr"
```

### 头文件保护

**100% 使用 `#pragma once`**，无传统 include guard。

### 命名约定

- **类名**: PascalCase（`JudgingThread`, `TaskJudger`, `LemonBaseApplication`）
- **成员变量**: camelCase，**无前缀**（`contestTitle`, `compilerList`, `isJudging`）
- **方法名**: camelCase（`getContestTitle()`, `setCompilerType()`, `judgeAll()`）
- **枚举值**: PascalCase（`CompileSuccessfully`, `TimeLimitExceeded`, `Traditional`）
- **命名空间**: PascalCase（`Lemon::base::config`，`Lemon::detail`，`Lemon::common`）
- **宏/常量**: 全大写或 PascalCase（`LEMON_MODULE_NAME`, `MagicNumber`, `maxDependValue`）
- **Getter/Setter**: `getXxx()` / `setXxx()` 风格

### 前向声明

广泛使用前向声明减少头文件依赖：

```cpp
// 摘自 contest.h
class Task;
class Settings;
class Contestant;
class JudgingController;
```

### 命名空间

项目使用 `Lemon` 作为顶级命名空间，子空间包括 `Lemon::base`、`Lemon::base::config`、`Lemon::detail`、`Lemon::common`。旧代码中的核心类（`Contest`, `Task` 等）不在命名空间内。

### UI 命名空间

Qt Designer 生成的类放在 `Ui` 命名空间中：

```cpp
namespace Ui {
    class LemonLime;
}
```

### const 正确性

Getter 方法一致使用 `const` 修饰，但返回引用时使用 `const Type&`：

```cpp
const QString &getContestTitle() const;
const QList<Task *> &getTaskList() const;
```

### 构造函数

QObject 派生类统一使用 `explicit` 修饰和默认 `parent = nullptr` 参数：

```cpp
explicit Contest(QObject *parent = nullptr);
explicit JudgingThread(QObject *parent = nullptr);
```

### QT_NO_FOREACH

项目定义了 `-DQT_NO_FOREACH`，禁止使用 Qt 的 `foreach` 宏，统一使用标准 C++ 范围 for 循环。

## Key Patterns

### JSON 序列化

所有数据模型类实现 `read(const QJsonObject&)` / `write(QJsonObject&) const` 方法对，通过 `LemonUtils.hpp` 中的模板工具函数实现：

```cpp
// LemonUtils.hpp 提供的宏（摘自实际代码）
#define READ_JSON(json, x) Lemon::readJson(x, #x, json)
#define WRITE_JSON(json, x) Lemon::writeJson(x, #x, json)
```

底层使用 SFINAE 模板 (`std::enable_if_t`) 支持多种类型（`int`, `bool`, `double`, `QString`, `QJsonArray`, `QList<T>`, 自定义枚举等）的自动序列化/反序列化。

### 日志系统

基于 spdlog，通过模块化宏封装：

```cpp
// 每个 .cpp 文件开头定义模块名
#define LEMON_MODULE_NAME "JudgingController"

// 使用宏打日志
LOG("Starting judge for", contestantName);
WARN("Compile failed:", message);
DEBUG("Score:", score, "Time:", timeUsed);
```

日志同时输出到 console（warn 级别以上）和 daily file（trace 级别）。日志文件保留最近 30 天。

### 预编译头

`src/pch.h` 包含 `<QtCore>` 和 `<QtGui>`，在所有三个编译目标（`lemon-base`, `lemon-core`, 主 `lemon`）中使用。

### 应用初始化

```cpp
// main.cpp 中的启动流程
initLogger();                          // 初始化 spdlog
Lemon::LemonBaseApplication app(...);  // 继承 SingleApplication（单实例）
app.Initialize();                      // 解析命令行、初始化翻译
if (app.sendMessage("")) { ... }       // 已有实例则激活已有窗口
LemonLime w;                           // 创建主窗口
screen.show(); ... screen.finish(&w);  // 启动画面
w.show(); w.welcome();                 // 显示欢迎对话框
```

### UI 结构

- `.ui` 文件定义在 `src/forms/` 目录（26 个文件）
- CMake 自动处理 `AUTOUIC`（搜索路径设为 `src/forms`）、`AUTOMOC`、`AUTORCC`
- UI 类以 `Ui::ClassName` 命名空间持有指针

### 评测结果状态机

```cpp
// 摘自 LemonType.hpp
enum ResultState {
    CorrectAnswer, WrongAnswer, PartlyCorrect,
    TimeLimitExceeded, MemoryLimitExceeded,
    CannotStartProgram, FileError, RunTimeError,
    InvalidSpecialJudge, SpecialJudgeTimeLimitExceeded,
    SpecialJudgeRunTimeError, Skipped,
    InteractorError, PresentationError, OutputLimitExceeded,
    LastResultState
};
```

### 子任务依赖

`subtaskdependencelib.h` 中定义了子任务依赖得分计算逻辑（`stateToStatus` / `statusToScore`），使用百万分比精度（`maxDependValue = 1000000`）。

## Testing

- **测试框架**: Qt Test，通过 CTest 执行
- `tests/test1/` 包含比赛评测集成测试
- `tests/windows-sandbox/` 包含 Windows 沙箱隔离、句柄、权限缓存、资源限制、取消和语言兼容测试，仅在 Windows 构建；C、C++、Python 和 Java 测试使用本机安装的工具，缺少对应工具时跳过该项
- `unix/test/` 目录包含 watcher 相关的测试 CMakeLists 和测试程序

## Important Notes

### 平台兼容性

- **Windows**: 实验性 AppContainer 沙箱默认关闭，可按编译器启用。启用时所有运行策略均由 Job Object 限制为一个活动进程，禁止提交程序创建子进程；时间和内存保持原有主进程计量。需要子进程的 Python 启动器和 Windows venv 会执行失败，应配置单进程运行入口。CI 使用 MSVC。启用沙箱后的准备需要所选运行目录具有适当的读取与执行授权，或允许当前用户配置该授权；失败时报告原因并停止该次运行。
- **macOS**: 需使用 `watcher_macos.mm`（Objective-C++）编译 watcher，否则内存限制功能异常。Apple Silicon 不保证评测稳定性。
- **Linux**: 默认栈空间与内存限制相同。watcher 使用 `watcher_linux.cpp`。静态编译为推荐分发方式。

### 禁止的写法

- 不得使用 `foreach` 宏（已通过 `QT_NO_FOREACH` 编译定义禁止）
- 不得使用旧式 `SIGNAL()`/`SLOT()` 宏连接信号槽
- 代码必须通过 `clang-format` 检查（CI 自动执行）

### Commit 规范

使用 [Conventional Commits](https://www.conventionalcommits.org/) 规范。

**格式**：

```
<type>(<scope>): <subject>

<body>
```

**type 必须是以下之一**：

| type       | 说明                       |
| ---------- | -------------------------- |
| `feat`     | 新功能                     |
| `fix`      | 修复 bug                   |
| `refactor` | 重构（不改变功能）         |
| `style`    | 代码格式调整（不影响逻辑） |
| `docs`     | 文档变更                   |
| `build`    | 构建系统或依赖变更         |
| `ci`       | CI 配置变更                |
| `perf`     | 性能优化                   |
| `test`     | 测试相关                   |
| `chore`    | 其他杂项                   |

**规则**：

- **提交前必须运行 clang-format**，确保代码格式符合 `.clang-format` 配置，避免 CI 格式检查失败：
  - **bash / Git Bash**：
    ```bash
    clang-format -i $(git diff --name-only --cached -- '*.cpp' '*.h' '*.hpp')
    ```
  - **PowerShell**：
    ```powershell
    git diff --name-only --cached -- '*.cpp', '*.h', '*.hpp' | ForEach-Object { clang-format -i $_ }
    ```
- **首行（subject line）不得超过 72 个字符**，保持简短概括
- `scope` 可选，用于标注影响范围（如 `feat(judging):`、`fix(export):`）
- 首行使用英文小写开头，不加句号
- 详细说明放到 body 中（空一行后书写），或在代码注释中说明
- 破坏性变更需在 body 中添加 `BREAKING CHANGE:` 前缀
- **AGENTS.md 维护**：当代码中涉及本文档描述的约定、架构、构建方式、代码风格等内容发生变更时，须同步更新 `AGENTS.md`
- **Commit 拆分**：`AGENTS.md` 的变更必须作为**独立 commit** 提交，不得与其他代码或文档变更混在同一个 commit 中（便于 cherry-pick 到 master 分支）

**示例**：

```
feat(judging): add subtask dependency support

Implement subtask dependency checking in TaskJudger.
Each test case can now specify dependent subtasks that
must pass before it is evaluated.
```

```
fix(export): correct HTML encoding for CJK characters
```

```
refactor: extract JSON serialization helpers to LemonUtils
```

### CI 工作流

| 工作流                 | 说明                         |
| ---------------------- | ---------------------------- |
| `windows-qt6.yml`      | Windows MSVC + Qt 6.9.3 构建 |
| `linux-static-qt6.yml` | Linux 静态链接 Qt6 构建      |
| `macos-qt6.yml`        | macOS Qt6 构建               |
| `cpack-deb-debian.yml` | Debian DEB 包构建            |
| `check_format.yml`     | 代码格式检查（clang-format） |

### 文件编码

- 源文件使用 CRLF 行尾（Windows 风格，通过 `.gitattributes` 管理）
- 注释和文档包含中文

### SPDX 许可证头

所有源文件以 SPDX 格式的许可证头开始：

```cpp
/*
 * SPDX-FileCopyrightText: 2019-2022 Project LemonLime
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */
```
