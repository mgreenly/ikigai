List tasks for the current branch.

**Usage:**
- `/task-list` - all tasks
- `/task-list pending` - filter by status
- `/task-list done`
- `/task-list in_progress`
- `/task-list failed`

---

{{#if args}}
```bash
deno run --allow-read --allow-write --allow-ffi --allow-env --allow-net --allow-run .claude/skills/task-db/list.ts {{args}}
```
{{else}}
```bash
deno run --allow-read --allow-write --allow-ffi --allow-env --allow-net --allow-run .claude/skills/task-db/list.ts
```
{{/if}}
