---
name: ralph-stats
description: Ralph execution statistics and goal file locations
---

# Ralph Stats

## File Locations

All ralph state lives in `$HOME/.local/state/ralph/`:

| File | Purpose |
|------|---------|
| `stats.jsonl` | One JSON object per run with timing, cost, tokens |
| `goals/` | Goal files (named `goal-<timestamp>.md`) |

## Getting a Summary

Run the stats script:

```bash
.claude/scripts/ralph-stats
```

Reports: total runs, total duration, model wait percentage, total cost.
