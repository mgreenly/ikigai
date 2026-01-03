Import tasks from existing order.json + task files.

**Usage:** `/task-import <tasks-directory>`

Reads order.json and imports all tasks (both todo and done) along with their markdown content.

---

{{#if args}}
```bash
deno run --allow-read --allow-write --allow-ffi --allow-env --allow-net --allow-run .claude/skills/task-db/import.ts {{args}}
```
{{else}}
Error: Please provide the tasks directory path.

Example: `/task-import docs/rel-07/tasks`
{{/if}}
