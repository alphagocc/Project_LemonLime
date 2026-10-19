# 构建与 CI

各平台依赖安装与部署命令见 [BUILD.md](../BUILD.md)。本页维护项目的构建选项、测试入口与 CI 产物约定。

## 依赖与构建

CMake 最低版本为 3.20，C++ 编译器须支持 C++23。推荐使用 Ninja。Qt 要求 6.8 或更高，可通过 `LEMON_QT_MAJOR_VERSION` 指定主版本。

主程序与独立 watcher 测试均设置 `CMAKE_CXX_STANDARD=23` 并要求对应标准模式。CMake 根据编译器版本选择标准参数；MSVC 可能使用 `/std:c++latest`。具体语言和标准库特性的可用范围仍取决于编译器、标准库和目标系统版本。

| Qt 模块            | 用途                             |
| ------------------ | -------------------------------- |
| Core、Gui、Widgets | 核心库和界面                     |
| Concurrent         | 线程池评测任务与 QFuture         |
| Network            | SingleApplication 单实例保护     |
| Svg                | SVG 图标插件                     |
| LinguistTools      | 翻译编译                         |
| Test               | Qt Test 测试                     |
| DBus               | Linux 评测期间阻止系统休眠       |
| AxContainer        | Windows XLS 导出，按构建选项启用 |

SingleApplication 和 spdlog 作为 Git 子模块位于 `3rdparty/`。源码获取与常规构建：

```bash
git clone https://github.com/Project-LemonLime/Project_LemonLime.git --recursive
cd Project_LemonLime
cmake -B build -DCMAKE_BUILD_TYPE=Release -GNinja
cmake --build build --parallel
```

现有仓库使用 `git submodule update --init --recursive` 同步依赖。

## CMake 选项

| 选项                     | 默认值 | 说明             |
| ------------------------ | ------ | ---------------- |
| `EMBED_TRANSLATIONS`     | `ON`   | 嵌入翻译文件     |
| `EMBED_DOCS`             | `ON`   | 嵌入用户手册     |
| `ENABLE_XLS_EXPORT`      | `OFF`  | Windows XLS 导出 |
| `ENABLE_LTO`             | `ON`   | 链接时优化       |
| `ENABLE_CCACHE`          | `OFF`  | ccache 编译缓存  |
| `BUILD_DEB`              | `OFF`  | 构建 DEB 包      |
| `BUILD_RPM`              | `OFF`  | 构建 RPM 包      |
| `LEMON_QT_MAJOR_VERSION` | `6`    | Qt 主版本        |
| `LEMON_QT_MIN_VERSION`   | `6.8`  | 最低 Qt 版本     |
| `LEMON_BUILD_INFO`       | 空     | 自定义构建信息   |
| `LEMON_CONFIG_DIR`       | 空     | 自定义配置目录   |

应用版本由 [makespec/VERSION](../makespec/VERSION) 与 [makespec/VERSIONSUFFIX](../makespec/VERSIONSUFFIX) 拼接，构建版本取 `git rev-parse --short HEAD`。无法读取 Git 信息时，CMake 使用 `no-git`。

## 平台构建差异

Windows CI 使用 MSVC，分别在 `windows-latest` 和 `windows-11-arm` 上构建 x64 与 ARM64。Qt 使用对应的 `win64_msvc2022_64` 和 `win64_msvc2022_arm64` 工具包；ARM64 安装来源为 `windows_arm64`。两种架构均启用 XLS 导出并链接 Qt AxContainer。实验性 AppContainer 沙箱的运行要求见 [Windows 沙箱](windows-sandbox.md)。

Ubuntu ARM64 CI 使用 `ubuntu-26.04-arm`，安装 Ubuntu 26.04 提供的 GCC 与 Qt 开发包，以原生编译生成 DEB。包安装前缀为 `/usr`，CPack 通过 `dpkg-shlibdeps` 补充共享库依赖，并在上传前检查包架构为 `arm64`。

Debian 与 Linux 静态构建使用容器，在 checkout 后将 `$GITHUB_WORKSPACE` 加入容器用户的 Git `safe.directory`。checkout 自身的信任设置使用临时配置，后续产物命名与 CMake 获取提交哈希时需要容器用户的配置。

CPU 核心检测要求使用包含 `PROCESSOR_RELATIONSHIP::EfficiencyClass` 定义的 Windows 10 SDK 或更新版本。CMake 在配置阶段编译检查该字段和 `GetLogicalProcessorInformationEx`，并在 SDK 缺少这些声明时报告错误。API 声明要求 `_WIN32_WINNT >= 0x0601`；项目通过 Qt 的 `qt_windows.h` 使用 Qt 默认的 Windows 目标定义。系统接口从 Windows 7 开始提供，`EfficiencyClass` 的核心分类语义从 Windows 10 开始提供。

Linux 链接 Qt DBus，并使用 `watcher_linux.cpp` 监控资源；默认栈空间与内存限制相同，推荐静态链接分发。macOS 链接 IOKit 与 CoreFoundation，并以 Objective-C++ 编译 `watcher_macos.mm`；Apple Silicon 的评测稳定性仍需针对目标环境验证。

