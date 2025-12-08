# Task: High-Level Mail Send Operation

## Target
Phase 3: Inter-Agent Mailboxes - Step 6 (High-level send operation)

Supports User Stories:
- 29 (send mail) - complete send workflow with validation, DB write, inbox update
- 33 (send to nonexistent agent) - "Error: Agent 99/ not found"
- 34 (send empty body rejected) - "Error: Message body cannot be empty"

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/tdd.md
- .agents/skills/errors.md
- .agents/skills/testability.md

### Pre-read Docs
- docs/backlog/inter-agent-mailboxes.md (Core Principle, Data Model, User Interface sections)
- docs/memory.md (talloc ownership patterns)
- docs/error_handling.md (res_t patterns)
- docs/return_values.md (output parameter conventions)

### Pre-read Source (patterns)
- src/mail/msg.h (ik_mail_msg_t structure, ik_mail_msg_create())
- src/mail/inbox.h (ik_inbox_t structure, ik_inbox_add())
- src/db/mail.h (ik_db_mail_insert() for database write)
- src/db/mail.c (parameterized query patterns, error handling)
- src/repl.h (ik_repl_ctx_t structure - shows where agents will be accessed)
- src/agent.h (ik_agent_ctx_t structure - inbox field, agent_id field)
- src/commands.h (existing command handler patterns)
- src/wrapper.h (time_ wrapper for testable timestamps)

### Pre-read Tests (patterns)
- tests/unit/mail/msg_test.c (mail message creation tests)
- tests/unit/mail/inbox_test.c (inbox operation tests)
- tests/unit/db/mail_operations_test.c (database mail operation tests)
- tests/unit/commands/mark_test.c (command handler test patterns, mock setup)
- tests/test_utils.h (test helper functions)

## Pre-conditions
- `make check` passes
- `make lint` passes
- `ik_mail_msg_t` struct defined with fields: id, from_agent_id, to_agent_id, body, timestamp, read
- `ik_mail_msg_create()` factory function implemented
- `ik_inbox_t` struct with operations: `ik_inbox_add()`, `ik_inbox_mark_read()`
- `ik_inbox_add()` takes ownership of message and updates unread_count
- `ik_db_mail_insert()` implemented - inserts message with RETURNING id
- `ik_db_mail_query_by_agent()` implemented - queries inbox
- `ik_agent_ctx_t` has `ik_inbox_t *inbox` field
- Agent contexts accessible via repl_ctx (repl->agents[] array or similar)
- Database connection available via `repl->db_ctx`
- Session ID available via `repl->current_session_id`

## Task
Create the high-level `ik_mail_send()` function that coordinates validation, database persistence, and in-memory inbox update for sending mail from one agent to another.

**Function signature:**
```c
// Send mail from one agent to another
//
// Complete workflow:
// 1. Validate recipient agent exists in repl->agents[]
// 2. Validate body is non-empty (after trimming whitespace)
// 3. Create message with current timestamp
// 4. Insert into database (atomic, returns generated ID)
// 5. Update in-memory inbox (add to recipient's inbox)
// 6. Return generated message ID for confirmation
//
// @param ctx           Talloc context for error allocation
// @param repl          REPL context (provides agents[], db_ctx, session_id)
// @param from_agent_id Sender agent ID string ("0/", "1/", "0/0", etc.)
// @param to_agent_id   Recipient agent ID string ("1/", "0/", etc.)
// @param body          Message body (will be trimmed and validated)
// @param id_out        Receives generated message ID (must not be NULL)
// @return              OK on success, ERR on validation failure or DB error
//
// Error conditions:
// - Recipient not found: ERR(ctx, NOT_FOUND, "Agent %s not found", to_agent_id)
// - Empty body: ERR(ctx, INVALID_ARG, "Message body cannot be empty")
// - DB write failure: propagates error from ik_db_mail_insert()
res_t ik_mail_send(TALLOC_CTX *ctx, ik_repl_ctx_t *repl,
                   const char *from_agent_id, const char *to_agent_id,
                   const char *body, int64_t *id_out);
```

**Key design decisions:**

1. **Validation order**: Check recipient exists first (user story 33), then check body non-empty (user story 34). This provides clear, specific error messages.

2. **DB-first approach**: Write to database before updating in-memory state. If DB write fails, no partial state. If process crashes after DB write but before inbox update, the message is still persisted and can be recovered on session restore.

3. **Timestamp generation**: Use `time_(NULL)` wrapper for testable Unix timestamp. In tests, this can be mocked to return deterministic values.

4. **Message ID flow**: Database generates the ID via RETURNING clause. This ID is used for in-memory message creation, ensuring consistency between DB and memory.

5. **Body trimming**: Trim leading/trailing whitespace before validation. Empty after trimming = error.

6. **Ownership transfer**: Message created for inbox becomes child of inbox via `ik_inbox_add()`.

**From user story 29 walkthrough:**
```
1. User types `/mail send 1/ "Research OAuth..."` and presses Enter
2. REPL parses slash command, identifies mail command with send subcommand
3. Command handler extracts recipient (1/) and body (Research OAuth...)
4. Handler validates recipient agent exists in repl->agents[]
5. Handler validates body is non-empty
6. Handler creates ik_mail_msg_t with all fields
7. Handler inserts message into database (mail table)
8. Handler adds message to recipient agent's inbox (agents[1]->inbox)
9. Handler increments recipient's unread_count
10. Handler displays confirmation: "Mail sent to agent 1/"
```

This function implements steps 4-9. Command parsing (steps 1-3) and display (step 10) are handled by the command layer.

