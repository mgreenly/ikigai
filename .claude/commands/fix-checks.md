---
description: Generate a ralph goal file for fixing quality checks
---

Generate a ralph goal file for fixing all quality checks.

**Steps:**

1. Run `jj log -r 'main..@' --no-graph` to see all commits from main to current
2. Run `jj diff` to see uncommitted changes in working copy
3. Analyze the changes to determine a descriptive name that reflects the primary purpose (e.g., "add-user-auth", "fix-memory-leak", "refactor-parser")
4. Copy `.claude/library/fix-checks/ralph-goal.md` to `<name>-ralph-goal.md` in project root

**Output:** Report the generated filename so the user can run ralph with it.

Execute now.
