# AGENTS.md

Project LemonLime 是基于 Lemon 与 LemonPlus 的 OI 竞赛评测系统，支持 Windows、Linux 和 macOS，采用 C++23、Qt 6.8 及以上版本，许可证为 GPL-3.0-or-later。

## 文档入口

按修改范围读取对应文档。现行实现与开发约定按主题维护，历史备忘单独保存。

| 文档 | 内容 |
| --- | --- |
| [构建与 CI](docs/build-and-ci.md) | 依赖、CMake 选项、版本、测试与产物命名 |
| [项目架构](docs/architecture.md) | 目录、数据模型、评测、线程、应用初始化与休眠控制 |
| [编码约定](docs/coding-conventions.md) | C++ 与 Qt 风格、序列化、日志、外观、资源与翻译 |
| [Windows 沙箱](docs/windows-sandbox.md) | 配置、进程限制、权限、运行环境、计量与资源所有权 |
| [沙箱历史备忘](docs/windows-sandbox-notes.md) | 设计取舍、历史实验与验证记录 |
| [平台构建说明](BUILD.md) | 各平台工具安装与构建命令 |

`docs/reviews/` 保存带日期的历史代码审查，阅读时应结合记录所针对的提交与现行代码。

## 构建与验证

首次构建前同步子模块，使用与 Qt 工具包匹配的 C++ 编译器：

```bash
git submodule update --init --recursive
cmake -B build -DCMAKE_BUILD_TYPE=Release -GNinja
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Qt Test 测试由 CTest 执行。Windows 另含沙箱与休眠控制测试，语言兼容测试使用本机安装的工具；具体要求见构建文档。

## 共同约定

1. 使用 C++23 和 `#pragma once`。代码按仓库 `.clang-format` 格式化，采用 Tab 缩进、110 列宽和 CRLF 行尾，源码保留 SPDX 许可证头。
2. 信号槽使用函数指针形式，重载信号使用 `qOverload<>()`。循环使用标准 C++ 范围 for，遵守 `QT_NO_FOREACH`。
3. 类名使用 PascalCase，成员与方法使用无前缀 camelCase，访问器使用 `getXxx()` 与 `setXxx()`。保留现有 `Lemon` 命名空间及旧全局类的组织方式。
4. QObject 优先由父子对象树管理。跨线程对象、Windows 句柄和权限资源的所有权遵循相应模块文档。
5. GUI 文本使用 `tr()` 或 `QObject::tr()`，非 QObject 类使用 `Q_DECLARE_TR_FUNCTIONS`；仅供日志使用的诊断保持普通字符串。
6. Windows 沙箱默认关闭，按编译器启用。修改相关代码前阅读沙箱文档，保留原有评测计量、取消返回值、错误信息与启动优先级；行为调整须取得用户明确授权。
7. macOS watcher 使用 `watcher_macos.mm`，Linux 使用 `watcher_linux.cpp`；平台资源和模块依赖见构建与架构文档。

## 提交与文档维护

使用 Conventional Commits：`<type>(<scope>): <subject>`，scope 可省略。类型限定为 `feat`、`fix`、`refactor`、`style`、`docs`、`build`、`ci`、`perf`、`test`、`chore`。

首行至多 72 个字符，英文小写开头，末尾省略句号。详细说明置于正文，破坏性变更在正文使用 `BREAKING CHANGE:` 前缀。

提交前对本次修改的 C++ 文件运行 `clang-format`，并执行与修改相关的检查。格式化命令与许可证示例见编码约定。

修改架构、构建或开发约定时，同步维护对应的 `docs/` 文档；入口或共同规则变化时更新本文件。现行说明以相应主题文档为准，历史验证结果保留日期与适用范围。

`AGENTS.md` 的变更必须作为独立 commit 提交，与其他代码和文档变更分开，以支持单独 cherry-pick 到 master 分支。