Unix 构建将 `watcher_unix` 嵌入 `watcher.qrc`。主程序与 `lemon-base`、`lemon-core` 共用 `src/pch.h` 中的 QtCore、QtGui 预编译头。界面和资源组织见 [编码约定](coding-conventions.md)。

基础层和核心层的源文件仅在对应静态库中编译，主程序链接这两个库。启用 `ENABLE_LTO` 且编译器支持时，主程序和两个库均启用 IPO，保持跨层优化。`integrated-test` 复用 `lemon-core` 的预编译头，并同步其 IPO 设置以保持编译选项兼容。

## 测试

测试框架为 Qt Test，通过 CTest 执行：

```bash
ctest --test-dir build --output-on-failure
```

| 目录                     | 覆盖范围                                                                  |
| ------------------------ | ------------------------------------------------------------------------- |
| `tests/integrated-test/` | CDF 往返、编译器配置兼容与比赛评测集成测试                                |
| `tests/judging-controller/` | 并发上限、GUI 结果应用、空批次、重复评测及取消后的进程与任务清理 |
| `tests/cpu-topology/`    | 默认评测线程策略、物理核心与 SMT 计数、Linux sysfs 核心类别和检测信息缺失 |
| `tests/windows-sandbox/` | Windows 隔离、句柄、权限复用、资源限制、取消及语言兼容                    |
| `tests/sleep-inhibitor/` | Windows 电源请求重复申请、嵌套释放及句柄回收                              |
| `unix/test/`             | watcher 的测试配置与程序                                                  |

`integrated-test` 要求 `g++` 和 `python3` 或 `python` 位于 `PATH`，缺少工具会使评测测试失败。测试从源码目录读取样例，仅在评测时复制到临时目录，并在退出时恢复工作目录。CTest 总超时为 120 秒。单独构建和执行：

```bash
cmake --build build --target integrated-test
ctest --test-dir build -R '^integrated-test$' --output-on-failure
```

整场评测样例分别使用 1 个和 3 个并行任务，核对相同的预期结果及 GUI 线程中的完成通知。`judging-controller` 使用测试程序自身的辅助进程模式验证实际并发、取消和进程退出，避免依赖额外的编译器。

测试精简的故障注入与消融记录见 [2026-10-10 集成测试验证](reviews/2026-10-10-integrated-test-ablation.md)。

沙箱与休眠控制测试仅在 Windows 构建。沙箱测试使用本机 C、C++、Java、Python 工具，缺少对应工具时跳过该项。运行 Qt 测试前须使动态库可供加载，Windows CI 使用 `windeployqt` 为每个 Qt 测试程序准备依赖。Windows ARM64 的主程序、测试程序和 Python 均为 ARM64，C++ 评测样例使用 runner 提供的 x64 MinGW，通过 Windows 仿真执行。Ubuntu ARM64 执行主项目 CTest 后生成 DEB。macOS CI 中的测试步骤当前关闭，构建成功与测试通过须分别说明。

## CI 配置

配置保存在 [.github/workflows](../.github/workflows)。

| 文件                   | 用途                            |
| ---------------------- | ------------------------------- |
| `windows-qt6.yml`      | Windows x64 与 ARM64，MSVC 与 Qt 6.12.0 构建 |
| `linux-static-qt6.yml` | Linux 静态 Qt6 构建             |
| `macos-qt6.yml`        | macOS Qt6，x64 与 arm64 构建    |
| `cpack-deb-debian.yml` | Debian testing 与 sid 的 DEB 包 |
| `cpack-deb-ubuntu-arm64.yml` | Ubuntu 26.04 ARM64 的 DEB 包 |
| `check_format.yml`     | clang-format 检查               |

五个主程序 CI 均构建 `Release`、`RelWithDebInfo` 和 `Debug`，构建矩阵设置 `fail-fast: false`，单项失败时其余任务继续执行。源码包、用户手册与 watcher 另有专用配置。

## 产物命名与上传

五个主程序 CI 共用 [.github/actions/artifact-name](../.github/actions/artifact-name/action.yml) 生成名称：

```text
lemonlime-[version]-[win/linux/macos]-[x64/arm64]-[release/relwithdebinfo/debug]-qt[qt-version]-[commithash]
```

项目版本取上述两个版本文件的拼接结果；Qt 完整版本由相应的 `qmake -query QT_VERSION` 获取；提交哈希取检出代码的短哈希。Debian 文件在提交哈希后追加 `-testing` 或 `-sid`，Ubuntu ARM64 文件追加 `-ubuntu-26.04`，区分发行版的产物。

| 产物                 | 扩展名   |
| -------------------- | -------- |
| Windows 压缩包       | `.zip`   |
| macOS 磁盘映像       | `.dmg`   |
| Debian 软件包        | `.deb`   |
| Linux 静态可执行文件 | 无扩展名 |

打包后的实际文件、Actions 下载文件和 Release 资产使用同一名称。Actions 采用 `actions/upload-artifact@v7` 的 `archive: false` 单文件模式；Release 上传检查 `github.ref` 的标签前缀，覆盖标签推送与 Release 发布事件。重复发布允许覆盖同名资产。源码包和手册保留各自的命名约定。
