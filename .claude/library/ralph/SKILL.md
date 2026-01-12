---
name: ralph
description: Ralph loop - iterative goal completion harness
---

# Ralph

Braindead agentic loop that iteratively works toward a goal until done or time expires. Named after Ralph Wiggum - just keep trying until it works.

## Philosophy

No sophisticated planning. No complex orchestration. Just:
1. Read the goal
2. Try to make progress
3. Commit progress
4. Repeat until DONE

History provides learning without complex state management. Time budget prevents runaway costs.

## Usage

```bash
.claude/harness/ralph/run \
  --goal=path/to/goal.md \
  [--duration=4h] \
  [--model=sonnet] \
  [--reasoning=low] \
  [--spinner] \
  [--continue]
```

**Flags:**
- `--goal` - Path to goal markdown file (required)
- `--duration` - Time budget (e.g., `4h`, `200m`); unlimited if omitted
- `--model` - `haiku`, `sonnet`, `opus` (default: `sonnet`)
- `--reasoning` - `none`, `low`, `med`, `high` (default: `low`)
- `--spinner` - Enable progress spinner (off by default)
- `--continue` - Continue from existing progress

**Reasoning levels** (fraction of model's max thinking budget):
- `none` = 0 (no thinking)
- `low` = 25% of max
- `med` = 50% of max
- `high` = 100% of max

## Goal File Format

Markdown file with required `## Objective` section:

```markdown
## Objective

Brief description of what needs to be accomplished.

## Reference

Optional pointers to specs, plans, or other context.

## Outcomes

Optional list of specific outcomes that indicate completion.

## Acceptance

Optional acceptance criteria (e.g., `check-build` passes).
```

**Required:** `## Objective` section must exist.

**State files** are derived from the goal file name:
- `<base>-goal.md` → `<base>-progress.jsonl`, `<base>-summary.md`
- Example: `rel-08/ralph-code-removal-goal.md` produces:
  - `rel-08/ralph-code-removal-progress.jsonl`
  - `rel-08/ralph-code-removal-summary.md`

## Progress File Format

JSONL (one JSON object per line):

```jsonl
{"iteration":1,"timestamp":"2026-01-11T08:37:27-06:00","progress":"Removed tool_dispatcher.c from Makefile"}
{"iteration":2,"timestamp":"2026-01-11T08:42:49-06:00","progress":"Deleted src/tool_bash.c and updated headers"}
{"iteration":3,"timestamp":"2026-01-11T08:51:20-06:00","progress":"DONE"}
```

**Fields:**
- `iteration` - Iteration number
- `timestamp` - ISO 8601 timestamp
- `progress` - What was accomplished or `DONE` when complete

## Summary File

Auto-generated markdown summarizing progress. Updated every 10 iterations by a summarizer agent. Provides condensed context for the worker agent without loading full history.

## Loop Behavior

1. **Validate** - Ensure goal file has `## Objective` section
2. **Initialize** - Create empty progress/summary files (or load if `--continue`)
3. **Handle dirty state** - If working copy has changes, commit them
4. **Work iteration** - Agent reads goal + history, makes progress, returns progress string
5. **Commit** - Changes committed via `jj commit`
6. **Record** - Progress appended to progress file
7. **Summarize** - Every 10 iterations, summarizer condenses history
8. **Repeat** - Until `DONE` returned or time expires

**Termination:**
- Agent returns `DONE`
- Time budget expires
- Manual interrupt (Ctrl+C)

## Writing Goals

**State the objective clearly:**
- Good: "Remove all internal tool implementation code. After completion, tool calls return stub responses."
- Bad: "Clean up the tool system"

**Include acceptance criteria:**
- Good: "`check-build` → `{\"ok\": true}`"
- Bad: "Tests should pass"

**Reference detailed specs:**
- Good: "`rel-08/plan/removal-specification.md` - detailed spec with exact changes"
- Bad: Inline the entire spec in the goal file

**List concrete outcomes:**
- Good: "`src/repl_tool.c` - tool dispatch returns stub error JSON"
- Bad: "Tool dispatch should be stubbed out"

## Tips

- Start with generous time budget - ralph may need multiple iterations
- Use `--continue` to resume after interruption
- Check progress file to see what's been accomplished
- Goal files should be outcome-focused, not action-focused
- Trust the agent to discover implementation details
