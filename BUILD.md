# 从源码构建 LemonLime

本文对应当前仓库的 CMake 构建配置。预编译程序见 [GitHub Releases](https://github.com/Project-LemonLime/Project_LemonLime/releases)。

## 构建依赖

| 依赖 | 要求 |
| --- | --- |
| CMake | 推荐 3.20 或更高；项目最低要求为 3.16，还须满足所选 Qt 工具包的要求 |
| C++ 编译器 | 支持 C++17，使用与 Qt 工具包匹配的 MSVC、GCC 或 Clang |
| Qt | 6.8 或更高，当前 Windows 和 macOS CI 使用 6.9.3 |
| Qt 模块 | Core、Gui、Widgets、Network、Svg、Qml、Quick、QuickControls2、LinguistTools、Test |
| Linux 附加模块 | Qt DBus，用于评测期间向系统申请阻止休眠 |
| 构建工具 | Ninja，或 CMake 支持的其他生成器 |
| Git | 用于获取源代码、子模块及构建版本号 |

Qt Network 由 SingleApplication 使用，Qt Test 是当前 CMake 测试目标的必需依赖。Windows 的 XLS 导出还需要 Qt ActiveQt 中的 AxContainer 模块，默认关闭。

应用界面使用 Qt Quick Controls。Qt 工具包须包含 qtdeclarative 及 QtQuick、QtQuick.Controls、QtQuick.Layouts、QtQuick.Dialogs、QtCore 的 QML 模块。Windows 运行与部署还需要 `QtQuick.Controls.FluentWinUI3` 及其依赖模块，以提供随应用深浅配色切换的 Windows 11 控件。QML 源文件通过 `qt_add_qml_module` 编译并嵌入可执行文件。

项目将 SingleApplication 和 spdlog 作为 Git 子模块构建。用户提交程序所需的 C、C++、Java 或 Python 工具应另外安装并在 LemonLime 中配置。

## 获取源代码

```bash
git clone --recursive https://github.com/Project-LemonLime/Project_LemonLime.git
cd Project_LemonLime
```

对于现有仓库，在切换分支或更新代码后同步子模块：

```bash
git submodule update --init --recursive
```

如果只需要当前版本，可以在克隆命令中添加 `--depth 1 --shallow-submodules`。GitHub 自动生成的源码压缩包缺少子模块内容，应优先使用 Git 克隆。

## 通用构建命令

在仓库根目录执行，将生成文件保存在独立的 `build` 目录中：

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

如果 CMake 未找到 Qt，配置时添加 `-DCMAKE_PREFIX_PATH="Qt工具包目录"`。该目录应包含 `bin`、`include` 和 `lib`，例如 `C:/Qt/6.9.3/msvc2022_64`。也可以通过 `Qt6_DIR` 指定包含 `Qt6Config.cmake` 的 `lib/cmake/Qt6` 目录。

Ninja 使用 `CMAKE_BUILD_TYPE` 选择 `Release`、`RelWithDebInfo` 或 `Debug`。使用 Visual Studio 等多配置生成器时，在构建、测试和安装命令中分别指定 `--config Release`、`-C Release` 和 `--config Release`。

更换编译器、Qt 工具包或目标架构时，应使用新的构建目录。

## Windows

安装 Visual Studio 的“使用 C++ 的桌面开发”组件、Windows SDK、CMake、Ninja，以及相应架构的 Qt MSVC 工具包。在 Visual Studio 的 **Developer PowerShell** 中执行：

```powershell
cmake -S . -B build -G Ninja `
    -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_PREFIX_PATH="C:/Qt/6.9.3/msvc2022_64"
cmake --build build --parallel
```

将示例 Qt 目录替换为本机安装位置。生成的程序为 `build/lemon.exe`。动态链接 Qt 时，使用同一工具包的 `windeployqt` 配置运行依赖：

```powershell
& "C:/Qt/6.9.3/msvc2022_64/bin/windeployqt.exe" --release --qmldir src/qml build/lemon.exe
& ./build/lemon.exe
```

分发时应将 `lemon.exe` 复制到独立目录，再对该目录中的程序运行 `windeployqt`，并包含生成的 Qt DLL、平台插件、样式和图像插件。编译器运行库同样需要满足部署要求。当前 Windows CMake 配置未提供主程序安装规则。

启用 XLS 导出时，安装相应 Qt 版本的 ActiveQt 模块，再配置：

```powershell
cmake -S . -B build -DENABLE_XLS_EXPORT=ON
cmake --build build --parallel
```

Qt 6 支持这一选项，运行 XLS 导出还需要本机安装 Microsoft Excel。

实验性 Windows AppContainer 沙箱包含在 Windows 构建中，通过高级编译器设置按编译器启用，默认关闭。启用后，每个提交程序仅允许一个活动进程。需要创建子进程的 Python 启动器和 Windows venv 应配置为基础 Python 等单进程入口。运行环境的准备、权限与验证记录见 [Windows 沙箱说明](docs/windows-sandbox-notes.md)。

## Linux

安装发行版提供的 C++ 工具链、CMake、Ninja 和 Qt 开发包。常见发行版的包名示例：

```bash
# Arch Linux
sudo pacman -S --needed base-devel cmake ninja qt6-base qt6-tools qt6-svg qt6-declarative

# Debian 或 Ubuntu
sudo apt install build-essential cmake ninja-build pkg-config lsb-release \
    qt6-base-dev qt6-tools-dev qt6-tools-dev-tools qt6-l10n-tools qt6-svg-dev \
    qt6-declarative-dev qml6-module-qtquick qml6-module-qtquick-controls \
    qml6-module-qtquick-layouts qml6-module-qtquick-dialogs qml6-module-qtquick-window \
    qml6-module-qtquick-templates qml6-module-qtqml-workerscript qml6-module-qtcore
```

发行版仓库中的 Qt 必须满足 6.8 的最低版本。对于提供较早版本 Qt 的发行版，应安装符合要求的 Qt 工具包，并设置 `CMAKE_PREFIX_PATH`。Qt DBus 通常由 Qt base 开发包提供，自行构建 Qt 时也须包含该模块。

完成依赖安装后，执行通用构建命令。主程序为 `build/lemon`；CMake 自动构建 `watcher_unix` 并嵌入程序，用于运行时间和内存监控。

```bash
./build/lemon
cmake --install build --prefix "$HOME/.local"
```

动态构建依赖目标系统中的相应 Qt 库和平台插件。静态构建需要预先构建的静态 Qt 工具包，通过 `CMAKE_PREFIX_PATH` 选择；项目的 Linux CI 使用单独维护的 Qt 6.9 静态工具包，配置见 [linux-static-qt6.yml](.github/workflows/linux-static-qt6.yml)。静态链接 Qt 后仍应检查目标系统库与运行环境兼容性。

“评测时阻止系统休眠”通过系统 D-Bus 的 `org.freedesktop.login1` 服务申请。服务缺失或申请失败时会记录日志，评测继续执行。

### DEB 和 RPM 打包

在相应发行版中安装 `dpkg-deb` 或 RPM 构建工具，再执行 CPack 的 `package` 目标。构建主程序的默认目标本身不会生成安装包。

```bash
# DEB
cmake -S . -B build-deb -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_DEB=ON
cmake --build build-deb --target package --parallel

# RPM
cmake -S . -B build-rpm -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_RPM=ON
cmake --build build-rpm --target package --parallel
```

安装包生成于对应构建目录。当前 Debian CI 针对 testing 和 sid 构建 DEB，配置见 [cpack-deb-debian.yml](.github/workflows/cpack-deb-debian.yml)。

## macOS

安装 Xcode Command Line Tools 和 Homebrew，然后安装构建依赖：

```bash
xcode-select --install
brew install cmake ninja qt
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build --parallel
open build/lemon.app
```

CMake 自动使用 `unix/watcher_macos.mm` 构建 macOS watcher。休眠控制使用系统 SDK 提供的 IOKit 与 CoreFoundation。Qt 工具包和目标架构应匹配；需要指定架构时添加 `-DCMAKE_OSX_ARCHITECTURES=arm64` 或 `-DCMAKE_OSX_ARCHITECTURES=x86_64`。

分发动态链接的应用时执行：

```bash
"$(brew --prefix qt)/bin/macdeployqt" build/lemon.app -dmg
```

应用程序和 DMG 位于 `build` 目录。项目 CI 构建 Intel 与 Apple Silicon 版本；macOS CI 当前停用了测试执行，因此构建通过后仍应在目标机器验证评测功能。

## 测试

当前 CMake 配置始终包含测试目标。完成构建后，在配置过运行依赖的环境中执行：

```bash
ctest --test-dir build --output-on-failure
```

比赛集成测试 `test1` 要求 `g++` 和 `python3` 或 `python` 可通过 `PATH` 找到。Windows 沙箱测试会使用本机的 C、C++、Python 和 Java；缺少特定工具时跳过相应语言测试。沙箱测试会临时修改运行环境文件权限，并在测试结束时撤销本次授权。Windows 还包含 `sleep-inhibitor` 测试，检查休眠请求的重复申请与释放。

Windows 动态 Qt 构建可以将所选 Qt 工具包的 `bin` 目录添加到当前终端的 `PATH`，供各测试程序加载 DLL：

```powershell
$env:PATH = "C:/Qt/6.9.3/msvc2022_64/bin;" + $env:PATH
ctest --test-dir build --output-on-failure
```

另行部署 Debug 程序时应使用 `windeployqt --debug`。Linux 无图形桌面的测试环境可以设置 `QT_QPA_PLATFORM=offscreen`。

`ctest --test-dir` 要求 CMake 3.20 或更高。使用较早版本时，进入构建目录执行 `ctest --output-on-failure`。

## CMake 选项

| 选项 | 默认值 | 用途 |
| --- | --- | --- |
| `EMBED_TRANSLATIONS` | `ON` | 将翻译文件嵌入主程序 |
| `EMBED_DOCS` | `ON` | 将现有 PDF 手册嵌入主程序 |
| `ENABLE_XLS_EXPORT` | `OFF` | 启用 Windows XLS 导出，需要 AxContainer |
| `ENABLE_LTO` | `ON` | 编译器支持时启用链接时优化 |
| `ENABLE_CCACHE` | `OFF` | 现有可选配置，实际编译器缓存请参见下文 |
| `BUILD_DEB` | `OFF` | 选择 CPack DEB 格式 |
| `BUILD_RPM` | `OFF` | 选择 CPack RPM 格式 |
| `LEMON_QT_MAJOR_VERSION` | `6` | Qt 主版本号 |
| `LEMON_QT_MIN_VERSION` | `6.8` | Qt 最低版本 |
| `LEMON_BUILD_INFO` | 自动生成 | 自定义构建说明 |
| `LEMON_BUILD_EXTRA_INFO` | 自动生成 | 自定义附加构建说明 |
| `LEMON_CONFIG_DIR` | 空 | 自定义应用配置目录 |

降低 `LEMON_QT_MIN_VERSION` 仅改变依赖检查，无法使使用较新 Qt API 的代码兼容旧版。当前 CI 验证 Qt 6，其他主版本须单独验证。

`ENABLE_CCACHE` 当前仅设置内部变量，未连接到目标的编译器启动器。使用 CMake 标准参数配置 ccache：

```bash
cmake -S . -B build -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
```

Linux 安装规则会在关闭嵌入时安装外置翻译和手册。Windows 与 macOS 分发建议保留默认嵌入设置，外置资源需要另外配置。

## 翻译与用户手册

CMake 构建时通过 LinguistTools 将 `translations/*.ts` 编译为 `.qm`，默认嵌入程序。修改界面文本后，使用所选 Qt 工具包 `bin` 目录中的 `lupdate` 提取条目，再补充各语言翻译：

```bash
lupdate src 3rdparty/SingleApplication -ts translations/en_US.ts translations/zh_CN.ts translations/zh_TW.ts -locations none -no-obsolete
```

Windows 可以使用 `& "C:/Qt/6.9.3/msvc2022_64/bin/lupdate.exe"` 调用工具，再附加同样的参数。当前 CMake 的 `lupdate` 目标依赖 `Qt_DIR`，首次仅使用 `CMAKE_PREFIX_PATH` 配置时可能缺少该变量，因此此处采用工具包中的程序。

普通应用构建使用仓库中的 `manual/llmanual.pdf`，无须安装 Typst。修改手册源文件时，按 [manual/README.md](manual/README.md) 准备字体并重新生成 PDF。

版本号由 `makespec/VERSION` 和 `makespec/VERSIONSUFFIX` 组合，构建版本使用当前 Git 提交的短哈希。
