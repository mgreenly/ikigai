Mark a task as completed.

**Usage:** `/task-done <name>`

Sets status to 'done', records completion time, and returns elapsed duration.

---

{{#if args}}
```bash
deno run --allow-read --allow-write --allow-ffi --allow-env --allow-net --allow-run .claude/skills/task-db/done.ts {{args}}
```
{{else}}
Error: Please provide a task name.

Example: `/task-done 01-add-auth.md`
{{/if}}
