# Task: Filter Database Queries by agent_id

## Target
User Story: 10-independent-scrollback.md (per-agent message isolation)

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/tdd.md
- .agents/skills/errors.md
- .agents/skills/di.md

### Pre-read Docs
- docs/backlog/manual-top-level-agents.md (Database Schema section)
- docs/rel-05/user-stories/10-independent-scrollback.md

### Pre-read Source (patterns)
- src/db/message.h (message interface)
- src/db/message.c (message queries)
- src/db/replay.h (session replay interface)
- src/db/replay.c (session replay queries)
- src/agent.h (ik_agent_ctx_t with agent_id)
- src/repl.h (access to current agent)

### Pre-read Tests (patterns)
- tests/unit/db/message_test.c (message tests)
- tests/unit/db/replay_test.c (replay tests)

## Pre-conditions
- `make check` passes
- db-agent-id-schema.md complete (agent_id column exists)
- Message persistence functions exist
- Session replay functions exist

## Task
Update all database message operations to include agent_id. This ensures each agent only sees its own messages - critical for independent conversation histories.

**Functions to update:**

1. **Message Insert** - Include agent_id when persisting messages:
   ```c
   res_t ik_db_message_insert(ik_db_ctx_t *db,
                               int64_t session_id,
                               const char *agent_id,  // NEW
                               const char *role,
                               const char *content,
                               int64_t *out_id);
   ```

2. **Message Select** - Filter by agent_id when loading:
   ```c
   res_t ik_db_messages_for_agent(ik_db_ctx_t *db,
                                   int64_t session_id,
                                   const char *agent_id,
                                   ik_message_t ***out_messages,
                                   size_t *out_count);
   ```

3. **Session Replay** - Filter replay by agent_id:
   ```c
   res_t ik_db_replay_agent(ik_db_ctx_t *db,
                            int64_t session_id,
                            const char *agent_id,
                            ik_replay_callback_t callback,
                            void *user_ctx);
   ```

**Query patterns:**
```sql
-- Insert with agent_id
INSERT INTO messages (session_id, agent_id, role, content, created_at)
VALUES ($1, $2, $3, $4, NOW());

-- Select for specific agent
SELECT id, role, content, created_at
FROM messages
WHERE session_id = $1 AND agent_id = $2
ORDER BY created_at ASC;
```

## TDD Cycle

### Red
1. Update function signatures in `src/db/message.h` to include agent_id parameter

2. Create/update tests in `tests/unit/db/message_agent_test.c`:
   - Test insert with agent_id stores correct value
   - Test select with agent_id returns only that agent's messages
   - Test agent 0/ messages not visible to agent 1/
   - Test agent 1/ messages not visible to agent 0/
   - Test multiple agents in same session have isolated messages

3. Update `src/db/replay.h` signatures if needed

4. Run `make check` - expect test failures (signature changes break callers)

### Green
1. Update `src/db/message.c`:
   ```c
   res_t ik_db_message_insert(ik_db_ctx_t *db,
                               int64_t session_id,
                               const char *agent_id,
                               const char *role,
                               const char *content,
                               int64_t *out_id)
   {
       assert(agent_id != NULL);  // LCOV_EXCL_BR_LINE

       const char *query =
           "INSERT INTO messages (session_id, agent_id, role, content, created_at) "
           "VALUES ($1, $2, $3, $4, NOW()) RETURNING id";

       // ... parameterized query with agent_id as $2
   }
   ```

2. Update all callers to pass agent_id:
   - `src/repl_callbacks.c` - message persistence callbacks
   - `src/commands.c` - if any commands persist messages
   - `src/repl/session_restore.c` - session replay

3. Update replay functions similarly

4. Run `make check` - expect pass

### Refactor
1. Verify no queries access messages without agent_id filter
2. Verify agent_id is never NULL in any code path
3. Consider: helper to get current agent_id from repl context
4. Run `make lint` - verify clean

## Post-conditions
- `make check` passes
- All message inserts include agent_id
- All message selects filter by agent_id
- Session replay filters by agent_id
- Messages from different agents are isolated
- No cross-agent message leakage possible
