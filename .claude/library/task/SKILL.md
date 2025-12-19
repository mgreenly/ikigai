---
name: task
description: SQLite-backed task management system for automated orchestration workflows
---

# Task Skill

SQLite-backed task management system for automated orchestration workflows.

## Overview

This skill provides a persistent task queue stored in SQLite. Tasks are scoped to the current git branch, enabling parallel work on multiple branches without conflicts.

**Database location:** `.claude/data/tasks.db` (gitignored)

## Commands

| Command | Description |
|---------|-------------|
| `/task-init` | Initialize the database |
| `/task-add <name>` | Add a new task |
| `/task-read <name>` | Read task content |
| `/task-edit <name>` | Edit task content |
| `/task-delete <name>` | Delete a task |
| `/task-next` | Get next pending task |
| `/task-start <name>` | Mark task in_progress |
| `/task-done <name>` | Mark task complete |
| `/task-fail <name>` | Mark task failed |
| `/task-escalate <name>` | Bump to next model/thinking level |
| `/task-list [status]` | List tasks |
| `/task-stats` | Show metrics report |
| `/task-import <dir>` | Import from order.json |

## Usage Examples

**Required Deno Permissions:**
- `--allow-read` - Read database and task files
- `--allow-write` - Write to database and temp files
- `--allow-ffi` - SQLite native library bindings
- `--allow-run` - Execute git commands (branch detection)
- `--allow-env` - Access DENO_SQLITE_PATH environment variable

### Initialize Database
```bash
deno run --allow-read --allow-write --allow-ffi --allow-run --allow-env \
  .claude/library/task/init.ts
```

### List Tasks
```bash
# All tasks (default)
deno run --allow-read --allow-ffi --allow-run --allow-env \
  .claude/library/task/list.ts

# Filter by status (pending, in_progress, done, failed)
deno run --allow-read --allow-ffi --allow-run --allow-env \
  .claude/library/task/list.ts pending
```

### Get Next Task
```bash
deno run --allow-read --allow-ffi --allow-run --allow-env \
  .claude/library/task/next.ts
```

### Read Task Content
```bash
deno run --allow-read --allow-ffi --allow-run --allow-env \
  .claude/library/task/read.ts my-task.md
```

### Mark Task In Progress
```bash
deno run --allow-read --allow-write --allow-ffi --allow-run --allow-env \
  .claude/library/task/start.ts my-task.md
```

### Mark Task Complete
```bash
deno run --allow-read --allow-write --allow-ffi --allow-run --allow-env \
  .claude/library/task/done.ts my-task.md
```

### Mark Task Failed
```bash
deno run --allow-read --allow-write --allow-ffi --allow-run --allow-env \
  .claude/library/task/fail.ts my-task.md
```

### Escalate Task
```bash
deno run --allow-read --allow-write --allow-ffi --allow-run --allow-env \
  .claude/library/task/escalate.ts my-task.md
```

### Delete Task
```bash
deno run --allow-read --allow-write --allow-ffi --allow-run --allow-env \
  .claude/library/task/delete.ts my-task.md
```

### Show Statistics
```bash
deno run --allow-read --allow-ffi --allow-run --allow-env \
  .claude/library/task/stats.ts
```

### Import Tasks
```bash
deno run --allow-read --allow-write --allow-ffi --allow-run --allow-env \
  .claude/library/task/import.ts /path/to/tasks/directory
```

## Task Lifecycle

```
pending → in_progress → done
                ↓
           escalate → pending (retry with higher capability)
                ↓
              fail (max escalation reached)
```

## Escalation Ladder

| Level | Model | Thinking |
|-------|-------|----------|
| 1 | sonnet | thinking |
| 2 | sonnet | extended |
| 3 | opus | extended |
| 4 | opus | ultrathink |

## Adding Tasks with Content

To add a task, you must write the content to a temp file first:

1. Generate temp path: `/tmp/ikigai-task-<8-random-hex-chars>.md`
2. Write content using Write tool
3. Run add command with `--file=<path> --cleanup`

Example:
```bash
# After writing content to /tmp/ikigai-task-a1b2c3d4.md
deno run --allow-read --allow-write --allow-ffi --allow-run --allow-env \
  .claude/library/task/add.ts my-task.md \
  --file=/tmp/ikigai-task-a1b2c3d4.md \
  --cleanup \
  --group=Core
```

The `--cleanup` flag deletes the temp file after reading.

## Editing Tasks

Same pattern as adding:

1. Read current content with `/task-read <name>`
2. Generate temp path
3. Write updated content
4. Run edit command with `--file=<path> --cleanup`

## JSON Response Format

All commands return JSON:

```json
// Success
{"success": true, "data": {...}}

// Error
{"success": false, "error": "message", "code": "ERROR_CODE"}
```

## Orchestration Workflow

The orchestrator should follow this loop:

1. `/task-next` - get next pending task
2. If null, all tasks complete - show stats and stop
3. `/task-start <name>` - mark in_progress
4. Spawn sub-agent with task content and model/thinking
5. Parse sub-agent response
6. If success: `/task-done <name>`
7. If failure:
   - `/task-escalate <name>` - try higher capability
   - If at max level: `/task-fail <name>` - stop for human review
8. Loop to step 1

## Metrics

`/task-stats` returns:

- Task counts by status
- Total runtime (sum of completed task durations)
- Average time per task
- Escalation breakdown
- Slowest tasks (top 5)

## Schema

```sql
tasks (
  id, branch, name, content, task_group,
  model, thinking, status,
  created_at, updated_at, started_at, completed_at
)

escalations (
  id, task_id,
  from_model, from_thinking,
  to_model, to_thinking,
  reason, escalated_at
)

sessions (
  id, task_id, event, timestamp
)
```

## Branch Scoping

All operations use the current git branch (`git branch --show-current`).
Tasks on different branches are completely isolated.
