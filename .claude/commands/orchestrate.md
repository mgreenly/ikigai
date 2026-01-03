Orchestrate task execution from release/tasks/ with automatic retry and escalation.

**Usage:** `/orchestrate`

Executes all pending tasks for the current bookmark, one at a time.

**UNATTENDED EXECUTION:** Runs autonomously without human oversight. Task files must provide complete context.

**Efficiency note:** This is a lean execution loop. Task authoring spent tokens to provide complete context, so sub-agents execute immediately without research/exploration.

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

1. `jj diff --summary` - abort if any output (uncommitted changes)
2. `make lint` - abort if fails
3. `make check` - abort if fails

## INITIALIZE

```bash
.claude/library/task/init.ts
.claude/library/task/import.ts
```

## EXECUTION LOOP

1. **Check context limit**: If token usage ≥ 140,000, run `.claude/library/task/stats.ts` and stop with message: `⚠ Stopped at 140k token limit. Resume with /orchestrate to continue.`
2. `.claude/library/task/next.ts` - get next task or stop
3. Check response type:
   - If `data.type` is `"stop"` → report stop message and exit:
     ```
     ⏸ STOP: <data.stop>

     <data.message>

     Run this to continue: .claude/library/task/continue.ts <data.stop>
     ```
   - If `data.type` is `"task"` and `data.task` is null → run `.claude/library/task/stats.ts` and stop
   - If `data.type` is `"task"` and `data.task` exists → continue to step 4
4. `.claude/library/task/start.ts <task.name>` - mark in_progress
5. Spawn ONE sub-agent (NOT in background):
   ```
   Execute task: release/tasks/<task.name>

   Read the task file and execute it. The task provides all needed skills, files, and context.

   Return ONLY JSON: {"ok": true} or {"ok": false, "reason": "..."}
   ```
6. On success: `.claude/library/task/done.ts <task.name>`
   Report: `✓ <data.name> [<data.elapsed>] | To stop: <data.remaining_to_stop> (<data.eta_to_stop>) | Total: <data.remaining_total> (<data.eta_total>)` → loop to step 1
7. On failure: `.claude/library/task/escalate.ts <task.name> "<reason>"`
   - If escalated: loop to step 1
   - If at max level: mark failed, report `✗ <task.name> failed. Human review needed.`, stop

## COMPLETION

Run `.claude/library/task/stats.ts` and report summary.

Begin orchestration now.
