# Task: /kill Self Command

## Target
User Stories: rel-06/user-stories/08-kill-self.md, rel-06/user-stories/11-kill-root-refused.md

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/scm.md
- .agents/skills/tdd.md
- .agents/skills/style.md
- .agents/skills/naming.md
- .agents/skills/errors.md
- .agents/skills/database.md

## Pre-read Docs
- project/agent-process-model.md (Signals section)

## Pre-read Source (READ - patterns)
- src/commands.h (command interface)
- src/commands.c (existing commands)
- src/repl.h (ik_repl_remove_agent)
- tests/unit/commands/*_test.c (test patterns)

## Pre-read Source (CREATE/MODIFY)
- src/db/agent.h (CREATE - will define ik_db_agent_mark_dead)
- src/db/agent.c (CREATE - will implement ik_db_agent_mark_dead)
- tests/unit/commands/cmd_kill_test.c (CREATE - new test file)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make lint` passes
- agent-switch-state.md complete (ik_repl_switch_agent exists)
- fork-sync-barrier.md complete
- registry-status-update.md complete (ik_db_agent_mark_dead will be created)
- repl-agent-array.md complete (ik_repl_remove_agent exists)

## Task
Implement `/kill` without arguments - terminates the current agent and switches to parent.

**Command:**
```
/kill
```

**Sync Barrier (Q10):**
- If fork_pending is true, wait for fork to complete before processing kill
- Kill command queues behind any pending fork operation

**Kill Event Tracking (Q20):**
- Record `agent_killed` event in message history before marking dead
- Event metadata: `{killed_by: "user"}` for self-kill
- Event persists in parent's history (child is about to be killed)

**Flow:**
1. Wait for fork_pending to be false (sync barrier)
2. Check if current agent is root (parent_uuid == NULL)
3. If root, display error: "Cannot kill root agent"
4. Otherwise:
   - Record agent_killed event in parent's message history
   - Mark agent dead in registry (set status='dead', ended_at=now)
   - Remove from agents array
   - Free agent context
   - Switch to parent
   - Display "Agent {uuid} terminated" on parent

## TDD Cycle

### Red
1. Create `tests/unit/commands/cmd_kill_test.c`:
   - Test /kill on non-root terminates agent
   - Test registry updated to status='dead'
   - Test registry ended_at is set to current timestamp
   - Test agent removed from array
   - Test switches to parent
   - Test /kill on root shows error
   - Test root agent not modified
   - Test kill waits for fork_pending to clear (sync barrier)
   - Test agent_killed event recorded in parent's history
   - Test agent_killed event has killed_by="user" metadata

2. Run `make check` - expect test failures

### Green
1. Implement `cmd_kill()` in `src/commands.c`:
   ```c
   res_t cmd_kill(ik_repl_ctx_t *repl, const char *args)
   {
       // Sync barrier (Q10): wait for pending fork
       while (repl->shared->fork_pending) {
           // Process events while waiting
           ik_repl_process_events(repl);
       }

       // No args = kill self
       if (args == NULL || args[0] == '\0') {
           if (repl->current->parent_uuid == NULL) {
               ik_scrollback_append_error(repl->current->scrollback,
                   "Cannot kill root agent");
               return OK(NULL);
           }

           const char *uuid = repl->current->uuid;
           ik_agent_ctx_t *parent = ik_repl_find_agent(repl,
               repl->current->parent_uuid);

           // Record kill event in parent's history (Q20)
           CHECK(ik_db_message_insert_event(repl->shared->db,
               parent->uuid,
               "agent_killed",
               "{\"killed_by\": \"user\", \"target\": \"%s\"}", uuid));

           // Mark dead in registry (sets status='dead', ended_at=now)
           CHECK(ik_db_agent_mark_dead(repl->shared->db, uuid));

           // Switch to parent first (saves state), then remove dead agent
           CHECK(ik_repl_switch_agent(repl, parent));
           CHECK(ik_repl_remove_agent(repl, uuid));

           // Notify
           char msg[64];
           snprintf(msg, sizeof(msg), "Agent %.22s terminated", uuid);
           ik_scrollback_append_system(parent->scrollback, msg);

           return OK(NULL);
       }

       // Handle targeted kill in kill-cmd-target.md
       return cmd_kill_target(repl, args);
   }
   ```

2. Register command

3. Run `make check` - expect pass

### Refactor
1. Verify cleanup order (registry before memory)
2. Run `make lint` - verify clean

## Sub-agent Usage
- Use haiku sub-agents for running `make check` and `make lint` verification
- Use haiku sub-agents for git commits after each TDD phase

## Post-conditions
- `make check` passes
- `make lint` passes
- `/kill` terminates current agent
- Root agent protected
- Switches to parent on success
- Working tree is clean (all changes committed)
