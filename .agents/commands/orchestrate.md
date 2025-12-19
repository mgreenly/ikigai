Orchestrate task execution from a tasks directory with automatic retry and escalation.

**Usage:**
- `/orchestrate PATH` - Run tasks from PATH (e.g., `rel-05/tasks`)

## CRITICAL: SEQUENTIAL EXECUTION ONLY

**ALL TASKS MUST BE EXECUTED ONE AT A TIME, IN SEQUENCE.**

- NEVER run multiple tasks in parallel
- NEVER use `run_in_background=true` for task agents
- NEVER spawn multiple Task agents simultaneously
- Wait for each task to FULLY COMPLETE before starting the next
- Each task modifies shared source code and uses the same build system
- Parallel execution causes merge conflicts, race conditions, and corrupted state

The workflow is: **get next task → spawn ONE agent → wait for completion → process result → repeat**

**Behavior:**
1. Call `.ikigai/scripts/tasks/next.ts` to get next task (with current model/thinking)
2. Call `.ikigai/scripts/tasks/session.ts start <task>`
3. Spawn sub-agent with model/thinking from order.json
4. Sub-agent returns `{"ok": true}` or `{"ok": false, "reason": "..."}`
5. If ok: `session.ts done`, `done.ts`, report success, loop to step 1
6. If not ok:
   - Call `escalate.ts` to bump model/thinking
   - If escalation available: `session.ts retry`, report escalation, loop to step 1
   - If at max level: `session.ts done`, report max-level failure, stop

**Escalation Ladder:**
| Level | Model | Thinking |
|-------|-------|----------|
| 1 | sonnet | thinking |
| 2 | sonnet | extended |
| 3 | opus | extended |
| 4 | opus | ultrathink |

**Critical rules:**
- **SEQUENTIAL ONLY:** Run ONE task at a time. Never parallelize. Never use run_in_background.
- NEVER read task files yourself (sub-agents do)
- NEVER run make commands yourself (sub-agents do)
- Only: spawn ONE agent, WAIT for it to finish, parse response, call scripts, report progress, then loop

**Sub-agent prompt template:**
```
Read and execute <PATH>/<task>.

Return ONLY a JSON response:
- {"ok": true} on success
- {"ok": false, "reason": "..."} on failure

You must:
1. Read the task file completely
2. Verify pre-conditions
3. Execute the TDD cycle
4. Verify post-conditions
5. Commit your changes
6. Return JSON response
```

**Progress reporting:**
```
✓ <task> [3m 42s] | Total: 12m 15s | Remaining: 48
⚠ <task> failed. Escalating to opus/extended (level 3/4)...
✗ <task> failed at max level (opus/ultrathink). Human review needed.
```

---

{{#if args}}
You are the task orchestrator for `{{args}}`.

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

1. Run: `deno run --allow-read .ikigai/scripts/tasks/next.ts {{args}}/order.json`
2. Parse the JSON response
3. If `data` is null, all tasks complete:
   - Commit task documents: `git add {{args}}/order.json {{args}}/session.json && git commit -m "chore: update task documents after orchestration"`
   - Report summary and stop
4. If `data` has a task:
   - Run: `deno run --allow-read --allow-write .ikigai/scripts/tasks/session.ts {{args}}/session.json start <task>`
   - Spawn ONE sub-agent (do NOT use run_in_background - wait for completion)
   - Use Task tool with specified model, do NOT set run_in_background=true
   - Sub-agent prompt: "Read and execute {{args}}/<task>. Return only JSON: {\"ok\": true} or {\"ok\": false, \"reason\": \"...\"}. You must: read task file, verify pre-conditions, execute TDD cycle, verify post-conditions, commit changes, return JSON."
   - Wait for sub-agent to fully complete (do not proceed until done)
   - Parse sub-agent response for `{"ok": ...}`

5. If ok:
   - Run: `deno run --allow-read --allow-write .ikigai/scripts/tasks/session.ts {{args}}/session.json done <task>`
   - Run: `deno run --allow-read --allow-write .ikigai/scripts/tasks/done.ts {{args}}/order.json <task>`
   - Report: `✓ <task> [task_time] | Total: elapsed_human | Remaining: N`
   - Loop to step 1

6. If not ok:
   - Run: `deno run --allow-read --allow-write .ikigai/scripts/tasks/escalate.ts {{args}}/order.json <task>`
   - If escalation data is not null:
     - Run: `deno run --allow-read --allow-write .ikigai/scripts/tasks/session.ts {{args}}/session.json retry <task>`
     - Report: `⚠ <task> failed. Escalating to <model>/<thinking> (level N/4)...`
     - Loop to step 1 (next.ts will return updated model/thinking)
   - If escalation data is null (at max level):
     - Run: `deno run --allow-read --allow-write .ikigai/scripts/tasks/session.ts {{args}}/session.json done <task>`
     - Commit task documents: `git add {{args}}/order.json {{args}}/session.json && git commit -m "chore: update task documents after orchestration (stopped at <task>)"`
     - Report: `✗ <task> failed at max level (opus/ultrathink). Human review needed.`
     - Report failure reason from sub-agent
     - Stop and wait for human input

**Remember:** You only orchestrate. Never read task files. Never run make. Sub-agents do all implementation work. **Execute ONE task at a time, sequentially. Never parallelize.**

Begin orchestration now.
{{else}}
Error: Please provide the tasks directory path.

Example: `/orchestrate rel-05/tasks`
{{/if}}
