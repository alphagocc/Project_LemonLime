# QML 界面迁移记录

本轮将应用界面迁移至 Qt Quick 与 QML，并以原 Qt Widgets 界面的字段、排列、菜单、按钮顺序、窗口尺寸和弹窗层级作为实现参照。比赛数据模型与评测核心继续由原基础层和核心层提供，产品界面由 `src/qml` 下的组件组成。

Windows 使用 Qt Quick Controls 的 `FluentWinUI3` 样式。页面结构采用原版布局，控件轮廓、焦点效果、选中效果及深浅配色由所选样式绘制。本文记录当前实现、原表单对应关系和验证范围。

## 页面结构与交互

主窗口由 `Main.qml` 创建，保留顶部“文件、控制、工具、帮助”菜单。主内容区采用原版左侧竖排标签，顺序为 Tasks、Contestants、Statistics，即题目、选手、统计。设置通过独立窗口打开。

题目页保留左侧 Summary 树与右侧 Detail 编辑区。树中区分题目和测试点，右侧随选择显示题目表单或测试点表单。提交文件及 grader 文件使用表格编辑，字段顺序与题型对应的启用规则以原表单为准。高级测试点修改器保留独立窗口，测试点导入保留限制、匹配规则、预览三个步骤。

选手成绩页保留成绩表、表头排序、选择操作和右键菜单。底部按钮依次为“清理文件”、伸缩空白、“刷新”“评测未评测项”“评测选中项”“评测全部”。成绩详情、删除确认和评测进度使用各自的弹窗。欢迎、新建比赛、打开比赛使用独立窗口；欢迎窗口保留“打开”和“新建”两个页签。

设置窗口包含“常规、编译器、视觉”三个页签及底部确认、取消按钮。编译器页保留列表、上移、下移、添加、删除和高级设置入口；添加编译器使用三步骤向导，包含自定义配置和六类预设配置。高级编译器设置、环境变量列表、单个变量编辑、成绩主题编辑分别使用独立窗口。每层编辑使用自己的草稿，确认后更新父层，取消时保留父层原值。

原版窗口尺寸作为默认值或最小布局约束。下表列出本机原版参照中的主要逻辑尺寸，实际控件尺寸由字体和平台样式共同决定，最终几何核对以本轮截图 JSON 为准。

| 窗口 | 原版参照逻辑尺寸 |
| --- | --- |
| 主窗口 | 800 × 600，最小 725 × 510 |
| 欢迎 | 470 × 350 |
| 新建比赛、打开比赛 | 450 × 320 |
| 设置 | 533 × 437 |
| 高级编译器设置 | 667 × 641，包含 Windows 沙箱设置 |
| 环境变量 | 298 × 196 |
| 单个变量编辑 | 325 × 128 |
| 成绩主题编辑 | 533 × 300 |
| 添加编译器向导 | 542 × 555 |
| 添加测试点向导 | 500 × 378 |

高级编译器原 Designer 尺寸为 667 × 490，本机原界面加入沙箱区域后由布局扩展至 667 × 641。成绩主题原 Designer 尺寸为 400 × 300，内部 `VisualSettings` 的最小宽度使实际窗口扩展至 533 × 300。迁移时采用实际参照尺寸，并保留内容需要更大最小尺寸时的布局计算。

## 平台样式、文字与图标

Windows 11 Metro 是本轮外观要求的描述。实际实现由 `src/main.cpp` 在 Windows 下调用 `QQuickStyle::setStyle("FluentWinUI3")`，采用 Qt 提供的 Windows 11 Fluent 控件。此前使用的 `Windows` 样式在应用请求深色模式后出现浅色背景与浅色文字混用；`FluentWinUI3` 的背景配置与文字颜色共同响应 `QStyleHints::colorScheme`。Linux 和 macOS 交由 Qt Quick Controls 选择平台默认样式。

通用控件位于 `src/qml/controls`，使用 `Button.qml`、`ToolButton.qml`、`TextField.qml`、`ComboBox.qml`、`CheckBox.qml`、`RadioButton.qml`、`SpinBox.qml` 等标准名称。页面通过 `import "controls" as Controls` 引用它们。普通输入控件使用 Qt Quick Controls 的平台实现；标签页轮廓、表头和树形展开图形等辅助绘制由 `StyleItem` 提供，`StyleMetrics` 负责字体测量、样式尺寸及调色板更新通知。测量时创建短生命周期、始终隐藏的 Qt 控件并缓存其尺寸，避免 Windows 样式在缺少控件上下文时发生无效访问；实际显示和交互仍由 QML 管理。窗口保留系统标题栏。

成绩详情、统计报告和评测日志共用 `controls/TextBrowser.qml`。其 `TextDocumentItem` 使用 `QTextDocument` 排版和绘制 HTML，包含表格、文字选择、复制与链接处理；外层提供横向和纵向滚动。成绩详情内容由 `resultdetails.cpp` 组织，统计及导出继续由 `ContestTools` 提供。

