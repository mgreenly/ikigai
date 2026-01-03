Get the next pending task for the current branch.

**Usage:** `/task-next`

Returns the first pending task with full content, model, and thinking level.

---

```bash
deno run --allow-read --allow-write --allow-ffi --allow-env --allow-net --allow-run .claude/skills/task-db/next.ts
```
