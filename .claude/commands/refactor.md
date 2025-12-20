Autonomous refactoring pipeline that finds and executes ONE high-impact, behavior-preserving code improvement.

**Usage:** `/refactor`

Analyzes `src/`, identifies the SINGLE most impactful refactoring target, generates tasks for it, and executes without human intervention. Run again for more refactoring - each run handles one target because code changes invalidate plans made against old code.

## CRITICAL CONSTRAINTS

1. **Behavior-preserving only** - No test changes. `make check` must pass before and after.
2. **Sequential execution** - One task at a time, never parallel.
3. **Self-contained tasks** - Each task has all context needed, no clarification possible.

## Pipeline Overview

```
Prechecks → Setup → Analyze → Generate → Review → Load → Execute
```

## Logging

All phases log to `refactor/details.log` with format:
```
[ISO timestamp] PHASE START: description
[ISO timestamp] PHASE: major decision or action
[ISO timestamp] PHASE END: summary (duration: Xm Ys)
```

---

You are the refactoring orchestrator.

## PHASE 0: PRECHECKS

Run these checks in order. If ANY fails, report and **STOP IMMEDIATELY**.

### 0.1 Clean Working Tree
```bash
git status --porcelain
```
- If ANY output: `✗ ABORTED: Uncommitted changes detected.`

### 0.2 Task Database Empty
```bash
deno run --allow-read --allow-ffi --allow-run --allow-env .claude/library/task/list.ts pending
```
- Parse JSON. If `data.tasks` is non-empty: `✗ ABORTED: Task database has pending tasks. Run /orchestrate first or clear tasks.`

### 0.3 No Existing Refactor Directory
```bash
test -d refactor && echo "exists"
```
- If "exists": `✗ ABORTED: refactor/ directory already exists. Remove it first.`

### 0.4 Lint Passes
```bash
make lint
```
- If fails: `✗ ABORTED: make lint failed.`

### 0.5 Tests Pass
```bash
make check
```
- If fails: `✗ ABORTED: make check failed.`

## PHASE 1: SETUP

1. Create directory:
```bash
mkdir -p refactor/tasks
```

2. Initialize log with header:
```bash
echo "[$(date -Iseconds)] REFACTOR START" > refactor/details.log
echo "[$(date -Iseconds)] SETUP: commit=$(git rev-parse HEAD)" >> refactor/details.log
```

3. Initialize task database:
```bash
deno run --allow-read --allow-write --allow-ffi --allow-env --allow-net --allow-run .claude/library/task/init.ts
```

## PHASE 2: ANALYZE

Record start time, then spawn a **single** sub-agent (do NOT use run_in_background):

**Model:** opus
**Prompt:**
```
You are a refactoring analyst for a C codebase.

Load these skills first:
- /load refactoring/smells
- /load refactoring/techniques
- /load refactoring/memory
- /load naming
- /load di
- /load style
- /load errors
- /load source-code

Then perform a deep analysis of src/ to find the MOST IMPACTFUL refactoring opportunities.

CONSTRAINTS:
- Behavior-preserving ONLY - no test changes allowed
- No public API signature changes
- No new functionality or bug fixes
- Focus on: naming conventions, style, memory patterns, DI, code smells

ANALYSIS APPROACH:
1. Scan all src/*.c files for pattern violations
2. Identify specific issues and their locations
3. Prioritize by: severity × alignment with project patterns
4. Select the SINGLE most impactful refactoring target

IMPORTANT: Select ONE target only. Code changes when you refactor - planning multiple refactors is wasteful since the landscape changes after each one. Find the best single improvement, apply it, done. User can run /refactor again for more.

OUTPUT:
Write refactor/plan.md with:
- **Target**: One-line description of the single refactoring target
- **What**: Exactly what will change (specific files, specific patterns)
- **How**: The transformation approach
- **Why**: Why this is the highest-impact choice right now
- **Files**: Exhaustive list of files that will be modified
- **Risk**: What could go wrong (should be minimal for true refactoring)

Do NOT include step-by-step implementation details.
Do NOT plan multiple unrelated refactors.

Log your major decisions to refactor/details.log:
- Which smells/violations you found and their frequency
- Why you chose this specific target as highest-impact
- What you explicitly excluded and why

Return {"ok": true} when complete, or {"ok": false, "reason": "..."} on failure.
```

After agent completes:
- Log end time and duration
- If failed, report and **STOP**

## PHASE 3: GENERATE TASKS

Record start time, then spawn a **single** sub-agent:

**Model:** opus
**Prompt:**
```
You are a task generator for refactoring work.

Load these skills:
- /load task-authoring
- /load refactoring/techniques

Read refactor/plan.md to understand the single refactoring target.

Generate ONE TASK PER FILE listed in the plan. All tasks are for the SAME refactoring target - they're just split by file for manageability.

TASK REQUIREMENTS:
- Self-contained: include all file paths, line references, patterns to match
- Include pre-read instructions for files the agent needs to understand
- Specify exact transformations (old pattern → new pattern)
- Post-condition: make check passes, no test file changes
- Model/thinking: start with sonnet/none, escalate only if complex

TASK FILE FORMAT (each task in refactor/tasks/<filename>.md):
```markdown
# Refactor: <brief description>

