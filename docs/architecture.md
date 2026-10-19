# 项目架构

LemonLime 采用 Qt 桌面界面、核心评测逻辑与基础设施三层结构。构建配置见 [构建与 CI](build-and-ci.md)，开发风格见 [编码约定](coding-conventions.md)。

## 目录与目标

| 目录或文件 | 职责 |
| --- | --- |
| `src/base/` | 编译器抽象、设置、日志、配置与工具函数，生成 `lemon-base` |
| `src/core/` | 比赛、题目、选手、评测控制与执行，生成 `lemon-core` |
| `src/component/` | exportutil 等组件 |
| `src/forms/` | Qt Designer 界面定义 |
| `src/main.cpp` | 应用入口 |
| `src/lemon.h`、`src/lemon.cpp` | `LemonLime` 主窗口 |
| `src/` 其他界面文件 | 对话框与自定义控件 |
| `src/plugins/` | SVG 图标插件 |
| `3rdparty/` | SingleApplication 与 spdlog 子模块 |
| `cmake/` | 三层目标、手册、翻译、打包及平台配置 |
| `translations/` | 英文、简体中文与繁体中文翻译 |
| `assets/` | 图标、图片等资源 |
| `manual/` | 用户手册 |
| `unix/` | Unix watcher |
| `makespec/` | 版本号与打包配置 |
| `.github/workflows/` | CI 配置 |

主可执行文件 `lemon` 编译界面层和应用资源，链接 `lemon-core` 与 `lemon-base`；`lemon-core` 依赖 `lemon-base`。基础层和核心层的源文件及 moc 生成代码分别由对应静态库编译一次，主程序和测试通过链接复用。

界面层的主要类包括 `LemonLime`、`JudgingDialog`、`ResultViewer` 与 `SummaryTree`；核心层包括 `Contest`、`Task`、`Contestant`、`TaskJudger`、`JudgingThread` 与 `JudgingController`；基础层包括 `Settings`、`Compiler`、`LemonLog` 与 `LemonConfig`。

## 数据模型

```text
Contest : QObject
├── Settings*
├── QList<Task*>
│   └── Task : QObject
│       └── QList<TestCase*>
└── QMap<QString, Contestant*>
    └── Contestant : QObject
```

`Task` 支持 `Traditional`、`AnswersOnly`、`Interaction`、`Communication` 四种题型。`TestCase` 为普通 C++ 类，由 `QList<TestCase*>` 保存；`Contestant` 保存各题编译状态、评测结果、得分、时间与内存。

旧核心类保留在全局命名空间；新的基础设施主要位于 `Lemon` 及其子命名空间。QObject 优先由父子对象树管理。

## 评测与线程

1. `Contest::judge()` 创建 `TaskJudger`，提交给 `JudgingController`。
2. `JudgingController` 使用 `QThread` 和 `QQueue` 管理并行任务，并发数量受 `maxJudgingThreads` 控制。
3. `TaskJudger` 编译源代码，并为各测试点创建 `JudgingThread`。
4. `JudgingThread` 重写 `QThread::run()`，运行提交程序并比较输出。
5. 评测结果通过逐级信号传递至界面。

`JudgingController` 以 `moveToThread()` 将任务对象移到新线程，通过 `QMetaObject::invokeMethod()` 调用。结束时依次退出、等待并释放线程，再释放 `TaskJudger`。

`Settings` 优先读取 `GeneralSettings/MaximumJudgingThreads` 中保存的设置。该项缺失时，由 `src/base/cputopology.cpp` 检测物理核心并计算默认值：同时具有 P-core 与 E-core 的处理器采用 P-core 数量；同构处理器采用全部物理核心数量的四分之三，向下取整且至少为 1。仅有 E-core 的处理器也按同构计算，例如 N100 的 4 个 E-core 对应 3 个评测线程。SMT 逻辑线程归属于同一个物理核心。

| 平台 | 核心检测方式 |
| --- | --- |
| Windows | `GetLogicalProcessorInformationEx(RelationProcessorCore)` 统计物理核心，`EfficiencyClass` 的最高性能类别数量用于异构计算 |
| macOS | `hw.physicalcpu` 统计物理核心，存在多个性能类别时读取 `hw.perflevel0.physicalcpu` |
| Linux | sysfs 的同核兄弟清单合并逻辑线程，过滤离线 CPU；Intel 使用 `cpu_core/cpus` 识别 P-core，ARM 使用完整的 `cpu_capacity` 信息区分性能类别 |

只有最高性能类别的核心数量介于 1 和全部物理核心数量之间时，才采用异构规则。核心类别信息缺失时按同构计算；物理核心信息缺失时，使用 `QThread::idealThreadCount()` 返回的逻辑处理器数计算四分之三，结果至少为 1。自动值仅控制并行评测数量，评测进程的 CPU 调度由操作系统负责。

开启调试日志后，`CpuTopology` 模块记录核心检测来源、核心数量、分类结果、系统接口失败信息和默认线程数的计算依据；`Setting` 模块记录实际采用的线程数，以及该值来自保存的设置还是自动计算。

`TaskJudger` 在复制测试文件和创建评测线程前检查测试点工作目录。创建失败时记录 `FileError` 与零分，报告目录错误，保持原有子任务依赖和后续独立子任务规则。普通模式与 Windows 沙箱模式共用该检查。

`subtaskdependencelib.h` 使用 `stateToStatus()` 与 `statusToScore()` 计算子任务依赖得分，精度为百万分比，`maxDependValue = 1000000`。

评测状态定义位于 `LemonType.hpp`，包括正确、错误、部分正确、时间或内存超限、启动与文件错误、运行错误、检查器与交互器错误、跳过、格式错误和输出超限。修改状态时以该枚举及相关显示、序列化代码为准。

## 进程执行

Linux 和 macOS 生成独立的 `watcher_unix`，分别组合 `watcher_unix.cpp` 与 `watcher_linux.cpp` 或 `watcher_macos.mm`，负责被评测程序的时间和内存监控。生成文件嵌入 `watcher.qrc` 后交由主程序使用。

Windows 的普通执行与实验性 AppContainer 执行共用 `WinProcessRunner::run()`。配置、进程限制、权限会话与资源计量集中记录在 [Windows 沙箱](windows-sandbox.md)。

## 评测期间阻止系统休眠

常规设置中的“评测时阻止系统休眠”默认启用，通过 QSettings 的 `GeneralSettings/PreventSleepWhileJudging` 保存。

`JudgingController` 在首次分派任务前创建 `SleepInhibitor`，最后一个运行任务结束后释放。取消评测同样等待运行任务退出后释放。申请仅阻止系统休眠，屏幕仍按系统设置关闭；申请失败记录日志并继续评测。

| 平台 | 系统接口 |
| --- | --- |
| Windows | 系统电源请求 |
| macOS | IOKit 的 `PreventUserIdleSystemSleep` assertion |
| Linux | 系统 D-Bus 的 `org.freedesktop.login1.Manager.Inhibit` |

Linux 因此依赖 Qt DBus，macOS 链接 IOKit 与 CoreFoundation。

## 应用初始化

`main.cpp` 先初始化 spdlog，再创建继承 SingleApplication 的 `LemonBaseApplication`。`Initialize()` 处理命令行和翻译；如果检测到现有实例，发送消息使其激活。随后创建 `LemonLime` 主窗口，显示启动画面，最后显示主窗口和欢迎对话框。

应用外观、SVG 配色、资源组织、翻译和日志的实现约定见 [编码约定](coding-conventions.md)。
