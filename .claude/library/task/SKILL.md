---
name: task
description: SQLite-backed task execution tracking for orchestration workflows
---

# Task Skill

SQLite-backed execution state tracking. Task content lives in `scratch/tasks/`, this DB tracks execution state (pending/in_progress/done/failed, escalation, timing).

**Database:** `.claude/data/tasks.db` (gitignored, branch-scoped)

## Commands

| Command | Description |
|---------|-------------|
| `/task-init` | Initialize database |
| `/task-import` | Import from scratch/tasks/ (default) |
| `/task-next` | Get next pending task |
| `/task-start <name>` | Mark in_progress |
| `/task-done <name>` | Mark complete |
| `/task-fail <name>` | Mark failed |
| `/task-escalate <name>` | Bump model/thinking level |
| `/task-list [status]` | List tasks |
| `/task-stats` | Show metrics |

## Key Operations

Use Bash tool to execute scripts directly:

```bash
.claude/library/task/init.ts          # Initialize DB
.claude/library/task/import.ts        # Import from scratch/tasks/ (default)
.claude/library/task/import.ts path   # Import from custom path
.claude/library/task/next.ts          # Get next pending task
.claude/library/task/start.ts <name>  # Mark in_progress
.claude/library/task/done.ts <name>   # Mark complete
.claude/library/task/fail.ts <name>   # Mark failed
.claude/library/task/escalate.ts <name> "<reason>"
.claude/library/task/list.ts [status] # List tasks
.claude/library/task/stats.ts         # Show metrics
```

## Task Lifecycle

```
pending → in_progress → done
              ↓
         escalate → pending (higher capability)
              ↓
            fail (max level reached)
```

## Escalation Ladder

| Level | Model | Thinking |
|-------|-------|----------|
| 1 | sonnet | thinking |
| 2 | sonnet | extended |
| 3 | opus | extended |
| 4 | opus | ultrathink |

## Task Files

Tasks are created directly in `scratch/tasks/` with an `order.json`:

```json
{
  "todo": [
    {"task": "foo.md", "group": "Core", "model": "sonnet", "thinking": "none"}
  ]
}
```

Array position determines execution order. The database tracks completion state.

Temp files during execution go in `scratch/tmp/`.

## JSON Response Format

All commands return: `{"success": true, "data": {...}}` or `{"success": false, "error": "...", "code": "..."}`
