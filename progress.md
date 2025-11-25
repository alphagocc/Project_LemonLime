# 进度记录

- 2025-11-23：补全中文记忆库（项目简述、产品背景、系统模式、技术栈、当前上下文），同步 TODO/约束，未改动代码逻辑。
- 2025-11-24：`AddCompilerWizard` 改为使用 `QStandardPaths` 自动查找编译器/解释器路径，合并 Win/Linux 逻辑；同步记忆库。
- 2025-11-25：修复 CI `check_commit_msg` 读取多行提交信息问题（使用 heredoc 输出、PR HEAD SHA）；添加 core QTest 集成测试骨架（未实现具体用例）。
