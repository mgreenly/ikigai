# Task: Create Mail Database Schema Migration

## Target
Phase 3: Inter-Agent Mailboxes - Step 4 (Database persistence layer)

Supports User Stories:
- 45 (mail persisted to DB) - "Handler immediately writes to database"
- 46 (mail scoped to session) - "session_id scopes mail to current session"

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/tdd.md
- .agents/skills/errors.md

## Pre-read Docs
- docs/backlog/inter-agent-mailboxes.md (Database Schema section)
- docs/memory.md (talloc ownership patterns)

## Pre-read Source (patterns)
- migrations/001-initial-schema.sql (existing migration patterns: BIGSERIAL, BIGINT, TIMESTAMPTZ, indexes)
- src/db/migration.h (migration interface, ik_db_migrate function)
- src/db/migration.c (migration system implementation, file naming conventions)
- src/db/message.h (message persistence patterns)
- src/db/message.c (INSERT/SELECT patterns, parameterized queries)
- src/mail/msg.h (ik_mail_msg_t structure - fields that map to columns)
- src/mail/inbox.h (ik_inbox_t structure)

## Pre-read Tests (patterns)
- tests/unit/db/zzz_migration_basic_test.c (migration testing patterns, temp directory, cleanup)
- tests/unit/db/message_test.c (database operation tests)

## Pre-conditions
- `make check` passes
- `make lint` passes
- `ik_mail_msg_t` struct defined with fields: id, from_agent_id, to_agent_id, body, timestamp, read
- `ik_inbox_t` struct defined with operations: create, add, get_all, get_by_id, mark_read
- `ik_agent_ctx_t` has `ik_inbox_t *inbox` and `bool mail_notification_pending` fields
- Agent initialization creates inbox
- src/mail/ module exists with msg.h, msg.c, inbox.h, inbox.c
- migrations/001-initial-schema.sql exists with sessions and messages tables
- Database migration system functional (ik_db_migrate)

## Task
Create database migration `migrations/002-mail-table.sql` that adds the mail table for persisting inter-agent messages. This migration adds:

1. **mail table** - Stores all inter-agent messages
2. **idx_mail_recipient index** - Optimizes "get unread mail for agent X" queries

**Schema (from design document):**
```sql
CREATE TABLE mail (
    id BIGSERIAL PRIMARY KEY,
    session_id BIGINT NOT NULL,
    from_agent_id TEXT NOT NULL,
    to_agent_id TEXT NOT NULL,
    body TEXT NOT NULL,
    timestamp BIGINT NOT NULL,
    read INTEGER DEFAULT 0,
    FOREIGN KEY (session_id) REFERENCES sessions(id) ON DELETE CASCADE
);

CREATE INDEX idx_mail_recipient ON mail(session_id, to_agent_id, read);
```

**Key design decisions:**
- `id` uses BIGSERIAL (matches existing sessions/messages pattern)
- `session_id` uses BIGINT with foreign key to sessions table
- `timestamp` uses BIGINT (Unix epoch seconds, matches ik_mail_msg_t.timestamp)
- `read` uses INTEGER (0/1) for PostgreSQL compatibility (no native boolean)
- `body` is TEXT NOT NULL (empty string allowed, NULL not allowed)
- Index on (session_id, to_agent_id, read) optimizes common query pattern
- ON DELETE CASCADE removes mail when session is deleted

**Query patterns this schema supports:**

From user story 45 (insert new mail):
```sql
INSERT INTO mail (session_id, from_agent_id, to_agent_id, body, timestamp, read)
VALUES ($1, $2, $3, $4, $5, 0)
RETURNING id
```

From user story 46 (get inbox for agent):
```sql
SELECT id, from_agent_id, to_agent_id, body, timestamp, read
FROM mail
WHERE session_id = $1 AND to_agent_id = $2
ORDER BY read ASC, timestamp DESC
```

**Migration versioning:**
- File: migrations/002-mail-table.sql
- Updates schema_metadata to version 2
- Uses BEGIN/COMMIT for atomicity
- Idempotent: uses IF NOT EXISTS where possible

## TDD Cycle

