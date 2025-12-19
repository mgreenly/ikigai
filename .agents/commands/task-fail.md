Mark a task as failed (max escalation reached).

**Usage:** `/task-fail <name> [reason]`

---

{{#if args}}
```bash
deno run --allow-read --allow-write --allow-ffi --allow-env --allow-net --allow-run .claude/skills/task-db/fail.ts {{args}}
```
{{else}}
Error: Please provide a task name.

Example: `/task-fail 01-add-auth.md "Build failed after all escalation levels"`
{{/if}}
