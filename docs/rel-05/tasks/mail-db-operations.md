# Task: Create Mail Database Operations

## Target
Phase 3: Inter-Agent Mailboxes - Step 5 (Database CRUD operations)

Supports User Stories:
- 29 (send mail) - `ik_db_mail_insert()` persists new message
- 30 (list inbox) - `ik_db_mail_query_by_agent()` loads inbox messages
- 31 (read mail) - `ik_db_mail_query_by_id()` and `ik_db_mail_mark_read()`
- 45 (mail persisted to DB) - INSERT with RETURNING id
- 46 (mail scoped to session) - all queries filter by session_id

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/tdd.md
- .agents/skills/errors.md
- .agents/skills/database.md
- .agents/skills/testability.md

## Pre-read Docs
- docs/backlog/inter-agent-mailboxes.md (Data Model, Database Schema sections)
- docs/memory.md (talloc ownership patterns)
- docs/error_handling.md (res_t patterns)
- docs/return_values.md (output parameter conventions)

## Pre-read Source (patterns)
- src/db/message.h (parameterized query patterns, INSERT without RETURNING)
- src/db/message.c (PQexecParams usage, pg_result wrapper, error handling)
- src/db/session.h (INSERT with RETURNING id pattern)
- src/db/session.c (strtoll for parsing IDs, parameter preparation, temporary context)
- src/db/replay.h (ik_message_t structure, array of struct pointers pattern)
- src/db/replay.c (SELECT with multiple rows, struct allocation, field copying)
- src/db/connection.h (ik_db_ctx_t structure)
- src/db/pg_result.h (ik_db_wrap_pg_result for automatic PQclear)
- src/mail/msg.h (ik_mail_msg_t structure definition)
- src/wrapper.h (pq_exec_params_ wrapper function)

## Pre-read Tests (patterns)
- tests/unit/db/session_test.c (database test setup/teardown, SKIP_IF_NO_DB pattern)
- tests/unit/db/message_test.c (INSERT test patterns)
- tests/unit/db/replay_core_test.c (SELECT with struct population tests)
- tests/unit/db/mail_schema_test.c (mail table tests from predecessor)
- tests/test_utils.h (ik_test_db_* helpers, transaction isolation)

## Pre-conditions
- `make check` passes
- `make lint` passes
- migrations/002-mail-table.sql exists
- mail table exists with columns: id, session_id, from_agent_id, to_agent_id, body, timestamp, read
- idx_mail_recipient index exists on (session_id, to_agent_id, read)
- `ik_mail_msg_t` struct defined in src/mail/msg.h
- `ik_mail_msg_create()` function implemented in src/mail/msg.c
- Schema tests pass (tests/unit/db/mail_schema_test.c)

## Task
Create database operation functions for the mail table in `src/db/mail.h` and `src/db/mail.c`. These functions provide the data access layer for the inter-agent mailbox system.

**Four operations required:**

### 1. Insert (User Story 45)
```c
// Insert new mail message into database
// Returns generated ID via id_out parameter
res_t ik_db_mail_insert(TALLOC_CTX *ctx, ik_db_ctx_t *db, int64_t session_id,
                         const char *from_agent_id, const char *to_agent_id,
                         const char *body, int64_t timestamp, int64_t *id_out);
```

SQL pattern:
```sql
INSERT INTO mail (session_id, from_agent_id, to_agent_id, body, timestamp, read)
VALUES ($1, $2, $3, $4, $5, 0)
RETURNING id
```

### 2. Query by Agent (User Story 46)
```c
// Query all messages for an agent (inbox query)
// Returns array of message pointers via msgs_out, count via count_out
// Empty inbox: returns OK with count_out=0, msgs_out=NULL
res_t ik_db_mail_query_by_agent(TALLOC_CTX *ctx, ik_db_ctx_t *db, int64_t session_id,
                                 const char *to_agent_id, ik_mail_msg_t ***msgs_out,
                                 size_t *count_out);
```

SQL pattern:
```sql
SELECT id, from_agent_id, to_agent_id, body, timestamp, read
FROM mail
WHERE session_id = $1 AND to_agent_id = $2
ORDER BY read ASC, timestamp DESC
```

### 3. Query by ID
```c
// Query single message by ID (for /mail read <id> command)
// Returns message via msg_out parameter
// Not found: returns OK with msg_out=NULL (not an error)
res_t ik_db_mail_query_by_id(TALLOC_CTX *ctx, ik_db_ctx_t *db, int64_t session_id,
                              int64_t id, ik_mail_msg_t **msg_out);
```

SQL pattern:
```sql
SELECT id, from_agent_id, to_agent_id, body, timestamp, read
FROM mail
WHERE session_id = $1 AND id = $2
```

### 4. Mark Read (User Story 31)
```c
// Mark message as read (update read flag to 1)
// No-op if message doesn't exist (returns OK)
res_t ik_db_mail_mark_read(ik_db_ctx_t *db, int64_t id);
```

SQL pattern:
```sql
UPDATE mail SET read = 1 WHERE id = $1
```

**Key design decisions:**
- All functions return `res_t` for consistent error handling
- Query functions use output parameters for results
- Empty results are not errors (OK with NULL/0)
- Insert returns generated ID via output parameter
- Mark read is fire-and-forget (no error on missing ID)
- Temporary talloc context (`tmp`) for query parameters, freed before return
- Message structs allocated on provided context for proper ownership
- Uses wrapper functions (`pq_exec_params_`) for testability

## TDD Cycle

