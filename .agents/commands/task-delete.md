Delete a task from the queue.

**Usage:** `/task-delete <name>`

---

{{#if args}}
```bash
deno run --allow-read --allow-write --allow-ffi --allow-env --allow-net --allow-run .claude/skills/task-db/delete.ts {{args}}
```
{{else}}
Error: Please provide a task name.

Example: `/task-delete 01-add-auth.md`
{{/if}}