图标继续使用原 `assets/pics` 图形及 `.llsvg` 资源别名。`controlstyle.cpp` 注册 `lemon-icons` image provider，将 `QIcon` 与现有 SVG 图标插件的绘制结果交给 QML。控件通过 `image://lemon-icons/...` 取图，应用调色板变化会更新请求标识，使图标随浅色、深色模式刷新。按钮图标明确使用 16 × 16 逻辑尺寸，provider 按请求像素尺寸绘制并使用显式设备像素比，修复重复缩放造成的按钮增高。

### 深色模式修正

2026-10-06 根据运行截图修正浅色控件背景与深色页面混用的问题。修改范围如下。

| 文件或区域 | 修改内容 |
| --- | --- |
| `src/main.cpp` | Windows 从 `Windows` 样式切换至 `FluentWinUI3`，按钮、菜单和输入控件共同响应应用配色 |
| `controls/Button.qml`、`TextField.qml`、`ComboBox.qml`、`ToolButton.qml` | Windows 使用紧凑内部留白，底部按钮维持 120 × 26，图标维持 16 × 16 |
| `controls/ScrollBar.qml` 及各页面滚动条 | 共用滚动条组件，滑块从调色板文字颜色生成，修正深色模式下滑块与背景过于接近的问题 |
| `controls/Dialog.qml`、`SettingsDialog.qml` | 采用 `ApplicationWindow` 传播应用字体，保留独立窗口与系统标题栏，修正 Fluent 默认字体造成的中文字体变化 |
| `controls/SpinBox.qml`、`controlstyle.*` | 窄数值输入框使用竖排箭头，保留 QML 编辑、步进及范围控制，输入框与箭头采用应用调色板绘制 |
| `controls/CheckBox.qml`、`RadioButton.qml` | Windows 调整内部留白，保留 Fluent 状态配色 |
| `tests/qml-ui/tst_qmlui.cpp` | 增加深浅色启动、确认切换、跟随系统与取消检查；检验实际背景像素、文字对比度、标题栏配色和数字输入框点击行为；原窗口截图扩展为深浅两套 |

测试继续使用临时 INI 配置及透明、禁止激活的 Windows 窗口。标题栏通过只读 DWM 属性查询验证；截图中的像素检查使用逻辑坐标换算设备像素，避开字体边缘的抗锯齿混色。

### 工具按钮边框修正

2026-10-06 根据“打开比赛”窗口截图恢复“添加”“隐藏”按钮的常驻背景与边框。修改 `controls/ToolButton.qml`，使用平台 `Button` 的背景，保留工具按钮原有尺寸、图标大小和显示方式。原版工具按钮采用 `autoRaise=false`，对应的文件浏览、编译器与主题维护、题目排序及导入参数按钮一并恢复轮廓。“添加”“隐藏”仍为 96 × 32 逻辑尺寸，其他控件沿用各自布局约束。

本次运行现有的窗口加载与截图测试，浅色和深色两组通过，包含初始化与清理共 4 项，零失败、零警告。截图确认两种配色下的启用及禁用按钮均保留轮廓，逻辑尺寸为 96 × 32。报告位于 `build/tool-button-test.txt`。首次构建时 `build/lemon.exe` 正在运行，链接遇到文件占用，因此先使用构建系统生成的目标文件、响应文件与链接参数生成 `build/lemon-updated.exe`。用户退出程序后，常规构建成功重新链接 `build/lemon.exe`，该文件包含相同的边框修正，运行依赖沿用同一目录。最终构建日志为 `build/tool-button-final-build.log`。

页面文字优先使用原上下文的 `qsTranslate()`，沿用原英文源文和简体、繁体译文。新控制器文字使用 `tr()`。本次翻译同步后，英文、简体中文、繁体中文各有 597 条活动消息，未完成数均为零；旧上下文及历史译文保留。

## 原表单与 QML 组件映射

下表覆盖迁移前 `src/forms` 下的全部 26 个表单。原表单及 Widgets 实现可通过 Git 历史或只读参照检出查看；下列 QML 文件均位于 `src/qml`。

