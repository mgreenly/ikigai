# task-done

Mark the current task as done and awaiting verification.

## Description

Updates the task state file to mark the current task as done. This is the second step in the task execution workflow:
1. start - Mark task as in_progress
2. Execute the task
3. **done** - Mark task as done (this script)
4. verify - Verify and advance to next task

After marking a task as done, it must be verified before starting the next task.

## Usage

```bash
deno run --allow-read --allow-write .agents/scripts/task-done/run.ts [task-dir]
```

## Arguments

| Argument | Required | Default | Description |
|----------|----------|---------|-------------|
| `task-dir` | No | `.tasks` | Path to the task directory containing state.json |

## Examples

```bash
# Mark current task as done in default .tasks directory
deno run --allow-read --allow-write .agents/scripts/task-done/run.ts

# Mark task as done in custom directory
deno run --allow-read --allow-write .agents/scripts/task-done/run.ts ./custom-tasks
```

## Output Format

Returns JSON with the following structure:

### Success Response

```json
{
  "success": true,
  "data": {
    "current": "1",
    "status": "done"
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
- `INVALID_TRANSITION` - Cannot mark as done (status is not in_progress)
- `WRITE_ERROR` - Failed to write state file

## State Transition

This script performs the following state transition:

**Before:**
```json
{
  "current": "1",
  "status": "in_progress"
}
```

**After:**
```json
{
  "current": "1",
  "status": "done"
}
```

## Rules

1. Can only mark task as done when status is "in_progress"
2. `current` is preserved (not changed)
3. `status` is set to "done"
4. After this, you must run `task-verify` before starting the next task

## Workflow

```
pending → [task-start] → in_progress → [task-done] → done → [task-verify] → pending
```
