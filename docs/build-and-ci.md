# 构建与 CI

各平台依赖安装与部署命令见 [BUILD.md](../BUILD.md)。本页维护项目的构建选项、测试入口与 CI 产物约定。

## 依赖与构建

CMake 最低版本为 3.16，C++ 编译器须支持 C++17。推荐使用 Ninja。Qt 要求 6.8 或更高，可通过 `LEMON_QT_MAJOR_VERSION` 指定主版本。

| Qt 模块 | 用途 |
| --- | --- |
| Core、Gui、Widgets | 核心库和界面 |
| Network | SingleApplication 单实例保护 |
| Svg | SVG 图标插件 |
| LinguistTools | 翻译编译 |
| Test | Qt Test 测试 |
| DBus | Linux 评测期间阻止系统休眠 |
| AxContainer | Windows XLS 导出，按构建选项启用 |

SingleApplication 和 spdlog 作为 Git 子模块位于 `3rdparty/`。源码获取与常规构建：

```bash
git clone https://github.com/Project-LemonLime/Project_LemonLime.git --recursive
cd Project_LemonLime
cmake -B build -DCMAKE_BUILD_TYPE=Release -GNinja
cmake --build build --parallel
```

现有仓库使用 `git submodule update --init --recursive` 同步依赖。

## CMake 选项

| 选项 | 默认值 | 说明 |
| --- | --- | --- |
| `EMBED_TRANSLATIONS` | `ON` | 嵌入翻译文件 |
| `EMBED_DOCS` | `ON` | 嵌入用户手册 |
| `ENABLE_XLS_EXPORT` | `OFF` | Windows XLS 导出 |
| `ENABLE_LTO` | `ON` | 链接时优化 |
| `ENABLE_CCACHE` | `OFF` | ccache 编译缓存 |
| `BUILD_DEB` | `OFF` | 构建 DEB 包 |
| `BUILD_RPM` | `OFF` | 构建 RPM 包 |
| `LEMON_QT_MAJOR_VERSION` | `6` | Qt 主版本 |
| `LEMON_QT_MIN_VERSION` | `6.8` | 最低 Qt 版本 |
| `LEMON_BUILD_INFO` | 空 | 自定义构建信息 |
| `LEMON_CONFIG_DIR` | 空 | 自定义配置目录 |

应用版本由 [makespec/VERSION](../makespec/VERSION) 与 [makespec/VERSIONSUFFIX](../makespec/VERSIONSUFFIX) 拼接，构建版本取 `git rev-parse --short HEAD`。无法读取 Git 信息时，CMake 使用 `no-git`。

## 平台构建差异

Windows CI 使用 MSVC，启用 XLS 导出时链接 Qt AxContainer。实验性 AppContainer 沙箱的运行要求见 [Windows 沙箱](windows-sandbox.md)。

Linux 链接 Qt DBus，并使用 `watcher_linux.cpp` 监控资源；默认栈空间与内存限制相同，推荐静态链接分发。macOS 链接 IOKit 与 CoreFoundation，并以 Objective-C++ 编译 `watcher_macos.mm`；Apple Silicon 的评测稳定性仍需针对目标环境验证。

Unix 构建将 `watcher_unix` 嵌入 `watcher.qrc`。主程序与 `lemon-base`、`lemon-core` 共用 `src/pch.h` 中的 QtCore、QtGui 预编译头。界面和资源组织见 [编码约定](coding-conventions.md)。

## 测试

测试框架为 Qt Test，通过 CTest 执行：

```bash
ctest --test-dir build --output-on-failure
```

| 目录 | 覆盖范围 |
| --- | --- |
| `tests/integrated-test/` | CDF 往返、编译器配置兼容与比赛评测集成测试 |
| `tests/windows-sandbox/` | Windows 隔离、句柄、权限复用、资源限制、取消及语言兼容 |
| `tests/sleep-inhibitor/` | Windows 电源请求重复申请、嵌套释放及句柄回收 |
| `unix/test/` | watcher 的测试配置与程序 |

`integrated-test` 要求 `g++` 和 `python3` 或 `python` 位于 `PATH`，缺少工具会使评测测试失败。测试从源码目录读取样例，仅在评测时复制到临时目录，并在退出时恢复工作目录。CTest 总超时为 120 秒。单独构建和执行：

```bash
cmake --build build --target integrated-test
ctest --test-dir build -R '^integrated-test$' --output-on-failure
```

测试精简的故障注入与消融记录见 [2026-10-10 集成测试验证](reviews/2026-10-10-integrated-test-ablation.md)。

沙箱与休眠控制测试仅在 Windows 构建。沙箱测试使用本机 C、C++、Java、Python 工具，缺少对应工具时跳过该项。运行 Qt 测试前须使动态库可供加载，Windows CI 使用 `windeployqt` 准备测试依赖。macOS CI 中的测试步骤当前关闭，构建成功与测试通过须分别说明。

## CI 配置

配置保存在 [.github/workflows](../.github/workflows)。

| 文件 | 用途 |
| --- | --- |
| `windows-qt6.yml` | Windows MSVC 与 Qt 6.12.0 构建 |
| `linux-static-qt6.yml` | Linux 静态 Qt6 构建 |
| `macos-qt6.yml` | macOS Qt6，x64 与 arm64 构建 |
| `cpack-deb-debian.yml` | Debian testing 与 sid 的 DEB 包 |
| `check_format.yml` | clang-format 检查 |

四个主程序 CI 均构建 `Release`、`RelWithDebInfo` 和 `Debug`。源码包、用户手册与 watcher 另有专用配置。

## 产物命名与上传

四个主程序 CI 共用 [.github/actions/artifact-name](../.github/actions/artifact-name/action.yml) 生成名称：

```text
lemonlime-[version]-[win/linux/macos]-[x64/arm64]-[release/relwithdebinfo/debug]-qt[qt-version]-[commithash]
```

项目版本取上述两个版本文件的拼接结果；Qt 完整版本由相应的 `qmake -query QT_VERSION` 获取；提交哈希取检出代码的短哈希。Debian 文件在提交哈希后追加 `-testing` 或 `-sid`，区分两个发行版的产物。

| 产物 | 扩展名 |
| --- | --- |
| Windows 压缩包 | `.zip` |
| macOS 磁盘映像 | `.dmg` |
| Debian 软件包 | `.deb` |
| Linux 静态可执行文件 | 无扩展名 |

打包后的实际文件、Actions 下载文件和 Release 资产使用同一名称。Actions 采用 `actions/upload-artifact@v7` 的 `archive: false` 单文件模式；Release 上传检查 `github.ref` 的标签前缀，覆盖标签推送与 Release 发布事件。重复发布允许覆盖同名资产。源码包和手册保留各自的命名约定。