| 原表单 | 当前窗口或组件 | 对应内容 |
| --- | --- | --- |
| `lemon.ui` | `Main.qml` | 原菜单、竖排三页签、状态栏及页面组织 |
| `welcomedialog.ui` | `ContestDialog.qml` 的 welcome 模式 | 独立欢迎窗口中的打开、新建页签 |
| `newcontestdialog.ui` | `ContestDialog.qml` 的 new 模式 | 新建比赛窗口及确认、取消 |
| `newcontestwidget.ui` | `NewContestForm.qml` | 比赛标题、保存名称、目录和浏览按钮 |
| `opencontestdialog.ui` | `ContestDialog.qml` 的 open 模式 | 打开比赛窗口及确认、取消 |
| `opencontestwidget.ui` | `OpenContestForm.qml` | 最近比赛表格、添加和隐藏记录 |
| `judgingdialog.ui` | `JudgingProgressDialog.qml` | 评测日志、进度与原底部按钮 |
| `detaildialog.ui` | `ResultsPage.qml` 中的详情窗口 | 成绩详情 HTML、重评链接和关闭按钮 |
| `statisticsbrowser.ui` | `StatisticsPage.qml` | 原统计页中的 HTML 报告及导出入口 |
| `taskeditwidget.ui` | `TaskSettingsForm.qml`、`TaskMappingTable.qml`、`TaskFileField.qml` | 题目字段、文件映射、文件选择及编译器配置 |
| `addtaskdialog.ui` | `TaskDiscoveryDialog.qml` | 自动发现题目后的题目选择与分数、限制设置 |
| `addtestcaseswizard.ui` | `TestCaseImportDialog.qml` | 限制、文件匹配规则、预览三个步骤 |
| `testcaseeditwidget.ui` | `TestCaseEditor.qml` | 输入输出文件、子任务依赖及分数、限制 |
| `exttestcasemodifier.ui` | `TestCaseModifierDialog.qml` 的编辑区域 | 测试点表格、文件对及结构调整按钮 |
| `exttestcasemodifierdialog.ui` | `TestCaseModifierDialog.qml` | 高级测试点修改器窗口及确认、取消 |
| `exttestcaseupdaterdialog.ui` | `TestCaseBatchDialog.qml` | 选中测试点或文件对的参数修改窗口 |
| `optionsdialog.ui` | `SettingsDialog.qml`、`SettingsPage.qml` | 独立设置窗口、原三页签及确认、取消 |
| `generalsettings.ui` | `SettingsGeneral.qml` | 四列表单中的常规设置 |
| `compilersettings.ui` | `SettingsCompilers.qml` | 编译器列表、顺序调整、名称、扩展名和高级入口 |
| `addcompilerwizard.ui` | `AddCompilerWizard.qml` | 配置方式、自定义或预设配置、结果确认 |
| `advancedcompilersettingsdialog.ui` | `AdvancedCompilerSettingsDialog.qml` | 位置、资源比例、配置参数、沙箱和环境变量入口 |
| `environmentvariablesdialog.ui` | `EnvironmentVariablesDialog.qml` | 环境变量表格及添加、编辑、删除 |
| `editvariabledialog.ui` | `EditVariableDialog.qml` | 变量名称和值 |
| `visualsettings.ui` | `ThemeEditDialog.qml` 的参数区域 | 主题名称、四组 HSL 颜色、总分补偿和倍率 |
| `visualmainsettings.ui` | `SettingsAppearance.qml` | 外观模式、主题选择及维护、启动画面时长 |
| `themeeditdialog.ui` | `ThemeEditDialog.qml` | 独立主题编辑窗口及确认、取消 |

原自定义控件 `ResultViewer` 由 `ResultsPage.qml` 和 `ResultModel` 承担；`SummaryTree` 由 `TasksPage.qml` 的题目树承担；`ExtTestCaseTable` 由高级测试点修改器的表格承担；`FileLineEdit` 由 `TaskFileField.qml` 承担。

## 数据、评测与构建

`AppController` 管理比赛打开、保存、评测和窗口状态；`TaskController` 管理题目与测试点编辑；`SettingsController` 管理设置草稿；`ContestTools` 提供统计、导出和源文件整理。它们向 QML 提供数据与操作接口，继续调用 `Contest`、`Task`、`TestCase`、`Contestant`、`JudgingController` 等原模型和评测对象。

比赛文件继续采用原 `.cdf` 数据定义，读取保留旧格式支持。题型、比较模式、编译状态、成绩状态、子任务依赖、编译器设置和原 QSettings 配置项沿用已有定义。Windows 沙箱、Unix watcher、资源计量和评测时阻止休眠仍由原评测核心执行。

QML 源码通过 `qt_add_qml_module` 组成 `LemonLime` 模块，由 `QQmlApplicationEngine::loadFromModule()` 加载。构建增加 Qml、Quick、QuickControls2 依赖；QApplication、SingleApplication、现有图标插件及样式辅助仍使用 Qt Widgets。共享控件子目录由 QML 模块保留相对目录关系。

构建使用当前工程配置，并行数量交由构建系统决定：

