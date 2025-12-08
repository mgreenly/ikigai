Orchestrate task execution from a tasks directory.

**Usage:**
- `/orchestrate PATH` - Run tasks from PATH (e.g., `docs/rel-05/tasks`)

**Behavior:**
1. Call `scripts/tasks/next.ts` to get next task
2. Call `scripts/tasks/session.ts start <task>`
3. Spawn sub-agent with model/thinking from order.json
4. Sub-agent reads and executes task file, returns `{"ok": true}` or `{"ok": false, "reason": "..."}`
5. Call `scripts/tasks/session.ts done <task>`
6. If ok: call `scripts/tasks/done.ts`, report progress, loop to step 1
7. If not ok: report failure, stop and wait for human input

**Critical rules:**
- NEVER read task files yourself (sub-agents do)
- NEVER run make commands yourself (sub-agents do)
- Only: spawn agents, parse responses, call scripts, report progress

**Sub-agent prompt template:**
```
Read and execute <PATH>/<task>.

Return ONLY a JSON response:
- {"ok": true} on success
- {"ok": false, "reason": "brief explanation"} on failure

You must:
1. Read the task file completely
2. Verify pre-conditions
3. Execute the TDD cycle
4. Verify post-conditions
5. Commit your changes
6. Return JSON response
```

---

{{#if args}}
You are the task orchestrator for `{{args}}`.

**Your workflow:**

1. Run: `deno run --allow-read scripts/tasks/next.ts {{args}}/order.json`
2. Parse the JSON response
3. If `data` is null, all tasks complete - report summary and stop
4. If `data` has a task:
   - Run: `deno run --allow-read --allow-write scripts/tasks/session.ts {{args}}/session.json start <task>`
   - Spawn sub-agent with specified model and thinking level
   - Sub-agent prompt: "Read and execute {{args}}/<task>. Return only JSON: {\"ok\": true} or {\"ok\": false, \"reason\": \"...\"}. You must: read task file, verify pre-conditions, execute TDD cycle, verify post-conditions, commit changes, return JSON."
   - Parse sub-agent response for `{"ok": ...}`
   - Run: `deno run --allow-read --allow-write scripts/tasks/session.ts {{args}}/session.json done <task>`
5. If ok:
   - Run: `deno run --allow-read --allow-write scripts/tasks/done.ts {{args}}/order.json <task>`
   - Report: `<task> [task_time] | Total: elapsed_human | Remaining: N`
   - Loop to step 1
6. If not ok:
   - Report failure reason
   - Stop and wait for human input

**Remember:** You only orchestrate. Never read task files. Never run make. Sub-agents do all implementation work.

Begin orchestration now.
{{else}}
Error: Please provide the tasks directory path.

Example: `/orchestrate docs/rel-05/tasks`
{{/if}}
