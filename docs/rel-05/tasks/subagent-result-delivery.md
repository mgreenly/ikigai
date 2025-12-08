# Task: Implement Sub-Agent Result Delivery

**Target**: Agent-Spawned Sub-Agents

**Agent model**: sonnet

## Pre-read

### Skills
- `.agents/skills/default.md`
- `.agents/skills/tdd.md`

### Docs
- `docs/rel-05/backlog/agent-spawned-sub-agents.md` - Result delivery

### Source patterns
- `src/tools/tool.c` - Tool result handling
- `src/repl/agent.c` - Agent context

## Pre-conditions

- Sub-agent completion detection works
- Tool handler blocks on sub-agent

## Task

When sub-agent completes:

1. Extract `sub_agent->final_response`
2. Package as tool result for parent
3. Clean up sub-agent:
   - Remove from parent's children array
   - Free sub-agent context (talloc_free)
4. Parent exits EXECUTING_TOOL state
5. Parent's conversation continues with tool result

If user was viewing sub-agent when it completes, auto-switch to parent.

## TDD Cycle

### Red
Write tests:
- Parent receives sub-agent's final response as tool result
- Sub-agent cleaned up after completion
- Parent continues after receiving result
- Auto-switch to parent if viewing completed sub-agent

### Green
Implement result delivery in tool handler completion path.

### Verify
`make check` passes.

## Post-conditions

- Results flow from sub-agent to parent
- Sub-agent resources freed
- All tests pass
