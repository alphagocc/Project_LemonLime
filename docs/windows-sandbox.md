# Windows 沙箱

本文集中说明 Windows AppContainer 沙箱的现行配置、执行行为和维护约定。设计实验与各阶段验证结果单独保存在 [Windows 沙箱备忘录](windows-sandbox-notes.md)。

## 范围与配置

沙箱仅覆盖提交程序的执行阶段，编译阶段和检查器继续使用宿主权限。功能按编译器启用，默认关闭；旧配置缺少启用字段时同样关闭。高级编译器设置提供自动、本机程序、Java 和 Python 策略，以及额外只读目录和准备时间预算。

配置定义见 [sandboxsettings.h](../src/base/sandboxsettings.h)，通过 [Compiler](../src/base/compiler.cpp) 的 JSON 字段 `windowsSandbox` 和 [Settings](../src/base/settings.cpp) 的 QSettings 分组 `WindowsSandbox` 保存。

| 含义 | JSON 字段 | QSettings 字段 | 默认值 |
| --- | --- | --- | --- |
| 启用沙箱 | `enabled` | `Enabled` | `false` |
| 运行策略 | `runtime` | `Runtime` | `Automatic` |
| 额外只读目录 | `readOnlyDirectories` | `ReadOnlyDirectories` | 空列表 |
| 准备时间预算 | `preparationTimeLimit` | `PreparationTimeLimit` | 15000 毫秒 |

界面允许设置 1 至 120 秒的准备预算，JSON 读取时同样限制在该区间。QSettings 按程序保存的类型和值读取，字段缺失时使用沙箱配置的默认值。

## 进程执行与评测行为

普通执行和沙箱执行共用 [WinProcessRunner::run()](../src/core/processrunner_win.cpp) 的启动、监控、计量和错误处理。关闭沙箱时沿用原有进程环境；启用时由 [WindowsSandbox](../src/core/windowssandbox.cpp) 准备权限、AppContainer 属性和 Job Object。

每次沙箱运行使用独立 Package SID、当前测试点工作目录和 Job Object。AppContainer 未授予网络能力。进程创建时通过属性配置 AppContainer、Job 和标准流句柄白名单；关闭标准流重定向时保留空句柄，白名单仅包含实际打开的标准流。可执行文件名沿用用户配置，由系统接口报告打开或启动错误。

自动、本机程序、Java 和 Python 策略统一设置 `ActiveProcessLimit = 1`，禁止提交程序创建子进程。限制使用 Job Object，因为子进程创建属性可能导致 Windows AppContainer 的 DLL 初始化失败。Job 保留 `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`，在句柄关闭时终止仍在运行的进程。

需要启动子进程的 Python 启动器和 Windows venv 会执行失败，应在编译器设置中指定基础 Python 等单进程入口。运行环境发现完成后，执行时继续使用用户配置的启动程序。

两种执行模式均返回主进程用户态时间与峰值工作集。内存限制检查主进程 `PrivateUsage` 与 `PeakWorkingSetSize` 的较大值；Job 仅承担单进程限制和关闭时终止进程，时间与内存统计继续使用主进程句柄，保持原有内存限制。

运行监控保留 10 毫秒等待间隔，输出结果判断保留在评测层。沙箱执行时，运行器使用 `TerminateProcess()` 终止主进程并等待退出。权限准备仅检查时间预算，停止标记由进程运行器处理；整体评测的取消入口按原有定义停止后续测试点。准备预算覆盖显式检查点，磁盘和系统调用的响应时间仍受操作系统影响。

取消返回值、运行错误信息和启动优先级保持原有定义。`CreateProcessW()` 接收可写命令行缓冲区，失败后立即保存系统错误码。界面提示使用翻译文本，底层诊断记录到日志。时间与内存计量、输出结果判断以及其他评测行为的调整须取得用户明确授权。

## 会话与运行环境

[TaskJudger::judge()](../src/core/taskjudger.cpp) 在测试点之间持有共享会话，结束或取消后释放。并发任务共用当前活动会话中的运行环境授权与 Python 探测结果；最后一个使用者退出时，撤销本会话新增的运行环境授权。会话使用独立命名 capability SID，授权与发现记录仅保存在会话内存中，禁止使用缓存文件或文件锁。

运行环境按选定工具发现。本机程序使用编译器所在目录的 DLL，Java 使用运行时目录，Python 识别解释器、基础安装与虚拟环境。`discoverJava()` 和 `discoverPython()` 分别实现语言发现，`discover()` 负责选择策略，授权阶段使用它返回的目录。

目录名使用 `QDir::canonicalPath()` 规范化，再通过 `removeDuplicates()` 精确去重。运行环境中的目录连接由 Windows 文件接口跟随，包括 Scoop 的 `current` 及父目录中的连接；无效目标由文件打开操作报告错误。目录名、程序文件名、缓存键、能力标识和目录包含判断保留大小写差异。