## Skills to Load
- /load <skill1>
- /load <skill2>

## Pre-Read
Read these files first:
- `path/to/file.c` - understand current implementation
- `path/to/related.c` - see correct pattern example

## Context
**What:** <specific goal for this file>
**How:** <transformation to apply>
**Why:** <rationale>

## Transformations
1. <specific change with line reference or pattern>
2. <specific change>

## Pre-Conditions
- [ ] File exists: `path/to/file.c`
- [ ] make check passes

## Post-Conditions
- [ ] Transformation applied correctly
- [ ] No test files modified
- [ ] make check passes
- [ ] git commit created

## Verification
```bash
make check
git diff --name-only HEAD~1 | grep -v '_test\.c$' || echo "OK: no test changes"
```
```

Also create refactor/tasks/order.json:
```json
{
  "todo": [
    {"task": "file1.md", "group": "Refactor", "model": "sonnet", "thinking": "none"},
    {"task": "file2.md", "group": "Refactor", "model": "sonnet", "thinking": "none"}
  ],
  "done": []
}
```

Log to refactor/details.log:
- Number of tasks generated
- Files covered
- Any files skipped and why

Return {"ok": true} when complete.
```

After agent completes, log end time and duration.

## PHASE 4: REVIEW TASKS

Record start time, then spawn a **single** sub-agent:

**Model:** sonnet
**Prompt:**
```
You are a task reviewer ensuring refactoring tasks work together correctly.

Read refactor/plan.md for the single refactoring target.
Read all files in refactor/tasks/*.md.
Read refactor/tasks/order.json.

REVIEW CHECKLIST:
1. Tasks are truly behavior-preserving (no test changes required)
2. Tasks are independent OR correctly ordered by dependency
3. Each task is self-contained with all needed context
4. Transformations are specific enough to execute unambiguously
5. Pre/post conditions chain correctly
6. Model/thinking levels are appropriate (prefer lower)

If issues found:
- Edit the task files to fix them
- Update order.json if dependency order needs changing

Log to refactor/details.log:
- Issues found and fixed
- Final task count and order

Return {"ok": true} when review complete.
```

After agent completes, log end time and duration.

## PHASE 5: LOAD TASKS

Record start time, then run:
```bash
deno run --allow-read --allow-write --allow-ffi --allow-env --allow-net --allow-run \
  .claude/library/task/import.ts refactor/tasks
```

Parse response. Log:
- Number of tasks imported
- Any import errors

Log end time and duration.

## PHASE 6: EXECUTE

This phase follows the same pattern as /orchestrate:

1. Get next task:
```bash
deno run --allow-read --allow-ffi --allow-run --allow-env .claude/library/task/next.ts
```

2. If `data.task` is null, all tasks complete:
   - Log completion to details.log
   - Run stats and report summary
   - **STOP** (success)

3. Mark task in progress:
```bash
deno run --allow-read --allow-write --allow-ffi --allow-run --allow-env .claude/library/task/start.ts <task.name>
```

4. Log task start to details.log

5. Spawn ONE sub-agent (do NOT use run_in_background):
   - Use model/thinking from task data
   - Prompt includes task content
   - Agent must return `{"ok": true}` or `{"ok": false, "reason": "..."}`

6. If ok:
   - Mark done: `deno run ... .claude/library/task/done.ts <task.name>`
   - Log success with duration
   - Report: `✓ <task.name> [elapsed] | Remaining: N`
   - Loop to step 1

7. If not ok:
   - Escalate: `deno run ... .claude/library/task/escalate.ts <task.name> "<reason>"`
   - If escalated: log and loop to step 1
   - If at max level: mark failed, log, skip task and dependents, continue to next

## COMPLETION

When all tasks done (or skipped due to failures):

1. Log final summary to details.log:
```
[timestamp] REFACTOR END: X tasks completed, Y failed, Z skipped
[timestamp] REFACTOR END: total duration Xh Ym Zs
```

2. Show stats:
```bash
deno run --allow-read --allow-ffi --allow-run --allow-env .claude/library/task/stats.ts
```

3. Report final status to user:
```
Refactoring complete.
- Completed: X tasks
- Failed: Y tasks
- Duration: Xh Ym Zs
- Log: refactor/details.log

Please review changes and validate with: make check && make lint
```

## IMPORTANT REMINDERS

- **NEVER parallelize** - sequential execution only
- **NEVER modify tests** - behavior-preserving refactoring only
- **Log everything** - details.log is the audit trail
- **Commit after each task** - sub-agents commit their changes

Begin refactoring now.
