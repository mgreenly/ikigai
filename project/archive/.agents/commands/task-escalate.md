Escalate a task to the next model/thinking level.

**Usage:** `/task-escalate <name> [reason]`

Escalation ladder:
- Level 1: sonnet + thinking (default)
- Level 2: sonnet + extended
- Level 3: opus + extended
- Level 4: opus + ultrathink

---

{{#if args}}
```bash
deno run --allow-read --allow-write --allow-ffi --allow-env --allow-net --allow-run .claude/skills/task-db/escalate.ts {{args}}
```
{{else}}
Error: Please provide a task name.

Example: `/task-escalate 01-add-auth.md "Tests failed"`
{{/if}}
