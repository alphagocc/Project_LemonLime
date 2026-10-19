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

界面层的主要类包括 `LemonLime`、`JudgingDialog`、`ResultViewer` 与 `SummaryTree`；核心层包括 `Contest`、`Task`、`Contestant`、`TaskJudger`、`CaseJudger` 与 `JudgingController`；基础层包括 `Settings`、`Compiler`、`LemonLog` 与 `LemonConfig`。

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

1. `Contest::judge()` 为每个选手与题目的组合创建 `TaskJudger`，提交给 `JudgingController`。
2. `JudgingController` 使用专用 `QThreadPool` 和 `QtConcurrent::run()` 的 promise 模式分派任务，并发数量受 `maxJudgingThreads` 控制，其余任务保留在等待队列。
3. `TaskJudger` 在线程池中编译源代码，随后通过同步 `CaseJudger::run()` 顺序执行测试点和重测，保留子任务依赖及失败后的跳过规则。
4. `TaskJudger::judgeIt()` 返回完整 `TaskResult`，由 `QPromise<TaskResult>` 发布至对应的 `QFuture<TaskResult>`。
5. GUI 线程中的 `QFutureWatcher` 收到完成通知后调用 `Contestant::applyResult()`，整组结果更新完成后才发送成绩刷新信号。逐测试点事件通过携带值数据的排队信号传递。

`TaskJudger` 与 watcher 由 GUI 线程的控制器管理。任务对象在提交前完成配置，执行期间其可变字段仅由一个 worker 访问；future 完成后由 GUI 释放对象及临时目录。worker 仅保存选手名称，成绩容器始终由 GUI 线程访问。`CaseJudger` 为普通同步执行类，进程对象在执行它的 worker 内创建和销毁。

GUI 的模态评测窗口阻止题目、测试点、设置和编译器配置的编辑，`Contest::judge()` 返回前保持这些对象的生命周期，worker 通过 const 接口访问配置。当前同步评测入口保留局部 `QEventLoop`，全部任务完成并应用结果后退出，调用方随后显示完成通知并释放对话框。

`Task::writeToJson()` 和 `TestCase::writeToJson()` 为 const 成员函数。自动保存遍历题目配置与测试点列表时使用只读容器接口；文件名规范化在局部副本中进行，避免序列化触发共享配置容器的数据分离。

停止操作清理等待队列，对仍在执行的 future 调用 `cancel()`。`TaskJudger`、`CaseJudger` 与 `ProcessRunner` 持有同一共享状态的 `QFuture<void>`，编译等待、进程监控和内置结果比较均检查取消状态，执行线程无需通过 `processEvents()` 接收停止请求。各执行器负责进程终止和资源清理，任务函数返回后 future 报告 `finished`；控制器据此判断整场结束并释放休眠控制。取消任务保留此前成绩数据，并将评测完成标志设为 false；尚未分派的任务保持原结果和标志。

普通文本比较在评测循环中检查取消，`BufferedStreamReader` 仅负责读取文件。外部 diff 沿用 `QProcess::execute()` 同步等待，正在执行的 diff 完成后，任务层再处理取消请求。

`Settings` 优先读取 `GeneralSettings/MaximumJudgingThreads` 中保存的设置。该项缺失时，由 `src/base/cputopology.cpp` 检测物理核心并计算默认值：同时具有 P-core 与 E-core 的处理器采用 P-core 数量；同构处理器采用全部物理核心数量的四分之三，向下取整且至少为 1。仅有 E-core 的处理器也按同构计算，例如 N100 的 4 个 E-core 对应 3 个评测线程。SMT 逻辑线程归属于同一个物理核心。

| 平台 | 核心检测方式 |
| --- | --- |
| Windows | `GetLogicalProcessorInformationEx(RelationProcessorCore)` 统计物理核心，`EfficiencyClass` 的最高性能类别数量用于异构计算 |
| macOS | `hw.physicalcpu` 统计物理核心，存在多个性能类别时读取 `hw.perflevel0.physicalcpu` |
| Linux | sysfs 的同核兄弟清单合并逻辑线程，过滤离线 CPU；Intel 使用 `cpu_core/cpus` 识别 P-core，ARM 使用完整的 `cpu_capacity` 信息区分性能类别 |

只有最高性能类别的核心数量介于 1 和全部物理核心数量之间时，才采用异构规则。核心类别信息缺失时按同构计算；物理核心信息缺失时，使用 `QThread::idealThreadCount()` 返回的逻辑处理器数计算四分之三，结果至少为 1。自动值仅控制并行评测数量，评测进程的 CPU 调度由操作系统负责。

开启调试日志后，`CpuTopology` 模块记录核心检测来源、核心数量、分类结果、系统接口失败信息和默认线程数的计算依据；`Setting` 模块记录实际采用的线程数，以及该值来自保存的设置还是自动计算。

`TaskJudger` 在复制测试文件和运行测试点前检查工作目录。创建失败时记录 `FileError` 与零分，报告目录错误，保持原有子任务依赖和后续独立子任务规则。普通模式与 Windows 沙箱模式共用该检查。

`subtaskdependencelib.h` 使用 `stateToStatus()` 与 `statusToScore()` 计算子任务依赖得分，精度为百万分比，`maxDependValue = 1000000`。

评测状态定义位于 `LemonType.hpp`，包括正确、错误、部分正确、时间或内存超限、启动与文件错误、运行错误、检查器与交互器错误、跳过、格式错误和输出超限。修改状态时以该枚举及相关显示、序列化代码为准。

## 进程执行

Linux 和 macOS 生成独立的 `watcher_unix`，分别组合 `watcher_unix.cpp` 与 `watcher_linux.cpp` 或 `watcher_macos.mm`，负责被评测程序的时间和内存监控。生成文件嵌入 `watcher.qrc` 后交由主程序使用。

`UnixProcessRunner::run()` 使用函数作用域内的 `QProcess` 对象，正常返回及提前返回均由析构函数释放进程对象；取消和超时继续先终止 watcher 并等待退出。

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