### Red
1. Create `tests/unit/db/mail_schema_test.c`:
   ```c
   #include "../../../src/db/migration.h"
   #include "../../../src/db/connection.h"
   #include "../../../src/db/pg_result.h"
   #include "../../../src/error.h"
   #include "../../test_utils.h"

   #include <check.h>
   #include <libpq-fe.h>
   #include <stdlib.h>
   #include <string.h>
   #include <talloc.h>

   static const char *DB_NAME = NULL;
   static TALLOC_CTX *ctx = NULL;
   static ik_db_ctx_t *db_ctx = NULL;

   // Suite-level setup: Create unique database for this test file
   static void suite_setup(void)
   {
       DB_NAME = ik_test_db_name(NULL, __FILE__);
       ik_test_db_create(DB_NAME);
   }

   // Suite-level teardown: Destroy the unique database
   static void suite_teardown(void)
   {
       ik_test_db_destroy(DB_NAME);
   }

   // Per-test setup
   static void test_setup(void)
   {
       ctx = talloc_new(NULL);
       ck_assert_ptr_nonnull(ctx);

       res_t res = ik_test_db_connect(ctx, DB_NAME, &db_ctx);
       ck_assert(is_ok(&res));

       // Run migrations (including 002-mail-table.sql)
       res = ik_db_migrate(db_ctx, "migrations");
       ck_assert(is_ok(&res));
   }

   // Per-test teardown
   static void test_teardown(void)
   {
       talloc_free(ctx);
       ctx = NULL;
       db_ctx = NULL;
   }

   // ============================================================
   // Table existence tests
   // ============================================================

   // Test: mail table exists after migration
   START_TEST(test_mail_table_exists)
   {
       ik_pg_result_wrapper_t *result_wrapper = ik_db_wrap_pg_result(ctx, PQexec(db_ctx->conn,
           "SELECT EXISTS ("
           "  SELECT FROM information_schema.tables "
           "  WHERE table_name = 'mail'"
           ")"));
       PGresult *result = result_wrapper->pg_result;

       ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
       ck_assert_str_eq(PQgetvalue(result, 0, 0), "t");
   }
   END_TEST

   // ============================================================
   // Column existence and type tests
   // ============================================================

   // Test: mail.id column exists with correct type (bigint/bigserial)
   START_TEST(test_mail_column_id)
   {
       ik_pg_result_wrapper_t *result_wrapper = ik_db_wrap_pg_result(ctx, PQexec(db_ctx->conn,
           "SELECT data_type FROM information_schema.columns "
           "WHERE table_name = 'mail' AND column_name = 'id'"));
       PGresult *result = result_wrapper->pg_result;

       ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
       ck_assert_int_eq(PQntuples(result), 1);
       ck_assert_str_eq(PQgetvalue(result, 0, 0), "bigint");
   }
   END_TEST

   // Test: mail.session_id column exists with correct type
   START_TEST(test_mail_column_session_id)
   {
       ik_pg_result_wrapper_t *result_wrapper = ik_db_wrap_pg_result(ctx, PQexec(db_ctx->conn,
           "SELECT data_type, is_nullable FROM information_schema.columns "
           "WHERE table_name = 'mail' AND column_name = 'session_id'"));
       PGresult *result = result_wrapper->pg_result;

       ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
       ck_assert_int_eq(PQntuples(result), 1);
       ck_assert_str_eq(PQgetvalue(result, 0, 0), "bigint");
       ck_assert_str_eq(PQgetvalue(result, 0, 1), "NO");  // NOT NULL
   }
   END_TEST

   // Test: mail.from_agent_id column exists with correct type
   START_TEST(test_mail_column_from_agent_id)
   {
       ik_pg_result_wrapper_t *result_wrapper = ik_db_wrap_pg_result(ctx, PQexec(db_ctx->conn,
           "SELECT data_type, is_nullable FROM information_schema.columns "
           "WHERE table_name = 'mail' AND column_name = 'from_agent_id'"));
       PGresult *result = result_wrapper->pg_result;

       ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
       ck_assert_int_eq(PQntuples(result), 1);
       ck_assert_str_eq(PQgetvalue(result, 0, 0), "text");
       ck_assert_str_eq(PQgetvalue(result, 0, 1), "NO");  // NOT NULL
   }
   END_TEST

   // Test: mail.to_agent_id column exists with correct type
   START_TEST(test_mail_column_to_agent_id)
   {
       ik_pg_result_wrapper_t *result_wrapper = ik_db_wrap_pg_result(ctx, PQexec(db_ctx->conn,
           "SELECT data_type, is_nullable FROM information_schema.columns "
           "WHERE table_name = 'mail' AND column_name = 'to_agent_id'"));
       PGresult *result = result_wrapper->pg_result;

       ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
       ck_assert_int_eq(PQntuples(result), 1);
       ck_assert_str_eq(PQgetvalue(result, 0, 0), "text");
       ck_assert_str_eq(PQgetvalue(result, 0, 1), "NO");  // NOT NULL
   }
   END_TEST

   // Test: mail.body column exists with correct type
   START_TEST(test_mail_column_body)
   {
       ik_pg_result_wrapper_t *result_wrapper = ik_db_wrap_pg_result(ctx, PQexec(db_ctx->conn,
           "SELECT data_type, is_nullable FROM information_schema.columns "
           "WHERE table_name = 'mail' AND column_name = 'body'"));
       PGresult *result = result_wrapper->pg_result;

       ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
       ck_assert_int_eq(PQntuples(result), 1);
       ck_assert_str_eq(PQgetvalue(result, 0, 0), "text");
       ck_assert_str_eq(PQgetvalue(result, 0, 1), "NO");  // NOT NULL
   }
   END_TEST

   // Test: mail.timestamp column exists with correct type
   START_TEST(test_mail_column_timestamp)
   {
       ik_pg_result_wrapper_t *result_wrapper = ik_db_wrap_pg_result(ctx, PQexec(db_ctx->conn,
           "SELECT data_type, is_nullable FROM information_schema.columns "
           "WHERE table_name = 'mail' AND column_name = 'timestamp'"));
       PGresult *result = result_wrapper->pg_result;

       ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
       ck_assert_int_eq(PQntuples(result), 1);
       ck_assert_str_eq(PQgetvalue(result, 0, 0), "bigint");
       ck_assert_str_eq(PQgetvalue(result, 0, 1), "NO");  // NOT NULL
   }
   END_TEST

   // Test: mail.read column exists with correct type and default
   START_TEST(test_mail_column_read)
   {
       ik_pg_result_wrapper_t *result_wrapper = ik_db_wrap_pg_result(ctx, PQexec(db_ctx->conn,
           "SELECT data_type, is_nullable, column_default FROM information_schema.columns "
           "WHERE table_name = 'mail' AND column_name = 'read'"));
       PGresult *result = result_wrapper->pg_result;

       ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
       ck_assert_int_eq(PQntuples(result), 1);
       ck_assert_str_eq(PQgetvalue(result, 0, 0), "integer");
       // Default value should be 0
       const char *default_val = PQgetvalue(result, 0, 2);
       ck_assert_ptr_nonnull(default_val);
       ck_assert(strstr(default_val, "0") != NULL);
   }
   END_TEST

   // ============================================================
   // Constraint tests
   // ============================================================

   // Test: mail.id is primary key
   START_TEST(test_mail_primary_key)
   {
       ik_pg_result_wrapper_t *result_wrapper = ik_db_wrap_pg_result(ctx, PQexec(db_ctx->conn,
           "SELECT constraint_type FROM information_schema.table_constraints "
           "WHERE table_name = 'mail' AND constraint_type = 'PRIMARY KEY'"));
       PGresult *result = result_wrapper->pg_result;

       ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
       ck_assert_int_eq(PQntuples(result), 1);
   }
   END_TEST

   // Test: mail.session_id has foreign key to sessions
   START_TEST(test_mail_foreign_key_session)
   {
       ik_pg_result_wrapper_t *result_wrapper = ik_db_wrap_pg_result(ctx, PQexec(db_ctx->conn,
           "SELECT tc.constraint_type, ccu.table_name AS foreign_table "
           "FROM information_schema.table_constraints tc "
           "JOIN information_schema.constraint_column_usage ccu "
           "  ON tc.constraint_name = ccu.constraint_name "
           "WHERE tc.table_name = 'mail' "
           "  AND tc.constraint_type = 'FOREIGN KEY' "
           "  AND ccu.table_name = 'sessions'"));
       PGresult *result = result_wrapper->pg_result;

       ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
       ck_assert_int_eq(PQntuples(result), 1);
   }
   END_TEST

   // ============================================================
   // Index tests
   // ============================================================

   // Test: idx_mail_recipient index exists
   START_TEST(test_mail_index_recipient)
   {
       ik_pg_result_wrapper_t *result_wrapper = ik_db_wrap_pg_result(ctx, PQexec(db_ctx->conn,
           "SELECT indexname FROM pg_indexes "
           "WHERE tablename = 'mail' AND indexname = 'idx_mail_recipient'"));
       PGresult *result = result_wrapper->pg_result;

       ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
       ck_assert_int_eq(PQntuples(result), 1);
   }
   END_TEST

   // Test: idx_mail_recipient covers correct columns (session_id, to_agent_id, read)
   START_TEST(test_mail_index_recipient_columns)
   {
       ik_pg_result_wrapper_t *result_wrapper = ik_db_wrap_pg_result(ctx, PQexec(db_ctx->conn,
           "SELECT indexdef FROM pg_indexes "
           "WHERE tablename = 'mail' AND indexname = 'idx_mail_recipient'"));
       PGresult *result = result_wrapper->pg_result;

       ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
       ck_assert_int_eq(PQntuples(result), 1);

       const char *indexdef = PQgetvalue(result, 0, 0);
       // Verify index includes all expected columns
       ck_assert_ptr_nonnull(strstr(indexdef, "session_id"));
       ck_assert_ptr_nonnull(strstr(indexdef, "to_agent_id"));
       ck_assert_ptr_nonnull(strstr(indexdef, "read"));
   }
   END_TEST

   // ============================================================
   // Schema version tests
   // ============================================================

   // Test: schema_metadata shows version >= 2 after migration
   START_TEST(test_schema_version_updated)
   {
       ik_pg_result_wrapper_t *result_wrapper = ik_db_wrap_pg_result(ctx, PQexec(db_ctx->conn,
           "SELECT schema_version FROM schema_metadata"));
       PGresult *result = result_wrapper->pg_result;

       ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
       ck_assert_int_eq(PQntuples(result), 1);

       int version = atoi(PQgetvalue(result, 0, 0));
       ck_assert_int_ge(version, 2);
   }
   END_TEST

   // ============================================================
   // Idempotency tests
   // ============================================================

   // Test: migration is idempotent (running twice is safe)
   START_TEST(test_migration_idempotent)
   {
       // Run migrations again
       res_t res = ik_db_migrate(db_ctx, "migrations");
       ck_assert(is_ok(&res));

       // Table should still exist
       ik_pg_result_wrapper_t *result_wrapper = ik_db_wrap_pg_result(ctx, PQexec(db_ctx->conn,
           "SELECT EXISTS ("
           "  SELECT FROM information_schema.tables "
           "  WHERE table_name = 'mail'"
           ")"));
       PGresult *result = result_wrapper->pg_result;

       ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
       ck_assert_str_eq(PQgetvalue(result, 0, 0), "t");
   }
   END_TEST

   // ============================================================
   // Data operation tests
   // ============================================================

   // Test: can insert mail record
   START_TEST(test_mail_insert)
   {
       // First create a session to reference
       ik_pg_result_wrapper_t *result_wrapper = ik_db_wrap_pg_result(ctx, PQexec(db_ctx->conn,
           "INSERT INTO sessions DEFAULT VALUES RETURNING id"));
       PGresult *result = result_wrapper->pg_result;
       ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
       int64_t session_id = atoll(PQgetvalue(result, 0, 0));

       // Insert mail
       char query[512];
       snprintf(query, sizeof(query),
           "INSERT INTO mail (session_id, from_agent_id, to_agent_id, body, timestamp, read) "
           "VALUES (%lld, '0/', '1/', 'Test message', 1700000000, 0) "
           "RETURNING id",
           (long long)session_id);

       result_wrapper = ik_db_wrap_pg_result(ctx, PQexec(db_ctx->conn, query));
       result = result_wrapper->pg_result;

       ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
       ck_assert_int_eq(PQntuples(result), 1);

       int64_t mail_id = atoll(PQgetvalue(result, 0, 0));
       ck_assert_int_gt(mail_id, 0);
   }
   END_TEST

   // Test: can query mail by recipient
   START_TEST(test_mail_query_by_recipient)
   {
       // Create session
       ik_pg_result_wrapper_t *result_wrapper = ik_db_wrap_pg_result(ctx, PQexec(db_ctx->conn,
           "INSERT INTO sessions DEFAULT VALUES RETURNING id"));
       PGresult *result = result_wrapper->pg_result;
       int64_t session_id = atoll(PQgetvalue(result, 0, 0));

       // Insert multiple mail records
       char query[512];

       // Mail to agent 1/
       snprintf(query, sizeof(query),
           "INSERT INTO mail (session_id, from_agent_id, to_agent_id, body, timestamp, read) "
           "VALUES (%lld, '0/', '1/', 'For agent 1', 1700000001, 0)",
           (long long)session_id);
       ik_db_wrap_pg_result(ctx, PQexec(db_ctx->conn, query));

       // Mail to agent 0/
       snprintf(query, sizeof(query),
           "INSERT INTO mail (session_id, from_agent_id, to_agent_id, body, timestamp, read) "
           "VALUES (%lld, '1/', '0/', 'For agent 0', 1700000002, 0)",
           (long long)session_id);
       ik_db_wrap_pg_result(ctx, PQexec(db_ctx->conn, query));

       // Another mail to agent 1/
       snprintf(query, sizeof(query),
           "INSERT INTO mail (session_id, from_agent_id, to_agent_id, body, timestamp, read) "
           "VALUES (%lld, '0/', '1/', 'Another for agent 1', 1700000003, 0)",
           (long long)session_id);
       ik_db_wrap_pg_result(ctx, PQexec(db_ctx->conn, query));

       // Query mail for agent 1/ only
       snprintf(query, sizeof(query),
           "SELECT id, from_agent_id, body FROM mail "
           "WHERE session_id = %lld AND to_agent_id = '1/' "
           "ORDER BY timestamp DESC",
           (long long)session_id);

       result_wrapper = ik_db_wrap_pg_result(ctx, PQexec(db_ctx->conn, query));
       result = result_wrapper->pg_result;

       ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
       ck_assert_int_eq(PQntuples(result), 2);  // Only 2 messages for agent 1/

       // Verify ordering (newest first)
       ck_assert_str_eq(PQgetvalue(result, 0, 2), "Another for agent 1");
       ck_assert_str_eq(PQgetvalue(result, 1, 2), "For agent 1");
   }
   END_TEST

   // Test: can update read status
   START_TEST(test_mail_update_read)
   {
       // Create session and mail
       ik_pg_result_wrapper_t *result_wrapper = ik_db_wrap_pg_result(ctx, PQexec(db_ctx->conn,
           "INSERT INTO sessions DEFAULT VALUES RETURNING id"));
       PGresult *result = result_wrapper->pg_result;
       int64_t session_id = atoll(PQgetvalue(result, 0, 0));

       char query[512];
       snprintf(query, sizeof(query),
           "INSERT INTO mail (session_id, from_agent_id, to_agent_id, body, timestamp, read) "
           "VALUES (%lld, '0/', '1/', 'Test', 1700000000, 0) RETURNING id",
           (long long)session_id);

       result_wrapper = ik_db_wrap_pg_result(ctx, PQexec(db_ctx->conn, query));
       result = result_wrapper->pg_result;
       int64_t mail_id = atoll(PQgetvalue(result, 0, 0));

       // Update read status
       snprintf(query, sizeof(query),
           "UPDATE mail SET read = 1 WHERE id = %lld",
           (long long)mail_id);

       result_wrapper = ik_db_wrap_pg_result(ctx, PQexec(db_ctx->conn, query));
       result = result_wrapper->pg_result;
       ck_assert_int_eq(PQresultStatus(result), PGRES_COMMAND_OK);

       // Verify update
       snprintf(query, sizeof(query),
           "SELECT read FROM mail WHERE id = %lld",
           (long long)mail_id);

       result_wrapper = ik_db_wrap_pg_result(ctx, PQexec(db_ctx->conn, query));
       result = result_wrapper->pg_result;

       ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
       ck_assert_str_eq(PQgetvalue(result, 0, 0), "1");
   }
   END_TEST

   // Test: cascade delete removes mail when session deleted
   START_TEST(test_mail_cascade_delete)
   {
       // Create session and mail
       ik_pg_result_wrapper_t *result_wrapper = ik_db_wrap_pg_result(ctx, PQexec(db_ctx->conn,
           "INSERT INTO sessions DEFAULT VALUES RETURNING id"));
       PGresult *result = result_wrapper->pg_result;
       int64_t session_id = atoll(PQgetvalue(result, 0, 0));

       char query[512];
       snprintf(query, sizeof(query),
           "INSERT INTO mail (session_id, from_agent_id, to_agent_id, body, timestamp, read) "
           "VALUES (%lld, '0/', '1/', 'Test', 1700000000, 0) RETURNING id",
           (long long)session_id);

       result_wrapper = ik_db_wrap_pg_result(ctx, PQexec(db_ctx->conn, query));
       result = result_wrapper->pg_result;
       int64_t mail_id = atoll(PQgetvalue(result, 0, 0));

       // Delete session
       snprintf(query, sizeof(query),
           "DELETE FROM sessions WHERE id = %lld",
           (long long)session_id);

       result_wrapper = ik_db_wrap_pg_result(ctx, PQexec(db_ctx->conn, query));
       result = result_wrapper->pg_result;
       ck_assert_int_eq(PQresultStatus(result), PGRES_COMMAND_OK);

       // Verify mail was cascade deleted
       snprintf(query, sizeof(query),
           "SELECT id FROM mail WHERE id = %lld",
           (long long)mail_id);

       result_wrapper = ik_db_wrap_pg_result(ctx, PQexec(db_ctx->conn, query));
       result = result_wrapper->pg_result;

       ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
       ck_assert_int_eq(PQntuples(result), 0);  // Mail should be deleted
   }
   END_TEST

   // Test: query sorts by read ASC, timestamp DESC (user story 46 pattern)
   START_TEST(test_mail_query_sort_order)
   {
       // Create session
       ik_pg_result_wrapper_t *result_wrapper = ik_db_wrap_pg_result(ctx, PQexec(db_ctx->conn,
           "INSERT INTO sessions DEFAULT VALUES RETURNING id"));
       PGresult *result = result_wrapper->pg_result;
       int64_t session_id = atoll(PQgetvalue(result, 0, 0));

       char query[512];

       // Insert messages in mixed order (different read status and timestamps)
       // Read=1, ts=1
       snprintf(query, sizeof(query),
           "INSERT INTO mail (session_id, from_agent_id, to_agent_id, body, timestamp, read) "
           "VALUES (%lld, '0/', '1/', 'Read-Old', 1700000001, 1)",
           (long long)session_id);
       ik_db_wrap_pg_result(ctx, PQexec(db_ctx->conn, query));

       // Read=0, ts=2
       snprintf(query, sizeof(query),
           "INSERT INTO mail (session_id, from_agent_id, to_agent_id, body, timestamp, read) "
           "VALUES (%lld, '0/', '1/', 'Unread-Middle', 1700000002, 0)",
           (long long)session_id);
       ik_db_wrap_pg_result(ctx, PQexec(db_ctx->conn, query));

       // Read=1, ts=3
       snprintf(query, sizeof(query),
           "INSERT INTO mail (session_id, from_agent_id, to_agent_id, body, timestamp, read) "
           "VALUES (%lld, '0/', '1/', 'Read-New', 1700000003, 1)",
           (long long)session_id);
       ik_db_wrap_pg_result(ctx, PQexec(db_ctx->conn, query));

       // Read=0, ts=4
       snprintf(query, sizeof(query),
           "INSERT INTO mail (session_id, from_agent_id, to_agent_id, body, timestamp, read) "
           "VALUES (%lld, '0/', '1/', 'Unread-New', 1700000004, 0)",
           (long long)session_id);
       ik_db_wrap_pg_result(ctx, PQexec(db_ctx->conn, query));

       // Query with user story 46 sort order
       snprintf(query, sizeof(query),
           "SELECT body, read FROM mail "
           "WHERE session_id = %lld AND to_agent_id = '1/' "
           "ORDER BY read ASC, timestamp DESC",
           (long long)session_id);

       result_wrapper = ik_db_wrap_pg_result(ctx, PQexec(db_ctx->conn, query));
       result = result_wrapper->pg_result;

       ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
       ck_assert_int_eq(PQntuples(result), 4);

       // Verify order: unread first (by timestamp desc), then read (by timestamp desc)
       ck_assert_str_eq(PQgetvalue(result, 0, 0), "Unread-New");      // read=0, ts=4
       ck_assert_str_eq(PQgetvalue(result, 1, 0), "Unread-Middle");   // read=0, ts=2
       ck_assert_str_eq(PQgetvalue(result, 2, 0), "Read-New");        // read=1, ts=3
       ck_assert_str_eq(PQgetvalue(result, 3, 0), "Read-Old");        // read=1, ts=1
   }
   END_TEST

   // Test: sub-agent IDs stored correctly
   START_TEST(test_mail_subagent_ids)
   {
       // Create session
       ik_pg_result_wrapper_t *result_wrapper = ik_db_wrap_pg_result(ctx, PQexec(db_ctx->conn,
           "INSERT INTO sessions DEFAULT VALUES RETURNING id"));
       PGresult *result = result_wrapper->pg_result;
       int64_t session_id = atoll(PQgetvalue(result, 0, 0));

       char query[512];

       // Insert mail from sub-agent
       snprintf(query, sizeof(query),
           "INSERT INTO mail (session_id, from_agent_id, to_agent_id, body, timestamp, read) "
           "VALUES (%lld, '0/0', '0/', 'Sub-agent report', 1700000000, 0) RETURNING id",
           (long long)session_id);

       result_wrapper = ik_db_wrap_pg_result(ctx, PQexec(db_ctx->conn, query));
       result = result_wrapper->pg_result;
       int64_t mail_id = atoll(PQgetvalue(result, 0, 0));

       // Query and verify
       snprintf(query, sizeof(query),
           "SELECT from_agent_id, to_agent_id FROM mail WHERE id = %lld",
           (long long)mail_id);

       result_wrapper = ik_db_wrap_pg_result(ctx, PQexec(db_ctx->conn, query));
       result = result_wrapper->pg_result;

       ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
       ck_assert_str_eq(PQgetvalue(result, 0, 0), "0/0");
       ck_assert_str_eq(PQgetvalue(result, 0, 1), "0/");
   }
   END_TEST

   // Test: empty body is allowed (NOT NULL but can be empty string)
   START_TEST(test_mail_empty_body)
   {
       // Create session
       ik_pg_result_wrapper_t *result_wrapper = ik_db_wrap_pg_result(ctx, PQexec(db_ctx->conn,
           "INSERT INTO sessions DEFAULT VALUES RETURNING id"));
       PGresult *result = result_wrapper->pg_result;
       int64_t session_id = atoll(PQgetvalue(result, 0, 0));

       char query[512];
       snprintf(query, sizeof(query),
           "INSERT INTO mail (session_id, from_agent_id, to_agent_id, body, timestamp, read) "
           "VALUES (%lld, '0/', '1/', '', 1700000000, 0) RETURNING id",
           (long long)session_id);

       result_wrapper = ik_db_wrap_pg_result(ctx, PQexec(db_ctx->conn, query));
       result = result_wrapper->pg_result;

       ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
       ck_assert_int_eq(PQntuples(result), 1);
   }
   END_TEST

   // Test: large body text is supported
   START_TEST(test_mail_large_body)
   {
       // Create session
       ik_pg_result_wrapper_t *result_wrapper = ik_db_wrap_pg_result(ctx, PQexec(db_ctx->conn,
           "INSERT INTO sessions DEFAULT VALUES RETURNING id"));
       PGresult *result = result_wrapper->pg_result;
       int64_t session_id = atoll(PQgetvalue(result, 0, 0));

       // Create a 10KB body
       char *large_body = talloc_array(ctx, char, 10240);
       memset(large_body, 'A', 10239);
       large_body[10239] = '\0';

       // Use parameterized query for large body
       const char *query = "INSERT INTO mail (session_id, from_agent_id, to_agent_id, body, timestamp, read) "
                           "VALUES ($1, $2, $3, $4, $5, $6) RETURNING id";

       char session_id_str[32];
       snprintf(session_id_str, sizeof(session_id_str), "%lld", (long long)session_id);

       const char *param_values[6] = {
           session_id_str,
           "0/",
           "1/",
           large_body,
           "1700000000",
           "0"
       };

       result_wrapper = ik_db_wrap_pg_result(ctx,
           PQexecParams(db_ctx->conn, query, 6, NULL, param_values, NULL, NULL, 0));
       result = result_wrapper->pg_result;

       ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
       ck_assert_int_eq(PQntuples(result), 1);

       int64_t mail_id = atoll(PQgetvalue(result, 0, 0));

       // Verify body was stored correctly
       char verify_query[64];
       snprintf(verify_query, sizeof(verify_query),
           "SELECT LENGTH(body) FROM mail WHERE id = %lld",
           (long long)mail_id);

       result_wrapper = ik_db_wrap_pg_result(ctx, PQexec(db_ctx->conn, verify_query));
       result = result_wrapper->pg_result;

       ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
       ck_assert_int_eq(atoi(PQgetvalue(result, 0, 0)), 10239);
   }
   END_TEST

   // ============================================================
   // Suite configuration
   // ============================================================

   static Suite *mail_schema_suite(void)
   {
       Suite *s = suite_create("MailDbSchema");

       TCase *tc_table = tcase_create("Table");
       tcase_add_checked_fixture(tc_table, test_setup, test_teardown);
       tcase_add_test(tc_table, test_mail_table_exists);
       suite_add_tcase(s, tc_table);

       TCase *tc_columns = tcase_create("Columns");
       tcase_add_checked_fixture(tc_columns, test_setup, test_teardown);
       tcase_add_test(tc_columns, test_mail_column_id);
       tcase_add_test(tc_columns, test_mail_column_session_id);
       tcase_add_test(tc_columns, test_mail_column_from_agent_id);
       tcase_add_test(tc_columns, test_mail_column_to_agent_id);
       tcase_add_test(tc_columns, test_mail_column_body);
       tcase_add_test(tc_columns, test_mail_column_timestamp);
       tcase_add_test(tc_columns, test_mail_column_read);
       suite_add_tcase(s, tc_columns);

       TCase *tc_constraints = tcase_create("Constraints");
       tcase_add_checked_fixture(tc_constraints, test_setup, test_teardown);
       tcase_add_test(tc_constraints, test_mail_primary_key);
       tcase_add_test(tc_constraints, test_mail_foreign_key_session);
       suite_add_tcase(s, tc_constraints);

       TCase *tc_indexes = tcase_create("Indexes");
       tcase_add_checked_fixture(tc_indexes, test_setup, test_teardown);
       tcase_add_test(tc_indexes, test_mail_index_recipient);
       tcase_add_test(tc_indexes, test_mail_index_recipient_columns);
       suite_add_tcase(s, tc_indexes);

       TCase *tc_version = tcase_create("Version");
       tcase_add_checked_fixture(tc_version, test_setup, test_teardown);
       tcase_add_test(tc_version, test_schema_version_updated);
       tcase_add_test(tc_version, test_migration_idempotent);
       suite_add_tcase(s, tc_version);

       TCase *tc_data = tcase_create("DataOperations");
       tcase_add_checked_fixture(tc_data, test_setup, test_teardown);
       tcase_add_test(tc_data, test_mail_insert);
       tcase_add_test(tc_data, test_mail_query_by_recipient);
       tcase_add_test(tc_data, test_mail_update_read);
       tcase_add_test(tc_data, test_mail_cascade_delete);
       tcase_add_test(tc_data, test_mail_query_sort_order);
       tcase_add_test(tc_data, test_mail_subagent_ids);
       tcase_add_test(tc_data, test_mail_empty_body);
       tcase_add_test(tc_data, test_mail_large_body);
       suite_add_tcase(s, tc_data);

       return s;
   }

   int main(void)
   {
       suite_setup();

       int number_failed;
       Suite *s = mail_schema_suite();
       SRunner *sr = srunner_create(s);

       srunner_run_all(sr, CK_NORMAL);
       number_failed = srunner_ntests_failed(sr);
       srunner_free(sr);

       suite_teardown();

       return (number_failed == 0) ? 0 : 1;
   }
   ```

