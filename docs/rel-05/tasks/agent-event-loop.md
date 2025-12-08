# Task: Per-Agent Event Loop

## Target
User Stories: All (agents always run - core architecture principle)

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/di.md
- .agents/skills/ddd.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/tdd.md
- .agents/skills/errors.md
- .agents/skills/patterns/context-struct.md
- .agents/skills/patterns/state-machine.md
- .agents/skills/patterns/observer.md
- .agents/skills/patterns/callback-context.md

## Pre-read Docs
- docs/backlog/manual-top-level-agents.md (Agent State Machine, Core Principle sections)
- docs/rel-05/user-stories/10-independent-scrollback.md
- docs/memory.md (talloc ownership)

## Pre-read Source (patterns)
- src/repl.h (current REPL structure)
- src/repl.c (current main loop)
- src/repl_event_handlers.c (event handling)
- src/agent.h (ik_agent_ctx_t from DI-1)
- src/openai/client_multi.h (curl_multi interface)
- src/openai/client_multi.c (curl_multi implementation)
- src/repl_tool.c (tool thread handling)
- src/tool.h (tool execution)

## Pre-read Tests (patterns)
- tests/unit/repl/repl_test.c (repl event tests)
- tests/unit/openai/client_multi_test.c (curl_multi tests)

## Pre-conditions
- `make check` passes
- repl-agent-array.md complete (agents[] array exists)
- DI-1 complete: each agent owns its own curl_multi handle
- Each agent owns its own tool thread state

## Task
Implement per-agent event loops so all agents run concurrently. This is the **critical architectural principle**: switching agents only changes I/O attachment, not execution state.

**Core principle (from design doc):**
> All agents continue executing at all times. Switching agents only changes which agent receives terminal input and renders to the terminal.

**What each agent's event loop handles:**
1. `curl_multi_perform()` for LLM streaming
2. Tool thread completion checking
3. State transitions (IDLE → SENDING → STREAMING → EXECUTING_TOOL → IDLE)
4. Scrollback updates from streaming chunks

**Application main loop structure:**
```c
while (!repl->quit) {
    // 1. Process terminal input (only for current agent)
    if (input_available()) {
        handle_input(CURRENT_AGENT(repl));
    }

    // 2. Process ALL agents' events (agents always run)
    for (size_t i = 0; i < repl->agent_count; i++) {
        ik_agent_ctx_t *agent = repl->agents[i];
        ik_agent_process_events(agent);
    }

    // 3. Render current agent only
    if (needs_redraw) {
        render_current_agent(repl);
    }

    // 4. Small sleep to prevent busy-wait
    usleep(10000);  // 10ms
}
```

**Agent event processing:**
```c
void ik_agent_process_events(ik_agent_ctx_t *agent)
{
    // Process curl_multi if streaming
    if (agent->state == IK_REPL_STATE_STREAMING) {
        curl_multi_perform(agent->multi->handle, &agent->curl_still_running);
        // ... handle completed transfers, update scrollback
    }

    // Check tool thread completion
    if (agent->tool_thread_running) {
        pthread_mutex_lock(&agent->tool_thread_mutex);
        if (agent->tool_thread_complete) {
            // ... handle tool result, transition state
        }
        pthread_mutex_unlock(&agent->tool_thread_mutex);
    }
}
```

**Key behaviors:**
- Non-current agents update their scrollback in memory (no render)
- Current agent's scrollback updates trigger terminal render
- Tool threads continue in background regardless of which agent is current
- LLM streaming continues in background regardless of which agent is current

## TDD Cycle

### Red
1. Create `src/agent_event.h` and `src/agent_event.c`:
   ```c
   // Process pending events for an agent
   // Returns true if agent's state changed (needs potential render)
   bool ik_agent_process_events(ik_agent_ctx_t *agent);
   ```

2. Create tests in `tests/unit/agent/agent_event_test.c`:
   - Test agent in IDLE state: process_events returns false (no change)
   - Test agent in STREAMING state: curl_multi_perform called
   - Test agent with tool_thread_complete: state transitions
   - Test scrollback updated from streaming chunks
   - Test multiple agents can process events independently

3. Update `src/repl.c` main loop to iterate all agents

4. Run `make check` - expect test failures

### Green
1. Implement `ik_agent_process_events()`:
   - Extract event processing logic from current repl.c
   - Move curl_multi handling to per-agent
   - Move tool thread checking to per-agent
   - Return true if state changed

2. Update main loop in `src/repl.c`:
   ```c
   // Process all agents
   bool any_changed = false;
   for (size_t i = 0; i < repl->agent_count; i++) {
       if (ik_agent_process_events(repl->agents[i])) {
           // Only trigger render if current agent changed
           if (i == repl->current_agent_idx) {
               any_changed = true;
           }
       }
   }
   if (any_changed) {
       ik_repl_render(repl);
   }
   ```

3. Ensure non-current agents update scrollback but don't render

4. Run `make check` - expect pass

### Refactor
1. Verify no global state in event processing
2. Verify each agent's curl_multi is independent
3. Verify tool threads use agent-specific mutex
4. Verify scrollback updates are thread-safe
5. Run `make lint` - verify clean
6. Profile: ensure iteration overhead is negligible

## Post-conditions
- `make check` passes
- Each agent processes its own events independently
- Main loop iterates all agents for event processing
- Non-current agents continue streaming/tool execution
- Only current agent triggers terminal render
- Switching agents shows accumulated state from background processing
- `ik_agent_process_events()` is the central agent event handler
