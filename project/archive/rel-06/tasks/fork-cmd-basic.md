# Task: Basic /fork Command

## Target
User Stories: 03-fork-creates-child.md, 07-fork-auto-switch.md

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/scm.md
- .agents/skills/tdd.md
- .agents/skills/style.md
- .agents/skills/naming.md
- .agents/skills/errors.md
- .agents/skills/database.md

## Pre-read Docs
- project/agent-process-model.md (Fork section)

## Pre-read Source (patterns)
- src/commands.h (READ - command interface)
- src/commands.c (READ - existing slash commands, MODIFY - add cmd_fork)
- src/agent.h (READ - ik_agent_create)
- src/repl.h (READ - ik_repl_add_agent, ik_repl_switch_agent)
- src/shared.h (MODIFY - add fork_pending flag to ik_shared_ctx_t)

Note: src/db/agent.h does not exist yet; agent registry functions are expected from prior tasks.

## Pre-read Tests (patterns)
- tests/unit/commands/*_test.c (READ - test structure patterns, CREATE - cmd_fork_test.c)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make lint` passes
- agent-switch-state.md complete (ik_repl_switch_agent exists)
- Agent registry functions exist (ik_db_agent_insert, ik_db_begin, ik_db_commit, ik_db_rollback)
- Agent array operations exist (ik_repl_add_agent)
- Agent creation function exists (ik_agent_create)

## Task
Implement the `/fork` command without prompt argument. Creates a child agent and switches to it.

**Command:**
```
/fork
```

**Concurrency Control (Q9):**
- `fork_pending` flag in shared_ctx prevents concurrent forks
- If fork already in progress, reject with "Fork already in progress"
- Set flag at start, clear on completion or failure

**Transaction Semantics (Q14):**
- Fork is atomic: wrap in database transaction
- On any failure, ROLLBACK and leave no trace
- Only COMMIT after all steps succeed

**Flow:**
1. Check fork_pending flag; if set, reject
2. Set fork_pending = true
3. BEGIN transaction
4. Generate UUID for child
5. Insert into registry with parent_uuid = current.uuid
6. Create ik_agent_ctx_t for child
7. Add child to agents array
8. COMMIT transaction
9. Switch to child agent
10. Clear fork_pending = false
11. Display "Forked from {parent_uuid}"

**On failure:** ROLLBACK, clear fork_pending, display error

**Handler:**
```c
res_t cmd_fork(ik_repl_ctx_t *repl, const char *args);
```

## TDD Cycle

### Red
1. Add command declaration in `src/commands.h`

2. Add `fork_pending` flag to `ik_shared_ctx_t` in `src/shared.h`

3. Create `tests/unit/commands/cmd_fork_test.c`:
   - Test /fork creates new agent
   - Test child has parent_uuid set to current
   - Test child added to agents array
   - Test switches to child (current updated)
   - Test child in registry with status='running'
   - Test confirmation message displayed
   - Test fork_pending flag set during fork
   - Test fork_pending flag cleared after fork
   - Test concurrent fork rejected with "Fork already in progress"
   - Test failed fork rolls back (no orphan registry entry)
   - Test failed fork clears fork_pending flag

4. Run `make check` - expect test failures

### Green
1. Implement `cmd_fork()` in `src/commands.c`:
   ```c
   res_t cmd_fork(ik_repl_ctx_t *repl, const char *args)
   {
       (void)args;  // Handled in fork-cmd-prompt.md

       // Concurrency check (Q9)
       if (repl->shared->fork_pending) {
           ik_scrollback_append_error(repl->current->scrollback,
               "Fork already in progress");
           return OK(NULL);
       }
       repl->shared->fork_pending = true;

       // Begin transaction (Q14)
       res_t res = ik_db_begin(repl->shared->db);
       if (is_err(&res)) {
           repl->shared->fork_pending = false;
           return res;
       }

       // Create child agent
       ik_agent_ctx_t *child = NULL;
       res = ik_agent_create(repl, repl->shared,
                             repl->current->uuid, &child);
       if (is_err(&res)) {
           ik_db_rollback(repl->shared->db);
           repl->shared->fork_pending = false;
           return res;
       }

       // Insert into registry
       res = ik_db_agent_insert(repl->shared->db, child);
       if (is_err(&res)) {
           ik_db_rollback(repl->shared->db);
           repl->shared->fork_pending = false;
           return res;
       }

       // Add to array
       res = ik_repl_add_agent(repl, child);
       if (is_err(&res)) {
           ik_db_rollback(repl->shared->db);
           repl->shared->fork_pending = false;
           return res;
       }

       // Commit transaction
       res = ik_db_commit(repl->shared->db);
       if (is_err(&res)) {
           repl->shared->fork_pending = false;
           return res;
       }

       // Switch to child (uses ik_repl_switch_agent for state save/restore)
       const char *parent_uuid = repl->current->uuid;
       CHECK(ik_repl_switch_agent(repl, child));
       repl->shared->fork_pending = false;

       // Display confirmation
       char msg[64];
       snprintf(msg, sizeof(msg), "Forked from %.22s", parent_uuid);
       ik_scrollback_append_system(child->scrollback, msg);

       return OK(NULL);
   }
   ```

2. Register command in commands table

3. Run `make check` - expect pass

### Refactor
1. Verify talloc ownership (child is child of repl)
2. Verify registry and memory in sync
3. Run `make lint` - verify clean

## Sub-agent Usage
- Use haiku sub-agents for running `make check` verification after each TDD phase
- Use haiku sub-agents for running `make lint` verification
- Use haiku sub-agents for git commits after completing each TDD phase (Red, Green, Refactor)

## Post-conditions
- `make check` passes
- `make lint` passes
- `/fork` command registered
- Creates child with correct parent relationship
- Switches to child automatically
- Child in registry and agents array
- Working tree is clean (all changes committed)
