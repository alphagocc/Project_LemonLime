# 编码约定

本页维护 C++、Qt 和资源实现细节。共同规则与提交要求见 [AGENTS.md](../AGENTS.md)，模块职责见 [项目架构](architecture.md)。

## C++ 格式与命名

项目使用 C++23，关闭编译器语言扩展；头文件统一使用 `#pragma once`。新增语言和标准库特性须兼容 CI 中的 GCC、MSVC 与 AppleClang，并满足目标系统版本要求。格式以 [.clang-format](../.clang-format) 为准：

```yaml
BasedOnStyle: LLVM
BreakBeforeBraces: Attach
IndentWidth: 4
TabWidth: 4
UseTab: ForIndentation
ColumnLimit: 110
ContinuationIndentWidth: 4
IndentCaseLabels: true
NamespaceIndentation: All
SpaceAfterLogicalNot: true
```

类名和枚举值使用 PascalCase，成员变量和方法使用无前缀 camelCase，访问器采用 `getXxx()` 与 `setXxx()`。宏与常量沿用全大写或 PascalCase。

顶层命名空间为 `Lemon`，现有子命名空间包括 `base`、`base::config`、`detail` 和 `common`；`Contest`、`Task` 等旧核心类保持全局定义。Qt Designer 生成的类位于 `Ui` 命名空间。

头文件优先使用前向声明减少依赖。Getter 使用 `const` 修饰，引用返回值使用 `const Type &`；QObject 派生类的构造函数使用 `explicit`，父对象参数默认为 `nullptr`。

```cpp
const QList<Task *> &getTaskList() const;
explicit Contest(QObject *parent = nullptr);
```

项目定义 `QT_NO_FOREACH`，统一使用标准 C++ 范围 for。

## 格式检查与许可证

提交前对本次修改的 C++ 文件运行 `clang-format`。以下命令处理暂存区对应的文件，格式化后应重新暂存这些文件。

```bash
while IFS= read -r -d '' file; do
    clang-format -i "$file"
done < <(git diff --name-only --cached -z -- '*.cpp' '*.h' '*.hpp')
```

```powershell
git diff --name-only --cached -- '*.cpp' '*.h' '*.hpp' |
    ForEach-Object { clang-format -i $_ }
```

源码使用 CRLF 行尾，中文注释和文档使用 UTF-8。源文件保留 SPDX 许可证头，现有格式为：

```cpp
/*
 * SPDX-FileCopyrightText: 2019-2022 Project LemonLime
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */
```

## 信号槽与内存管理

信号槽统一采用 C++ 函数指针形式；重载信号使用 `qOverload<>()` 消歧。

```cpp
connect(ui->problemTitle, &QLineEdit::textChanged, this, &TaskEditWidget::problemTitleChanged);
connect(ui->comparisonMode, qOverload<int>(&QComboBox::currentIndexChanged), this,
        &TaskEditWidget::comparisonModeChanged);
```

QObject 优先由父子对象树管理。`JudgingController` 显式管理 `QThread` 与 `TaskJudger`，释放前先退出并等待线程；`LemonTranslator` 使用 `std::unique_ptr<QTranslator>`，spdlog logger 使用 `std::shared_ptr`。`TestCase` 为普通类，由 `QList<TestCase*>` 保存。

线程调度见 [项目架构](architecture.md)，Windows 资源所有权见 [Windows 沙箱](windows-sandbox.md)。

## JSON 与日志

数据模型提供 `read(const QJsonObject &)` 与 `write(QJsonObject &) const`，使用 `LemonUtils.hpp` 的模板工具：

```cpp
#define READ_JSON(json, x) Lemon::readJson(x, #x, json)
#define WRITE_JSON(json, x) Lemon::writeJson(x, #x, json)
```

底层通过 SFINAE 和 `std::enable_if_t` 支持数值、字符串、JSON 数组、QList、自定义枚举等类型。

每个使用日志的 `.cpp` 定义 `LEMON_MODULE_NAME`，再调用项目宏：

```cpp
#define LEMON_MODULE_NAME "JudgingController"

LOG("Starting judge for", contestantName);
WARN("Compile failed:", message);
DEBUG("Score:", score, "Time:", timeUsed);
```

spdlog 同时输出到控制台与每日文件，控制台采用 warn 及以上级别，文件采用 trace 及以上级别，并保留最近 30 天日志。

## 界面与外观

Qt Designer 的 `.ui` 文件保存在 `src/forms/`，界面类通过 `Ui::ClassName` 指针持有生成的界面对象。CMake 开启 `AUTOUIC`、`AUTOMOC` 与 `AUTORCC`，并将 `AUTOUIC` 搜索目录设为 `src/forms`。

外观提供跟随系统、浅色、深色三种模式，默认跟随系统。`Settings` 通过 QSettings 的 `VisualSettings/ColorScheme` 保存 `Qt::ColorScheme`，系统模式使用 `Unknown`。

应用启动和设置确认后调用 Qt 6.8 起提供的 `QStyleHints::setColorScheme()`；取消设置保留原模式。`ResultViewer` 响应 `PaletteChange` 更新成绩背景颜色，保留当前选择与排序。

## 资源与 SVG 图标

| 资源文件 | 内容 |
| --- | --- |
| `resource.qrc` | 图标、logo、版本文件 |
| `translations/translations.qrc` | 可嵌入或外置的翻译 |
| `manual/manual.qrc` | 可嵌入的用户手册 |
| `unix/watcher.qrc` | Unix watcher 可执行文件 |

SVG 图形在 `assets/pics/` 维护一份，资源别名使用 `.llsvg`。静态 `LemonSvgIconPlugin` 创建自定义 `QIconEngine`，使用 XML 解析替换 `id="current-color-scheme"` 的样式块。

`ColorScheme-Text` 采用应用调色板，`ColorScheme-NegativeText` 采用集中管理的浅色与深色提示颜色，禁用与选中状态采用对应的调色板颜色。绘制使用应用级调色板；插件缓存配色后的 SVG，并按源码摘要、调色板、尺寸、状态、设备像素比缓存图像。

新增图标在专用样式块声明上述颜色角色，图形使用 `currentColor`，并在 `resource.qrc` 注册 `.llsvg` 别名，表单使用同一别名。专用样式块仅存放主题颜色声明，其他样式放在该块之外。普通 `.svg` 交由 Qt 原生插件处理。

实现位于 `src/plugins/lemonsvgiconplugin.cpp`，链接 Qt Svg，由 `qt_add_plugin` 在链接时自动注册。插件参考 KIconThemes 的颜色角色与 XML 样式替换设计，采用独立实现和 GPL-3.0-or-later 许可证；上游参考版本与许可证见 [插件说明](../src/plugins/README.md)。

## 国际化

面向 GUI 的文本使用 `tr()` 或 `QObject::tr()`；非 QObject 类使用 `Q_DECLARE_TR_FUNCTIONS`，例如 `Settings`。仅供日志使用的内部诊断保持普通字符串。

翻译文件为 `translations/en_US.ts`、`translations/zh_CN.ts` 与 `translations/zh_TW.ts`。`LemonTranslator` 管理翻译加载及多个搜索目录，包含 Snap 与 AppImage 支持；CMake 使用 `qt_add_translation()` 将 `.ts` 编译为 `.qm`。
