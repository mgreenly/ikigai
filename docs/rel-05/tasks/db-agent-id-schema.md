# Task: Add agent_id Column to Database Schema

## Target
User Story: 10-independent-scrollback.md (foundation for per-agent message storage)

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/tdd.md
- .agents/skills/errors.md
- .agents/skills/ddd.md

## Pre-read Docs
- docs/backlog/manual-top-level-agents.md (Database Schema section)
- docs/rel-05/user-stories/10-independent-scrollback.md

## Pre-read Source (patterns)
- src/db/migration.h (migration interface)
- src/db/migration.c (existing migrations)
- src/db/message.h (message structure)
- src/db/message.c (message persistence)
- src/db/connection.h (database connection)

## Pre-read Tests (patterns)
- tests/unit/db/migration_test.c (migration tests)
- tests/unit/db/message_test.c (message tests)

## Pre-conditions
- `make check` passes
- Database migration system exists
- Messages table exists with session_id column
- No agent_id column in messages table yet

## Task
Add `agent_id` column to the messages table via database migration. This enables per-agent message storage - essential for independent conversation histories.

**Schema change:**
```sql
ALTER TABLE messages
ADD COLUMN agent_id TEXT NOT NULL DEFAULT '0/';

CREATE INDEX idx_messages_agent ON messages(session_id, agent_id);
```

**Design decisions:**
- `agent_id` is TEXT type to support hierarchical paths ("0/", "1/", "0/0/")
- Default '0/' ensures existing messages belong to agent 0
- Composite index on (session_id, agent_id) for efficient queries
- NOT NULL constraint prevents orphan messages

**Migration versioning:**
- Add new migration version (e.g., v3 or next available)
- Migration runs on startup before REPL init
- Idempotent: safe to run multiple times

## TDD Cycle

### Red
1. Add migration version constant in `src/db/migration.h`:
   ```c
   #define IK_DB_MIGRATION_AGENT_ID 3  // or next available version
   ```

2. Create/update tests in `tests/unit/db/migration_agent_id_test.c`:
   - Test migration adds agent_id column to messages table
   - Test agent_id column has NOT NULL constraint
   - Test agent_id column defaults to '0/'
   - Test existing messages get agent_id = '0/'
   - Test idx_messages_agent index exists after migration
   - Test migration is idempotent (running twice is safe)

3. Run `make check` - expect test failures

### Green
1. Add migration function in `src/db/migration.c`:
   ```c
   static res_t migrate_to_v3(PGconn *conn)
   {
       // Add agent_id column with default
       const char *add_column =
           "ALTER TABLE messages "
           "ADD COLUMN IF NOT EXISTS agent_id TEXT NOT NULL DEFAULT '0/'";

       PGresult *res = PQexec(conn, add_column);
       if (PQresultStatus(res) != PGRES_COMMAND_OK) {
           PQclear(res);
           return ERR(NULL, IK_ERR_DB, "Failed to add agent_id column");
       }
       PQclear(res);

       // Create composite index
       const char *create_index =
           "CREATE INDEX IF NOT EXISTS idx_messages_agent "
           "ON messages(session_id, agent_id)";

       res = PQexec(conn, create_index);
       if (PQresultStatus(res) != PGRES_COMMAND_OK) {
           PQclear(res);
           return ERR(NULL, IK_ERR_DB, "Failed to create agent index");
       }
       PQclear(res);

       return OK(NULL);
   }
   ```

2. Register migration in migration dispatcher

3. Update schema version constant

4. Run `make check` - expect pass

### Refactor
1. Verify migration handles existing data correctly
2. Verify index improves query plan for (session_id, agent_id) lookups
3. Run `make lint` - verify clean
4. Test with real database (integration test if available)

## Post-conditions
- `make check` passes
- Messages table has agent_id column (TEXT NOT NULL DEFAULT '0/')
- Composite index exists on (session_id, agent_id)
- Existing messages have agent_id = '0/'
- Migration is idempotent
- Schema version updated