```powershell
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Windows 部署需要让 `windeployqt` 扫描 `src/qml`；macOS 和 Linux 同样需要随程序提供对应 QML 模块、平台样式和插件。完整依赖与部署说明见 [BUILD.md](../BUILD.md)。

## 原版参照与测试配置

[原版参照工具](../tests/ui-reference/README.md)位于 `tests/ui-reference`。配置 `LEMON_UI_REFERENCE_SOURCE_DIR` 后，可以单独构建 `lemon-ui-reference`；该目标从默认构建中排除。工具只读取原 Widgets 检出，将所需界面源码复制到构建目录，复用当前基础层和核心层，产品可执行文件继续使用 QML 界面。

本轮原版参照按两种环境分别生成 29 组 PNG 与对应 JSON：`build/tests/ui-reference/screenshots` 使用 offscreen 与 Fusion；`build/tests/ui-reference/screenshots-windows` 使用 Windows 平台、windows11 样式和浅色配色，设置 `WA_DontShowOnScreen` 后采集。后者用于同平台的尺寸核对。每组的 `manifest.json` 列出截图名称、平台、实际样式、字体和窗口尺寸，每页 JSON 记录控件位置、字号、文字、显示状态和布局参数。

新界面截图由 `tests/qml-ui` 生成，保存在 `build/tests/qml-ui/screenshots`。Windows 测试采用 Windows 平台插件和 D3D11 渲染，创建具备实际 Windows 窗口句柄的测试窗口。窗口透明度为零，并设置 `WindowDoesNotAcceptFocus`、`WindowTransparentForInput`，保持绘制所需窗口状态，同时避免接收桌面输入和焦点。测试通过 `grabWindow()` 采集内容，并检查图像有效性及窗口标志。其他平台的测试配置继续使用 offscreen 软件渲染。

原版参照和 QML 测试均在创建设置对象及控制器前切换至临时目录，使用临时 INI 文件，并检查配置文件确实位于该目录。UserScope 与 SystemScope 同时重定向。原版主窗口中固定使用 NativeFormat 的构造在构建目录副本中改为 INI；原检出保持只读。测试中的 NativeFormat 对象仅用于只读定位检查，本轮测试设置写入全部限定在临时 INI，真实偏好和注册表未作为测试写入目标。

offscreen 参照的设备像素比为 1；Windows 原版参照和 QML 截图均为 2。PNG 像素尺寸随设备像素比改变；位置、尺寸与是否超出窗口的比较以 JSON 中的逻辑坐标为准，再结合图像检查字段、按钮和文字。

上一轮本机配置处理的历史说明保存在[配置修复记录](../build/qml-settings-repair.md)。本轮保留该记录供查阅，修复操作仅属于上一轮执行记录。

## 本轮验证记录

记录日期为 2026-10-06。Windows x64 本机使用 Qt 6.12.0、MSVC 19.51、Debug、`ENABLE_XLS_EXPORT=ON` 和 `ENABLE_LTO=OFF`。

| 项目 | 当前记录 |
| --- | --- |
| 原版 Widgets 参照 | 两种采集环境各 29 组 PNG 与 JSON，含采集清单 |
| 三种语言翻译 | 每种语言 597 条活动消息，未完成 0 条；lrelease 和占位符校验通过 |
| Windows 主程序与测试目标构建 | 通过，编译并行数由构建工具决定 |
| Windows Fluent 样式 QML 加载、交互与截图 | Qt Test 18 项通过，零失败、零跳过、零 QML 警告；74 组截图与逻辑几何记录，包括全部窗口的深浅两套截图及主题切换检查 |
| 比赛评测、Windows 沙箱、睡眠抑制测试 | 迁移阶段三组通过，加上 qml-ui，CTest 四组全部通过；本次深色修正执行完整 qml-ui 测试组，其中包含实际提交答案评测与自动保存检查 |
| 原版与 QML 页面几何核对 | 核对主窗口、设置、题目和向导；修正图标重复缩放、组框重叠、表格挤压、输入框绘制和多余行间伸展。页签、菜单、字段、底部按钮与弹窗层级按原版保留；平台控件绘制与指标仍以 Windows 样式为准 |
| Windows 运行文件 | `build/lemon.exe`，通过 windeployqt 部署 Qt 库、平台插件和 QML 模块 |
| Linux 与 macOS | 尚未执行对应平台的编译和运行验证 |

QML 测试报告位于 `build/tests/qml-ui/qml-ui-results.txt`。本次深色修正的 CTest 结果位于 `build/dark-mode-test.log`，构建与部署日志分别为 `build/dark-mode-build.log` 和 `build/dark-mode-deploy.log`；迁移阶段完整 CTest 结果保留在 `build/final-test.log`。深浅色配色测试同时检查系统标题栏，底部按钮保持 120 × 26，10 号字体的数字输入框按原版参照保持 27 逻辑像素高度。XLS 分支完成编译，尚未调用 Microsoft Excel 验证实际导出。这里记录结构还原、功能与几何检查，未将原生样式间的绘制差异表述为逐像素零差异。
