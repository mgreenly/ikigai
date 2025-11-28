# task-verify

Verify the current task and advance to the next task.

## Description

Updates the task state file to verify the current task and advance to the next task in sequence. This is the final step in the task execution workflow:
1. start - Mark task as in_progress
2. Execute the task
3. done - Mark task as done
4. **verify** - Verify and advance to next task (this script)

After verification, the system is ready to start the next task.

## Usage

```bash
deno run --allow-read --allow-write .agents/scripts/task-verify/run.ts [task-dir]
```

## Arguments

| Argument | Required | Default | Description |
|----------|----------|---------|-------------|
| `task-dir` | No | `.tasks` | Path to the task directory containing state.json |

## Examples

```bash
# Verify current task in default .tasks directory
deno run --allow-read --allow-write .agents/scripts/task-verify/run.ts

# Verify task in custom directory
deno run --allow-read --allow-write .agents/scripts/task-verify/run.ts ./custom-tasks
```

## Output Format

Returns JSON with the following structure:

### Success Response (More Tasks)

```json
{
  "success": true,
  "data": {
    "current": "2",
    "status": "pending",
    "next_task": "2"
  }
}
```

### Success Response (All Complete)

```json
{
  "success": true,
  "data": {
    "current": null,
    "status": "pending",
    "next_task": null
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

- `STATE_FILE_NOT_FOUND` - state.json file does not exist
- `STATE_PARSE_ERROR` - state.json is not valid JSON
- `INVALID_TRANSITION` - Cannot verify (status is not done)
- `WRITE_ERROR` - Failed to write state file

## State Transition

This script performs the following state transition:

**Before (Task 1 done):**
```json
{
  "current": "1",
  "status": "done"
}
```

**After (Advanced to Task 2):**
```json
{
  "current": "2",
  "status": "pending"
}
```

**Before (Last task done):**
```json
{
  "current": "3",
  "status": "done"
}
```

**After (All complete):**
```json
{
  "current": null,
  "status": "pending"
}
```

## Rules

1. Can only verify tasks when status is "done"
2. `current` is set to the next task number (or null if no more tasks)
3. `status` is set to "pending"
4. Tasks are ordered by decimal number (1 < 1.1 < 1.2 < 2)

## Next Task Selection

The script finds the next task by:
1. Reading all task files in the directory
2. Sorting them by decimal task number
3. Finding the first task number greater than the current task

## Workflow

```
pending → [task-start] → in_progress → [task-done] → done → [task-verify] → pending
```

## Output Field

The `next_task` field in the response data indicates which task will be executed next:
- A task number (e.g., "2") means there are more tasks
- `null` means all tasks are completed
