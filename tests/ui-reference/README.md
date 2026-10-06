# Original QWidget reference capture

This optional tool builds the original QWidget sources without adding them to the product executable. CMake copies only the UI sources, headers, forms, and export utility into its build directory. All base/core includes resolve to the current libraries, so the original and current copies of those headers cannot be mixed. Set `LEMON_UI_REFERENCE_SOURCE_DIR` to a read-only checkout containing the original `src/lemon.cpp` and `src/forms/`. Add `add_subdirectory(ui-reference)` from `tests/CMakeLists.txt` only when that option is set. Build the `lemon-ui-reference` target explicitly. The target is excluded from the default build.

The executable defaults to the `offscreen` platform and writes PNG screenshots with matching JSON files into `build/tests/ui-reference/screenshots`. A single optional argument selects another output directory. `manifest.json` records the actual Qt platform, style class, application font, palette, dimensions, device scale, and settings location. The tool leaves QApplication's platform style untouched. On Windows it applies the original `Microsoft YaHei` application font and registers the installed Windows font files for offscreen rendering. An offscreen platform capture is a layout reference; its manifest identifies the style actually provided by Qt on that platform.

Every JSON contains actual widget geometry, coordinates relative to the capture root, visibility, size hints, font information, text, tabs, table columns, and nested layout information. PNG images contain widget client areas, excluding operating system title bars.

## Settings isolation

Before creating any original UI object, the tool creates a temporary directory, selects QSettings IniFormat, redirects both UserScope and SystemScope to that directory, checks the resulting settings filename, and switches the working directory to the temporary directory. It links the current base library, whose Settings implementation honors `QSettings::defaultFormat()`.

The original LemonLime window uses the QSettings organization/application constructor, which selects the native backend independently of `defaultFormat()`. CMake makes a build-directory copy of its source and changes those two constructors to explicitly select IniFormat. The build-local LemonLime header also exposes its contest initializer and members for test setup; every translation unit uses that same declaration so MSVC symbol decoration remains consistent. Layouts and widget behavior remain the original implementation. The original checkout remains unchanged. This avoids native registry reads and writes when the window loads or saves its size. The tool's sample contest and all save operations are also confined to the temporary directory. It never invokes judging, compiler execution, or file cleanup actions.

## Designer dimensions and structure

These values come from the original .ui files. The screenshots and JSON record actual dimensions after layouts and fonts are applied.

| Form | Designer size | Minimum size | Structure |
| --- | --- | --- | --- |
| Main | 800 × 600 | 725 × 510 | Menu bar, West-position tab bar, three pages, status bar |
| Options | 533 × 437 | 533 × 437 | General, Compiler, Visual tabs above OK and Cancel |
| Welcome | 470 × 350 | Layout derived | Open and New tabs above OK and Cancel |
| New contest | 450 × 320 | 450 × 320 | NewContestWidget and dialog buttons |
| Task editor | 664 × 837 | Layout derived | Grid layout with type-dependent visible controls |
| Test case editor | 359 × 424 | Layout derived | Vertical layouts with 10 px spacing |
| Advanced compiler | 667 × 490 | Layout derived | Compiler/interpreter fields, configurations, sandbox, dialog buttons |
| Theme editor | 400 × 300 | Layout derived | Embedded VisualSettings and dialog buttons |

Main tab order is Tasks, Contestants, Statistics. The tab font is 10 pt and bold. Tasks uses a Summary group and Detail group with horizontal stretch 1:3. Summary's minimum is 176 × 387. Detail contains the original empty, TaskEditWidget, and TestCaseEditWidget stacked pages. The result table has no cell grid, hides its vertical header, uses a default 27 px row height and a 75 px minimum horizontal section size. Its lower buttons appear in this order: Clean Up Files, expanding spacer, Refresh, Judge Unjudged, Judge Selected, Judge All. The cleanup and judging buttons have a 120 px minimum width; Refresh has an 80 px minimum width.

The original screenshot sample uses the same contest title, contestants, tasks, scores, and deterministic judging time as the QML screenshot fixture. The added GNU C++ compiler is a display-only configuration and is never executed.
