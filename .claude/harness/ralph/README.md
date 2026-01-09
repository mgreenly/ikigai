# Ralph - Iterative Requirement Completion Harness

Ralph is an agentic loop that iteratively works through a list of requirements, learning from failures and running quality checks.

## Usage

```bash
.claude/harness/ralph/run \
  --duration=4h \
  --requirements=my-requirements.json \
  --history=my-history.jsonl \
  [--model=sonnet] \
  [--reasoning=low] \
  [--no-spinner]
```

## Arguments

- `--duration` - Time budget (e.g., `4h`, `200m`, `3600` for seconds)
- `--requirements` - Path to requirements JSON file
- `--history` - Path to history JSONL file (created if doesn't exist)
- `--model` - Model for sub-agents: `haiku`, `sonnet`, `opus` (default: `sonnet`)
- `--reasoning` - Reasoning level: `none`, `low`, `med`, `high` (default: `low`)
- `--no-spinner` - Disable progress spinner (for non-interactive use)

## Model & Reasoning

The `--model` flag selects which Claude model to use for all sub-agents.

The `--reasoning` flag sets the thinking budget as a fraction of the model's max:
- `none` - No extended thinking (budget: 0)
- `low` - 1/3 of max budget
- `med` - 2/3 of max budget
- `high` - Full max budget

Max budgets per model:
- `haiku`: 32,000 tokens
- `sonnet`: 64,000 tokens
- `opus`: 32,000 tokens

## How It Works

1. **Select** - Agent picks next requirement based on history and dependencies
2. **Work** - Agent implements the requirement
3. **Commit** - On success, commits the changes
4. **Quality** - Runs full quality check suite
5. **Loop** - Repeat until time expires or all requirements done

## Output Format

Ralph uses the same logging format as other harness scripts:

```
2026-01-08 10:30:15 | ralph      | Starting
⠋ Waiting for selection agent [00:03]
2026-01-08 10:30:18 | ralph      | Selected: req-001
⠙ Waiting for work agent [00:45]
2026-01-08 10:31:03 | ralph      | Committed changes
2026-01-08 10:31:03 | ralph      | Running quality checks...
2026-01-08 10:31:03 | ralph      |   Running check:all...
```

The spinner shows elapsed time in `[MM:SS]` format while agents are running.

## Requirements File Format

```json
{
  "requirements": [
    {
      "id": "req-001",
      "description": "What needs to be implemented",
      "acceptance_criteria": [
        "Specific criteria 1",
        "Specific criteria 2"
      ],
      "status": "pending"
    }
  ]
}
```

Status values: `pending` or `done`

## History File Format

JSONL (JSON Lines) with one entry per work attempt:

```json
{"timestamp": "2026-01-08T10:30:00Z", "requirement_id": "req-001", "success": true, "message": "Implemented feature X"}
{"timestamp": "2026-01-08T10:35:00Z", "requirement_id": "req-002", "success": false, "message": "Blocked: needs req-001 to be complete first"}
```

## Quality Checks

After each successful implementation, Ralph runs:

- check:all (tests)
- check:filesize
- check:complexity
- check:sanitize
- check:tsan
- check:valgrind
- check:helgrind
- check:coverage

If any check fails, the loop restarts from requirement selection.

## Example

See `example-requirements.json` for a sample requirements file.

Run the example:
```bash
.claude/harness/ralph/run \
  --duration=30m \
  --requirements=.claude/harness/ralph/example-requirements.json \
  --history=.claude/harness/ralph/example-history.jsonl
```