### Red
1. Create `src/db/mail.h`:
   ```c
   #ifndef IK_DB_MAIL_H
   #define IK_DB_MAIL_H

   #include "../error.h"
   #include "../mail/msg.h"
   #include "connection.h"

   #include <stdint.h>
   #include <talloc.h>

   /**
    * Insert a new mail message into the database.
    *
    * Writes a mail message to the mail table with read=0 (unread).
    * Returns the database-generated ID via the id_out parameter.
    *
    * @param ctx           Talloc context for error allocation
    * @param db            Database connection context (must not be NULL)
    * @param session_id    Session ID (must be positive, FK to sessions)
    * @param from_agent_id Sender agent ID (must not be NULL)
    * @param to_agent_id   Recipient agent ID (must not be NULL)
    * @param body          Message body (must not be NULL, empty string OK)
    * @param timestamp     Unix timestamp (seconds since epoch)
    * @param id_out        Receives generated message ID (must not be NULL)
    * @return              OK on success, ERR on database error
    */
   res_t ik_db_mail_insert(TALLOC_CTX *ctx, ik_db_ctx_t *db, int64_t session_id,
                            const char *from_agent_id, const char *to_agent_id,
                            const char *body, int64_t timestamp, int64_t *id_out);

   /**
    * Query all messages for a specific agent (inbox query).
    *
    * Returns messages ordered by: unread first (read ASC), then newest first
    * (timestamp DESC). This matches the user story 46 display order.
    *
    * Memory management:
    *   - Messages array allocated on ctx
    *   - Each message struct allocated on ctx (sibling to array)
    *   - String fields are children of their message struct
    *   - Single talloc_free(ctx) releases everything
    *
    * @param ctx         Talloc context for allocations
    * @param db          Database connection context (must not be NULL)
    * @param session_id  Session ID to filter by (must be positive)
    * @param to_agent_id Recipient agent ID to filter by (must not be NULL)
    * @param msgs_out    Receives array of message pointers (NULL if empty)
    * @param count_out   Receives message count (0 if empty)
    * @return            OK on success (even if empty), ERR on database error
    */
   res_t ik_db_mail_query_by_agent(TALLOC_CTX *ctx, ik_db_ctx_t *db, int64_t session_id,
                                    const char *to_agent_id, ik_mail_msg_t ***msgs_out,
                                    size_t *count_out);

   /**
    * Query a single message by ID.
    *
    * Session ID is required to prevent cross-session message access.
    * If message not found, returns OK with msg_out=NULL (not an error).
    *
    * @param ctx         Talloc context for allocations
    * @param db          Database connection context (must not be NULL)
    * @param session_id  Session ID to filter by (must be positive)
    * @param id          Message ID to query (must be positive)
    * @param msg_out     Receives message pointer (NULL if not found)
    * @return            OK on success (even if not found), ERR on database error
    */
   res_t ik_db_mail_query_by_id(TALLOC_CTX *ctx, ik_db_ctx_t *db, int64_t session_id,
                                 int64_t id, ik_mail_msg_t **msg_out);

   /**
    * Mark a message as read.
    *
    * Updates the read flag to 1 for the specified message ID.
    * No-op if message doesn't exist (returns OK, not an error).
    *
    * @param db  Database connection context (must not be NULL)
    * @param id  Message ID to mark as read (must be positive)
    * @return    OK on success, ERR on database error
    */
   res_t ik_db_mail_mark_read(ik_db_ctx_t *db, int64_t id);

   #endif // IK_DB_MAIL_H
   ```

