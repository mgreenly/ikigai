# Task: Implement /spawn Command

## Target
User Stories: 01-spawn-agent.md, 13-max-agents-limit.md

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/di.md
- .agents/skills/ddd.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/tdd.md
- .agents/skills/errors.md
- .agents/skills/patterns/factory.md
- .agents/skills/patterns/command.md

## Pre-read Docs
- docs/backlog/manual-top-level-agents.md (Spawning an Agent section)
- docs/rel-05/user-stories/01-spawn-agent.md
- docs/rel-05/user-stories/13-max-agents-limit.md

## Pre-read Source (patterns)
- src/commands.h (command interface)
- src/commands.c (existing slash commands)
- src/repl.h (repl context, agents array)
- src/agent.h (ik_agent_ctx_t, ik_agent_create)
- src/agent.c (agent creation)
- src/agent_switch.h (switching function)
- src/scrollback.h (scrollback for confirmation message)

## Pre-read Tests (patterns)
- tests/unit/commands/commands_test.c (command tests)

## Pre-conditions
- `make check` passes
- repl-agent-array.md complete (agents[] array, next_agent_serial)
- agent-switch.md complete (ik_repl_switch_agent)
- db-agent-id-queries.md complete (agent messages isolated)
- `IK_MAX_AGENTS` defined as 20

## Task
Implement the `/spawn` command to create a new agent and switch to it.

**Command specification:**
- Name: `/spawn`
- Arguments: None
- Success: Creates new agent, switches to it, displays "Agent N/ created"
- Failure: If 20 agents exist, displays "Error: Maximum agents (20) reached"

**Flow:**
```c
res_t cmd_spawn(ik_repl_ctx_t *repl, const char *args)
{
    (void)args;  // No arguments

    // 1. Check limit
    if (repl->agent_count >= IK_MAX_AGENTS) {
        ik_scrollback_append_error(CURRENT_AGENT(repl)->scrollback,
                                   "Maximum agents (20) reached");
        return OK(NULL);  // Not a fatal error
    }

    // 2. Create new agent
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(repl, repl->next_agent_serial, &agent);
    if (is_err(&res)) return res;

    // 3. Add to array
    size_t new_idx = repl->agent_count;
    repl->agents[new_idx] = agent;
    repl->agent_count++;
    repl->next_agent_serial++;

    // 4. Switch to new agent
    CHECK(ik_repl_switch_agent(repl, new_idx));

    // 5. Display confirmation
    char msg[64];
    snprintf(msg, sizeof(msg), "Agent %s created", agent->agent_id);
    ik_scrollback_append_system(agent->scrollback, msg);

    return OK(NULL);
}
```

**Registration:**
```c
static const ik_command_t commands[] = {
    // ... existing commands ...
    { "/spawn", cmd_spawn, "Create a new agent" },
};
```

**Agent initialization:**
- Fresh scrollback (empty)
- Fresh input buffer (empty)
- Fresh conversation (no messages)
- Fresh layer_cake
- State: IDLE
- needs_reflow: false

## TDD Cycle

### Red
1. Add command handler declaration in `src/commands.h` (if needed)

2. Create/update tests in `tests/unit/commands/cmd_spawn_test.c`:
   - Test /spawn creates new agent
   - Test /spawn increments agent_count
   - Test /spawn increments next_agent_serial
   - Test new agent has correct agent_id (next serial + "/")
   - Test /spawn switches to new agent (current_agent_idx updated)
   - Test confirmation message appears in new agent's scrollback
   - Test /spawn at 20 agents fails with error message
   - Test error message appears in current agent's scrollback (not new)
   - Test agent_count unchanged when limit reached

3. Run `make check` - expect test failures

### Green
1. Implement `cmd_spawn()` in `src/commands.c`:
   - Check agent limit
   - Create agent via `ik_agent_create()`
   - Add to agents array
   - Increment counters
   - Switch to new agent
   - Display confirmation

2. Register command in commands table

3. Add help text for /spawn

4. Run `make check` - expect pass

### Refactor
1. Verify talloc ownership: new agent is child of repl_ctx
2. Verify agent_id is correctly formatted
3. Verify error handling doesn't leave partial state
4. Run `make lint` - verify clean

## Post-conditions
- `make check` passes
- `/spawn` command registered and functional
- Creates new agent with unique ID
- Switches to new agent automatically
- Displays confirmation message
- Refuses to exceed 20 agents with clear error
- Help text includes /spawn
