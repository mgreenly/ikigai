Mark a task as in_progress.

**Usage:** `/task-start <name>`

Sets status to 'in_progress' and records the start time.

---

{{#if args}}
```bash
deno run --allow-read --allow-write --allow-ffi --allow-env --allow-net --allow-run .claude/skills/task-db/start.ts {{args}}
```
{{else}}
Error: Please provide a task name.

Example: `/task-start 01-add-auth.md`
{{/if}}
