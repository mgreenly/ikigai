Orchestrate task execution from scratch/tasks/ with automatic retry and escalation.

**Usage:** `/orchestrate`

Executes all pending tasks for the current git branch, one at a time.

## CRITICAL: SEQUENTIAL EXECUTION ONLY

**ALL TASKS MUST BE EXECUTED ONE AT A TIME, IN SEQUENCE.**

- NEVER use `run_in_background=true` for task agents
- NEVER spawn multiple Task agents simultaneously
- Each task modifies shared source code - parallel execution corrupts state

## Escalation Ladder

| Level | Model | Thinking |
|-------|-------|----------|
| 1 | sonnet | thinking |
| 2 | sonnet | extended |
| 3 | opus | extended |
| 4 | opus | ultrathink |

---

You are the task orchestrator for the current branch.

## PRE-FLIGHT CHECKS

Run these in order. If ANY fails, report and **STOP**:

1. `git status --porcelain` - abort if any output (uncommitted changes)
2. `make lint` - abort if fails
3. `make check` - abort if fails

## INITIALIZE

```bash
.claude/library/task/init.ts
.claude/library/task/import.ts
```

## EXECUTION LOOP

1. `.claude/library/task/next.ts` - get next task
2. If `data.task` is null → run `.claude/library/task/stats.ts` and stop
3. `.claude/library/task/start.ts <task.name>` - mark in_progress
4. Spawn ONE sub-agent (NOT in background) with task content:
   ```
   Execute this task:

   <task>
   [task content]
   </task>

   Return ONLY JSON: {"ok": true} or {"ok": false, "reason": "..."}
   ```
5. On success: `.claude/library/task/done.ts <task.name>`
   Report: `✓ <task.name> [elapsed] | Remaining: N` → loop to step 1
6. On failure: `.claude/library/task/escalate.ts <task.name> "<reason>"`
   - If escalated: loop to step 1
   - If at max level: mark failed, report `✗ <task.name> failed. Human review needed.`, stop

## COMPLETION

Run `.claude/library/task/stats.ts` and report summary.

Begin orchestration now.
