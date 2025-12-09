# Task: Implement Cascade Kill

**Target**: Agent-Spawned Sub-Agents

**Agent model**: sonnet

## Pre-read

### Skills
- `.agents/skills/default.md`
- `.agents/skills/tdd.md`
- `.agents/skills/patterns/arena-allocator.md`

### Docs
- `docs/rel-05/backlog/agent-spawned-sub-agents.md` - Kill cascade

### Source patterns
- `src/repl/agent.c` - Agent lifecycle
- `src/repl/commands.c` - /kill command

## Pre-conditions

- Working tree is clean (`git status --porcelain` returns empty)
- Agent hierarchy exists
- /kill command works for single agents

## Task

Update /kill to cascade through descendants:

1. **Depth-first traversal**: Kill children before parent
2. **Recursive**: Each child's children killed first
3. **Order for /kill 0/**:
   - Kill 0/0/0 (deepest)
   - Kill 0/0
   - Kill 0/1
   - Kill 0/2
   - Kill 0/

If user is viewing a descendant of the killed agent, auto-switch to next available agent (circular navigation).

Agent 0/ protection still applies - cannot kill agent 0/, but CAN kill 0/'s children.

## TDD Cycle

### Red
Write tests:
- Kill agent with no children (unchanged behavior)
- Kill agent with children kills all descendants
- Kill order is depth-first
- Auto-switch when viewing killed descendant
- Cannot kill agent 0/ (but can kill 0/0)

### Green
Implement recursive kill with depth-first traversal.

### Verify
`make check` passes.

## Post-conditions

- /kill cascades through descendants
- Correct depth-first order
- All tests pass
- Working tree is clean (all changes committed)
