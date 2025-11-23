# 系统模式与架构要点

- 运行时骨架：`main.cpp` 初始化 spdlog（控制台警告级 + 按日文件 trace，保留 30 天），构建 `LemonBaseApplication` 处理命令行与翻译加载，再创建 `LemonLime` 主窗体。
- 单实例与资源：依赖 3rdparty SingleApplication 防重复启动；资源由 Qt qrc（`resource.qrc` + `manual.qrc` + `translations.qrc`）打包，默认嵌入翻译与手册（可通过 CMake 开关控制）。
- UI 组成：Qt Widgets + `.ui` 表单；主窗体提供 SummaryTree/TaskEdit/TestCaseEdit/ResultViewer/StatisticsBrowser 等页签，并通过 QAction/QToolButton 绑定评测、刷新、清理、导出等操作。
- 数据模型：`Contest` 聚合 `Task`（包含题型、比对模式、SPJ/交互/通信配置、编译器参数、测试点列表）与 `Contestant`；`Settings` 记录编译器、默认限制、语言/主题等偏好；通过 QFileSystemWatcher 监听数据目录变动并自动刷新。
- 评测管线：ResultViewer 触发 `Contest` → `JudgingController` → 多线程 `TaskJudger`；支持局部/全部/未测/灰/紫结果重判，编译/运行/比对状态通过枚举（CompileState/ResultState）与进度信号回传 UI。
- 持久化与导出：比赛可写入 JSON（亦兼容旧二进制流）；导出结果/统计由 `component/exportutil` 和 StatisticsBrowser 生成；清理功能可备份选手目录并按题目重排文件。
