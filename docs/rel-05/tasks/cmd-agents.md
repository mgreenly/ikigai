# Task: Implement /agents Command

## Target
User Story: 06-list-agents.md

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/tdd.md
- .agents/skills/patterns/command.md
- .agents/skills/patterns/iterator.md

## Pre-read Docs
- docs/backlog/manual-top-level-agents.md (Agent State Machine, /agents command)
- docs/rel-05/user-stories/06-list-agents.md

## Pre-read Source (patterns)
- src/commands.h (command interface)
- src/commands.c (existing slash commands)
- src/repl.h (repl context, agents array, ik_repl_state_t)
- src/agent.h (ik_agent_ctx_t, state field)
- src/scrollback.h (for output)

## Pre-read Tests (patterns)
- tests/unit/commands/commands_test.c (command tests)

## Pre-conditions
- `make check` passes
- repl-agent-array.md complete (agents[] array)
- DI-1 complete: agents have state field (ik_repl_state_t)

## Task
Implement the `/agents` command to list all agents with their current state.

**Command specification:**
- Name: `/agents`
- Arguments: None
- Output: List of all agents with state

**Output format:**
```
Agents:
  0/ (current) - IDLE
  1/ - STREAMING
  2/ - EXECUTING_TOOL
```

**State names:**
```c
static const char *state_names[] = {
    [IK_REPL_STATE_IDLE] = "IDLE",
    [IK_REPL_STATE_SENDING] = "SENDING",
    [IK_REPL_STATE_STREAMING] = "STREAMING",
    [IK_REPL_STATE_EXECUTING_TOOL] = "EXECUTING_TOOL",
};
```

**Implementation:**
```c
res_t cmd_agents(ik_repl_ctx_t *repl, const char *args)
{
    (void)args;  // No arguments

    ik_scrollback_t *sb = CURRENT_AGENT(repl)->scrollback;

    ik_scrollback_append_system(sb, "Agents:");

    for (size_t i = 0; i < repl->agent_count; i++) {
        ik_agent_ctx_t *agent = repl->agents[i];
        const char *state = state_names[agent->state];
        bool is_current = (i == repl->current_agent_idx);

        char line[128];
        if (is_current) {
            snprintf(line, sizeof(line), "  %s (current) - %s",
                     agent->agent_id, state);
        } else {
            snprintf(line, sizeof(line), "  %s - %s",
                     agent->agent_id, state);
        }
        ik_scrollback_append_system(sb, line);
    }

    return OK(NULL);
}
```

**Use cases:**
- User wants to see all active agents
- User wants to know which agent is current
- User wants to see agent states (who is busy)
- User wants to know agent IDs for /kill command

## TDD Cycle

### Red
1. Add command handler in `src/commands.c`

2. Create/update tests in `tests/unit/commands/cmd_agents_test.c`:
   - Test /agents with single agent shows one entry
   - Test /agents with multiple agents shows all
   - Test current agent marked with "(current)"
   - Test agent IDs displayed correctly
   - Test states displayed correctly (IDLE, STREAMING, etc.)
   - Test format matches spec ("  0/ (current) - IDLE")
   - Test header line is "Agents:"

3. Run `make check` - expect test failures

### Green
1. Define state_names array in `src/commands.c`:
   ```c
   static const char *state_names[] = {
       [IK_REPL_STATE_IDLE] = "IDLE",
       [IK_REPL_STATE_SENDING] = "SENDING",
       [IK_REPL_STATE_STREAMING] = "STREAMING",
       [IK_REPL_STATE_EXECUTING_TOOL] = "EXECUTING_TOOL",
   };
   ```

2. Implement `cmd_agents()` in `src/commands.c`

3. Register command in commands table

4. Add help text for /agents

5. Run `make check` - expect pass

### Refactor
1. Verify all possible states have names
2. Consider: add agent creation time? (Not for Phase 1)
3. Consider: add message count? (Not for Phase 1)
4. Run `make lint` - verify clean

## Post-conditions
- `make check` passes
- `/agents` command registered and functional
- Lists all agents with IDs and states
- Current agent marked with "(current)"
- States shown as human-readable names
- Help text includes /agents
- Output appears in current agent's scrollback
