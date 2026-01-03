# Task: /kill --cascade Flag

## Target
User Story: rel-06/user-stories/10-kill-cascade.md

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
- src/commands.c (cmd_kill, cmd_kill_target)
- src/repl.h (ik_repl_find_agent, agent array traversal)
- src/agent.h (agent context, parent_uuid field)

## Pre-read Tests (MODIFY if exists, CREATE if needed)
- tests/unit/commands/cmd_kill_test.c (add cascade tests to existing file from kill-cmd-target.md)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make lint` passes
- Task kill-cmd-target.md complete (dependency)

## Task
Add `--cascade` flag to `/kill` to terminate an agent and all its descendants.

**Command:**
```
/kill xyz789... --cascade
```

**Note:** Sync barrier is already handled in cmd_kill() from kill-cmd-self.md.

## Complexity Assessment
This task involves **recursive tree traversal** to find all descendants in the agent hierarchy. The implementation requires:
- Depth-first traversal of the agent tree structure
- Transaction management for atomic operations
- Careful ordering (children before parent)
- Event tracking with metadata

**Recommendation:** This task is well-suited for `sonnet` with `thinking` mode as specified in order.json. The recursive nature is manageable with clear algorithmic patterns.

**Transaction Semantics (Q15):**
- Cascade kill is atomic: wrap in database transaction
- On any failure, ROLLBACK - no partial kills
- Only COMMIT after all descendants + target are marked dead

**Kill Event Tracking (Q20):**
- Record a single `agent_killed` event for the cascade operation
- Event metadata: `{killed_by: "user", target: "<target_uuid>", cascade: true, count: N}`
- Event recorded in current agent's history (the killer)

**Flow:**
1. Parse --cascade flag
2. BEGIN transaction
3. Find all descendants (depth-first traversal)
4. For each descendant (depth-first order - children before parent):
   - Mark dead in registry (set status='dead', ended_at=now)
   - Remove from agents array
5. Mark target dead in registry
6. Remove target from agents array
7. Record agent_killed event with cascade metadata
8. COMMIT transaction
9. Report count of terminated agents

**On failure:** ROLLBACK, display error, no agents modified

## TDD Cycle

### Red
1. Add tests to `tests/unit/commands/cmd_kill_test.c`:
   - Test --cascade kills target and children
   - Test children killed before parent (depth-first)
   - Test grandchildren included
   - Test report shows correct count
   - Test without --cascade only kills target
   - Test all killed agents have ended_at set
   - Test transaction rollback on failure (no partial kills)
   - Test agent_killed event has cascade=true metadata
   - Test agent_killed event count matches killed agents

2. Run `make check` - expect test failures

### Green
1. Implement cascade helper:
   ```c
   static size_t collect_descendants(ik_repl_ctx_t *repl,
                                     const char *uuid,
                                     ik_agent_ctx_t **out,
                                     size_t max)
   {
       size_t count = 0;
       // Find children
       for (size_t i = 0; i < repl->agent_count && count < max; i++) {
           if (repl->agents[i]->parent_uuid &&
               strcmp(repl->agents[i]->parent_uuid, uuid) == 0) {
               // Recurse first (depth-first)
               count += collect_descendants(repl, repl->agents[i]->uuid,
                                           out + count, max - count);
               out[count++] = repl->agents[i];
           }
       }
       return count;
   }
   ```

2. Implement cascade kill with transaction:
   ```c
   static res_t cmd_kill_cascade(ik_repl_ctx_t *repl, const char *uuid)
   {
       // Begin transaction (Q15)
       res_t res = ik_db_begin(repl->shared->db);
       if (is_err(&res)) return res;

       // Collect descendants
       ik_agent_ctx_t *victims[256];
       size_t count = collect_descendants(repl, uuid, victims, 256);

       // Kill descendants (depth-first order)
       for (size_t i = 0; i < count; i++) {
           res = ik_db_agent_mark_dead(repl->shared->db, victims[i]->uuid);
           if (is_err(&res)) {
               ik_db_rollback(repl->shared->db);
               return res;
           }
       }

       // Kill target
       res = ik_db_agent_mark_dead(repl->shared->db, uuid);
       if (is_err(&res)) {
           ik_db_rollback(repl->shared->db);
           return res;
       }

       // Record cascade kill event (Q20)
       res = ik_db_message_insert_event(repl->shared->db,
           repl->current->uuid,
           "agent_killed",
           "{\"killed_by\": \"user\", \"target\": \"%s\", \"cascade\": true, \"count\": %zu}",
           uuid, count + 1);
       if (is_err(&res)) {
           ik_db_rollback(repl->shared->db);
           return res;
       }

       // Commit
       res = ik_db_commit(repl->shared->db);
       if (is_err(&res)) return res;

       // Remove from memory (after DB commit succeeds)
       for (size_t i = 0; i < count; i++) {
           ik_repl_remove_agent(repl, victims[i]->uuid);
       }
       ik_repl_remove_agent(repl, uuid);

       // Report
       char msg[64];
       snprintf(msg, sizeof(msg), "Killed %zu agents", count + 1);
       ik_scrollback_append_system(repl->current->scrollback, msg);

       return OK(NULL);
   }
   ```

3. Update `cmd_kill()` to handle --cascade

4. Run `make check` - expect pass

### Refactor
1. Consider iterative instead of recursive traversal
2. Run `make lint` - verify clean

## Post-conditions
- `make check` passes
- `make lint` passes
- `--cascade` kills entire subtree
- Depth-first order (children first)
- Count reported to user
- Working tree is clean (all changes committed)

## Sub-agent Usage
- Use haiku sub-agents for running `make check` and `make lint` verification
- Use haiku sub-agents for git commits after each TDD phase (Red, Green, Refactor)
