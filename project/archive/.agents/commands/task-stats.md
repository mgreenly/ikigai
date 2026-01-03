Generate metrics report for the current branch.

**Usage:** `/task-stats`

Returns:
- Task counts by status
- Total runtime
- Average time per task
- Escalation summary
- Slowest tasks

---

```bash
deno run --allow-read --allow-write --allow-ffi --allow-env --allow-net --allow-run .claude/skills/task-db/stats.ts
```
