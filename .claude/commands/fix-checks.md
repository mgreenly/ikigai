---
description: Create and queue a goal for fixing quality checks
---

Create and queue a ralph-plans goal for fixing all quality checks.

{{#if args}}
**Title provided:** `{{args}}`
{{else}}
**No title provided — auto-detect from commits.**

Run `jj log -r 'main..@' --no-graph` and `jj diff` to determine a descriptive title.
{{/if}}

**Steps:**
1. Read `.claude/library/fix-checks/ralph-goal.md` for the goal body
2. Create goal: pipe body to `goal-create --org "$RALPH_ORG" --repo "$RALPH_REPO" --title "<title>"`
3. Queue goal: `goal-queue <id>`

**Output:** Report the goal number so the user knows it's queued.

Execute now.
