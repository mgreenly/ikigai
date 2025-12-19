Orchestrate task execution from the task database with automatic retry and escalation.

**Usage:** `/orchestrate`

Executes all pending tasks for the current git branch, one at a time.

## CRITICAL: SEQUENTIAL EXECUTION ONLY

**ALL TASKS MUST BE EXECUTED ONE AT A TIME, IN SEQUENCE.**

- NEVER run multiple tasks in parallel
- NEVER use `run_in_background=true` for task agents
- NEVER spawn multiple Task agents simultaneously
- Wait for each task to FULLY COMPLETE before starting the next
- Each task modifies shared source code and uses the same build system
- Parallel execution causes merge conflicts, race conditions, and corrupted state

The workflow is: **get next task → spawn ONE agent → wait for completion → process result → repeat**

## Escalation Ladder

| Level | Model | Thinking |
|-------|-------|----------|
| 1 | sonnet | thinking |
| 2 | sonnet | extended |
| 3 | opus | extended |
| 4 | opus | ultrathink |

## Progress Reporting

```
✓ task-name.md [3m 42s] | Remaining: 5
⚠ task-name.md failed. Escalating to opus/extended (level 3/4)...
✗ task-name.md failed at max level. Human review needed.
```

---

You are the task orchestrator for the current branch.

## MANDATORY: PRE-FLIGHT CHECKS

Before starting ANY orchestration, you MUST verify these conditions:

### 1. Clean Working Tree
Run: `git status --porcelain`
- If ANY output: Report `✗ Orchestration ABORTED: Uncommitted changes detected.`
- Show `git status --short` output
- **STOP IMMEDIATELY**

### 2. Lint Passes
Run: `make lint`
- If fails: Report `✗ Orchestration ABORTED: make lint failed.`
- Show the lint errors
- **STOP IMMEDIATELY**

### 3. Tests Pass
Run: `make check`
- If fails: Report `✗ Orchestration ABORTED: make check failed.`
- Show the failing tests
- **STOP IMMEDIATELY**

Only proceed with orchestration if ALL THREE checks pass.

## MANDATORY: INITIALIZE DATABASE

Run:
```bash
deno run --allow-read --allow-write --allow-ffi --allow-env --allow-net --allow-run .claude/library/task/init.ts
```

## MANDATORY: SEQUENTIAL EXECUTION

You MUST execute tasks ONE AT A TIME. This is non-negotiable.

- Do NOT use `run_in_background=true` when spawning Task agents
- Do NOT spawn multiple Task agents in a single message
- Do NOT try to parallelize for efficiency - it will break everything
- WAIT for each agent to fully complete before proceeding to the next task
- All tasks share the same codebase and build system - parallel execution corrupts state

**Your workflow (strictly sequential):**

0. **PRE-FLIGHT:** Run all three checks in order:
   - `git status --porcelain` - abort if any output
   - `make lint` - abort if fails
   - `make check` - abort if fails
   If any check fails, report the specific failure and stop.

1. Run: `deno run --allow-read --allow-write --allow-ffi --allow-env --allow-net --allow-run .claude/library/task/next.ts`
2. Parse the JSON response (format: `{"success": true, "data": {"task": {...} | null, "counts": {...}}}`)
3. If `data.task` is null, all tasks complete:
   - Run: `deno run --allow-read --allow-write --allow-ffi --allow-env --allow-net --allow-run .claude/library/task/stats.ts`
   - Report summary and stop
4. If `data.task` has content (fields: id, name, content, task_group, model, thinking):
   - Run: `deno run --allow-read --allow-write --allow-ffi --allow-env --allow-net --allow-run .claude/library/task/start.ts <task.name>`
   - Spawn ONE sub-agent (do NOT use run_in_background - wait for completion)
   - Use Task tool with specified model from task data, do NOT set run_in_background=true
   - Sub-agent prompt should include the task content directly:
     ```
     Execute this task:

     <task>
     [task content here]
     </task>

     Return ONLY a JSON response:
     - {"ok": true} on success
     - {"ok": false, "reason": "..."} on failure

     You must:
     1. Read and understand the task
     2. Verify pre-conditions
     3. Execute the TDD cycle
     4. Verify post-conditions
     5. Commit your changes
     6. Return JSON response
     ```
   - Wait for sub-agent to fully complete (do not proceed until done)
   - Parse sub-agent response for `{"ok": ...}`

5. If ok:
   - Run: `deno run --allow-read --allow-write --allow-ffi --allow-env --allow-net --allow-run .claude/library/task/done.ts <task.name>`
   - Parse response: `{"success": true, "data": {"elapsed_human": "...", "remaining": N}}`
   - Report: `✓ <task.name> [data.elapsed_human] | Remaining: <data.remaining>`
   - Loop to step 1

6. If not ok:
   - Run: `deno run --allow-read --allow-write --allow-ffi --allow-env --allow-net --allow-run .claude/library/task/escalate.ts <task.name> "<reason>"`
   - Parse response: `{"success": true, "data": {"escalated": bool, "at_max_level": bool, ...}}`
   - If `data.escalated` is true:
     - Report: `⚠ <task.name> failed. Escalating to <data.to.model>/<data.to.thinking> (level <data.level>/<data.max_level>)...`
     - Loop to step 1 (next.ts will return task with updated model/thinking)
   - If `data.at_max_level` is true:
     - Run: `deno run --allow-read --allow-write --allow-ffi --allow-env --allow-net --allow-run .claude/library/task/fail.ts <task.name> "<reason>"`
     - Report: `✗ <task.name> failed at max level (opus/ultrathink). Human review needed.`
     - Report failure reason from sub-agent
     - Stop and wait for human input

**Remember:** You only orchestrate. Never read task files yourself - the task content comes from the database. Never run make commands yourself - sub-agents do all implementation work. **Execute ONE task at a time, sequentially. Never parallelize.**

Begin orchestration now.
