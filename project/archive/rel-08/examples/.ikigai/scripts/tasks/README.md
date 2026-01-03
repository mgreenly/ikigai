# Task Orchestration Scripts

Scripts for managing task execution order and session timing.

## Scripts

### next.ts

Get the next task from order.json.

```bash
deno run --allow-read scripts/tasks/next.ts <order.json>
```

**Arguments:**
| Arg | Description |
|-----|-------------|
| order.json | Path to order.json file |

**Output:**
```json
{"success": true, "data": {"task": "my-task.md", "group": "Group Name", "model": "sonnet", "thinking": "none"}}
```

When no tasks remain:
```json
{"success": true, "data": null}
```

### done.ts

Mark a task as done (moves from todo to done array).

```bash
deno run --allow-read --allow-write scripts/tasks/done.ts <order.json> <task-name>
```

**Arguments:**
| Arg | Description |
|-----|-------------|
| order.json | Path to order.json file |
| task-name | Filename of task to mark done |

**Output:**
```json
{"success": true, "data": {"remaining": 52}}
```

### escalate.ts

Escalate a task's model/thinking to the next level on the escalation ladder.

```bash
deno run --allow-read --allow-write scripts/tasks/escalate.ts <order.json> <task-name>
```

**Arguments:**
| Arg | Description |
|-----|-------------|
| order.json | Path to order.json file |
| task-name | Filename of task to escalate |

**Escalation Ladder:**
| Level | Model | Thinking |
|-------|-------|----------|
| 1 | sonnet | thinking |
| 2 | sonnet | extended |
| 3 | opus | extended |
| 4 | opus | ultrathink |

**Output:**
```json
{"success": true, "data": {"model": "sonnet", "thinking": "extended", "level": 2, "max_level": 4}}
```

When already at max level:
```json
{"success": true, "data": null}
```

### session.ts

Log session events and get elapsed work time.

```bash
deno run --allow-read --allow-write scripts/tasks/session.ts <session.json> <start|done|retry> <task>
```

**Arguments:**
| Arg | Description |
|-----|-------------|
| session.json | Path to session.json file |
| event | Either "start" or "done" |
| task | Task filename |

**Output:**
```json
{"success": true, "data": {"elapsed_seconds": 1508, "elapsed_human": "25m 8s"}}
```

## File Formats

### order.json

```json
{
  "todo": [
    {"task": "my-task.md", "group": "Feature Group", "model": "sonnet", "thinking": "none"}
  ],
  "done": []
}
```

**Fields:**
- `task`: Filename of the task (relative to tasks/ directory)
- `group`: Logical grouping for reporting
- `model`: Agent model (haiku, sonnet, opus)
- `thinking`: Thinking level (none, thinking, extended, ultrathink)

### session.json

```json
[
  {"event": "start", "task": "my-task.md", "time": "2025-01-07T10:30:00Z"},
  {"event": "done", "task": "my-task.md", "time": "2025-01-07T10:42:15Z"}
]
```

Append-only event log. Elapsed time is calculated from start/done pairs.

## Error Responses

All scripts return errors in this format:
```json
{"success": false, "error": "Human-readable message", "code": "ERROR_CODE"}
```

Error codes: `INVALID_ARGS`, `FILE_NOT_FOUND`, `INVALID_JSON`, `INVALID_FORMAT`, `TASK_NOT_FOUND`, `WRITE_ERROR`, `READ_ERROR`, `INVALID_EVENT`

## Sub-agent Response Format

Sub-agents return JSON responses to the orchestrator:

**Success:**
```json
{"ok": true}
```

**Failure:**
```json
{"ok": false, "reason": "Brief explanation", "progress_made": true}
```

The `progress_made` field indicates whether forward progress occurred:

| progress_made | Meaning | Orchestrator Action |
|---------------|---------|---------------------|
| `true` | Wrote code, made commits, fixed errors | Escalate and retry |
| `false` | Blocked, pre-conditions failed, no change possible | Abort |

**Examples of progress_made = true:**
- Wrote failing test (even if implementation failed)
- Made partial implementation (even if tests still fail)
- Fixed compilation errors (even if new ones emerged)
- Made commits (even if post-conditions not met)

**Examples of progress_made = false:**
- Pre-conditions not met (dependency missing)
- Task file unclear or invalid
- Same error repeated with no change possible
- External blocker (permissions, network, etc.)
