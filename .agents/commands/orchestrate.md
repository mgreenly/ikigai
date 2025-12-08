Orchestrate task execution from a tasks directory with automatic retry and escalation.

**Usage:**
- `/orchestrate PATH` - Run tasks from PATH (e.g., `docs/rel-05/tasks`)

**Behavior:**
1. Call `.ikigai/scripts/tasks/next.ts` to get next task (with current model/thinking)
2. Call `.ikigai/scripts/tasks/session.ts start <task>`
3. Spawn sub-agent with model/thinking from order.json
4. Sub-agent returns `{"ok": true}` or `{"ok": false, "reason": "...", "progress_made": true|false}`
5. If ok: `session.ts done`, `done.ts`, report success, loop to step 1
6. If not ok AND progress_made:
   - Call `escalate.ts` to bump model/thinking
   - If escalation available: `session.ts retry`, report escalation, loop to step 1
   - If at max level: report max-level failure, stop
7. If not ok AND NOT progress_made: report no-progress failure, stop

**Escalation Ladder:**
| Level | Model | Thinking |
|-------|-------|----------|
| 1 | sonnet | thinking |
| 2 | sonnet | extended |
| 3 | opus | extended |
| 4 | opus | ultrathink |

**Critical rules:**
- NEVER read task files yourself (sub-agents do)
- NEVER run make commands yourself (sub-agents do)
- Only: spawn agents, parse responses, call scripts, report progress

**Sub-agent prompt template:**
```
Read and execute <PATH>/<task>.

Return ONLY a JSON response:
- {"ok": true} on success
- {"ok": false, "reason": "...", "progress_made": true|false} on failure

progress_made = true if you:
- Wrote test or implementation code
- Made any commits
- Fixed errors (even if new ones emerged)

progress_made = false if you:
- Couldn't start (pre-conditions failed)
- Blocked on external issue
- Same error with no change possible

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
⚠ <task> failed with progress. Escalating to opus/extended (level 3/4)...
✗ <task> failed at max level (opus/ultrathink). Human review needed.
✗ <task> failed with no progress: <reason>
```

---

{{#if args}}
You are the task orchestrator for `{{args}}`.

**Your workflow:**

1. Run: `deno run --allow-read .ikigai/scripts/tasks/next.ts {{args}}/order.json`
2. Parse the JSON response
3. If `data` is null, all tasks complete - report summary and stop
4. If `data` has a task:
   - Run: `deno run --allow-read --allow-write .ikigai/scripts/tasks/session.ts {{args}}/session.json start <task>`
   - Spawn sub-agent with specified model and thinking level
   - Sub-agent prompt: "Read and execute {{args}}/<task>. Return only JSON: {\"ok\": true} or {\"ok\": false, \"reason\": \"...\", \"progress_made\": true|false}. progress_made=true if you wrote code, made commits, or fixed errors. progress_made=false if blocked, pre-conditions failed, or no change possible. You must: read task file, verify pre-conditions, execute TDD cycle, verify post-conditions, commit changes, return JSON."
   - Parse sub-agent response for `{"ok": ...}`

5. If ok:
   - Run: `deno run --allow-read --allow-write .ikigai/scripts/tasks/session.ts {{args}}/session.json done <task>`
   - Run: `deno run --allow-read --allow-write .ikigai/scripts/tasks/done.ts {{args}}/order.json <task>`
   - Report: `✓ <task> [task_time] | Total: elapsed_human | Remaining: N`
   - Loop to step 1

6. If not ok AND progress_made is true:
   - Run: `deno run --allow-read --allow-write .ikigai/scripts/tasks/escalate.ts {{args}}/order.json <task>`
   - If escalation data is not null:
     - Run: `deno run --allow-read --allow-write .ikigai/scripts/tasks/session.ts {{args}}/session.json retry <task>`
     - Report: `⚠ <task> failed with progress. Escalating to <model>/<thinking> (level N/4)...`
     - Loop to step 1 (next.ts will return updated model/thinking)
   - If escalation data is null (at max level):
     - Run: `deno run --allow-read --allow-write .ikigai/scripts/tasks/session.ts {{args}}/session.json done <task>`
     - Report: `✗ <task> failed at max level (opus/ultrathink). Human review needed.`
     - Report failure reason from sub-agent
     - Stop and wait for human input

7. If not ok AND progress_made is false (or missing):
   - Run: `deno run --allow-read --allow-write .ikigai/scripts/tasks/session.ts {{args}}/session.json done <task>`
   - Report: `✗ <task> failed with no progress: <reason>`
   - Stop and wait for human input

**Remember:** You only orchestrate. Never read task files. Never run make. Sub-agents do all implementation work.

Begin orchestration now.
{{else}}
Error: Please provide the tasks directory path.

Example: `/orchestrate docs/rel-05/tasks`
{{/if}}
