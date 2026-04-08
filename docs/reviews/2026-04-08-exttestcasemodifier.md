# Code Review: ExtTestCaseModifier 子系统 — 2026-04-08

审计范围：外部测试数据编辑子系统  
涉及文件：

- `src/exttestcasemodifier.h` / `.cpp`
- `src/exttestcasetable.h` / `.cpp`
- `src/exttestcaseupdaterdialog.h` / `.cpp`

审计维度：DRY、Dead Code、Modernization/Optimization、Memory Safety

---

## 架构概览

```
ExtTestCaseModifierDialog (QDialog, 在 summarytree.cpp 中创建)
  └── ExtTestCaseModifier (QWidget)
        ├── ExtTestCaseTable (QTableWidget)  — 显示 + 选区跟踪 + 操作可用性判断
        └── 8 个按钮 → 对应 8 个 slot
              └── ExtTestCaseUpdaterDialog (QDialog)  — 编辑单个测试点属性

数据模型:
  Task → QList<TestCase*>
    TestCase: score, timeLimit, memoryLimit, inputFiles, outputFiles, dependenceSubtask
```

选区模型有两层语义：
- **haveSub** (`QList<int>`)：选中的 subtask 索引（整行选中）
- **resSub** (`QList<pair<int, pair<int,int>>>`)：部分选中某 subtask 内的具体测试点范围

几乎所有操作都需根据 `res.empty()` 分支处理这两层语义，是复杂度的主要来源。

---

## Findings (按严重级别)

### High

1. **subtask 依赖重映射逻辑重复 3 次（DRY + Correctness）**  
   位置：`src/exttestcasemodifier.cpp:137-172`（moveUpSelected）、`src/exttestcasemodifier.cpp:200-248`（moveDownSelected）、`src/exttestcasemodifier.cpp:285-301`（removeSelected）  
   证据：三处均遍历 task 的 testCaseList，对每个 testCase 的 `dependenceSubtask` 做索引重映射，但映射规则因操作不同而存在微妙差异（上移 -1、下移 +1、删除跳过+偏移）。  
   风险：
   - 任何依赖规则的修改需同步三处，极易漂移产生 bug。
   - 三处逻辑的遍历范围也不一致（moveUp 分两段：`[l-1, r-1]` 和 `[r+1, end)`；moveDown 分三段：`{l}` 和 `[l+1, r+1]` 和 `[r+2, end)`；remove 只做 `[l+1, end)`），增加理解和验证成本。

   建议：提取统一的依赖重映射函数：
   ```cpp
   void remapDependencies(Task *task, int rangeStart, int rangeEnd,
                          std::function<int(int)> indexMapper);
   ```
   各操作只需提供不同的 `indexMapper` lambda。

2. **原始指针手动 new/delete 管理 dialog（Memory Safety）**  
   位置：`src/exttestcasemodifier.cpp:66,86`、`src/exttestcasemodifier.cpp:88,100`、`src/exttestcasemodifier.cpp:105,112`（delete 缺失）、`src/exttestcasemodifier.cpp:378,393`、`src/exttestcasemodifier.cpp:401,408`  
   证据：5 处 `new ExtTestCaseUpdaterDialog(...)` + 手动 `delete dialog`。其中 `modifySelected()` 的第三分支（line 105-112）在 `if (dialog->exec() == QDialog::Accepted)` 后没有 `delete dialog`，存在内存泄漏。  
   风险：泄漏路径已存在；后续维护者添加 early return 时极易遗漏 delete。  
   建议：改用栈上构造或 `std::unique_ptr<ExtTestCaseUpdaterDialog>`：
   ```cpp
   auto dialog = std::make_unique<ExtTestCaseUpdaterDialog>(...);
   if (dialog->exec() == QDialog::Accepted) { ... }
   // 自动释放
   ```

3. **`removeSelected` 中 `ban1`/`ban2` 逻辑脆弱且难以理解（Correctness）**  
   位置：`src/exttestcasemodifier.cpp:260-283`  
   证据：
   - `ban1`、`ban2` 用于标记"仅部分删除文件、不整体删除"的 subtask 索引，但只用两个 int 变量，硬编码最多处理 2 个部分选中的 subtask。
   - 删除循环 `while (l == ban1 || l == ban2) l++;` 和 `while (r == ban1 || r == ban2) r--;` 假设 ban 只出现在两端。
   - 若选区包含 3 个以上部分选中的 subtask（虽然当前 UI 逻辑可能阻止），此处会产生错误删除。
   - `hav.front()` / `hav.back()` 在 `hav` 经 ban 过滤后可能导致越界。

   建议：
   - 用 `QSet<int> bannedSubtasks` 替代 `ban1`/`ban2`，消除硬编码限制。
   - 分离"删除 subtask 内部分文件"和"删除整个 subtask"为两个阶段，逻辑更清晰。

### Medium

