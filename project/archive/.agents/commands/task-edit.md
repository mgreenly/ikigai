Edit a task's content.

**Usage:** `/task-edit <name>`

---

{{#if args}}
To edit task `{{args}}`:

1. First read the current content:
```bash
deno run --allow-read --allow-write --allow-ffi --allow-env --allow-net --allow-run .claude/skills/task-db/read.ts {{args}}
```

2. Generate a temp file path: `/tmp/ikigai-task-XXXXXXXX.md` (8 random hex chars)
3. Write the updated content to that temp file using the Write tool
4. Run:
```bash
deno run --allow-read --allow-write --allow-ffi --allow-env --allow-net --allow-run .claude/skills/task-db/edit.ts {{args}} --file=<temp-path> --cleanup
```
{{else}}
Error: Please provide a task name.

Example: `/task-edit 01-add-auth.md`
{{/if}}
