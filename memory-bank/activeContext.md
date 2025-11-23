# Active Context - 当前工作重点

## 当前工作重点
### 代码质量修复 🔧 (2025-10-28)
- [x] 修复 test_contestant.cpp 中 Qt 容器操作符优先级问题 (253-255, 268-269)
- [x] 修复 src/core/testcase.cpp 中输出文件路径处理的严重 bug (121)
- [x] Linux 下添加 bwrap 存在性检查 (judgingthread.cpp)

### 构建系统改进 🔧 (2025-10-28)
- [x] 修改 CMakeLists.txt 使用 git commit hash 作为 buildversion
  - 替换自增数字版本号为 git commit hash
  - 添加 git 命令执行获取短 hash
  - 提供 fallback 机制（"no-git"当 git 不可用时）
- [x] 修改 pre-commit hook 移除自增逻辑
- [x] 更新 resource.qrc 移除 BUILDVERSION 文件引用
- [x] 修复编译错误（宏定义缺少引号、QString::number 使用不当）
  - CMakeLists.txt:159 添加引号到宏定义
  - lemon.cpp:1002 修复语法错误（多余括号）
  - lemon.cpp:1006 使用 QString() 替代 QString::number()

### 核心模块单元测试实施 ✅ (2025-10-28)
- [x] 分析 core 模块的核心类和测试需求
- [x] 创建 tests/core 目录结构
- [x] 编写 Contest 类的单元测试 (test_contest.cpp)
- [x] 编写 Contestant 类的单元测试 (test_contestant.cpp)
- [x] 编写 Task 类的单元测试 (test_task.cpp)
- [x] 编写 TestCase 类的单元测试 (test_testcase.cpp)
- [x] 编写 JudgingController 类的单元测试 (test_judgingcontroller.cpp)
- [x] 创建测试数据文件 (valid_contest.json, invalid_contest.json)
- [x] 配置 CMakeLists.txt 集成测试构建
- [x] 更新主 CMakeLists.txt 支持测试构建 (-DENABLE_TESTS=ON)
- [x] 编写测试说明文档 (README.md)

### 成绩导出功能重构 (待处理)
- [x] 分析现有exportutil.cpp实现
  - 发现大量硬编码HTML字符串，维护困难
  - getContestantHtmlCode 和 getSmallerContestantHtmlCode 存在大量重复代码
  - 直接耦合了 UI 逻辑和数据导出逻辑
- [ ] 设计最小化改动方案
  - 提取公共的 HTML 生成辅助函数
  - 将数据准备与 HTML 生成分离
  - 考虑使用简单的模板机制或结构化构建器
- [ ] 重构HTML生成逻辑
- [ ] 优化代码结构和可维护性
- [ ] 保持原始配色方案
- [ ] 生成单文件HTML输出

### 记忆库初始化 (已完成)
- [x] 创建项目基础文档 (projectbrief.md)
- [x] 创建产品背景文档 (productContext.md)
- [x] 创建系统架构文档 (systemPatterns.md)
- [x] 创建技术上下文文档 (techContext.md)
- [x] 创建进度跟踪文档 (progress.md)

### 项目状态概览
**当前阶段**：记忆库初始化阶段
**主要任务**：建立项目文档体系和知识库
**优先级**：高 - 为后续开发工作奠定基础

## 最近变更
- 创建了 memory-bank 目录结构
- 完成了项目基础文档，明确了 LemonLime 作为离线竞赛编程评测系统的定位
- 完成了产品背景文档，强调了系统的离线特性和评测准确性、稳定性的核心指标
- **规则导入**: 读取并整合了 `.clinerules` 和 `claude.md` 中的开发规范到 `techContext.md`，确立了 C++ 编码标准（无异常、RAII、命名规范）和工作流约定（构建目录、提交规范）。

## 下一步计划
1. 完成剩余的记忆库文档创建
2. 分析现有代码结构和架构模式
3. 识别关键技术决策和设计模式
4. 建立开发工作流和最佳实践

## 活跃决策与考虑

### 技术架构决策
- **离线优先策略**：确保系统在没有网络连接的情况下完全可用
- **Qt框架选择**：利用Qt的跨平台特性，支持Windows/Linux/macOS
- **CMake构建系统**：提供现代化的构建和依赖管理
- **模块化设计**：支持未来的在线评测和多机分布式扩展

### 质量目标
- **评测准确性**：99.9%+ 的准确率是首要目标
- **系统稳定性**：确保比赛期间零故障运行
- **用户体验**：直观的界面和流畅的操作流程

### 扩展性考虑
- 保持架构的灵活性，为未来添加在线评测功能预留接口
- 设计可插拔的编译器和评测引擎架构
- 考虑多语言支持和国际化需求

## 重要模式与经验总结

### 项目模式识别
1. **竞赛评测模式**：多题目、多测试用例、时间限制的评测流程
2. **离线工作模式**：无需网络依赖，确保数据安全和稳定性
3. **跨平台模式**：统一的代码库支持多个操作系统
4. **插件化模式**：支持多种编程语言和编译器的动态加载

### 关键经验
- 评测系统的核心在于准确性和稳定性，功能丰富度次之
- 离线特性是重要竞争优势，不能为了在线功能而妥协
- 用户体验对于教育场景至关重要，需要特别关注易用性
- 开源社区驱动的发展模式有助于长期可持续性

## 待解决问题
1. 需要深入了解现有代码架构和实现细节
2. 需要建立完整的开发和测试工作流
3. 需要制定代码质量标准和审查流程
4. 需要规划版本发布和更新策略

## 风险评估
- **技术风险**：Qt版本兼容性、跨平台实现的复杂性
- **质量风险**：评测准确性的验证和测试覆盖度
- **进度风险**：功能开发与技术债务的平衡
- **社区风险**：开源项目的长期维护和社区建设
