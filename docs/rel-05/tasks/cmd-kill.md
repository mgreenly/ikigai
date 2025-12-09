# Task: Implement /kill Command

## Target
User Stories: 07-kill-current-agent.md, 08-kill-specific-agent.md, 09-kill-agent-zero-refused.md

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/di.md
- .agents/skills/ddd.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/tdd.md
- .agents/skills/errors.md
- .agents/skills/patterns/command.md
- .agents/skills/patterns/arena-allocator.md

## Pre-read Docs
- docs/backlog/manual-top-level-agents.md (Killing an Agent section)
- docs/rel-05/user-stories/07-kill-current-agent.md
- docs/rel-05/user-stories/08-kill-specific-agent.md
- docs/rel-05/user-stories/09-kill-agent-zero-refused.md
- docs/memory.md (talloc cleanup)

## Pre-read Source (patterns)
- src/commands.h (command interface)
- src/commands.c (existing slash commands)
- src/repl.h (repl context, agents array)
- src/agent.h (ik_agent_ctx_t)
- src/agent_switch.h (switching function)
- src/scrollback.h (for messages)

## Pre-read Tests (patterns)
- tests/unit/commands/commands_test.c (command tests)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- repl-agent-array.md complete (agents[] array)
- agent-switch.md complete (ik_repl_switch_agent)
- cmd-spawn.md complete (can create agents to kill)

## Task
Implement the `/kill` command to terminate an agent and clean up its resources.

**Command specification:**
- Name: `/kill [agent_id]`
- Arguments: Optional agent_id (e.g., "1/")
- No argument: Kill current agent
- With argument: Kill specified agent
- Success: Terminates agent, displays "Agent N/ terminated"
- Failure: Agent 0/ cannot be killed

**Cases:**

1. **Kill current agent (not agent 0/):**
   - Switch to next agent first
   - Remove killed agent from array
   - Free agent resources
   - Display confirmation

2. **Kill specific agent (not current, not agent 0/):**
   - Find agent by ID
   - Remove from array
   - Free agent resources
   - Adjust current_agent_idx if needed
   - Display confirmation

3. **Refuse to kill agent 0/:**
   - Display error: "Cannot kill agent 0/"
   - No state changes

**Array compaction:**
When removing agent at index N:
```c
// Shift remaining agents down
for (size_t i = target_idx; i < repl->agent_count - 1; i++) {
    repl->agents[i] = repl->agents[i + 1];
}
repl->agents[repl->agent_count - 1] = NULL;
repl->agent_count--;

// Adjust current_agent_idx if necessary
if (repl->current_agent_idx >= target_idx && repl->current_agent_idx > 0) {
    repl->current_agent_idx--;
}
```

**Finding agent by ID:**
```c
static int64_t find_agent_idx(ik_repl_ctx_t *repl, const char *agent_id)
{
    for (size_t i = 0; i < repl->agent_count; i++) {
        if (strcmp(repl->agents[i]->agent_id, agent_id) == 0) {
            return (int64_t)i;
        }
    }
    return -1;  // Not found
}
```

**Resource cleanup:**
- `talloc_free(agent)` cleans up entire agent hierarchy
- Includes: scrollback, input_buffer, layer_cake, conversation, curl_multi, etc.
- Tool thread: must be stopped/joined before free (if running)

## TDD Cycle

### Red
1. Add command handler in `src/commands.c`

2. Create/update tests in `tests/unit/commands/cmd_kill_test.c`:
   - Test /kill with no arg kills current agent
   - Test /kill switches before killing current
   - Test /kill "1/" kills specific agent
   - Test /kill "0/" returns error
   - Test /kill 0/ from agent 1/ returns error
   - Test /kill decrements agent_count
   - Test array compacts correctly (no gaps)
   - Test current_agent_idx adjusted after removal
   - Test killed agent resources freed (talloc hierarchy)
   - Test /kill nonexistent agent shows error
   - Test confirmation message in scrollback

3. Run `make check` - expect test failures

### Green
1. Implement `cmd_kill()` in `src/commands.c`:
   ```c
   res_t cmd_kill(ik_repl_ctx_t *repl, const char *args)
   {
       size_t target_idx;

       if (args == NULL || args[0] == '\0') {
           // Kill current agent
           target_idx = repl->current_agent_idx;
       } else {
           // Kill specified agent
           int64_t idx = find_agent_idx(repl, args);
           if (idx < 0) {
               ik_scrollback_append_error(CURRENT_AGENT(repl)->scrollback,
                                          "Agent not found: %s", args);
               return OK(NULL);
           }
           target_idx = (size_t)idx;
       }

       // Check if agent 0/
       if (repl->agents[target_idx]->numeric_id == 0) {
           ik_scrollback_append_error(CURRENT_AGENT(repl)->scrollback,
                                      "Cannot kill agent 0/");
           return OK(NULL);
       }

       // Store agent_id for confirmation message
       char agent_id[32];
       snprintf(agent_id, sizeof(agent_id), "%s",
                repl->agents[target_idx]->agent_id);

       // If killing current, switch first
       if (target_idx == repl->current_agent_idx) {
           size_t next = ik_repl_next_agent_idx(repl);
           if (next == target_idx) {
               // Only other option is previous
               next = ik_repl_prev_agent_idx(repl);
           }
           CHECK(ik_repl_switch_agent(repl, next));
           // Note: target_idx may need adjustment after switch
       }

       // Stop tool thread if running
       ik_agent_ctx_t *target = repl->agents[target_idx];
       if (target->tool_thread_running) {
           // Cancel and join thread
           // ... implementation details ...
       }

       // Remove from array and compact
       talloc_free(target);
       for (size_t i = target_idx; i < repl->agent_count - 1; i++) {
           repl->agents[i] = repl->agents[i + 1];
       }
       repl->agents[repl->agent_count - 1] = NULL;
       repl->agent_count--;

       // Adjust current_agent_idx if needed
       if (repl->current_agent_idx > target_idx) {
           repl->current_agent_idx--;
       }

       // Display confirmation
       char msg[64];
       snprintf(msg, sizeof(msg), "Agent %s terminated", agent_id);
       ik_scrollback_append_system(CURRENT_AGENT(repl)->scrollback, msg);

       return OK(NULL);
   }
   ```

2. Register command in commands table

3. Add help text for /kill

4. Run `make check` - expect pass

### Refactor
1. Verify no memory leaks after kill
2. Verify no dangling pointers
3. Verify tool thread properly stopped
4. Consider: what if curl transfer in progress? (Cancel gracefully)
5. Run `make lint` - verify clean

## Post-conditions
- `make check` passes
- `/kill` command registered and functional
- `/kill` with no arg kills current agent (except 0/)
- `/kill N/` kills specific agent (except 0/)
- Agent 0/ is protected from kill
- Array compacts without gaps
- current_agent_idx remains valid after kill
- Resources properly freed via talloc
- Confirmation message displayed
- Error messages for invalid operations
- Working tree is clean (all changes committed)
