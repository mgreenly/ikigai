---
name: ralph1
description: Ralph loop - iterative requirement completion harness
---

# Ralph1

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
.claude/harness/ralph1/run \
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

## Requirements Philosophy

Requirements are **intentionally minimal**. They specify WHAT, not HOW.

**The agent's job is to figure out a working solution** using:
1. The requirement itself
2. Recent history (past attempts, what worked, what failed)
3. Project-level guidance (style, patterns, conventions)

**Priority: Working solution > perfect adherence to conventions.**

This is a deliberate contrast to fully-specified task files. Ralph trusts the agent to discover and adapt rather than pre-specifying everything. The requirements file is a target list, not an instruction manual.

## Writing Requirements

**State outcomes, not actions:**
- Good: "`src/tools/bash/` directory exists"
- Bad: "Create `src/tools/bash/` directory"

**Be minimal:**
- Good: "`make bash_tool` produces `libexec/ikigai/bash_tool` without warnings"
- Bad: "Add a Makefile target called bash_tool that compiles src/tools/bash/main.c with TOOL_COMMON_SRCS and links against talloc, outputting to libexec/ikigai/bash_tool"

**One concept per requirement:**
- Good: "Schema JSON contains `name`, `description`, `parameters` fields"
- Good: "Schema `parameters` specifies `command` as required string property"
- Bad: "Schema has name, description, and parameters where parameters defines command as a required string"

**Trust the agent** to discover:
- File locations and patterns
- Existing conventions
- Implementation details
- How to verify their work

## Requirements File Format

```json
{
  "requirements": [
    {
      "id": 1,
      "requirement": "Brief declarative statement of desired outcome",
      "status": "pending"
    }
  ]
}
```

**Fields:**
- `id` - Unique identifier (integer or string)
- `requirement` - Declarative outcome statement (1 sentence)
- `status` - `pending` or `done` (ralph updates this)

**Ordering:** Ralph selects requirements in whatever order makes sense based on history. The array order is randomized to avoid implying a fixed sequence - the agent determines dependencies.

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