2. Create `tests/unit/db/mail_operations_test.c`:
   ```c
   /**
    * @file mail_operations_test.c
    * @brief Database mail operations tests
    *
    * Tests CRUD operations for the mail table:
    * - Insert new messages
    * - Query by agent (inbox)
    * - Query by ID
    * - Mark as read
    */

   #include "../../../src/db/mail.h"
   #include "../../../src/db/connection.h"
   #include "../../../src/db/session.h"
   #include "../../../src/error.h"
   #include "../../../src/mail/msg.h"
   #include "../../test_utils.h"

   #include <check.h>
   #include <libpq-fe.h>
   #include <string.h>
   #include <talloc.h>
   #include <unistd.h>

   // ========== Test Database Setup ==========

   static const char *DB_NAME;
   static bool db_available = false;

   // Per-test state
   static TALLOC_CTX *test_ctx;
   static ik_db_ctx_t *db;
   static int64_t session_id;

   // Suite-level setup
   static void suite_setup(void)
   {
       const char *skip_live = getenv("SKIP_LIVE_DB_TESTS");
       if (skip_live && strcmp(skip_live, "1") == 0) {
           db_available = false;
           return;
       }

       DB_NAME = ik_test_db_name(NULL, __FILE__);

       res_t res = ik_test_db_create(DB_NAME);
       if (is_err(&res)) {
           db_available = false;
           return;
       }

       res = ik_test_db_migrate(NULL, DB_NAME);
       if (is_err(&res)) {
           ik_test_db_destroy(DB_NAME);
           db_available = false;
           return;
       }

       db_available = true;
   }

   // Suite-level teardown
   static void suite_teardown(void)
   {
       if (db_available) {
           ik_test_db_destroy(DB_NAME);
       }
   }

   // Per-test setup
   static void test_setup(void)
   {
       if (!db_available) {
           test_ctx = NULL;
           db = NULL;
           session_id = 0;
           return;
       }

       test_ctx = talloc_new(NULL);
       res_t res = ik_test_db_connect(test_ctx, DB_NAME, &db);
       if (is_err(&res)) {
           talloc_free(test_ctx);
           test_ctx = NULL;
           db = NULL;
           return;
       }

       res = ik_test_db_begin(db);
       if (is_err(&res)) {
           talloc_free(test_ctx);
           test_ctx = NULL;
           db = NULL;
           return;
       }

       // Create a session for tests
       res = ik_db_session_create(db, &session_id);
       if (is_err(&res)) {
           ik_test_db_rollback(db);
           talloc_free(test_ctx);
           test_ctx = NULL;
           db = NULL;
           session_id = 0;
       }
   }

   // Per-test teardown
   static void test_teardown(void)
   {
       if (test_ctx != NULL) {
           if (db != NULL) {
               ik_test_db_rollback(db);
           }
           talloc_free(test_ctx);
           test_ctx = NULL;
           db = NULL;
           session_id = 0;
       }
   }

   #define SKIP_IF_NO_DB() do { if (db == NULL) return; } while (0)

   // ========== Insert Tests ==========

   // Test: Insert returns generated ID
   START_TEST(test_mail_insert_returns_id)
   {
       SKIP_IF_NO_DB();

       int64_t id = 0;
       res_t res = ik_db_mail_insert(test_ctx, db, session_id,
                                      "0/", "1/", "Hello", 1700000000, &id);

       ck_assert(is_ok(&res));
       ck_assert_int_gt(id, 0);
   }
   END_TEST

   // Test: Insert sequential IDs
   START_TEST(test_mail_insert_sequential_ids)
   {
       SKIP_IF_NO_DB();

       int64_t id1 = 0, id2 = 0, id3 = 0;

       res_t res1 = ik_db_mail_insert(test_ctx, db, session_id, "0/", "1/", "Msg1", 1700000001, &id1);
       res_t res2 = ik_db_mail_insert(test_ctx, db, session_id, "0/", "1/", "Msg2", 1700000002, &id2);
       res_t res3 = ik_db_mail_insert(test_ctx, db, session_id, "1/", "0/", "Msg3", 1700000003, &id3);

       ck_assert(is_ok(&res1));
       ck_assert(is_ok(&res2));
       ck_assert(is_ok(&res3));
       ck_assert_int_gt(id2, id1);
       ck_assert_int_gt(id3, id2);
   }
   END_TEST

   // Test: Insert with empty body (allowed)
   START_TEST(test_mail_insert_empty_body)
   {
       SKIP_IF_NO_DB();

       int64_t id = 0;
       res_t res = ik_db_mail_insert(test_ctx, db, session_id,
                                      "0/", "1/", "", 1700000000, &id);

       ck_assert(is_ok(&res));
       ck_assert_int_gt(id, 0);
   }
   END_TEST

   // Test: Insert with sub-agent IDs
   START_TEST(test_mail_insert_subagent_ids)
   {
       SKIP_IF_NO_DB();

       int64_t id = 0;
       res_t res = ik_db_mail_insert(test_ctx, db, session_id,
                                      "0/0", "0/", "Sub-agent report", 1700000000, &id);

       ck_assert(is_ok(&res));
       ck_assert_int_gt(id, 0);
   }
   END_TEST

   // Test: Insert stores read=0 by default
   START_TEST(test_mail_insert_unread_by_default)
   {
       SKIP_IF_NO_DB();

       int64_t id = 0;
       res_t res = ik_db_mail_insert(test_ctx, db, session_id,
                                      "0/", "1/", "Test", 1700000000, &id);
       ck_assert(is_ok(&res));

       // Query to verify read=0
       ik_mail_msg_t *msg = NULL;
       res = ik_db_mail_query_by_id(test_ctx, db, session_id, id, &msg);

       ck_assert(is_ok(&res));
       ck_assert_ptr_nonnull(msg);
       ck_assert(!msg->read);
   }
   END_TEST

   // Test: Insert with large body
   START_TEST(test_mail_insert_large_body)
   {
       SKIP_IF_NO_DB();

       // Create 10KB body
       char *large_body = talloc_array(test_ctx, char, 10240);
       memset(large_body, 'A', 10239);
       large_body[10239] = '\0';

       int64_t id = 0;
       res_t res = ik_db_mail_insert(test_ctx, db, session_id,
                                      "0/", "1/", large_body, 1700000000, &id);

       ck_assert(is_ok(&res));
       ck_assert_int_gt(id, 0);

       // Verify full body stored
       ik_mail_msg_t *msg = NULL;
       res = ik_db_mail_query_by_id(test_ctx, db, session_id, id, &msg);

       ck_assert(is_ok(&res));
       ck_assert_ptr_nonnull(msg);
       ck_assert_int_eq(strlen(msg->body), 10239);
   }
   END_TEST

   // Test: Insert with newlines in body
   START_TEST(test_mail_insert_multiline_body)
   {
       SKIP_IF_NO_DB();

       const char *body = "Line 1\nLine 2\nLine 3";
       int64_t id = 0;
       res_t res = ik_db_mail_insert(test_ctx, db, session_id,
                                      "0/", "1/", body, 1700000000, &id);

       ck_assert(is_ok(&res));

       // Verify newlines preserved
       ik_mail_msg_t *msg = NULL;
       res = ik_db_mail_query_by_id(test_ctx, db, session_id, id, &msg);

       ck_assert(is_ok(&res));
       ck_assert_str_eq(msg->body, body);
   }
   END_TEST

   // ========== Query by Agent Tests ==========

   // Test: Query empty inbox
   START_TEST(test_mail_query_by_agent_empty)
   {
       SKIP_IF_NO_DB();

       ik_mail_msg_t **msgs = NULL;
       size_t count = 999;

       res_t res = ik_db_mail_query_by_agent(test_ctx, db, session_id,
                                              "1/", &msgs, &count);

       ck_assert(is_ok(&res));
       ck_assert_ptr_null(msgs);
       ck_assert_int_eq(count, 0);
   }
   END_TEST

   // Test: Query inbox with messages
   START_TEST(test_mail_query_by_agent_with_messages)
   {
       SKIP_IF_NO_DB();

       // Insert messages to agent 1/
       int64_t id1, id2;
       ik_db_mail_insert(test_ctx, db, session_id, "0/", "1/", "First", 1700000001, &id1);
       ik_db_mail_insert(test_ctx, db, session_id, "0/", "1/", "Second", 1700000002, &id2);

       ik_mail_msg_t **msgs = NULL;
       size_t count = 0;

       res_t res = ik_db_mail_query_by_agent(test_ctx, db, session_id,
                                              "1/", &msgs, &count);

       ck_assert(is_ok(&res));
       ck_assert_ptr_nonnull(msgs);
       ck_assert_int_eq(count, 2);

       // Verify messages have correct data
       ck_assert_ptr_nonnull(msgs[0]);
       ck_assert_ptr_nonnull(msgs[1]);
       ck_assert_str_eq(msgs[0]->to_agent_id, "1/");
       ck_assert_str_eq(msgs[1]->to_agent_id, "1/");
   }
   END_TEST

   // Test: Query only returns messages for specified agent
   START_TEST(test_mail_query_by_agent_filters_correctly)
   {
       SKIP_IF_NO_DB();

       // Insert messages to different agents
       int64_t id;
       ik_db_mail_insert(test_ctx, db, session_id, "0/", "1/", "For 1", 1700000001, &id);
       ik_db_mail_insert(test_ctx, db, session_id, "0/", "2/", "For 2", 1700000002, &id);
       ik_db_mail_insert(test_ctx, db, session_id, "1/", "0/", "For 0", 1700000003, &id);

       // Query for agent 1/ only
       ik_mail_msg_t **msgs = NULL;
       size_t count = 0;

       res_t res = ik_db_mail_query_by_agent(test_ctx, db, session_id,
                                              "1/", &msgs, &count);

       ck_assert(is_ok(&res));
       ck_assert_int_eq(count, 1);
       ck_assert_str_eq(msgs[0]->body, "For 1");
   }
   END_TEST

   // Test: Query sorts by read ASC, timestamp DESC
   START_TEST(test_mail_query_by_agent_sort_order)
   {
       SKIP_IF_NO_DB();

       // Insert in non-sorted order with mixed read status
       int64_t id;
       ik_db_mail_insert(test_ctx, db, session_id, "0/", "1/", "Read-Old", 1700000001, &id);
       ik_db_mail_mark_read(db, id);

       ik_db_mail_insert(test_ctx, db, session_id, "0/", "1/", "Unread-Middle", 1700000002, &id);

       int64_t read_new_id;
       ik_db_mail_insert(test_ctx, db, session_id, "0/", "1/", "Read-New", 1700000003, &read_new_id);
       ik_db_mail_mark_read(db, read_new_id);

       ik_db_mail_insert(test_ctx, db, session_id, "0/", "1/", "Unread-New", 1700000004, &id);

       ik_mail_msg_t **msgs = NULL;
       size_t count = 0;

       res_t res = ik_db_mail_query_by_agent(test_ctx, db, session_id,
                                              "1/", &msgs, &count);

       ck_assert(is_ok(&res));
       ck_assert_int_eq(count, 4);

       // Verify order: unread first (by timestamp desc), then read (by timestamp desc)
       ck_assert_str_eq(msgs[0]->body, "Unread-New");
       ck_assert_str_eq(msgs[1]->body, "Unread-Middle");
       ck_assert_str_eq(msgs[2]->body, "Read-New");
       ck_assert_str_eq(msgs[3]->body, "Read-Old");
   }
   END_TEST

   // Test: Query returns all message fields
   START_TEST(test_mail_query_by_agent_all_fields)
   {
       SKIP_IF_NO_DB();

       int64_t id = 0;
       ik_db_mail_insert(test_ctx, db, session_id, "sender/", "1/", "Body text", 1700000042, &id);

       ik_mail_msg_t **msgs = NULL;
       size_t count = 0;

       res_t res = ik_db_mail_query_by_agent(test_ctx, db, session_id,
                                              "1/", &msgs, &count);

       ck_assert(is_ok(&res));
       ck_assert_int_eq(count, 1);

       ik_mail_msg_t *msg = msgs[0];
       ck_assert_int_eq(msg->id, id);
       ck_assert_str_eq(msg->from_agent_id, "sender/");
       ck_assert_str_eq(msg->to_agent_id, "1/");
       ck_assert_str_eq(msg->body, "Body text");
       ck_assert_int_eq(msg->timestamp, 1700000042);
       ck_assert(!msg->read);
   }
   END_TEST

   // Test: Query with sub-agent ID
   START_TEST(test_mail_query_by_agent_subagent)
   {
       SKIP_IF_NO_DB();

       int64_t id;
       ik_db_mail_insert(test_ctx, db, session_id, "0/", "0/0", "For sub-agent", 1700000001, &id);
       ik_db_mail_insert(test_ctx, db, session_id, "0/", "0/", "For parent", 1700000002, &id);

       // Query for sub-agent
       ik_mail_msg_t **msgs = NULL;
       size_t count = 0;

       res_t res = ik_db_mail_query_by_agent(test_ctx, db, session_id,
                                              "0/0", &msgs, &count);

       ck_assert(is_ok(&res));
       ck_assert_int_eq(count, 1);
       ck_assert_str_eq(msgs[0]->body, "For sub-agent");
   }
   END_TEST

   // Test: Query across different sessions (isolation)
   START_TEST(test_mail_query_by_agent_session_isolation)
   {
       SKIP_IF_NO_DB();

       // Insert in current session
       int64_t id;
       ik_db_mail_insert(test_ctx, db, session_id, "0/", "1/", "Session A", 1700000001, &id);

       // Create second session
       int64_t session2_id;
       res_t res = ik_db_session_create(db, &session2_id);
       ck_assert(is_ok(&res));

       // Insert in second session
       ik_db_mail_insert(test_ctx, db, session2_id, "0/", "1/", "Session B", 1700000002, &id);

       // Query first session - should only get first message
       ik_mail_msg_t **msgs = NULL;
       size_t count = 0;

       res = ik_db_mail_query_by_agent(test_ctx, db, session_id, "1/", &msgs, &count);

       ck_assert(is_ok(&res));
       ck_assert_int_eq(count, 1);
       ck_assert_str_eq(msgs[0]->body, "Session A");
   }
   END_TEST

   // ========== Query by ID Tests ==========

   // Test: Query existing message by ID
   START_TEST(test_mail_query_by_id_found)
   {
       SKIP_IF_NO_DB();

       int64_t id = 0;
       ik_db_mail_insert(test_ctx, db, session_id, "0/", "1/", "Test body", 1700000042, &id);

       ik_mail_msg_t *msg = NULL;
       res_t res = ik_db_mail_query_by_id(test_ctx, db, session_id, id, &msg);

       ck_assert(is_ok(&res));
       ck_assert_ptr_nonnull(msg);
       ck_assert_int_eq(msg->id, id);
       ck_assert_str_eq(msg->from_agent_id, "0/");
       ck_assert_str_eq(msg->to_agent_id, "1/");
       ck_assert_str_eq(msg->body, "Test body");
       ck_assert_int_eq(msg->timestamp, 1700000042);
       ck_assert(!msg->read);
   }
   END_TEST

   // Test: Query non-existent ID returns NULL (not error)
   START_TEST(test_mail_query_by_id_not_found)
   {
       SKIP_IF_NO_DB();

       ik_mail_msg_t *msg = (ik_mail_msg_t *)0xDEADBEEF;  // Non-NULL sentinel
       res_t res = ik_db_mail_query_by_id(test_ctx, db, session_id, 99999, &msg);

       ck_assert(is_ok(&res));
       ck_assert_ptr_null(msg);
   }
   END_TEST

   // Test: Query by ID enforces session isolation
   START_TEST(test_mail_query_by_id_wrong_session)
   {
       SKIP_IF_NO_DB();

       // Insert in current session
       int64_t id = 0;
       ik_db_mail_insert(test_ctx, db, session_id, "0/", "1/", "Test", 1700000000, &id);

       // Create second session
       int64_t session2_id;
       ik_db_session_create(db, &session2_id);

       // Query with wrong session - should not find
       ik_mail_msg_t *msg = (ik_mail_msg_t *)0xDEADBEEF;
       res_t res = ik_db_mail_query_by_id(test_ctx, db, session2_id, id, &msg);

       ck_assert(is_ok(&res));
       ck_assert_ptr_null(msg);
   }
   END_TEST

   // Test: Query by ID returns read status
   START_TEST(test_mail_query_by_id_read_status)
   {
       SKIP_IF_NO_DB();

       int64_t id = 0;
       ik_db_mail_insert(test_ctx, db, session_id, "0/", "1/", "Test", 1700000000, &id);

       // Before marking read
       ik_mail_msg_t *msg = NULL;
       res_t res = ik_db_mail_query_by_id(test_ctx, db, session_id, id, &msg);
       ck_assert(is_ok(&res));
       ck_assert(!msg->read);

       // Mark as read
       ik_db_mail_mark_read(db, id);

       // After marking read
       msg = NULL;
       res = ik_db_mail_query_by_id(test_ctx, db, session_id, id, &msg);
       ck_assert(is_ok(&res));
       ck_assert(msg->read);
   }
   END_TEST

   // ========== Mark Read Tests ==========

   // Test: Mark read updates flag
   START_TEST(test_mail_mark_read_success)
   {
       SKIP_IF_NO_DB();

       int64_t id = 0;
       ik_db_mail_insert(test_ctx, db, session_id, "0/", "1/", "Test", 1700000000, &id);

       res_t res = ik_db_mail_mark_read(db, id);
       ck_assert(is_ok(&res));

       // Verify via direct query
       ik_mail_msg_t *msg = NULL;
       ik_db_mail_query_by_id(test_ctx, db, session_id, id, &msg);
       ck_assert(msg->read);
   }
   END_TEST

   // Test: Mark read on non-existent ID is no-op (returns OK)
   START_TEST(test_mail_mark_read_not_found)
   {
       SKIP_IF_NO_DB();

       res_t res = ik_db_mail_mark_read(db, 99999);
       ck_assert(is_ok(&res));  // Not an error
   }
   END_TEST

   // Test: Mark read is idempotent
   START_TEST(test_mail_mark_read_idempotent)
   {
       SKIP_IF_NO_DB();

       int64_t id = 0;
       ik_db_mail_insert(test_ctx, db, session_id, "0/", "1/", "Test", 1700000000, &id);

       // Mark read multiple times
       res_t res1 = ik_db_mail_mark_read(db, id);
       res_t res2 = ik_db_mail_mark_read(db, id);
       res_t res3 = ik_db_mail_mark_read(db, id);

       ck_assert(is_ok(&res1));
       ck_assert(is_ok(&res2));
       ck_assert(is_ok(&res3));

       // Still read
       ik_mail_msg_t *msg = NULL;
       ik_db_mail_query_by_id(test_ctx, db, session_id, id, &msg);
       ck_assert(msg->read);
   }
   END_TEST

   // ========== Memory Ownership Tests ==========

   // Test: Messages owned by provided context
   START_TEST(test_mail_query_memory_ownership)
   {
       SKIP_IF_NO_DB();

       // Create child context
       TALLOC_CTX *child_ctx = talloc_new(test_ctx);

       int64_t id;
       ik_db_mail_insert(test_ctx, db, session_id, "0/", "1/", "Test", 1700000000, &id);

       ik_mail_msg_t **msgs = NULL;
       size_t count = 0;

       res_t res = ik_db_mail_query_by_agent(child_ctx, db, session_id, "1/", &msgs, &count);
       ck_assert(is_ok(&res));
       ck_assert_int_eq(count, 1);

       // Verify parent relationship
       ck_assert_ptr_eq(talloc_parent(msgs), child_ctx);
       ck_assert_ptr_eq(talloc_parent(msgs[0]), child_ctx);
       ck_assert_ptr_eq(talloc_parent(msgs[0]->body), msgs[0]);

       // Free child - should not crash, all memory cleaned up
       talloc_free(child_ctx);
   }
   END_TEST

   // Test: Query by ID memory ownership
   START_TEST(test_mail_query_by_id_memory_ownership)
   {
       SKIP_IF_NO_DB();

       TALLOC_CTX *child_ctx = talloc_new(test_ctx);

       int64_t id;
       ik_db_mail_insert(test_ctx, db, session_id, "0/", "1/", "Test", 1700000000, &id);

       ik_mail_msg_t *msg = NULL;
       res_t res = ik_db_mail_query_by_id(child_ctx, db, session_id, id, &msg);
       ck_assert(is_ok(&res));

       // Verify parent relationship
       ck_assert_ptr_eq(talloc_parent(msg), child_ctx);
       ck_assert_ptr_eq(talloc_parent(msg->body), msg);
       ck_assert_ptr_eq(talloc_parent(msg->from_agent_id), msg);
       ck_assert_ptr_eq(talloc_parent(msg->to_agent_id), msg);

       talloc_free(child_ctx);
   }
   END_TEST

   // ========== Error Handling Tests ==========

   // Test: Insert with invalid session_id fails
   START_TEST(test_mail_insert_invalid_session)
   {
       SKIP_IF_NO_DB();

       int64_t id = 0;
       // Use non-existent session ID (foreign key violation)
       res_t res = ik_db_mail_insert(test_ctx, db, 99999999,
                                      "0/", "1/", "Test", 1700000000, &id);

       ck_assert(is_err(&res));
       ck_assert_int_eq(id, 0);  // ID should not be set on error
   }
   END_TEST

   // ========== Suite Configuration ==========

   static Suite *mail_operations_suite(void)
   {
       Suite *s = suite_create("MailDbOperations");

       TCase *tc_insert = tcase_create("Insert");
       tcase_add_unchecked_fixture(tc_insert, suite_setup, suite_teardown);
       tcase_add_checked_fixture(tc_insert, test_setup, test_teardown);
       tcase_add_test(tc_insert, test_mail_insert_returns_id);
       tcase_add_test(tc_insert, test_mail_insert_sequential_ids);
       tcase_add_test(tc_insert, test_mail_insert_empty_body);
       tcase_add_test(tc_insert, test_mail_insert_subagent_ids);
       tcase_add_test(tc_insert, test_mail_insert_unread_by_default);
       tcase_add_test(tc_insert, test_mail_insert_large_body);
       tcase_add_test(tc_insert, test_mail_insert_multiline_body);
       suite_add_tcase(s, tc_insert);

       TCase *tc_query_agent = tcase_create("QueryByAgent");
       tcase_add_unchecked_fixture(tc_query_agent, suite_setup, suite_teardown);
       tcase_add_checked_fixture(tc_query_agent, test_setup, test_teardown);
       tcase_add_test(tc_query_agent, test_mail_query_by_agent_empty);
       tcase_add_test(tc_query_agent, test_mail_query_by_agent_with_messages);
       tcase_add_test(tc_query_agent, test_mail_query_by_agent_filters_correctly);
       tcase_add_test(tc_query_agent, test_mail_query_by_agent_sort_order);
       tcase_add_test(tc_query_agent, test_mail_query_by_agent_all_fields);
       tcase_add_test(tc_query_agent, test_mail_query_by_agent_subagent);
       tcase_add_test(tc_query_agent, test_mail_query_by_agent_session_isolation);
       suite_add_tcase(s, tc_query_agent);

       TCase *tc_query_id = tcase_create("QueryById");
       tcase_add_unchecked_fixture(tc_query_id, suite_setup, suite_teardown);
       tcase_add_checked_fixture(tc_query_id, test_setup, test_teardown);
       tcase_add_test(tc_query_id, test_mail_query_by_id_found);
       tcase_add_test(tc_query_id, test_mail_query_by_id_not_found);
       tcase_add_test(tc_query_id, test_mail_query_by_id_wrong_session);
       tcase_add_test(tc_query_id, test_mail_query_by_id_read_status);
       suite_add_tcase(s, tc_query_id);

       TCase *tc_mark_read = tcase_create("MarkRead");
       tcase_add_unchecked_fixture(tc_mark_read, suite_setup, suite_teardown);
       tcase_add_checked_fixture(tc_mark_read, test_setup, test_teardown);
       tcase_add_test(tc_mark_read, test_mail_mark_read_success);
       tcase_add_test(tc_mark_read, test_mail_mark_read_not_found);
       tcase_add_test(tc_mark_read, test_mail_mark_read_idempotent);
       suite_add_tcase(s, tc_mark_read);

       TCase *tc_memory = tcase_create("Memory");
       tcase_add_unchecked_fixture(tc_memory, suite_setup, suite_teardown);
       tcase_add_checked_fixture(tc_memory, test_setup, test_teardown);
       tcase_add_test(tc_memory, test_mail_query_memory_ownership);
       tcase_add_test(tc_memory, test_mail_query_by_id_memory_ownership);
       suite_add_tcase(s, tc_memory);

       TCase *tc_errors = tcase_create("Errors");
       tcase_add_unchecked_fixture(tc_errors, suite_setup, suite_teardown);
       tcase_add_checked_fixture(tc_errors, test_setup, test_teardown);
       tcase_add_test(tc_errors, test_mail_insert_invalid_session);
       suite_add_tcase(s, tc_errors);

       return s;
   }

   int main(void)
   {
       Suite *s = mail_operations_suite();
       SRunner *sr = srunner_create(s);

       srunner_run_all(sr, CK_NORMAL);
       int number_failed = srunner_ntests_failed(sr);
       srunner_free(sr);

       return (number_failed == 0) ? 0 : 1;
   }
   ```

