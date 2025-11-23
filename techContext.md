# 技术栈与构建约束

- 语言与框架：C++17，Qt Widgets；默认依赖 Qt 6.8+，可通过 `-DLEMON_QT_MAJOR_VERSION=<6|7>` 选择更高 Qt 主版本。少用异常，偏好值语义错误处理。
- 构建系统：CMake 3.16+；推荐 Ninja；产物放 `../build`。主要开关：`EMBED_TRANSLATIONS`、`EMBED_DOCS`、`ENABLE_XLS_EXPORT`（Win）、`BUILD_DEB`、`BUILD_RPM`、`ENABLE_LTO`、`ENABLE_CCACHE`、`LEMON_QT6`。编译命令导出开启。
- 第三方依赖：SingleApplication（单实例），spdlog（日志），Qt Core/Gui/Widgets；可选 XLS 导出依赖；打包脚本位于 `cmake/platforms/*.cmake` 与 `makespec/*`。
- 目录布局：`src`（UI/业务），`src/core`（评测模型与线程）、`src/base`（设置/翻译/日志/工具）、`src/component/exportutil`（导出）、`src/forms`（UI 设计稿）；`translations`（多语言），`assets`（图标/桌面文件），`manual`（LaTeX 手册），`unix/test`（限额与运行自测脚本）。
- 平台注意：Windows 指定字体/Hinting；Linux/macOS watcher/limits 在 `unix/watcher_*.cpp/mm`。日志与缓存位于用户数据目录。