2. Run `make check` - expect test failures (migration file doesn't exist)

### Green
1. Create `migrations/002-mail-table.sql`:
   ```sql
   -- Migration: 002-mail-table
   -- Description: Add mail table for inter-agent mailbox system
   --
   -- This migration creates the mail table for storing inter-agent messages.
   -- Messages are scoped to sessions (deleted when session deleted).
   --
   -- Design notes:
   -- - session_id foreign key enables session-scoped mail queries
   -- - from_agent_id and to_agent_id support hierarchical paths ("0/", "0/0", etc.)
   -- - timestamp is Unix epoch (BIGINT) for consistency with ik_mail_msg_t
   -- - read is INTEGER (0/1) for PostgreSQL compatibility
   -- - Index on (session_id, to_agent_id, read) optimizes inbox queries
   --
   -- Query patterns:
   -- - Insert: INSERT INTO mail (session_id, from_agent_id, to_agent_id, body, timestamp, read) VALUES (...)
   -- - Inbox:  SELECT * FROM mail WHERE session_id = ? AND to_agent_id = ? ORDER BY read ASC, timestamp DESC
   -- - Mark read: UPDATE mail SET read = 1 WHERE id = ?

   BEGIN;

   -- Mail table: Stores inter-agent messages
   CREATE TABLE IF NOT EXISTS mail (
       id BIGSERIAL PRIMARY KEY,
       session_id BIGINT NOT NULL REFERENCES sessions(id) ON DELETE CASCADE,
       from_agent_id TEXT NOT NULL,
       to_agent_id TEXT NOT NULL,
       body TEXT NOT NULL,
       timestamp BIGINT NOT NULL,
       read INTEGER NOT NULL DEFAULT 0
   );

   -- Index for efficient inbox queries
   -- Optimizes: SELECT * FROM mail WHERE session_id = ? AND to_agent_id = ? ORDER BY read ASC, timestamp DESC
   CREATE INDEX IF NOT EXISTS idx_mail_recipient ON mail(session_id, to_agent_id, read);

   -- Update schema version to 2
   UPDATE schema_metadata SET schema_version = 2 WHERE schema_version = 1;

   COMMIT;
   ```

2. Run `make check` - expect all tests pass

### Refactor
1. Verify migration file follows existing patterns:
   - Header comment with migration name and description
   - Design notes explaining column choices
   - Query pattern examples
   - BEGIN/COMMIT transaction wrapper
   - IF NOT EXISTS for idempotency

2. Verify column types match ik_mail_msg_t fields:
   - id: BIGSERIAL -> int64_t
   - session_id: BIGINT -> int64_t (from session context)
   - from_agent_id: TEXT -> char *
   - to_agent_id: TEXT -> char *
   - body: TEXT -> char *
   - timestamp: BIGINT -> int64_t
   - read: INTEGER (0/1) -> bool

3. Verify index supports user story 46 query pattern:
   ```sql
   SELECT * FROM mail
   WHERE session_id = ? AND to_agent_id = ?
   ORDER BY read ASC, timestamp DESC
   ```

4. Run `make lint` - verify clean

5. Run `make check-valgrind` - verify no memory leaks in tests

## Post-conditions
- `make check` passes
- `make lint` passes
- Migration file exists at migrations/002-mail-table.sql
- mail table created with columns:
  - id (BIGSERIAL PRIMARY KEY)
  - session_id (BIGINT NOT NULL, FK to sessions)
  - from_agent_id (TEXT NOT NULL)
  - to_agent_id (TEXT NOT NULL)
  - body (TEXT NOT NULL)
  - timestamp (BIGINT NOT NULL)
  - read (INTEGER NOT NULL DEFAULT 0)
- idx_mail_recipient index created on (session_id, to_agent_id, read)
- Foreign key constraint with ON DELETE CASCADE
- schema_metadata updated to version 2
- Migration is idempotent (safe to run multiple times)
- Tests verify:
  - Table exists
  - All columns exist with correct types and nullability
  - Primary key constraint
  - Foreign key to sessions
  - Index exists with correct columns
  - Schema version updated
  - Data operations work (insert, query, update, delete cascade)
  - Query sort order matches user story 46 pattern
  - Sub-agent IDs stored correctly
  - Empty and large body text supported

## Notes

### Future Tasks (not implemented here)

1. **mail-db-insert.md**: Create `ik_mail_persist()` function to INSERT mail
2. **mail-db-load.md**: Create `ik_mail_load_inbox()` to load inbox from DB
3. **mail-db-mark-read.md**: Create `ik_mail_update_read()` to UPDATE read status

### Column Type Decisions

1. **timestamp as BIGINT vs TIMESTAMPTZ**: Using BIGINT (Unix epoch) to match ik_mail_msg_t.timestamp field. Simpler to serialize/deserialize than PostgreSQL timestamp types. Consistent with how message system handles time.

2. **read as INTEGER vs BOOLEAN**: Using INTEGER (0/1) because PostgreSQL's boolean type has quirks with some client libraries. INTEGER is universally supported and matches the boolean-as-int pattern used elsewhere.

3. **Agent IDs as TEXT**: Supports hierarchical paths ("0/", "0/0", "1/0/3"). TEXT allows flexible-length paths without schema changes.

### Index Design

The idx_mail_recipient index is designed for the primary query pattern:
```sql
SELECT * FROM mail
WHERE session_id = ? AND to_agent_id = ? AND read = ?
ORDER BY read ASC, timestamp DESC
```

Column order (session_id, to_agent_id, read) allows:
- Efficient filtering by session (required for all queries)
- Efficient filtering by recipient agent
- Efficient filtering/sorting by read status

The timestamp is NOT in the index because:
- It's used only for ORDER BY, not WHERE
- PostgreSQL can sort results after index lookup efficiently
- Adding timestamp would increase index size significantly

### Testing Strategy

Tests are comprehensive to catch schema drift:
- **Existence tests**: Verify table and columns exist
- **Type tests**: Verify correct PostgreSQL types
- **Constraint tests**: Verify PK, FK, NOT NULL
- **Index tests**: Verify index exists and covers correct columns
- **Data tests**: Verify actual SQL operations work
- **Idempotency tests**: Verify migration can run multiple times

This ensures migration works correctly and matches expected schema.
