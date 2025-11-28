# task-list

List and sort task files from a task directory.

## Description

Retrieves all task files (`.md` files matching pattern `{number}-{name}.md`) from a specified directory and returns them sorted in proper decimal numerical order (1 → 1.1 → 1.1.1 → 1.2 → 2).

Supports pagination with `count` and `start-after` parameters.

## Usage

```bash
deno run --allow-read .agents/scripts/task-list/run.ts <task-dir> [count] [start-after]
```

## Arguments

| Argument | Required | Default | Description |
|----------|----------|---------|-------------|
| `task-dir` | Yes | - | Path to the task directory containing `.md` files |
| `count` | No | 5 | Number of tasks to return |
| `start-after` | No | - | Task number to start after (for pagination) |

## Examples

```bash
# List first 5 tasks from .tasks directory
deno run --allow-read .agents/scripts/task-list/run.ts .tasks

# List first 10 tasks
deno run --allow-read .agents/scripts/task-list/run.ts .tasks 10

# List 5 tasks after task 1.2
deno run --allow-read .agents/scripts/task-list/run.ts .tasks 5 1.2
```

## Output Format

Returns JSON with the following structure:

### Success Response

```json
{
  "success": true,
  "data": {
    "tasks": [
      {
        "number": "1",
        "name": "setup-database",
        "filename": "1-setup-database.md",
        "path": "/path/to/tasks/1-setup-database.md"
      }
    ],
    "total_count": 10,
    "returned_count": 5,
    "has_more": true,
    "start_after": "1.2"
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

- `DIR_NOT_FOUND` - Task directory does not exist
- `NOT_A_DIRECTORY` - Path is not a directory
- `NO_TASKS_FOUND` - No task files found in directory
- `START_TASK_NOT_FOUND` - Specified start-after task number not found
- `READ_ERROR` - General read error

## Task Filename Format

Task files must follow this naming convention:
- Pattern: `{number}-{name}.md`
- Number: Decimal format (e.g., `1`, `1.1`, `1.2.3`)
- Name: Kebab-case (e.g., `setup-database`, `create-api`)

Example valid filenames:
- `1-setup-database.md`
- `1.1-configure-postgres.md`
- `2.3.1-add-error-handling.md`

The file `README.md` is always excluded from task lists.

## Sorting

Tasks are sorted in decimal numerical order:
1. Compare each part of the version number left to right
2. Missing parts are treated as 0
3. Examples of sort order:
   - 1 < 1.1 < 1.1.1 < 1.1.2 < 1.2 < 1.10 < 2
