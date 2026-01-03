Read a task's content and metadata.

**Usage:** `/task-read <name>`

---

{{#if args}}
```bash
deno run --allow-read --allow-write --allow-ffi --allow-env --allow-net --allow-run .claude/skills/task-db/read.ts {{args}}
```
{{else}}
Error: Please provide a task name.

Example: `/task-read 01-add-auth.md`
{{/if}}