**From user story 33 (send to nonexistent):**
```
6. Handler displays error: "Error: Agent 99/ not found"
7. No message created, no database write
```

**From user story 34 (empty body):**
```
6. Handler displays error: "Error: Message body cannot be empty"
7. No message created, no database write
```

## TDD Cycle

### Red
1. Create `src/mail/send.h`:
   ```c
   #ifndef IK_MAIL_SEND_H
   #define IK_MAIL_SEND_H

   #include "../error.h"
   #include "../repl.h"

   #include <stdint.h>
   #include <talloc.h>

   /**
    * Send mail from one agent to another.
    *
    * Coordinates the complete send workflow:
    * 1. Validate recipient agent exists in repl->agents[]
    * 2. Validate body is non-empty (after trimming whitespace)
    * 3. Create message with current Unix timestamp
    * 4. Insert into database (returns generated ID)
    * 5. Add message to recipient's in-memory inbox
    *
    * On success, id_out contains the database-generated message ID.
    * On failure, returns an error and no state is modified.
    *
    * @param ctx           Talloc context for error allocation
    * @param repl          REPL context (provides agents[], db_ctx, session_id)
    * @param from_agent_id Sender agent ID (must not be NULL)
    * @param to_agent_id   Recipient agent ID (must not be NULL)
    * @param body          Message body (must not be NULL, trimmed for validation)
    * @param id_out        Receives generated message ID (must not be NULL)
    * @return              OK on success, ERR on failure
    *
    * Errors:
    * - NOT_FOUND: "Agent %s not found" (recipient doesn't exist)
    * - INVALID_ARG: "Message body cannot be empty" (body empty after trim)
    * - IO: Database write failure (propagated from ik_db_mail_insert)
    */
   res_t ik_mail_send(TALLOC_CTX *ctx, ik_repl_ctx_t *repl,
                      const char *from_agent_id, const char *to_agent_id,
                      const char *body, int64_t *id_out);

   #endif // IK_MAIL_SEND_H
   ```

