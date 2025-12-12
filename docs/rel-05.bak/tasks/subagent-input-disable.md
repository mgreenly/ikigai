# Task: Disable Input for Sub-Agents

**Target**: Agent-Spawned Sub-Agents

**Agent model**: haiku

## Pre-read

### Skills
- `.agents/skills/default.md`
- `.agents/skills/tdd.md`

### Docs
- `docs/rel-05/backlog/agent-spawned-sub-agents.md` - Observable but not interactive

### Source patterns
- `src/repl/repl.c` - Input handling
- `src/repl/agent.c` - Agent context

## Pre-conditions

- Working tree is clean (`git status --porcelain` returns empty)
- Sub-agents exist with `is_sub_agent = true`
- Navigation to sub-agents works

## Task

When viewing a sub-agent (is_sub_agent=true), disable user input:

1. Input buffer hidden or shows "Observing sub-agent..."
2. Keypresses ignored (except navigation keys)
3. User can still:
   - Navigate away (Ctrl+arrows)
   - Kill sub-agent (/kill)
   - Use other navigation commands

Visual indication that input is disabled.

## TDD Cycle

### Red
Write tests:
- Regular agent accepts input
- Sub-agent ignores input (except navigation)
- Navigation keys still work on sub-agent
- Visual indicator shown for sub-agent

### Green
Add input gating based on `is_sub_agent` flag.

### Verify
`make check` passes.

## Post-conditions

- Sub-agents are observe-only
- Navigation still works
- All tests pass
- Working tree is clean (all changes committed)
