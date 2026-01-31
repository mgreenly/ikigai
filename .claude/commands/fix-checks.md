---
description: Generate a ralph goal file for fixing quality checks
---

Generate a ralph goal file for fixing all quality checks.

{{#if args}}
**Branch name provided:** `{{args}}`

Use `{{args}}` as the goal file name.

**Steps:**
1. Copy `.claude/library/fix-checks/ralph-goal.md` to `{{args}}-ralph-goal.md` in project root

{{else}}
**No branch name provided - auto-detect from commits.**

**Steps:**
1. Run `jj log -r 'main..@' --no-graph` to see all commits from main to current
2. Run `jj diff` to see uncommitted changes in working copy
3. Analyze the changes to determine a descriptive name that reflects the primary purpose (e.g., "add-user-auth", "fix-memory-leak", "refactor-parser")
4. Copy `.claude/library/fix-checks/ralph-goal.md` to `<name>-ralph-goal.md` in project root

{{/if}}

**Output:** Report the generated filename so the user can run ralph with it.

Execute now.