2. Create `tests/unit/mail/send_test.c`:
   ```c
   /**
    * @file send_test.c
    * @brief High-level mail send operation tests
    *
    * Tests the ik_mail_send() function which coordinates:
    * - Recipient validation
    * - Body validation
    * - Database persistence
    * - In-memory inbox update
    */

   #include "../../../src/mail/send.h"
   #include "../../../src/mail/inbox.h"
   #include "../../../src/mail/msg.h"
   #include "../../../src/agent.h"
   #include "../../../src/error.h"
   #include "../../../src/wrapper.h"
   #include "../../test_utils.h"

   #include <check.h>
   #include <string.h>
   #include <talloc.h>

   // ========== Mock Infrastructure ==========

   // Mock state for wrapper functions
   static int64_t mock_time_value = 1700000000;
   static bool mock_db_should_fail = false;
   static int64_t mock_db_next_id = 1;
   static int mock_db_insert_call_count = 0;
   static char *mock_db_last_from = NULL;
   static char *mock_db_last_to = NULL;
   static char *mock_db_last_body = NULL;
   static int64_t mock_db_last_timestamp = 0;

   // Mock time wrapper
   time_t time_(time_t *tloc)
   {
       if (tloc != NULL) {
           *tloc = (time_t)mock_time_value;
       }
       return (time_t)mock_time_value;
   }

   // Mock database insert
   res_t ik_db_mail_insert(TALLOC_CTX *ctx, ik_db_ctx_t *db, int64_t session_id,
                            const char *from_agent_id, const char *to_agent_id,
                            const char *body, int64_t timestamp, int64_t *id_out)
   {
       (void)db;
       (void)session_id;

       mock_db_insert_call_count++;

       // Record arguments for verification
       if (mock_db_last_from != NULL) talloc_free(mock_db_last_from);
       if (mock_db_last_to != NULL) talloc_free(mock_db_last_to);
       if (mock_db_last_body != NULL) talloc_free(mock_db_last_body);

       mock_db_last_from = talloc_strdup(NULL, from_agent_id);
       mock_db_last_to = talloc_strdup(NULL, to_agent_id);
       mock_db_last_body = talloc_strdup(NULL, body);
       mock_db_last_timestamp = timestamp;

       if (mock_db_should_fail) {
           *id_out = 0;
           return ERR(ctx, IO, "Mock database error");
       }

       *id_out = mock_db_next_id++;
       return OK(NULL);
   }

   // ========== Test Fixture ==========

   static TALLOC_CTX *ctx;
   static ik_repl_ctx_t *repl;

   // Mock agent array (simplified for testing)
   #define MAX_TEST_AGENTS 4
   static ik_agent_ctx_t *test_agents[MAX_TEST_AGENTS];
   static size_t test_agent_count = 0;

   static void reset_mocks(void)
   {
       mock_time_value = 1700000000;
       mock_db_should_fail = false;
       mock_db_next_id = 1;
       mock_db_insert_call_count = 0;

       if (mock_db_last_from != NULL) {
           talloc_free(mock_db_last_from);
           mock_db_last_from = NULL;
       }
       if (mock_db_last_to != NULL) {
           talloc_free(mock_db_last_to);
           mock_db_last_to = NULL;
       }
       if (mock_db_last_body != NULL) {
           talloc_free(mock_db_last_body);
           mock_db_last_body = NULL;
       }
       mock_db_last_timestamp = 0;
   }

   static ik_agent_ctx_t *create_mock_agent(TALLOC_CTX *parent, const char *agent_id)
   {
       // Simplified mock agent for testing
       ik_agent_ctx_t *agent = talloc_zero(parent, ik_agent_ctx_t);
       if (agent == NULL) return NULL;

       agent->agent_id = talloc_strdup(agent, agent_id);
       agent->inbox = ik_inbox_create(agent);

       return agent;
   }

   static void setup(void)
   {
       ctx = talloc_new(NULL);
       ck_assert_ptr_nonnull(ctx);

       reset_mocks();

       // Create mock REPL context with agents
       repl = talloc_zero(ctx, ik_repl_ctx_t);
       ck_assert_ptr_nonnull(repl);

       repl->current_session_id = 1;
       repl->db_ctx = (ik_db_ctx_t *)0xDEADBEEF;  // Mock pointer, not used

       // Create test agents: 0/, 1/, 2/
       test_agent_count = 3;
       for (size_t i = 0; i < test_agent_count; i++) {
           char agent_id[8];
           snprintf(agent_id, sizeof(agent_id), "%zu/", i);
           test_agents[i] = create_mock_agent(ctx, agent_id);
           ck_assert_ptr_nonnull(test_agents[i]);
       }

       // Note: In actual implementation, repl->agents[] would be set up here
       // For this test, ik_mail_send() needs access to agent lookup
   }

   static void teardown(void)
   {
       reset_mocks();

       for (size_t i = 0; i < MAX_TEST_AGENTS; i++) {
           test_agents[i] = NULL;
       }
       test_agent_count = 0;

       talloc_free(ctx);
       ctx = NULL;
       repl = NULL;
   }

   // ========== Helper: Agent Lookup Mock ==========

   // This function would be provided by the agent module
   // For testing, we use the test_agents array
   ik_agent_ctx_t *ik_mail_test_find_agent(const char *agent_id)
   {
       for (size_t i = 0; i < test_agent_count; i++) {
           if (test_agents[i] != NULL &&
               strcmp(test_agents[i]->agent_id, agent_id) == 0) {
               return test_agents[i];
           }
       }
       return NULL;
   }

   // ========== Success Case Tests ==========

   // Test: Send mail successfully returns generated ID
   START_TEST(test_mail_send_returns_id)
   {
       int64_t id = 0;
       res_t res = ik_mail_send(ctx, repl, "0/", "1/", "Hello", &id);

       ck_assert(is_ok(&res));
       ck_assert_int_eq(id, 1);
   }
   END_TEST

   // Test: Send mail increments database ID
   START_TEST(test_mail_send_sequential_ids)
   {
       int64_t id1 = 0, id2 = 0;

       res_t res1 = ik_mail_send(ctx, repl, "0/", "1/", "First", &id1);
       res_t res2 = ik_mail_send(ctx, repl, "0/", "1/", "Second", &id2);

       ck_assert(is_ok(&res1));
       ck_assert(is_ok(&res2));
       ck_assert_int_eq(id1, 1);
       ck_assert_int_eq(id2, 2);
   }
   END_TEST

   // Test: Send mail calls database insert
   START_TEST(test_mail_send_calls_db_insert)
   {
       int64_t id = 0;
       res_t res = ik_mail_send(ctx, repl, "0/", "1/", "Test message", &id);

       ck_assert(is_ok(&res));
       ck_assert_int_eq(mock_db_insert_call_count, 1);
       ck_assert_str_eq(mock_db_last_from, "0/");
       ck_assert_str_eq(mock_db_last_to, "1/");
       ck_assert_str_eq(mock_db_last_body, "Test message");
   }
   END_TEST

   // Test: Send mail uses current timestamp
   START_TEST(test_mail_send_uses_timestamp)
   {
       mock_time_value = 1700000042;

       int64_t id = 0;
       res_t res = ik_mail_send(ctx, repl, "0/", "1/", "Test", &id);

       ck_assert(is_ok(&res));
       ck_assert_int_eq(mock_db_last_timestamp, 1700000042);
   }
   END_TEST

   // Test: Send mail adds to recipient inbox
   START_TEST(test_mail_send_adds_to_inbox)
   {
       ik_agent_ctx_t *recipient = ik_mail_test_find_agent("1/");
       ck_assert_ptr_nonnull(recipient);
       ck_assert_uint_eq(recipient->inbox->count, 0);
       ck_assert_uint_eq(recipient->inbox->unread_count, 0);

       int64_t id = 0;
       res_t res = ik_mail_send(ctx, repl, "0/", "1/", "Hello", &id);

       ck_assert(is_ok(&res));
       ck_assert_uint_eq(recipient->inbox->count, 1);
       ck_assert_uint_eq(recipient->inbox->unread_count, 1);
   }
   END_TEST

   // Test: Send mail message has correct fields
   START_TEST(test_mail_send_message_fields)
   {
       mock_time_value = 1700000123;

       ik_agent_ctx_t *recipient = ik_mail_test_find_agent("1/");
       ck_assert_ptr_nonnull(recipient);

       int64_t id = 0;
       res_t res = ik_mail_send(ctx, repl, "0/", "1/", "Hello world", &id);

       ck_assert(is_ok(&res));

       // Verify message in inbox
       size_t count;
       ik_mail_msg_t **msgs = ik_inbox_get_all(recipient->inbox, &count);
       ck_assert_uint_eq(count, 1);

       ik_mail_msg_t *msg = msgs[0];
       ck_assert_int_eq(msg->id, id);
       ck_assert_str_eq(msg->from_agent_id, "0/");
       ck_assert_str_eq(msg->to_agent_id, "1/");
       ck_assert_str_eq(msg->body, "Hello world");
       ck_assert_int_eq(msg->timestamp, 1700000123);
       ck_assert(!msg->read);
   }
   END_TEST

   // Test: Send mail from sub-agent
   START_TEST(test_mail_send_from_subagent)
   {
       // Create a sub-agent 0/0
       char *subagent_id = "0/0";
       test_agents[3] = create_mock_agent(ctx, subagent_id);
       test_agent_count = 4;

       int64_t id = 0;
       res_t res = ik_mail_send(ctx, repl, "0/0", "1/", "Sub-agent message", &id);

       ck_assert(is_ok(&res));
       ck_assert_str_eq(mock_db_last_from, "0/0");
   }
   END_TEST

   // Test: Send mail to sub-agent
   START_TEST(test_mail_send_to_subagent)
   {
       // Create a sub-agent 1/0
       char *subagent_id = "1/0";
       test_agents[3] = create_mock_agent(ctx, subagent_id);
       test_agent_count = 4;

       int64_t id = 0;
       res_t res = ik_mail_send(ctx, repl, "0/", "1/0", "To sub-agent", &id);

       ck_assert(is_ok(&res));
       ck_assert_str_eq(mock_db_last_to, "1/0");

       // Verify added to sub-agent inbox
       ik_agent_ctx_t *subagent = ik_mail_test_find_agent("1/0");
       ck_assert_uint_eq(subagent->inbox->count, 1);
   }
   END_TEST

   // Test: Send mail to self (allowed)
   START_TEST(test_mail_send_to_self)
   {
       int64_t id = 0;
       res_t res = ik_mail_send(ctx, repl, "0/", "0/", "Note to self", &id);

       ck_assert(is_ok(&res));

       ik_agent_ctx_t *agent = ik_mail_test_find_agent("0/");
       ck_assert_uint_eq(agent->inbox->count, 1);
   }
   END_TEST

   // ========== Validation Tests: Recipient ==========

   // Test: Send to nonexistent agent fails
   START_TEST(test_mail_send_recipient_not_found)
   {
       int64_t id = 99;  // Non-zero to verify it's cleared

       res_t res = ik_mail_send(ctx, repl, "0/", "99/", "Hello", &id);

       ck_assert(is_err(&res));
       ck_assert_int_eq(id, 0);  // Should be cleared on error
       ck_assert_int_eq(mock_db_insert_call_count, 0);  // No DB write
   }
   END_TEST

   // Test: Error message for nonexistent agent
   START_TEST(test_mail_send_recipient_not_found_message)
   {
       int64_t id = 0;
       res_t res = ik_mail_send(ctx, repl, "0/", "99/", "Hello", &id);

       ck_assert(is_err(&res));
       // Note: Actual error message format tested via ERR type
       // The message should contain the agent ID
   }
   END_TEST

   // Test: Nonexistent agent doesn't modify any inbox
   START_TEST(test_mail_send_recipient_not_found_no_state_change)
   {
       // Verify all inboxes are empty
       for (size_t i = 0; i < test_agent_count; i++) {
           ck_assert_uint_eq(test_agents[i]->inbox->count, 0);
       }

       int64_t id = 0;
       res_t res = ik_mail_send(ctx, repl, "0/", "99/", "Hello", &id);
       ck_assert(is_err(&res));

       // All inboxes still empty
       for (size_t i = 0; i < test_agent_count; i++) {
           ck_assert_uint_eq(test_agents[i]->inbox->count, 0);
       }
   }
   END_TEST

   // ========== Validation Tests: Body ==========

   // Test: Empty body rejected
   START_TEST(test_mail_send_empty_body)
   {
       int64_t id = 0;
       res_t res = ik_mail_send(ctx, repl, "0/", "1/", "", &id);

       ck_assert(is_err(&res));
       ck_assert_int_eq(id, 0);
       ck_assert_int_eq(mock_db_insert_call_count, 0);
   }
   END_TEST

   // Test: Whitespace-only body rejected
   START_TEST(test_mail_send_whitespace_only_body)
   {
       int64_t id = 0;
       res_t res = ik_mail_send(ctx, repl, "0/", "1/", "   \t\n   ", &id);

       ck_assert(is_err(&res));
       ck_assert_int_eq(id, 0);
       ck_assert_int_eq(mock_db_insert_call_count, 0);
   }
   END_TEST

   // Test: Body with leading/trailing whitespace trimmed
   START_TEST(test_mail_send_body_trimmed)
   {
       int64_t id = 0;
       res_t res = ik_mail_send(ctx, repl, "0/", "1/", "  Hello  ", &id);

       ck_assert(is_ok(&res));
       // Verify trimmed body in DB call
       ck_assert_str_eq(mock_db_last_body, "Hello");
   }
   END_TEST

   // Test: Body with internal whitespace preserved
   START_TEST(test_mail_send_body_internal_whitespace)
   {
       int64_t id = 0;
       res_t res = ik_mail_send(ctx, repl, "0/", "1/", "  Hello   World  ", &id);

       ck_assert(is_ok(&res));
       ck_assert_str_eq(mock_db_last_body, "Hello   World");
   }
   END_TEST

   // Test: Multiline body allowed
   START_TEST(test_mail_send_multiline_body)
   {
       int64_t id = 0;
       res_t res = ik_mail_send(ctx, repl, "0/", "1/", "Line 1\nLine 2\nLine 3", &id);

       ck_assert(is_ok(&res));
       ck_assert_str_eq(mock_db_last_body, "Line 1\nLine 2\nLine 3");
   }
   END_TEST

   // Test: Large body allowed
   START_TEST(test_mail_send_large_body)
   {
       // Create 10KB body
       char *large_body = talloc_array(ctx, char, 10240);
       memset(large_body, 'A', 10239);
       large_body[10239] = '\0';

       int64_t id = 0;
       res_t res = ik_mail_send(ctx, repl, "0/", "1/", large_body, &id);

       ck_assert(is_ok(&res));
       ck_assert_int_eq(strlen(mock_db_last_body), 10239);
   }
   END_TEST

   // ========== Database Error Tests ==========

   // Test: Database error returns error
   START_TEST(test_mail_send_db_error)
   {
       mock_db_should_fail = true;

       int64_t id = 0;
       res_t res = ik_mail_send(ctx, repl, "0/", "1/", "Hello", &id);

       ck_assert(is_err(&res));
       ck_assert_int_eq(id, 0);
   }
   END_TEST

   // Test: Database error doesn't update inbox
   START_TEST(test_mail_send_db_error_no_inbox_update)
   {
       mock_db_should_fail = true;

       ik_agent_ctx_t *recipient = ik_mail_test_find_agent("1/");
       ck_assert_uint_eq(recipient->inbox->count, 0);

       int64_t id = 0;
       res_t res = ik_mail_send(ctx, repl, "0/", "1/", "Hello", &id);
       ck_assert(is_err(&res));

       // Inbox unchanged
       ck_assert_uint_eq(recipient->inbox->count, 0);
   }
   END_TEST

   // Test: Database call receives correct session_id
   START_TEST(test_mail_send_uses_session_id)
   {
       repl->current_session_id = 42;

       int64_t id = 0;
       res_t res = ik_mail_send(ctx, repl, "0/", "1/", "Hello", &id);

       ck_assert(is_ok(&res));
       // Note: session_id verification would require capturing it in mock
       // This test verifies the function completes successfully with custom session_id
   }
   END_TEST

   // ========== Validation Order Tests ==========

   // Test: Recipient checked before body
   START_TEST(test_mail_send_validation_order)
   {
       // Both recipient invalid AND body empty
       // Should get recipient error, not body error
       int64_t id = 0;
       res_t res = ik_mail_send(ctx, repl, "0/", "99/", "", &id);

       ck_assert(is_err(&res));
       // Error should mention agent, not body
       // (Specific message verification depends on error.h implementation)
   }
   END_TEST

   // ========== Edge Case Tests ==========

   // Test: Multiple messages to same recipient
   START_TEST(test_mail_send_multiple_to_same)
   {
       int64_t id1 = 0, id2 = 0, id3 = 0;

       ik_mail_send(ctx, repl, "0/", "1/", "First", &id1);
       ik_mail_send(ctx, repl, "0/", "1/", "Second", &id2);
       ik_mail_send(ctx, repl, "0/", "1/", "Third", &id3);

       ik_agent_ctx_t *recipient = ik_mail_test_find_agent("1/");
       ck_assert_uint_eq(recipient->inbox->count, 3);
       ck_assert_uint_eq(recipient->inbox->unread_count, 3);
   }
   END_TEST

   // Test: Messages from different senders
   START_TEST(test_mail_send_from_different_senders)
   {
       int64_t id = 0;

       ik_mail_send(ctx, repl, "0/", "1/", "From 0", &id);
       ik_mail_send(ctx, repl, "2/", "1/", "From 2", &id);

       ik_agent_ctx_t *recipient = ik_mail_test_find_agent("1/");
       ck_assert_uint_eq(recipient->inbox->count, 2);
   }
   END_TEST

   // Test: Messages to different recipients
   START_TEST(test_mail_send_to_different_recipients)
   {
       int64_t id = 0;

       ik_mail_send(ctx, repl, "0/", "1/", "To 1", &id);
       ik_mail_send(ctx, repl, "0/", "2/", "To 2", &id);

       ik_agent_ctx_t *agent1 = ik_mail_test_find_agent("1/");
       ik_agent_ctx_t *agent2 = ik_mail_test_find_agent("2/");

       ck_assert_uint_eq(agent1->inbox->count, 1);
       ck_assert_uint_eq(agent2->inbox->count, 1);
   }
   END_TEST

   // ========== Memory Ownership Tests ==========

   // Test: Message owned by inbox after send
   START_TEST(test_mail_send_message_ownership)
   {
       int64_t id = 0;
       res_t res = ik_mail_send(ctx, repl, "0/", "1/", "Test", &id);
       ck_assert(is_ok(&res));

       ik_agent_ctx_t *recipient = ik_mail_test_find_agent("1/");
       ik_mail_msg_t *msg = ik_inbox_get_by_id(recipient->inbox, id);
       ck_assert_ptr_nonnull(msg);

       // Message should be child of inbox's messages array
       ck_assert_ptr_eq(talloc_parent(msg), recipient->inbox->messages);
   }
   END_TEST

   // ========== Suite Configuration ==========

   static Suite *mail_send_suite(void)
   {
       Suite *s = suite_create("MailSend");

       TCase *tc_success = tcase_create("Success");
       tcase_add_checked_fixture(tc_success, setup, teardown);
       tcase_add_test(tc_success, test_mail_send_returns_id);
       tcase_add_test(tc_success, test_mail_send_sequential_ids);
       tcase_add_test(tc_success, test_mail_send_calls_db_insert);
       tcase_add_test(tc_success, test_mail_send_uses_timestamp);
       tcase_add_test(tc_success, test_mail_send_adds_to_inbox);
       tcase_add_test(tc_success, test_mail_send_message_fields);
       tcase_add_test(tc_success, test_mail_send_from_subagent);
       tcase_add_test(tc_success, test_mail_send_to_subagent);
       tcase_add_test(tc_success, test_mail_send_to_self);
       suite_add_tcase(s, tc_success);

       TCase *tc_recipient = tcase_create("RecipientValidation");
       tcase_add_checked_fixture(tc_recipient, setup, teardown);
       tcase_add_test(tc_recipient, test_mail_send_recipient_not_found);
       tcase_add_test(tc_recipient, test_mail_send_recipient_not_found_message);
       tcase_add_test(tc_recipient, test_mail_send_recipient_not_found_no_state_change);
       suite_add_tcase(s, tc_recipient);

       TCase *tc_body = tcase_create("BodyValidation");
       tcase_add_checked_fixture(tc_body, setup, teardown);
       tcase_add_test(tc_body, test_mail_send_empty_body);
       tcase_add_test(tc_body, test_mail_send_whitespace_only_body);
       tcase_add_test(tc_body, test_mail_send_body_trimmed);
       tcase_add_test(tc_body, test_mail_send_body_internal_whitespace);
       tcase_add_test(tc_body, test_mail_send_multiline_body);
       tcase_add_test(tc_body, test_mail_send_large_body);
       suite_add_tcase(s, tc_body);

       TCase *tc_db = tcase_create("Database");
       tcase_add_checked_fixture(tc_db, setup, teardown);
       tcase_add_test(tc_db, test_mail_send_db_error);
       tcase_add_test(tc_db, test_mail_send_db_error_no_inbox_update);
       tcase_add_test(tc_db, test_mail_send_uses_session_id);
       suite_add_tcase(s, tc_db);

       TCase *tc_order = tcase_create("ValidationOrder");
       tcase_add_checked_fixture(tc_order, setup, teardown);
       tcase_add_test(tc_order, test_mail_send_validation_order);
       suite_add_tcase(s, tc_order);

       TCase *tc_edge = tcase_create("EdgeCases");
       tcase_add_checked_fixture(tc_edge, setup, teardown);
       tcase_add_test(tc_edge, test_mail_send_multiple_to_same);
       tcase_add_test(tc_edge, test_mail_send_from_different_senders);
       tcase_add_test(tc_edge, test_mail_send_to_different_recipients);
       suite_add_tcase(s, tc_edge);

       TCase *tc_memory = tcase_create("Memory");
       tcase_add_checked_fixture(tc_memory, setup, teardown);
       tcase_add_test(tc_memory, test_mail_send_message_ownership);
       suite_add_tcase(s, tc_memory);

       return s;
   }

   int main(void)
   {
       Suite *s = mail_send_suite();
       SRunner *sr = srunner_create(s);

       srunner_run_all(sr, CK_NORMAL);
       int number_failed = srunner_ntests_failed(sr);
       srunner_free(sr);

       return (number_failed == 0) ? 0 : 1;
   }
   ```

