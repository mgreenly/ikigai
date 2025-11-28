# task-next

Get the next task to execute based on the current state.

## Description

Determines which task should be executed next by reading the `state.json` file in the task directory. The script handles different scenarios:
- If a task is in progress, returns that task
- If a task is done and waiting for verification, returns null
- If status is pending, returns the next task after `last_verified`
- If all tasks are completed, returns null

## Usage

```bash
deno run --allow-read .agents/scripts/task-next/run.ts [task-dir]
```

## Arguments

| Argument | Required | Default | Description |
|----------|----------|---------|-------------|
| `task-dir` | No | `.tasks` | Path to the task directory containing state.json |

## Examples

```bash
# Get next task from default .tasks directory
deno run --allow-read .agents/scripts/task-next/run.ts

# Get next task from custom directory
deno run --allow-read .agents/scripts/task-next/run.ts ./custom-tasks
```

## Output Format

Returns JSON with the following structure:

### Success with Task

```json
{
  "success": true,
  "data": {
    "task": {
      "number": "1",
      "name": "setup-database",
      "filename": "1-setup-database.md",
      "path": "/path/to/tasks/1-setup-database.md"
    }
  }
}
```

### Success with No Task (Waiting)

```json
{
  "success": true,
  "data": {
    "task": null,
    "reason": "Waiting for verification of task 1"
  }
}
```

### Success with No Task (Complete)

```json
{
  "success": true,
  "data": {
    "task": null,
    "reason": "All tasks completed"
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
- `CURRENT_TASK_NOT_FOUND` - Current task in state.json doesn't exist
- `NO_TASKS_FOUND` - No task files found in directory
- `READ_ERROR` - General read error

## State File Format

The `state.json` file should have this structure:

```json
{
  "current": "2",
  "status": "in_progress"
}
```

### State Fields

- `current`: Task number currently being worked on (null if none)
- `status`: One of:
  - `"pending"` - Ready to start next task
  - `"in_progress"` - Currently working on a task
  - `"done"` - Task completed, waiting for verification

## Decision Logic

1. If `status === "done"`: Returns null with reason "Waiting for verification"
2. If `status === "in_progress"`: Returns the current task
3. If `status === "pending"`:
   - If `current` is null: Returns first task
   - Otherwise: Returns first task after `current`
4. If no more tasks exist: Returns null with reason "All tasks completed"
