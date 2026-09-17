# Interactive Workspace Implementation Plan

Use superpowers:subagent-driven-development. Scope: CHR-12.
Spec: ../specs/2026-09-16-interactive-workspace-design.md

- [x] Parent: New scene model/API, atomic create, tests, split layout and Win32 UI integration.
- [x] Terminal implementer: isolated TerminalSession/TerminalScreen/TerminalPane and tests; no shared GUI/CMake edits.
- [x] Preview implementer: isolated PlaySession and input sample YAML + model tests; no GUI/CMake edits.
- [x] Parent: focused reviews, CMake links, smoke checks and real window demo.
- [x] Parent: docs, example-based development rule, full verify/wiki/CI, commit/push/PR.

## Ledger

2026-09-16: baseline verify 6 steps passed; CHR-12 claimed and pushed.
Ruling: continue in the existing dedicated checkout branch; user previously approved implementation,
push and main integration. Separate worktrees would hide the interactive binary from the shared workspace.
Ruling: use editor-owned preview consuming public Runtime APIs; preserve Sidney's general runtime backlog.

2026-09-17 resume: CHR-11 PR #5 merged as 53f201c; CHR-12 claim 159b953 pushed.
Implementation files remain uncommitted pending full verification and review.
New-scene YAML/JSON and eight preview tests passed. Layout minimum-width mutant failed then restored green.
Preview review caught missing parse hints and actor repair locations; regressions failed old code, all eight
passed after repair and scoped re-review approved. Original preview dt/hint mutant also failed five cases.
Terminal seven tests cover VT/UTF-8, alternate screen, bounded scrollback, real TTY/PSReadLine,
cursor/backspace editing, resize and flood-output shutdown. PowerShell Core module inheritance was corrected
in the child environment without changing execution policy. Final terminal review is in progress.
GUI smoke caught both failed-New-scene pending-edit regressions before the fix. Inspector now passes;
source test setup is being checked for native multiline EN_CHANGE behavior by workspace_review.
Final reviewer: final_review. No other main build while workspace_review finishes GUI diagnosis.

2026-09-17 verification: multiline WM_SETTEXT does not send EN_CHANGE; native WM_CHAR now sets up
the regression faithfully. The original Refresh mutant fails both preservation checks; restored fix
passes all 20. Whole-branch review found no additional important defects. Official verify 6/6,
Windows 173/173, CTest 2/2 enabled, Wiki tests 2/2 and static build 68 routes passed.
Fresh GUI capture confirms right-side terminal, all dividers, cubes, and Korean/CLI command output.
Remaining: push implementation, PR/CI and main integration under existing user authorization.

Implementation ab40001 pushed, PR #6 opened. CI 35189631348 passed Windows/wiki/conventions,
but GCC rejected a copied structured-binding loop and Clang caught three test macro references to
temporary actor-vector elements. Tests now use a reference loop and owned actor snapshots.

CI fix ea39e74 passed all five jobs in run 35189888295. Scoped review approved the CI fixes.
Task completion bookkeeping follows with full verify; merge PR #6 after its final checks.
Wiki preview served updated HTML/Markdown with HTTP 200 at http://127.0.0.1:4173/guide/editor/.