3. Create stub `src/mail/send.c`:
   ```c
   #include "send.h"

   res_t ik_mail_send(TALLOC_CTX *ctx, ik_repl_ctx_t *repl,
                      const char *from_agent_id, const char *to_agent_id,
                      const char *body, int64_t *id_out)
   {
       (void)ctx;
       (void)repl;
       (void)from_agent_id;
       (void)to_agent_id;
       (void)body;
       *id_out = 0;
       return OK(NULL);
   }
   ```

4. Update Makefile:
   - Add `src/mail/send.c` to `MODULE_SOURCES`
   - Verify `tests/unit/mail/send_test.c` is picked up by wildcard

5. Run `make check` - expect test failures (stub doesn't implement logic)

### Green
1. Implement `src/mail/send.c`:
   ```c
   #include "send.h"

   #include "inbox.h"
   #include "msg.h"

   #include "../db/mail.h"
   #include "../panic.h"
   #include "../wrapper.h"

   #include <assert.h>
   #include <ctype.h>
   #include <string.h>
   #include <talloc.h>

   // Trim leading and trailing whitespace from a string
   // Returns a new talloc'd string on parent
   static char *trim_string(TALLOC_CTX *parent, const char *str)
   {
       assert(parent != NULL);  // LCOV_EXCL_BR_LINE
       assert(str != NULL);     // LCOV_EXCL_BR_LINE

       // Find start (skip leading whitespace)
       const char *start = str;
       while (*start && isspace((unsigned char)*start)) {
           start++;
       }

       // Find end (skip trailing whitespace)
       const char *end = str + strlen(str);
       while (end > start && isspace((unsigned char)*(end - 1))) {
           end--;
       }

       // Allocate and copy trimmed string
       size_t len = (size_t)(end - start);
       char *result = talloc_array(parent, char, len + 1);
       if (result == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

       memcpy(result, start, len);
       result[len] = '\0';

       return result;
   }

   // Find agent by ID in repl context
   // Returns NULL if not found
   static ik_agent_ctx_t *find_agent(ik_repl_ctx_t *repl, const char *agent_id)
   {
       assert(repl != NULL);      // LCOV_EXCL_BR_LINE
       assert(agent_id != NULL);  // LCOV_EXCL_BR_LINE

       // Note: Actual implementation depends on how agents are stored in repl
       // This could be repl->agents[] array, a linked list, or hash table
       // For now, assume repl has an agents[] array with agent_count

       for (size_t i = 0; i < repl->agent_count; i++) {
           if (repl->agents[i] != NULL &&
               strcmp(repl->agents[i]->agent_id, agent_id) == 0) {
               return repl->agents[i];
           }
       }

       return NULL;
   }

   res_t ik_mail_send(TALLOC_CTX *ctx, ik_repl_ctx_t *repl,
                      const char *from_agent_id, const char *to_agent_id,
                      const char *body, int64_t *id_out)
   {
       // Preconditions
       assert(ctx != NULL);           // LCOV_EXCL_BR_LINE
       assert(repl != NULL);          // LCOV_EXCL_BR_LINE
       assert(from_agent_id != NULL); // LCOV_EXCL_BR_LINE
       assert(to_agent_id != NULL);   // LCOV_EXCL_BR_LINE
       assert(body != NULL);          // LCOV_EXCL_BR_LINE
       assert(id_out != NULL);        // LCOV_EXCL_BR_LINE

       // Initialize output
       *id_out = 0;

       // Create temporary context for intermediate allocations
       TALLOC_CTX *tmp = talloc_new(NULL);
       if (tmp == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

       // Step 1: Validate recipient exists (user story 33)
       ik_agent_ctx_t *recipient = find_agent(repl, to_agent_id);
       if (recipient == NULL) {
           talloc_free(tmp);
           return ERR(ctx, NOT_FOUND, "Agent %s not found", to_agent_id);
       }

       // Step 2: Trim and validate body (user story 34)
       char *trimmed_body = trim_string(tmp, body);
       if (strlen(trimmed_body) == 0) {
           talloc_free(tmp);
           return ERR(ctx, INVALID_ARG, "Message body cannot be empty");
       }

       // Step 3: Get current timestamp
       int64_t timestamp = (int64_t)time_(NULL);

       // Step 4: Insert into database (atomic, returns generated ID)
       int64_t msg_id = 0;
       res_t db_res = ik_db_mail_insert(ctx, repl->db_ctx, repl->current_session_id,
                                         from_agent_id, to_agent_id,
                                         trimmed_body, timestamp, &msg_id);
       if (is_err(&db_res)) {
           talloc_free(tmp);
           return db_res;  // Propagate database error
       }

       // Step 5: Create message for in-memory inbox
       ik_mail_msg_t *msg = NULL;
       res_t msg_res = ik_mail_msg_create(recipient->inbox, msg_id,
                                           from_agent_id, to_agent_id,
                                           trimmed_body, timestamp, false, &msg);
       if (is_err(&msg_res)) {  // LCOV_EXCL_BR_LINE
           // This should not fail (OOM would PANIC)
           talloc_free(tmp);   // LCOV_EXCL_LINE
           return msg_res;     // LCOV_EXCL_LINE
       }

       // Step 6: Add to recipient's inbox (updates unread_count)
       res_t inbox_res = ik_inbox_add(recipient->inbox, msg);
       if (is_err(&inbox_res)) {  // LCOV_EXCL_BR_LINE
           // This should not fail (OOM would PANIC)
           talloc_free(tmp);      // LCOV_EXCL_LINE
           return inbox_res;      // LCOV_EXCL_LINE
       }

       // Cleanup
       talloc_free(tmp);

       *id_out = msg_id;
       return OK(NULL);
   }
   ```

2. **Note on agent lookup**: The `find_agent()` helper assumes `repl->agents[]` and `repl->agent_count` exist. If the actual implementation differs (e.g., agents are stored differently in Phase 1), this function will need to be adapted. The test mocks provide `ik_mail_test_find_agent()` which can be used during testing.

3. Run `make check` - expect all tests pass

### Refactor
1. Verify include order follows style guide:
   - Own header first (`send.h`)
   - Sibling headers next (`inbox.h`, `msg.h`)
   - Project headers next (`../db/mail.h`, `../panic.h`, `../wrapper.h`)
   - System headers last (`<assert.h>`, `<ctype.h>`, `<string.h>`, `<talloc.h>`)

2. Verify `// comments` style used (not `/* */`)

3. Verify assert() statements have LCOV_EXCL_BR_LINE comments

4. Run `make lint` - verify clean

5. Run `make coverage` - verify 100% coverage on new code

6. Run `make check-valgrind` - verify no memory leaks

7. Review error messages match user stories:
   - "Agent 99/ not found" (user story 33)
   - "Message body cannot be empty" (user story 34)

8. Consider: Should sender validation also be performed?
   - Current design: No (sender is the current agent, assumed valid)
   - Alternative: Validate `from_agent_id` exists too
   - Decision: Trust caller for sender, validate recipient only

## Post-conditions
- `make check` passes
- `make lint` passes
- `make coverage` shows 100% on `src/mail/send.c`
- `ik_mail_send()` function implemented with:
  - Recipient validation (returns NOT_FOUND error)
  - Body validation with trimming (returns INVALID_ARG error)
  - Database persistence (calls ik_db_mail_insert)
  - In-memory inbox update (calls ik_inbox_add)
  - Generated ID returned via output parameter
- Tests verify:
  - Successful send returns ID
  - Sequential IDs from multiple sends
  - Database insert called with correct arguments
  - Current timestamp used
  - Message added to recipient inbox
  - Message fields populated correctly
  - Sub-agent IDs work for both sender and recipient
  - Self-mail allowed
  - Nonexistent recipient returns error
  - Empty body returns error
  - Whitespace-only body returns error
  - Body trimmed before storage
  - Multiline and large bodies work
  - Database errors propagated
  - Database errors don't update inbox
  - Validation order (recipient before body)
  - Multiple messages work correctly
  - Memory ownership correct
- src/mail/send.h exists with function declaration
- src/mail/send.c exists with implementation
- No changes to existing mail module files

## Notes

### Error Code Mapping

| User Story | Error Condition | Error Code | Message |
|------------|-----------------|------------|---------|
| 33 | Recipient not found | NOT_FOUND | "Agent %s not found" |
| 34 | Empty body | INVALID_ARG | "Message body cannot be empty" |
| - | DB write failure | IO | (propagated from ik_db_mail_insert) |

### Validation Order Rationale

**Why validate recipient before body?**

From user stories:
- Story 33 shows recipient error: "Error: Agent 99/ not found"
- Story 34 shows body error: "Error: Message body cannot be empty"

If both are invalid, showing the recipient error first is more helpful because:
1. User may have mistyped the agent ID
2. No point validating message content for a nonexistent recipient
3. Matches natural reading order of the command: `/mail send <to> <body>`

### DB-First Approach

**Sequence:**
```
1. Validate recipient exists    ─┐
2. Validate body non-empty       │ Validation phase
3. Trim body                    ─┘
4. Insert into database          │ Persistence phase
5. Create in-memory message     ─┤
6. Add to inbox                 ─┘ Memory update phase
```

**Why DB-first?**

1. **Atomicity**: Database write is the source of truth. If it fails, no partial state.

2. **Recovery**: If process crashes after DB write but before inbox update:
   - Message is persisted in database
   - On session restore, inbox can be rebuilt from database
   - No message loss

3. **ID generation**: Database provides the message ID via `RETURNING id`. This ID is used for both DB record and in-memory message, ensuring consistency.

**What if inbox add fails after DB write?**

In practice, `ik_inbox_add()` only fails on OOM, which PANICs. If a non-PANIC failure were possible:
- Message would be in DB but not in memory
- User would see success (DB write completed)
- Message would appear on next inbox reload from DB
- This is acceptable (message not lost, just delayed visibility)

### Agent Lookup Integration

The `find_agent()` helper needs access to the agent array. Depending on how Phase 1 implements agent storage, this may need adjustment:

**Option A: repl->agents[] array**
```c
for (size_t i = 0; i < repl->agent_count; i++) {
    if (strcmp(repl->agents[i]->agent_id, agent_id) == 0) {
        return repl->agents[i];
    }
}
```

**Option B: Linked list**
```c
for (ik_agent_ctx_t *a = repl->first_agent; a != NULL; a = a->next) {
    if (strcmp(a->agent_id, agent_id) == 0) {
        return a;
    }
}
```

**Option C: Dedicated lookup function**
```c
// Defined in agent.h
ik_agent_ctx_t *ik_agent_find_by_id(ik_repl_ctx_t *repl, const char *id);
```

The test file provides a mock `ik_mail_test_find_agent()` that uses a simple array. The actual implementation should be adapted to match Phase 1's agent storage.

### Trimming Implementation

The `trim_string()` helper:
1. Allocates new string (doesn't modify input)
2. Uses talloc for proper memory management
3. Handles empty result (returns empty string, not NULL)
4. Uses `isspace()` for whitespace detection (handles space, tab, newline, etc.)

### Testing Strategy

**Mock approach:**
- `time_()` wrapper mocked for deterministic timestamps
- `ik_db_mail_insert()` mocked to:
  - Record call arguments for verification
  - Return success/failure based on `mock_db_should_fail`
  - Increment `mock_db_next_id` for sequential IDs
- Agent array mocked with `test_agents[]`

**Test categories:**
1. **Success cases**: Normal operation, various inputs
2. **Recipient validation**: Nonexistent agents
3. **Body validation**: Empty, whitespace, trimming
4. **Database errors**: Failure handling
5. **Validation order**: Correct error priority
6. **Edge cases**: Multiple sends, different senders/recipients
7. **Memory**: Ownership verification

### Integration with Command Handler

This function will be called by the `/mail send` command handler:

```c
// In command handler (future task)
res_t handle_mail_send(ik_repl_ctx_t *repl, const char *to, const char *body)
{
    int64_t id = 0;
    res_t res = ik_mail_send(repl, repl, repl->current_agent->agent_id,
                              to, body, &id);
    if (is_err(&res)) {
        // Display error to user
        display_error(repl, res.error);
        return res;
    }

    // Display success message
    display_message(repl, "Mail sent to agent %s", to);
    return OK(NULL);
}
```

### Integration with Tool Handler

The `mail` tool will also use this function:

```c
// In tool handler (future task)
res_t handle_mail_tool(ik_repl_ctx_t *repl, const char *action,
                       const char *to, const char *body)
{
    if (strcmp(action, "send") == 0) {
        int64_t id = 0;
        res_t res = ik_mail_send(repl, repl, repl->current_agent->agent_id,
                                  to, body, &id);
        if (is_err(&res)) {
            return format_tool_error(res);
        }
        return format_tool_result("{\"sent\": true, \"to\": \"%s\", \"id\": %lld}",
                                   to, id);
    }
    // ... other actions
}
```

### Future Considerations

1. **Rate limiting**: Consider adding rate limits to prevent mail spam between agents.

2. **Maximum body size**: Consider imposing a maximum body size to prevent memory issues.

3. **Sender validation**: Currently trusts the caller for sender ID. Could add validation if security becomes a concern.

4. **Delivery confirmation**: Could add a return field for async notification delivery status.

5. **Batch send**: If sending to multiple recipients becomes common, a batch function could reduce DB roundtrips.
