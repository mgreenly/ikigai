# Task: Implement spawn_sub_agent Tool Handler

**Target**: Agent-Spawned Sub-Agents

**Agent model**: opus

## Pre-read

### Skills
- `.agents/skills/default.md`
- `.agents/skills/tdd.md`
- `.agents/skills/errors.md`
- `.agents/skills/patterns/factory.md`

### Docs
- `docs/rel-05/backlog/agent-spawned-sub-agents.md` - Full design

### Source patterns
- `src/tools/tool.h` - Tool handler interface
- `src/repl/agent.c` - Agent creation

## Pre-conditions

- Tool schema registered
- Child array management works
- Depth calculation works

## Task

Implement the tool handler for `spawn_sub_agent`:

1. **Validation**:
   - Check depth < 4 (max depth)
   - Check total agents < 20 (max agents)
   - Return error if either exceeded

2. **Creation**:
   - Generate agent ID: `parent_id + next_child_serial`
   - Create new `ik_agent_ctx_t` with `is_sub_agent = true`
   - Set parent pointer
   - Add to parent's children array
   - Initialize conversation with system_prompt + prompt

3. **Blocking**:
   - Parent enters EXECUTING_TOOL state
   - Sub-agent runs its event loop
   - Tool handler blocks until sub-agent completes

4. **Result**:
   - Extract sub-agent's final_response
   - Return as tool result string

## TDD Cycle

### Red
Write tests:
- Spawn sub-agent successfully
- Max depth exceeded returns error
- Max agents exceeded returns error
- Sub-agent has correct parent link
- Sub-agent has is_sub_agent=true

### Green
Implement handler with validation and creation logic.

### Verify
`make check` passes.

## Post-conditions

- Tool handler creates sub-agents correctly
- Validation enforces limits
- Parent-child links established
- All tests pass