3. Run `make check` - expect test failures (functions not implemented)

### Green
1. Create `src/db/mail.c`:
   ```c
   #include "mail.h"

   #include "pg_result.h"

   #include "../error.h"
   #include "../panic.h"
   #include "../wrapper.h"

   #include <assert.h>
   #include <inttypes.h>
   #include <libpq-fe.h>
   #include <stdio.h>
   #include <stdlib.h>
   #include <string.h>
   #include <talloc.h>

   res_t ik_db_mail_insert(TALLOC_CTX *ctx, ik_db_ctx_t *db, int64_t session_id,
                            const char *from_agent_id, const char *to_agent_id,
                            const char *body, int64_t timestamp, int64_t *id_out)
   {
       // Preconditions
       assert(ctx != NULL);           // LCOV_EXCL_BR_LINE
       assert(db != NULL);            // LCOV_EXCL_BR_LINE
       assert(db->conn != NULL);      // LCOV_EXCL_BR_LINE
       assert(session_id > 0);        // LCOV_EXCL_BR_LINE
       assert(from_agent_id != NULL); // LCOV_EXCL_BR_LINE
       assert(to_agent_id != NULL);   // LCOV_EXCL_BR_LINE
       assert(body != NULL);          // LCOV_EXCL_BR_LINE
       assert(id_out != NULL);        // LCOV_EXCL_BR_LINE

       // Initialize output
       *id_out = 0;

       // Create temporary context for query parameters
       TALLOC_CTX *tmp = talloc_new(NULL);
       if (tmp == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

       // Build parameterized query
       const char *query =
           "INSERT INTO mail (session_id, from_agent_id, to_agent_id, body, timestamp, read) "
           "VALUES ($1, $2, $3, $4, $5, 0) "
           "RETURNING id";

       // Prepare parameters
       char *session_id_str = talloc_asprintf(tmp, "%" PRId64, session_id);
       if (session_id_str == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

       char *timestamp_str = talloc_asprintf(tmp, "%" PRId64, timestamp);
       if (timestamp_str == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

       const char *params[5];
       params[0] = session_id_str;
       params[1] = from_agent_id;
       params[2] = to_agent_id;
       params[3] = body;
       params[4] = timestamp_str;

       // Execute query
       ik_pg_result_wrapper_t *res_wrapper =
           ik_db_wrap_pg_result(tmp, pq_exec_params_(db->conn, query, 5, NULL, params, NULL, NULL, 0));
       PGresult *res = res_wrapper->pg_result;

       // Check result
       if (PQresultStatus(res) != PGRES_TUPLES_OK) {
           const char *pq_err = PQerrorMessage(db->conn);
           res_t error_res = ERR(ctx, IO, "Mail insert failed: %s", pq_err);
           talloc_free(tmp);
           return error_res;
       }

       // Verify we got exactly one row back
       if (PQntuples(res) != 1) {                              // LCOV_EXCL_BR_LINE
           PANIC("Mail insert returned unexpected row count"); // LCOV_EXCL_LINE
       }

       // Extract generated ID
       const char *id_str = PQgetvalue(res, 0, 0);
       int64_t id = strtoll(id_str, NULL, 10);

       talloc_free(tmp);

       *id_out = id;
       return OK(NULL);
   }

   res_t ik_db_mail_query_by_agent(TALLOC_CTX *ctx, ik_db_ctx_t *db, int64_t session_id,
                                    const char *to_agent_id, ik_mail_msg_t ***msgs_out,
                                    size_t *count_out)
   {
       // Preconditions
       assert(ctx != NULL);          // LCOV_EXCL_BR_LINE
       assert(db != NULL);           // LCOV_EXCL_BR_LINE
       assert(db->conn != NULL);     // LCOV_EXCL_BR_LINE
       assert(session_id > 0);       // LCOV_EXCL_BR_LINE
       assert(to_agent_id != NULL);  // LCOV_EXCL_BR_LINE
       assert(msgs_out != NULL);     // LCOV_EXCL_BR_LINE
       assert(count_out != NULL);    // LCOV_EXCL_BR_LINE

       // Initialize outputs
       *msgs_out = NULL;
       *count_out = 0;

       // Create temporary context for query parameters
       TALLOC_CTX *tmp = talloc_new(NULL);
       if (tmp == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

       // Build parameterized query
       const char *query =
           "SELECT id, from_agent_id, to_agent_id, body, timestamp, read "
           "FROM mail "
           "WHERE session_id = $1 AND to_agent_id = $2 "
           "ORDER BY read ASC, timestamp DESC";

       // Prepare parameters
       char *session_id_str = talloc_asprintf(tmp, "%" PRId64, session_id);
       if (session_id_str == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

       const char *params[2];
       params[0] = session_id_str;
       params[1] = to_agent_id;

       // Execute query
       ik_pg_result_wrapper_t *res_wrapper =
           ik_db_wrap_pg_result(tmp, pq_exec_params_(db->conn, query, 2, NULL, params, NULL, NULL, 0));
       PGresult *res = res_wrapper->pg_result;

       // Check result
       if (PQresultStatus(res) != PGRES_TUPLES_OK) {
           const char *pq_err = PQerrorMessage(db->conn);
           res_t error_res = ERR(ctx, IO, "Mail query failed: %s", pq_err);
           talloc_free(tmp);
           return error_res;
       }

       // Get row count
       int num_rows = PQntuples(res);
       if (num_rows == 0) {
           talloc_free(tmp);
           return OK(NULL);
       }

       // Allocate array of message pointers
       ik_mail_msg_t **msgs = talloc_array(ctx, ik_mail_msg_t *, (size_t)num_rows);
       if (msgs == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

       // Populate messages
       for (int i = 0; i < num_rows; i++) {
           // Extract fields
           const char *id_str = PQgetvalue(res, i, 0);
           const char *from_agent = PQgetvalue(res, i, 1);
           const char *to_agent = PQgetvalue(res, i, 2);
           const char *body = PQgetvalue(res, i, 3);
           const char *timestamp_str = PQgetvalue(res, i, 4);
           const char *read_str = PQgetvalue(res, i, 5);

           // Parse values
           int64_t id = strtoll(id_str, NULL, 10);
           int64_t timestamp = strtoll(timestamp_str, NULL, 10);
           bool read = (strcmp(read_str, "1") == 0);

           // Create message via factory function
           ik_mail_msg_t *msg = NULL;
           res_t create_res = ik_mail_msg_create(ctx, id, from_agent, to_agent,
                                                  body, timestamp, read, &msg);
           if (is_err(&create_res)) {  // LCOV_EXCL_BR_LINE
               talloc_free(tmp);       // LCOV_EXCL_LINE
               talloc_free(msgs);      // LCOV_EXCL_LINE
               return create_res;      // LCOV_EXCL_LINE
           }

           msgs[i] = msg;
       }

       talloc_free(tmp);

       *msgs_out = msgs;
       *count_out = (size_t)num_rows;
       return OK(NULL);
   }

   res_t ik_db_mail_query_by_id(TALLOC_CTX *ctx, ik_db_ctx_t *db, int64_t session_id,
                                 int64_t id, ik_mail_msg_t **msg_out)
   {
       // Preconditions
       assert(ctx != NULL);       // LCOV_EXCL_BR_LINE
       assert(db != NULL);        // LCOV_EXCL_BR_LINE
       assert(db->conn != NULL);  // LCOV_EXCL_BR_LINE
       assert(session_id > 0);    // LCOV_EXCL_BR_LINE
       assert(id > 0);            // LCOV_EXCL_BR_LINE
       assert(msg_out != NULL);   // LCOV_EXCL_BR_LINE

       // Initialize output
       *msg_out = NULL;

       // Create temporary context for query parameters
       TALLOC_CTX *tmp = talloc_new(NULL);
       if (tmp == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

       // Build parameterized query
       const char *query =
           "SELECT id, from_agent_id, to_agent_id, body, timestamp, read "
           "FROM mail "
           "WHERE session_id = $1 AND id = $2";

       // Prepare parameters
       char *session_id_str = talloc_asprintf(tmp, "%" PRId64, session_id);
       if (session_id_str == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

       char *id_str = talloc_asprintf(tmp, "%" PRId64, id);
       if (id_str == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

       const char *params[2];
       params[0] = session_id_str;
       params[1] = id_str;

       // Execute query
       ik_pg_result_wrapper_t *res_wrapper =
           ik_db_wrap_pg_result(tmp, pq_exec_params_(db->conn, query, 2, NULL, params, NULL, NULL, 0));
       PGresult *res = res_wrapper->pg_result;

       // Check result
       if (PQresultStatus(res) != PGRES_TUPLES_OK) {
           const char *pq_err = PQerrorMessage(db->conn);
           res_t error_res = ERR(ctx, IO, "Mail query by ID failed: %s", pq_err);
           talloc_free(tmp);
           return error_res;
       }

       // Check if found
       if (PQntuples(res) == 0) {
           talloc_free(tmp);
           return OK(NULL);  // Not found, but not an error
       }

       // Extract fields
       const char *from_agent = PQgetvalue(res, 0, 1);
       const char *to_agent = PQgetvalue(res, 0, 2);
       const char *body = PQgetvalue(res, 0, 3);
       const char *timestamp_str = PQgetvalue(res, 0, 4);
       const char *read_str = PQgetvalue(res, 0, 5);

       // Parse values
       int64_t timestamp = strtoll(timestamp_str, NULL, 10);
       bool read = (strcmp(read_str, "1") == 0);

       // Create message via factory function
       ik_mail_msg_t *msg = NULL;
       res_t create_res = ik_mail_msg_create(ctx, id, from_agent, to_agent,
                                              body, timestamp, read, &msg);
       if (is_err(&create_res)) {  // LCOV_EXCL_BR_LINE
           talloc_free(tmp);       // LCOV_EXCL_LINE
           return create_res;      // LCOV_EXCL_LINE
       }

       talloc_free(tmp);

       *msg_out = msg;
       return OK(NULL);
   }

   res_t ik_db_mail_mark_read(ik_db_ctx_t *db, int64_t id)
   {
       // Preconditions
       assert(db != NULL);        // LCOV_EXCL_BR_LINE
       assert(db->conn != NULL);  // LCOV_EXCL_BR_LINE
       assert(id > 0);            // LCOV_EXCL_BR_LINE

       // Create temporary context for query parameters
       TALLOC_CTX *tmp = talloc_new(NULL);
       if (tmp == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

       // Build parameterized query
       const char *query = "UPDATE mail SET read = 1 WHERE id = $1";

       // Prepare parameters
       char id_str[32];
       snprintf(id_str, sizeof(id_str), "%" PRId64, id);

       const char *params[1];
       params[0] = id_str;

       // Execute query
       ik_pg_result_wrapper_t *res_wrapper =
           ik_db_wrap_pg_result(tmp, pq_exec_params_(db->conn, query, 1, NULL, params, NULL, NULL, 0));
       PGresult *res = res_wrapper->pg_result;

       // Check result
       if (PQresultStatus(res) != PGRES_COMMAND_OK) {
           const char *pq_err = PQerrorMessage(db->conn);
           res_t error_res = ERR(db, IO, "Mail mark read failed: %s", pq_err);
           talloc_free(tmp);
           return error_res;
       }

       talloc_free(tmp);
       return OK(NULL);
   }
   ```

