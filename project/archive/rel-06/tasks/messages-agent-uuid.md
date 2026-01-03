# Task: Add agent_uuid to Messages Table

## Target
User Stories: 05-fork-inherits-history.md

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/scm.md
- .agents/skills/tdd.md
- .agents/skills/style.md
- .agents/skills/naming.md
- .agents/skills/errors.md
- .agents/skills/database.md

## Pre-read Docs
- project/agent-process-model.md (History Storage section)
- rel-06/README.md (Phase 5 - Replay Algorithm Detail)

## Pre-read Source (patterns)
- migrations/001-initial-schema.sql (READ - existing schema)
- src/db/message.c (READ - message insertion patterns)
- src/db/message.h (READ - function signatures)

## Files to CREATE
- migrations/003-messages-agent-uuid.sql (CREATE - new migration)
- tests/unit/db/messages_agent_test.c (CREATE - new test file)

## Files to MODIFY
- src/db/message.c (MODIFY - add agent_uuid parameter)
- src/db/message.h (MODIFY - update function signature)
- All callers of ik_db_message_insert() (MODIFY - pass agent_uuid)

## Pre-read Tests (patterns)
- tests/unit/db/message_test.c

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- `make lint` passes
- Prior task dependencies:
  - registry-schema.md must be complete (agents table must exist with uuid column)
  - migrations/002-agents-table.sql must exist (referenced by this migration)

## Task
Add `agent_uuid` column to the messages table. Currently messages only have `session_id`. For sub-agent replay, each message must be attributed to a specific agent.

**Why needed:**
The replay algorithm queries messages by `agent_uuid` to load each agent's segment:
```sql
SELECT * FROM messages
WHERE agent_uuid = $1
  AND id > $2
  AND ($3 = 0 OR id <= $3)
ORDER BY created_at
```

Without `agent_uuid`, we cannot distinguish which agent created which message.

**Schema change:**
```sql
ALTER TABLE messages ADD COLUMN agent_uuid TEXT REFERENCES agents(uuid);
CREATE INDEX idx_messages_agent ON messages(agent_uuid, id);
```

**Design decisions:**
- `agent_uuid` references agents(uuid) for referential integrity
- Composite index on (agent_uuid, id) for efficient range queries
- Column is nullable initially for backward compatibility with existing data
- New inserts must include agent_uuid

**Event Kind Updates (from questions.md Q5):**
- Add "agent_killed" to `ik_db_message_is_valid_kind()` function
- Used by /kill commands to record agent termination events
- Event metadata format: `{"killed_by": "user", "target": "<uuid>"}`

**API changes:**
```c
// Update signature to require agent_uuid
res_t ik_db_message_insert(ik_db_ctx_t *db,
                            int64_t session_id,
                            const char *agent_uuid,  // NEW
                            const char *kind,
                            const char *content,
                            const char *data_json);
```

## TDD Cycle

### Red
1. Create `tests/unit/db/messages_agent_test.c`:
   - Test messages table has agent_uuid column after migration
   - Test agent_uuid references agents(uuid)
   - Test idx_messages_agent index exists
   - Test message insert with agent_uuid succeeds
   - Test message insert without agent_uuid fails (for new code paths)
   - Test query by agent_uuid returns correct subset
   - Test query with agent_uuid and id range works
   - Test "agent_killed" is valid event kind
   - Test message insert with kind="agent_killed" succeeds

2. Run `make check` - expect test failures

### Green
1. Create `migrations/003-messages-agent-uuid.sql`:
   ```sql
   BEGIN;

   -- Add agent_uuid column (nullable for existing data)
   ALTER TABLE messages ADD COLUMN IF NOT EXISTS agent_uuid TEXT REFERENCES agents(uuid);

   -- Index for efficient agent-based range queries
   CREATE INDEX IF NOT EXISTS idx_messages_agent ON messages(agent_uuid, id);

   UPDATE schema_metadata SET schema_version = 3 WHERE schema_version = 2;

   COMMIT;
   ```

2. Update `src/db/message.c`:
   - Add agent_uuid parameter to ik_db_message_insert()
   - Update INSERT query to include agent_uuid
   - Update parameter binding
   - Add "agent_killed" to ik_db_message_is_valid_kind() function

3. Update `src/db/message.h`:
   - Update function signature

4. Update all callers of ik_db_message_insert() to pass agent_uuid

5. Run `make check` - expect pass

### Refactor
1. Verify all message inserts now include agent_uuid
2. Run `make lint` - verify clean

## Sub-agent Usage
- Use haiku sub-agents for running `make check` and `make lint` verification
- Use haiku sub-agents for git commits after each TDD phase (Red, Green, Refactor)

## Post-conditions
- `make check` passes
- `make lint` passes
- Migration file exists at migrations/003-messages-agent-uuid.sql
- messages.agent_uuid column exists with FK constraint
- Index created for efficient queries
- All message inserts include agent_uuid
- Working tree is clean (all changes committed)