运行目录允许与比赛数据或工作目录重叠，按配置准备只读权限。额外只读目录使用同一授权机制；运行目录之外的启动器文件单独获得读取与执行权限。禁止对整个宿主 `PATH` 重复设置继承 ACL。运行环境准备结果在工具或根目录的大小、修改时间变化后重新检查。

Python 首次发现仅执行用户选定的可信解释器，使用普通 `QProcess`。准备超时返回时，`QProcess` 析构函数终止探测进程并等待退出。AppContainer、Job 与标准流句柄白名单用于提交程序执行。

沙箱执行环境保留用户显式配置的临时目录与用户目录环境变量，`LOCALAPPDATA` 缺省时设为当前测试点工作目录，以满足 Windows 创建 AppContainer 进程的要求。Python 输出编码、用户包加载和字节码缓存遵循解释器默认行为及用户显式配置。

## 权限准备与撤销

`prepare()` 先创建 AppContainer 身份，再完成运行环境发现、运行环境授权和工作文件授权，最后设置执行环境。`prepareRuntimePermissions()` 在函数作用域中管理权限互斥锁和失败回滚守卫，成功后解除回滚。运行环境准备失败或超时后撤销该阶段新增的授权。

运行环境与工作文件共用文件检查和权限合并实现，在原 DACL 上追加本次授权，保留原有用户、其他主体的权限项及保护状态。所选运行目录须具有适当的读取与执行授权，或允许当前用户配置授权；准备失败时报告原因并停止该次运行。

运行目录的现有文件逐项获得非继承 ACE，新增文件因此无需继承未记录的临时权限。每次新增授权保存在会话内存记录中。运行环境目录句柄使用 `MAXIMUM_ALLOWED` 抑制 `SetSecurityInfo` 的递归传播，文件打开方式兼容正在使用的 DLL。

`hasGrant()` 按 ACE 顺序检查剩余请求权限：拒绝项命中尚缺少的权限时失败，允许项提供全部所需权限时结束检查。`ALL APPLICATION PACKAGES` SID 在会话内复用。

`preparePrivateFiles()` 仅处理调用方创建的 `config.workingDirectory` 及其文件，按先子项后父目录的顺序使用范围 for 授权。工作文件句柄请求元数据及 DACL 权限，工作目录保留 Windows ACL 继承行为；Package SID 授予读取、写入、执行和删除权限，目录授权允许新建对象继承。工作文件中的重解析点和硬链接予以拒绝。

具名输入使用工作目录中的独立副本，与其他工作文件统一授权；标准输入通过原始输入文件的只读句柄提供。进程结束后，清理从现行 DACL 中移除本次 SID 的允许项，保留其他主体和其他活动任务随后追加的授权，以及原 DACL 的继承与保护状态。

运行环境和工作文件的准备、撤销共用进程内权限互斥锁。工作文件的 Package SID 授权在进程结束后撤销；工作目录由 `TaskJudger` 原有的 `QTemporaryDir` 生命周期统一删除。

## Windows 资源所有权

共享头文件 [windowsprocessutils.h](../src/core/windowsprocessutils.h) 提供 `Handle`、`LocalDeleter`、`LocalMemory` 和 `wide()`。生产代码与测试中须由 `LocalFree()` 释放的独立内存统一使用 `LocalMemory<Pointer>`；模板参数保留 `PSECURITY_DESCRIPTOR`、`PACL`、`PSID`、`LPWSTR` 等 Windows 指针类型。

`LocalMemory` 通过 `put()` 接收 API 输出、`get()` 访问资源、`release()` 转移所有权，每项资源仅保留一个所有者。`Handle` 提供 `put()` 输出接口。`SidArray` 将 SID 数组和计数封装为私有成员，通过输出地址接口接收能力 SID，并逐项释放数组中的内存。

选中的 capability SID 由独占所有权对象管理，进程属性数组仅引用这些 SID。Package SID 使用 `FreeSid()` 释放。`quoteArgument()` 保留在测试源文件中，`errorText()` 保留在沙箱实现的匿名命名空间中。

## 测试点工作目录

`TaskJudger::judge()` 在复制测试文件和创建评测线程前检查测试点目录创建结果。创建失败时记录 `FileError`、零分及包含目录名的错误信息，通过原有测试点完成信号报告结果，并停止该测试点的文件复制与线程启动。

同一子任务的后续测试点和依赖子任务按现有规则跳过，其他独立子任务继续评测。普通模式与沙箱模式共用此检查，`preparePrivateFiles()` 以调用方成功创建工作目录为前提。

## 验证入口

[tests/windows-sandbox](../tests/windows-sandbox/) 使用 Qt Test 验证隔离、句柄继承、ACL 保留与撤销、会话复用、子进程限制、主进程计量、取消、准备预算和语言兼容。目录创建失败用例同时覆盖普通模式与沙箱模式。测试仅在 Windows 构建，C、C++、Python 和 Java 用例使用本机安装的工具，缺少对应工具时跳过该项。

历史测试数量、使用环境和性能样例见 [Windows 沙箱备忘录](windows-sandbox-notes.md)，每个提交的验证结果以该次测试输出为准。
