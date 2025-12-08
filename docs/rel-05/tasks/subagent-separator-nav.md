# Task: Update Separator for Hierarchy Navigation Display

**Target**: Agent-Spawned Sub-Agents

**Agent model**: sonnet

## Pre-read

### Skills
- `.agents/skills/default.md`
- `.agents/skills/tdd.md`

### Docs
- `docs/rel-05/backlog/agent-spawned-sub-agents.md` - Separator format

### Source patterns
- `src/separator/separator.c` - Separator rendering

## Pre-conditions

- Hierarchy navigation (Ctrl+Up/Down) works
- Separator displays agent ID

## Task

Update separator to show full navigation context:

```
─────── ↑0/ ←0/0 [0/1] 0/2→ ↓- ───────
```

Format rules:
- Current agent in brackets: `[0/1]`
- Parent (↑): show parent ID or `-` if top-level
- Previous sibling (←): show ID or omit if single agent
- Next sibling (→): show ID or omit if single agent
- First child (↓): show ID or `-` if no children

Single agent at level: just show `[0/]` without left/right.

## TDD Cycle

### Red
Write tests:
- Top-level agent shows ↑- (no parent)
- Agent with parent shows ↑parent_id
- Agent with children shows ↓child_id
- Agent without children shows ↓-
- Siblings shown correctly

### Green
Update separator render to include navigation context.

### Verify
`make check` passes.

## Post-conditions

- Separator shows complete navigation context
- All directions displayed correctly
- All tests pass