4. **变量命名严重损害可读性（Maintainability）**  
   位置：贯穿 `exttestcasemodifier.cpp` 和 `exttestcasetable.cpp`  
   典型案例：

   | 变量 | 出处 | 含义 |
   |------|------|------|
   | `hav` | modifier 各 slot | 选中的 subtask 索引列表 |
   | `res` | modifier 各 slot | 部分选中的测试点范围 |
   | `dlt` | moveUp/Down | 选区偏移量 delta |
   | `ban1`, `ban2` | removeSelected | 不整体删除的 subtask 索引 |
   | `mi`, `mx` | exttestcasetable.cpp:179 | 选区最小/最大行号 |
   | `noDfs` | exttestcasetable.h:57 | 防止选区变更信号重入的标志 |
   | `xlen` | exttestcasetable.cpp:108 | 当前 subtask 的文件数量 |
   | `a_` | modifier 各处 | 1-indexed 依赖索引（紧接着 `a = a_ - 1` 转 0-indexed） |
   | `nowd` | modifier 各处 | "now dependence" 当前依赖列表 |
   | `havd` | modifier 各处 | "have dependence" 重映射后的依赖集合 |

   建议：统一重命名为语义化名称，如 `selectedSubtasks`、`partialSelections`、`selectionDelta`、`skippedSubtasks`、`minRow`/`maxRow`、`isProcessingSelection`、`fileCount`、`oldIndex`/`newIndex`、`currentDeps`、`remappedDeps`。

5. **Magic constants 作为全局 const int（Modernization）**  
   位置：`src/exttestcaseupdaterdialog.h:20-22`  
   证据：
   ```cpp
   const int NO_EDIT = -400;
   const int MAY_EDIT = -300;
   const int EDIT_WITH_DEFAULT = -200;
   ```
   这些值既作为构造参数传入，又作为返回值（`getScore()` 可能返回 `MAY_EDIT` 表示"未修改"）。同一个 int 参数同时承载"编辑模式"和"实际值"两种语义。  
   风险：调用者传入任何小于 -200 的分数都会被误解释为编辑模式标志；ODR 违规风险（多翻译单元）。  
   建议：
   - 用 `enum class EditMode { NoEdit, MayEdit, EditWithDefault }` 替代。
   - 将"编辑模式"和"初始值"拆分为独立参数或使用 `std::optional<int>`。

6. **`ExtTestCaseTable` 职责过多（Design）**  
   位置：`src/exttestcasetable.h`  
   证据：一个类同时负责：
   - 表格渲染（`refreshTask`、`addItem`）
   - 选区状态跟踪（`haveSub`、`resSub`、`selectMi`、`selectMx`）
   - 8 个操作的可用性判断（`canModify`、`canUp` ... 共 8 个 bool + 对应判断逻辑）
   - 选区修改（`modifySelected`）

   该类有 8 个 bool 状态 + 2 个列表 + 2 个 int + 1 个 reentrancy guard = 13 个内部状态变量，`whenItemSelectionChanged()` 方法 90+ 行。  
   建议：将选区分析与操作可用性判断提取为独立的 `SelectionModel` / `SelectionAnalyzer` 类，`ExtTestCaseTable` 仅负责渲染和基础交互。

7. **逗号运算符滥用（Maintainability）**  
   位置：`src/exttestcaseupdaterdialog.cpp:57,59,61-62,64`  
   证据：
   ```cpp
   ui->labelScore->hide(), ui->lineEditScore->hide(), defScore = NO_EDIT;
   // ...
   score = defScore = nowSettings->getDefaultFullScore(),
   ui->lineEditScore->setText(QString::number(defScore));
   ```
   逗号运算符将多个语句压缩到单行，降低可读性，且在赋值表达式中混用极易出错。  
   建议：拆为独立语句，用大括号包裹分支。

8. **输入/输出文件查找对话框代码重复（DRY）**  
   位置：`src/exttestcaseupdaterdialog.cpp:122-142`（whenButtonFindInputClicked）、`src/exttestcaseupdaterdialog.cpp:144-164`（whenButtonFindOutputClicked）  
   证据：两个方法仅在 filter 标题（"Input Data" vs "Output Data"）、文件扩展名来源（`getInputFileExtensions` vs `getOutputFileExtensions`）和目标控件上不同，其余 20 行完全相同。  
   建议：提取公共方法：
   ```cpp
   QString chooseFile(const QString &title, const QStringList &extensions);
   ```

### Low

