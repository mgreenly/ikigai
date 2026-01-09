---
name: ralph
description: Ralph loop - iterative requirement completion harness
---

# Ralph

Braindead agentic loop that iteratively completes requirements until done or time expires. Named after Ralph Wiggum - just keep trying until it works.

## Philosophy

No sophisticated planning. No complex orchestration. Just:
1. Pick a requirement
2. Try to implement it
3. If it works, commit and run quality checks
4. Repeat

History provides learning without complex state management. Time budget prevents runaway costs.

## Usage

```bash
.claude/harness/ralph/run \
  --duration=4h \
  --requirements=path/to/requirements.json \
  --history=path/to/history.jsonl \
  [--model=sonnet] \
  [--reasoning=low] \
  [--no-spinner]
```

**Flags:**
- `--duration` - Time budget (e.g., `4h`, `200m`)
- `--requirements` - Path to requirements JSON file
- `--history` - Path to history JSONL file
- `--model` - `haiku`, `sonnet`, `opus` (default: `sonnet`)
- `--reasoning` - `none`, `low`, `med`, `high` (default: `low`)
- `--no-spinner` - Disable spinner for non-interactive use

**Reasoning levels** (fraction of model's max thinking budget):
- `none` = 0 (no thinking)
- `low` = 1/3 of max
- `med` = 2/3 of max
- `high` = full max

## Requirements File Format

```json
{
  "requirements": [
    {
      "id": "req-001",
      "description": "Brief description of what needs to be implemented",
      "acceptance_criteria": [
        "Specific testable criterion 1",
        "Specific testable criterion 2"
      ],
      "status": "pending"
    }
  ]
}
```

**Fields:**
- `id` - Unique identifier (e.g., `req-001`, `feature-auth`, `fix-memory-leak`)
- `description` - What needs to be implemented (1-2 sentences)
- `acceptance_criteria` - Array of specific, testable criteria
- `status` - `pending` or `done` (ralph updates this)

**Writing Good Requirements:**
- Each requirement should be independently implementable
- Acceptance criteria should be verifiable by tests or inspection
- If requirement A depends on B, put B first in the array
- Keep requirements small - prefer many small over few large

## History File Format

JSONL (one JSON object per line):

```jsonl
{"timestamp": "2026-01-08T10:30:00Z", "requirement_id": "req-001", "success": true, "message": "Implemented feature X"}
{"timestamp": "2026-01-08T10:35:00Z", "requirement_id": "req-002", "success": false, "message": "Blocked: needs req-001 first"}
```

**Fields:**
- `timestamp` - ISO 8601 timestamp
- `requirement_id` - Which requirement was attempted
- `success` - Whether implementation succeeded
- `message` - What was done or why it failed

**History Purpose:**
- Selection agent reads history to avoid blocked requirements
- Work agent reads history to learn from past attempts
- Provides audit trail of all attempts

## Loop Behavior

1. **Select** - Agent picks next pending requirement based on history
2. **Work** - Agent implements requirement (max 20 turns)
3. **On success:**
   - Commit changes via `jj commit`
   - Run quality checks (all, filesize, complexity, sanitize, tsan, valgrind, helgrind, coverage)
   - If all pass, mark requirement `done`
   - If any fail, loop restarts from selection
4. **On failure/blocked:**
   - Restore changes via `jj restore`
   - Log failure to history
   - Loop restarts from selection
5. **Termination:**
   - Time budget expired
   - All requirements done
   - Selection returns `none`

## Preparing Work for Ralph

1. Create requirements file with all work items
2. Create empty history file (or reuse existing for continuity)
3. Run ralph with appropriate time budget
4. Monitor progress via log output
5. Review commits after completion

## Tips

- Start with generous time budget - ralph may need multiple attempts
- Put foundational requirements first (selection agent considers order)
- Keep requirements atomic - one concept per requirement
- Acceptance criteria should map to testable outcomes
- History file can be shared across runs for learning continuity
