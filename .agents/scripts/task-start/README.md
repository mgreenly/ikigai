# task-start

Mark a task as in_progress.

## Description

Updates the task state file to mark a specific task as currently in progress. This is the first step in the task execution workflow:
1. **start** - Mark task as in_progress (this script)
2. Execute the task
3. **done** - Mark task as done
4. **verify** - Verify and advance to next task

## Usage

```bash
deno run --allow-read --allow-write .agents/scripts/task-start/run.ts [task-dir] <task-number>
```

## Arguments

| Argument | Required | Default | Description |
|----------|----------|---------|-------------|
| `task-dir` | No | `.tasks` | Path to the task directory containing state.json |
| `task-number` | Yes | - | Task number to mark as in_progress |

## Examples

```bash
# Start task 1 in default .tasks directory
deno run --allow-read --allow-write .agents/scripts/task-start/run.ts 1

# Start task 1.2 in custom directory
deno run --allow-read --allow-write .agents/scripts/task-start/run.ts ./custom-tasks 1.2
```

## Output Format

Returns JSON with the following structure:

### Success Response

```json
{
  "success": true,
  "data": {
    "current": "1",
    "status": "in_progress"
  }
}
```

### Error Response

```json
{
  "success": false,
  "error": "Error message",
  "code": "ERROR_CODE"
}
```

## Error Codes

- `STATE_PARSE_ERROR` - state.json is not valid JSON
- `AWAITING_VERIFICATION` - Cannot start new task while waiting for verification
- `WRITE_ERROR` - Failed to write state file

## State Transition

This script performs the following state transition:

**Before:**
```json
{
  "current": null,
  "status": "pending"
}
```

**After:**
```json
{
  "current": "2",
  "status": "in_progress"
}
```

## Rules

1. Cannot start a task if status is "done" (must verify first)
2. `current` is set to the specified task number
3. `status` is set to "in_progress"
4. If state.json doesn't exist, it will be created

## State File Location

The state file is located at `<task-dir>/state.json`.
