# Task: Refactor State Transitions to Take Agent

## Target

Background Agent Event Loop (gap.md) - State Transitions

## Pre-read

### Skills

- default
- tdd
- style
- errors
- quality
- scm

### Source Files

- src/repl.c (transition function definitions, lines 212-258)
- src/repl.h (transition function declarations, lines 96-99)
- src/repl_actions_llm.c (callers, lines 129, 138)
- src/repl_event_handlers.c (callers, lines 252, 292, 315)
- src/repl_tool.c (caller, line 183)
- src/commands_fork.c (callers, lines 129, 139)

### Test Patterns

- tests/unit/repl/transitions_test.c (if exists, or create)
- tests/integration/repl_test.c

## Pre-conditions

- Working tree is clean
- `make check` passes
- Task `agent-repl-backpointer` is complete (agent has repl field)

## Task

Refactor state transition functions from taking `ik_repl_ctx_t *repl` to taking `ik_agent_ctx_t *agent`.

### What

Rename and refactor:
- `ik_repl_transition_to_waiting_for_llm(repl)` → `ik_agent_transition_to_waiting_for_llm(agent)`
- `ik_repl_transition_to_idle(repl)` → `ik_agent_transition_to_idle(agent)`
- `ik_repl_transition_to_executing_tool(repl)` → `ik_agent_transition_to_executing_tool(agent)`
- `ik_repl_transition_from_executing_tool(repl)` → `ik_agent_transition_from_executing_tool(agent)`

### How

1. In `src/agent.h`:
   - Add new function declarations with agent parameter

2. In `src/agent.c` (or create new file `src/agent_transitions.c`):
   - Move transition functions from repl.c
   - Change `repl->current->` to `agent->`
   - Keep the same logic

3. In `src/repl.h`:
   - Remove old declarations

4. In `src/repl.c`:
   - Remove old implementations

5. Update all callers to pass the specific agent:
   - `src/repl_actions_llm.c`: `ik_agent_transition_to_*(repl->current)`
   - `src/repl_event_handlers.c`: `ik_agent_transition_to_*(repl->current)`
   - `src/repl_tool.c`: `ik_agent_transition_to_*(repl->current)`
   - `src/commands_fork.c`: `ik_agent_transition_to_*(repl->current)` or child agent

### Why

Current transitions use `repl->current` which breaks for background agents. When processing HTTP/tool completions for a background agent, we need to transition that specific agent, not whatever agent is currently displayed.

## TDD Cycle

### Red

Write tests that:
1. Create an agent in IDLE state
2. Call `ik_agent_transition_to_waiting_for_llm(agent)`
3. Verify agent->state == IK_AGENT_STATE_WAITING_FOR_LLM
4. Verify agent->spinner_state.visible == true
5. Verify agent->input_buffer_visible == false

Similar tests for other transitions.

### Green

1. Add declarations to agent.h
2. Implement functions (move from repl.c, change repl->current to agent)
3. Update all callers
4. Remove old declarations and implementations from repl.h/repl.c
5. Run `make check`

### Verify

- `make check` passes
- `make lint` passes

## Post-conditions

- Transition functions are in agent.h/agent.c (or agent_transitions.c)
- Transition functions take `ik_agent_ctx_t *agent` parameter
- All callers updated to pass specific agent
- Old `ik_repl_transition_*` functions no longer exist
- All tests pass
