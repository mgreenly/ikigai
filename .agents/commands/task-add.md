Add a new task to the queue.

**Usage:** `/task-add <name> [--group=<group>] [--model=<model>] [--thinking=<thinking>]`

**Example:** `/task-add 01-add-auth.md --group=Core`

---

{{#if args}}
To add task `{{args}}`:

1. Generate a temp file path: `/tmp/ikigai-task-XXXXXXXX.md` (8 random hex chars)
2. Write the task content to that temp file using the Write tool
3. Run:
```bash
deno run --allow-read --allow-write --allow-ffi --allow-env --allow-net --allow-run .claude/skills/task-db/add.ts {{args}} --file=<temp-path> --cleanup
```

The `--cleanup` flag deletes the temp file after reading.
{{else}}
Error: Please provide a task name.

Example: `/task-add 01-add-auth.md --group=Core`
{{/if}}