2. Run `make check` - expect all tests pass

### Refactor
1. Verify code follows existing patterns:
   - Parameter preparation matches session.c
   - Error messages include PostgreSQL error text
   - Temporary context freed before return
   - Output parameters initialized at function start
   - assert() macros with LCOV_EXCL_BR_LINE comments

2. Verify memory ownership:
   - Messages allocated on provided context
   - String fields are children of message struct
   - Array allocated on provided context
   - All temporary allocations freed

3. Verify query patterns match user stories:
   - Insert uses RETURNING id
   - Query by agent sorts by read ASC, timestamp DESC
   - Query by ID filters by session_id
   - Mark read uses simple UPDATE

4. Run `make lint` - verify clean

5. Run `make check-valgrind` - verify no memory leaks

6. Verify test coverage:
   - All code paths covered
   - Success cases tested
   - Not-found cases tested
   - Error cases tested
   - Memory ownership tested

## Post-conditions
- `make check` passes
- `make lint` passes
- src/db/mail.h exists with 4 function declarations
- src/db/mail.c exists with 4 function implementations
- All functions follow existing database patterns:
  - Parameterized queries
  - pg_result wrapper for cleanup
  - Temporary context for parameters
  - Proper error handling with PostgreSQL error text
- Tests verify:
  - Insert returns generated ID
  - Insert stores read=0 by default
  - Insert handles empty body, large body, multiline body
  - Insert handles sub-agent IDs
  - Query by agent returns empty for no messages
  - Query by agent filters correctly
  - Query by agent sorts correctly (read ASC, timestamp DESC)
  - Query by agent returns all fields
  - Query by agent respects session isolation
  - Query by ID finds existing message
  - Query by ID returns NULL for not found
  - Query by ID respects session isolation
  - Mark read updates flag
  - Mark read is no-op for non-existent ID
  - Mark read is idempotent
  - Memory ownership is correct
  - Foreign key violations return errors