9. **`noDfs` 重入守卫存在逻辑缺陷（Dead Code / Correctness）**  
   位置：`src/exttestcasetable.cpp:159-163,252`  
   证据：
   ```cpp
   void ExtTestCaseTable::whenItemSelectionChanged() {
       if (noDfs) return;   // line 160-161
       noDfs = false;       // line 163 — 此处应为 true 才能起到守卫作用！
       // ... 处理逻辑 ...
       noDfs = false;       // line 252 — 结束时释放
   }
   ```
   line 163 设置 `noDfs = false` 而非 `true`，导致守卫完全无效 —— 若 `setRangeSelected()`（line 196-197, 210-211）触发递归的 `itemSelectionChanged` 信号，将无限递归。  
   实际未崩溃可能是因为 Qt 内部的信号合并或 `selectedRanges()` 恰好不再变化，但这是脆弱的巧合。  
   建议：修复为 `noDfs = true;`（line 163），或改用 `QSignalBlocker` RAII 守卫。

10. **magic number 作为哨兵值（Modernization）**  
    位置：`src/exttestcasetable.cpp:179`  
    证据：`int mi = 1e9, mx = -1;` 使用浮点字面量隐式转 int 作为哨兵。  
    建议：使用 `std::numeric_limits<int>::max()` 和 `std::numeric_limits<int>::min()`，或用 `std::optional<int>` 表示"无选区"。

11. **选区 getter 每次返回 QList 拷贝（Optimization）**  
    位置：`src/exttestcasetable.h:33-34`、`src/exttestcasetable.cpp:63-67`  
    证据：
    ```cpp
    QList<int> getSelectedHaveSub() const;
    QList<std::pair<int, std::pair<int, int>>> getSelectedResSub() const;
    ```
    按值返回，每次调用产生深拷贝。`modifySelected()` 等方法中同一函数被调用后立即使用，虽然 Qt 的隐式共享（COW）可缓解，但 `resSub` 类型嵌套较深，拷贝开销非零。  
    建议：返回 `const QList<...> &` 引用。

12. **大括号风格不一致（Style）**  
    位置：贯穿 `exttestcasemodifier.cpp` 和 `exttestcaseupdaterdialog.cpp`  
    证据：同一文件中混用有大括号和无大括号的单行 if/else：
    ```cpp
    if (l != r) {
        // ...
    } else {
        // ...
    }
    // 然后：
    if (! res.empty()) {
        // ...
    } else
        // 无大括号
    ```
    建议：统一使用大括号。

13. **`splitSelected` 分数分配算法缺少注释（Maintainability）**  
    位置：`src/exttestcasemodifier.cpp:347-356`  
    证据：
    ```cpp
    int allScore = now->getFullScore(), gar = 0;
    if (! in.empty())
        gar = in.size() - allScore % in.size();
    else
        gar = 0;
    for (int j = 0; j < in.size(); j++) {
        app->setFullScore((allScore / in.size()) + (j >= gar));
    ```
    这段代码将总分均匀分配到各测试点，余数分配给后面的测试点。`gar` 命名不明（可能是 "gap remaining"？），`(j >= gar)` 作为 bonus 的条件也不直观。  
    建议：重命名 `gar` 为 `baseCaseCount`（获得基础分数的测试点数），并添加简短注释说明分配策略。

---

## 四维度总结

| 维度 | 状态 | 说明 |
|------|------|------|
| **DRY** | 差 | 依赖重映射逻辑重复 3 次（High #1），文件查找对话框重复 2 次（Medium #8） |
| **Dead Code** | 一般 | `noDfs` 守卫逻辑实际无效（Low #9），可视为 dead code |
| **Modernization/Optimization** | 差 | magic constants 作全局 int（Medium #5），浮点哨兵（Low #10），按值返回容器（Low #11），逗号运算符（Medium #7） |
| **Memory Safety** | 差 | 手动 new/delete dialog 且存在泄漏（High #2），`removeSelected` 边界处理脆弱（High #3） |

---

## Action Items

| 优先级 | 项目 | 对应 Finding |
|--------|------|-------------|
| **P0** | 修复 `modifySelected()` 第三分支 dialog 内存泄漏 | High #2 |
| **P0** | 修复 `noDfs` 守卫（line 163 `false` → `true`）| Low #9 |
| **P1** | 所有 dialog 创建改用 `std::unique_ptr` 或栈上构造 | High #2 |
| **P1** | 提取依赖重映射公共函数 | High #1 |
| **P1** | 重构 `removeSelected` 中 `ban1`/`ban2` 为 `QSet` | High #3 |
| **P2** | 全局 const int → `enum class EditMode` | Medium #5 |
| **P2** | 变量重命名（hav/res/dlt/ban/nowd/havd/gar 等） | Medium #4 |
| **P2** | 提取文件查找对话框公共方法 | Medium #8 |
| **P2** | 拆分 `ExtTestCaseTable` 职责 | Medium #6 |
| **P2** | 消除逗号运算符 | Medium #7 |
| **P3** | 哨兵值改用 `std::numeric_limits` 或 `std::optional` | Low #10 |
| **P3** | 选区 getter 改返回 const 引用 | Low #11 |
| **P3** | 统一大括号风格 | Low #12 |
| **P3** | `splitSelected` 分数分配添加注释 + 重命名 `gar` | Low #13 |
