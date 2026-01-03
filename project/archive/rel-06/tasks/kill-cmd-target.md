# Task: /kill Targeted Command

## Target
User Story: 09-kill-specific.md

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
- src/commands.c (cmd_kill)
- src/repl.h (ik_repl_find_agent)

## Pre-read Tests (CREATE)
- tests/unit/commands/cmd_kill_test.c

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make lint` passes
- kill-cmd-self.md complete

## Task
Extend `/kill` to accept a UUID argument to terminate a specific agent.

**Command:**
```
/kill xyz789...
```

**Note:** Sync barrier is already handled in cmd_kill() from kill-cmd-self.md. This extends that function.

**Kill Event Tracking (Q20):**
- Record `agent_killed` event in current agent's history (the killer)
- Event metadata: `{killed_by: "user", target: "<target_uuid>"}`
- For targeted kills, current agent is the one issuing the command

**Flow:**
1. Parse UUID from args
2. Find agent by UUID (partial match allowed)
3. Validate agent exists and is running
4. If target is current agent, delegate to self-kill logic
5. If target is root, show error
6. Otherwise:
   - Record agent_killed event in current agent's history
   - Mark agent dead in registry (set status='dead', ended_at=now)
   - Remove from agents array
   - Free agent context
   - Display confirmation on current agent

## TDD Cycle

### Red
1. Add tests to `tests/unit/commands/cmd_kill_test.c`:
   - Test /kill <uuid> terminates specific agent
   - Test partial UUID matching works
   - Test ambiguous UUID shows error
   - Test non-existent UUID shows error
   - Test killing current agent switches to parent
   - Test killing root shows error
   - Test user stays on current agent
   - Test registry ended_at is set to current timestamp
   - Test agent_killed event recorded in current agent's history
   - Test agent_killed event has correct target UUID in metadata

2. Run `make check` - expect test failures

### Green
1. Implement `cmd_kill_target()` in `src/commands.c`:
   ```c
   static res_t cmd_kill_target(ik_repl_ctx_t *repl, const char *uuid_arg)
   {
       // Find target
       ik_agent_ctx_t *target = ik_repl_find_agent(repl, uuid_arg);
       if (target == NULL) {
           if (ik_repl_uuid_ambiguous(repl, uuid_arg)) {
               ik_scrollback_append_error(repl->current->scrollback,
                   "Ambiguous UUID prefix");
           } else {
               ik_scrollback_append_error(repl->current->scrollback,
                   "Agent not found");
           }
           return OK(NULL);
       }

       // Check if root
       if (target->parent_uuid == NULL) {
           ik_scrollback_append_error(repl->current->scrollback,
               "Cannot kill root agent");
           return OK(NULL);
       }

       // If killing current, use self-kill logic
       if (target == repl->current) {
           return cmd_kill(repl, NULL);
       }

       // Record kill event in current agent's history (Q20)
       CHECK(ik_db_message_insert_event(repl->shared->db,
           repl->current->uuid,
           "agent_killed",
           "{\"killed_by\": \"user\", \"target\": \"%s\"}", target->uuid));

       // Kill target (sets status='dead', ended_at=now)
       CHECK(ik_db_agent_mark_dead(repl->shared->db, target->uuid));
       CHECK(ik_repl_remove_agent(repl, target->uuid));

       char msg[64];
       snprintf(msg, sizeof(msg), "Agent %.22s terminated", target->uuid);
       ik_scrollback_append_system(repl->current->scrollback, msg);

       return OK(NULL);
   }
   ```

2. Run `make check` - expect pass

### Refactor
1. Consider what happens to target's children (cascade in next task)
2. Run `make lint` - verify clean

## Sub-agent Usage
- Use haiku sub-agents for running `make check` and `make lint` verification
- Use haiku sub-agents for git commits after each TDD phase

## Post-conditions
- `make check` passes
- `make lint` passes
- `/kill <uuid>` terminates specific agent
- Partial UUID matching works
- Error handling for ambiguous/missing UUIDs
- Working tree is clean (all changes committed)
