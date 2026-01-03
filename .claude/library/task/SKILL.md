---
name: task
description: File-based task execution tracking for orchestration workflows
---

# Task Skill

File-based execution state tracking. Task order and metadata live in `order.json`, task status is tracked by filesystem location, and all events are logged to `history.jsonl`.

**Storage:** `release/tasks/` directory with status subdirectories

## Directory Structure

```
release/tasks/
├── pending/         # Tasks not yet started
├── in_progress/     # Currently running (should only have 1)
├── completed/       # Successfully finished
├── failed/          # Failed tasks
├── order.json       # Ordered list + metadata
└── history.jsonl    # Append-only audit log
```

## Commands

| Command | Description |
|---------|-------------|
| `/task-init` | Initialize directory structure |
| `/task-import` | Import from order.json and move tasks to pending/ |
| `/task-next` | Get next pending task or stop |
| `/task-start <name>` | Mark in_progress |
| `/task-done <name>` | Mark complete |
| `/task-fail <name>` | Mark failed |
| `/task-escalate <name>` | Bump model/thinking level |
| `/task-continue <stop-id>` | Mark stop as passed and continue |
| `/task-list [status]` | List tasks |
| `/task-stats` | Show metrics |

## Key Operations

Use Bash tool to execute scripts directly:

```bash
.claude/library/task/init.ts          # Initialize directory structure
.claude/library/task/import.ts        # Import from release/tasks/order.json
.claude/library/task/import.ts path   # Import from custom path
.claude/library/task/next.ts          # Get next pending task or stop
.claude/library/task/start.ts <name>  # Mark in_progress
.claude/library/task/done.ts <name>   # Mark complete
.claude/library/task/fail.ts <name>   # Mark failed
.claude/library/task/escalate.ts <name> "<reason>"
.claude/library/task/continue.ts <stop-id>  # Mark stop as passed
.claude/library/task/list.ts [status] # List tasks
.claude/library/task/stats.ts         # Show metrics
```

## Task Lifecycle

```
pending → in_progress → completed
              ↓
         escalate → pending (higher capability)
              ↓
            failed (max level reached)
```

## Stop Lifecycle

```
not_passed → stop_reached (orchestrator halts) → stop_continue (user approval) → passed
```

Stops are identified by their `stop` field. Once passed (via `continue.ts`), they're skipped in future runs.

## Escalation Ladder

| Level | Model | Thinking |
|-------|-------|----------|
| 1 | sonnet | thinking |
| 2 | sonnet | extended |
| 3 | opus | extended |
| 4 | opus | ultrathink |

## Order File (order.json)

Tracks execution order, task metadata, and stop points:

```json
{
  "tasks": [
    {"task": "foo.md", "group": "Core", "model": "sonnet", "thinking": "none"},
    {"task": "bar.md", "group": "Tests", "model": "sonnet", "thinking": "thinking"},
    {"stop": "verify-core", "message": "Verify core features work before proceeding"},
    {"task": "baz.md", "group": "Integration", "model": "sonnet", "thinking": "none"}
  ]
}
```

- **Ordering:** Array position determines execution order
- **Tasks:** Work items with model and thinking levels
- **Stops:** Manual verification checkpoints (message is optional)
- **Status:** Task status derived from filesystem location, stop status from history.jsonl

## History File (history.jsonl)

Append-only audit log with one event per line:

```jsonl
{"timestamp": "2025-12-24T10:30:00Z", "action": "import", "task": "foo.md", "to": "pending"}
{"timestamp": "2025-12-24T10:31:00Z", "action": "start", "task": "foo.md", "from": "pending", "to": "in_progress"}
{"timestamp": "2025-12-24T10:45:00Z", "action": "done", "task": "foo.md", "from": "in_progress", "to": "completed", "elapsed_seconds": 840}
{"timestamp": "2025-12-24T11:00:00Z", "action": "escalate", "task": "bar.md", "from_model": "sonnet", "from_thinking": "thinking", "to_model": "sonnet", "to_thinking": "extended"}
{"timestamp": "2025-12-24T11:30:00Z", "action": "stop_reached", "task": "verify-core", "stop": "verify-core", "message": "Verify core features work"}
{"timestamp": "2025-12-24T12:00:00Z", "action": "stop_continue", "task": "verify-core", "stop": "verify-core"}
```

## Orchestrator Instructions

**Ownership:** Only the orchestrator manipulates task state. Sub-agents executing tasks must NOT use task commands.

**Visibility:** Task status is always visible via `ls release/tasks/*/` - no hidden state in databases.

**Stops:** When `next.ts` returns `type: "stop"`, the orchestrator must exit and wait for user to run `continue.ts <stop-id>` before resuming.

**Recovery:** If state becomes inconsistent, history.jsonl provides full audit trail for reconstruction.

## Task Files

Tasks are created in `release/tasks/` with an `order.json`:

```json
{
  "tasks": [
    {"task": "foo.md", "group": "Core", "model": "sonnet", "thinking": "none"}
  ]
}
```

After running `import.ts`:
- Source `order.json` content becomes `release/tasks/order.json`
- Task files move to `pending/` directory
- Import events logged to `history.jsonl`

Temp files during execution go in `release/tmp/`.

**Note:** The `release/` directory may contain other files and directories beyond `tasks/` and `tmp/` (such as `research/`, `user-stories/`, `plan/`, etc.). These are permitted and will be ignored by task execution.

## JSON Response Format

All commands return: `{"success": true, "data": {...}}` or `{"success": false, "error": "...", "code": "..."}`