- 100% test coverage maintained

## Notes

### Function Design Rationale

**Why output parameters instead of returning values directly?**

Following existing project patterns (session.c, replay.c):
- Consistent API across all database functions
- res_t allows structured error handling
- Output parameters work well with multiple return values
- NULL output + OK indicates "not found" vs error

**Why session_id required for query_by_id?**

Security consideration: prevents cross-session message access. Even if attacker knows message ID, they cannot access messages from other sessions.

**Why mark_read doesn't take session_id?**

Simpler API since:
- Message IDs are globally unique (BIGSERIAL)
- No security benefit (marking someone else's message as read is harmless)
- Consistent with typical UPDATE patterns

**Why ik_mail_msg_create used instead of inline allocation?**

- Single source of truth for message construction
- Proper talloc ownership automatically handled
- String copying handled by factory function
- Easier to maintain

### Error Handling Strategy

**Database errors**: Return ERR with PostgreSQL error message. Caller decides how to handle (log, display, retry).

**Not found**: Return OK with NULL output. Not an error - caller checks output parameter.

**Foreign key violations**: Returned as database error (insert with invalid session_id).

### Memory Pattern

```
ctx (caller's context)
  |
  +-- msgs (array of pointers)
  |
  +-- msg[0] (ik_mail_msg_t)
  |     |
  |     +-- from_agent_id (string)
  |     +-- to_agent_id (string)
  |     +-- body (string)
  |
  +-- msg[1] (ik_mail_msg_t)
        |
        +-- ...
```

Single `talloc_free(ctx)` releases everything.

### Testing Strategy

Tests organized by operation type:
1. **Insert tests**: Basic insert, sequential IDs, edge cases
2. **Query by agent tests**: Empty, populated, filtering, sorting
3. **Query by ID tests**: Found, not found, session isolation
4. **Mark read tests**: Success, not found, idempotent
5. **Memory tests**: Ownership verification
6. **Error tests**: Database failures

Uses transaction isolation (`BEGIN`/`ROLLBACK`) for test independence.

### Integration Points

These functions will be called by:
- `/mail send` command handler (via ik_db_mail_insert)
- `/mail` command handler (via ik_db_mail_query_by_agent)
- `/mail read <id>` command handler (via ik_db_mail_query_by_id, ik_db_mail_mark_read)
- `mail` tool handler (same functions)
- Inbox loading on agent initialization (via ik_db_mail_query_by_agent)
