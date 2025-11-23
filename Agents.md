# Agents Guide

## Purpose
- Capture the house rules for AI coding agents on Project LemonLime.
- Keep workflows aligned with the memory-bank documents and C++ standards.
- Ensure every session starts with the correct context and ends with recorded updates.

## Core Context (read before any work)
- projectbrief.md
- productContext.md
- systemPatterns.md
- techContext.md
- activeContext.md
- progress.md

## Workflow
- Plan mode: read all core docs; verify current scope; draft a concrete plan; present it.
- Execution mode: keep actions aligned to the plan and project goals; implement code or docs; record important decisions and changes; update activeContext.md/progress.md when the user requests a memory-bank update or when major milestones land.
- Documentation triggers: new pattern adoption, major feature/architecture changes, required memory-bank updates, or new constraints/risks discovered.

## C++ Practices
- Follow C++ Core Guidelines; use RAII for all resources.
- Error handling: prefer std::optional for maybe-values; prefer std::variant or Result-like types for success/error; use enum class error codes for simple cases; avoid nullptr as failure signals.
- Const correctness: use const/constexpr instead of macros; keep macros minimal and uppercase when required (include guards or token glue only).
- Naming: types/enums/structs PascalCase; functions/variables camelCase; private members m_prefix; constants/enums kPrefix.
- Formatting: run clang-format; keep comments concise and focused on intent or rationale; add Doxygen for public APIs when appropriate.
- Memory/resource use: prefer std::unique_ptr/std::shared_ptr over raw new/delete.
- STL first: prefer standard containers/algorithms before custom code.
- auto usage: avoid when it hurts readability; explicit types are fine.
- Inheritance: use override for virtuals; final to seal when needed.

## Repo Conventions
- Build outputs live in ../build (outside workspace tree) to keep repo clean.
- Commits: commit every working session; messages should be a single concise sentence (no cherry-picks across branches).
- Be explicit about platform targets: Qt 5.15/6.2; Windows/Linux/macOS (Ubuntu 20.04 focus).

## When in doubt
- Re-read the memory-bank docs; confirm assumptions; prefer clarity over speed.
