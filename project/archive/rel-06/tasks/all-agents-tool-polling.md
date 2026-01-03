# Task: Poll Tool Completion for All Agents

## Target

Background Agent Event Loop (gap.md) - Tool Polling

## Pre-read

### Skills

- default
- tdd
- style
- errors
- quality
- scm

### Source Files

- src/repl.c (event loop, lines 117-125)
- src/repl_event_handlers.c (handle_tool_completion at line 304)
- src/repl_event_handlers.h

### Test Patterns

- tests/unit/repl/event_loop_test.c
- tests/integration/tool_execution_test.c

## Pre-conditions

- Working tree is clean
- `make check` passes
- Task `agent-repl-backpointer` is complete
- Task `agent-state-transitions` is complete
- Task `agent-tool-execution` is complete

## Task

Update the event loop to poll for tool completion across ALL agents, not just `repl->current`.

### What

Currently in `ik_repl_run()`:
```c
pthread_mutex_lock_(&repl->current->tool_thread_mutex);
ik_agent_state_t current_state = repl->current->state;
bool complete = repl->current->tool_thread_complete;
pthread_mutex_unlock_(&repl->current->tool_thread_mutex);

if (current_state == IK_AGENT_STATE_EXECUTING_TOOL && complete) {
    handle_tool_completion(repl);
}
```

After this task, it checks all agents.

### How

#### Refactor `handle_tool_completion`

```c
// Old signature:
void handle_tool_completion(ik_repl_ctx_t *repl)

// New signature:
void handle_agent_tool_completion(ik_repl_ctx_t *repl, ik_agent_ctx_t *agent)
```

Update implementation:
- Call `ik_agent_complete_tool_execution(agent)` (from previous task)
- Call `ik_agent_should_continue_tool_loop(agent)` (from HTTP handling task)
- Call `ik_agent_transition_to_idle(agent)` (from transitions task)
- Only call `ik_repl_render_frame(repl)` if `agent == repl->current`

#### Update Event Loop in `ik_repl_run()`

```c
// Poll for tool thread completion - check ALL agents
for (size_t i = 0; i < repl->agent_count; i++) {
    ik_agent_ctx_t *agent = repl->agents[i];

    pthread_mutex_lock_(&agent->tool_thread_mutex);
    ik_agent_state_t state = agent->state;
    bool complete = agent->tool_thread_complete;
    pthread_mutex_unlock_(&agent->tool_thread_mutex);

    if (state == IK_AGENT_STATE_EXECUTING_TOOL && complete) {
        handle_agent_tool_completion(repl, agent);
    }
}
```

#### Update `submit_tool_loop_continuation` Calls

If `handle_agent_tool_completion` needs to continue the tool loop, it must pass the correct agent to `submit_tool_loop_continuation`:

```c
if (ik_agent_should_continue_tool_loop(agent)) {
    agent->tool_iteration_count++;
    submit_agent_tool_loop_continuation(repl, agent);
} else {
    ik_agent_transition_to_idle(agent);
}
```

### Why

When user switches to Agent B while Agent A has a tool running:
1. FD monitoring task ensures timeout triggers (50ms poll)
2. This task ensures we check Agent A's completion flag
3. Tool execution task ensures results go to Agent A
4. Tool loop continues on Agent A autonomously

Without this, Agent A's tool appears to never complete until user switches back.

## TDD Cycle

### Red

Write tests that:
1. Create two agents (A and B)
2. Start tool execution on Agent A (set state to EXECUTING_TOOL, tool_thread_complete = true)
3. Switch repl->current to Agent B
4. Run one iteration of event loop (or call the polling code directly)
5. Verify Agent A's tool completion was handled
6. Verify Agent A transitioned appropriately
7. Verify Agent B was not affected

### Green

1. Refactor handle_tool_completion to take agent parameter
2. Update implementation to use agent-based functions
3. Add conditional rendering (only for current agent)
4. Update event loop to poll all agents
5. Run `make check`

### Verify

- `make check` passes
- `make lint` passes

## Post-conditions

- Event loop polls `tool_thread_complete` for ALL agents
- `handle_agent_tool_completion()` operates on passed agent
- Tool loop continues for background agents
- Rendering only happens for current agent
- Background agents complete tools and reach IDLE autonomously
- All tests pass

## Final Verification

After this task, the full background agent loop should work:

1. User sends message on Agent A
2. LLM responds with tool call
3. Tool starts executing
4. User switches to Agent B
5. Agent A's tool completes → detected by polling
6. Agent A's tool result added to conversation
7. Agent A submits next request to LLM
8. LLM responds (no more tools)
9. Agent A transitions to IDLE
10. When user switches back to Agent A, they see all the work completed
