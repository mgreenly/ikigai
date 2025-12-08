# Task: Read Message Operation

## Target
Phase 3: Inter-Agent Mailboxes - Step 8 (High-level read operation)

Supports User Stories:
- 31 (read mail) - display full message, mark as read, clear notification_pending
- 38 (notification not repeated) - clears mail_notification_pending flag after reading

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/tdd.md
- .agents/skills/errors.md
- .agents/skills/testability.md

## Pre-read Docs
- docs/backlog/inter-agent-mailboxes.md (User Interface section, /mail read example, Notification Timing section)
- docs/memory.md (talloc ownership patterns)
- docs/error_handling.md (res_t patterns)
- docs/return_values.md (output parameter conventions)
- docs/rel-05/user-stories/31-read-mail.md (exact output format, walkthrough steps)
- docs/rel-05/user-stories/38-notification-not-repeated.md (notification_pending flag behavior)

## Pre-read Source (patterns)
- src/mail/msg.h (ik_mail_msg_t structure - timestamp field)
- src/mail/inbox.h (ik_inbox_t structure, ik_inbox_get_by_id(), ik_inbox_mark_read())
- src/mail/list.h (ik_mail_list() - sibling high-level operation pattern)
- src/mail/list.c (high-level operation implementation pattern)
- src/mail/send.h (ik_mail_send() - sibling high-level operation pattern)
- src/db/mail.h (ik_db_mail_mark_read() for database update)
- src/agent.h (ik_agent_ctx_t structure - inbox field, mail_notification_pending flag)
- src/wrapper.h (time_ wrapper for testable timestamps)

## Pre-read Tests (patterns)
- tests/unit/mail/list_test.c (mail operation tests, mock patterns, fixture setup)
- tests/unit/mail/send_test.c (mail operation tests, agent mock patterns)
- tests/unit/mail/inbox_test.c (inbox tests, message creation helpers)
- tests/test_utils.h (test helper functions)

## Pre-conditions
- `make check` passes
- `make lint` passes
- `ik_mail_msg_t` struct defined with fields: id, from_agent_id, to_agent_id, body, timestamp, read
- `ik_mail_msg_create()` factory function implemented
- `ik_inbox_t` struct with operations: `ik_inbox_get_by_id()`, `ik_inbox_mark_read()`
- `ik_inbox_get_by_id()` returns message by ID or NULL if not found
- `ik_inbox_mark_read()` marks message as read and decrements unread_count
- `ik_db_mail_mark_read()` implemented - updates database read flag
- `ik_mail_list()` implemented in src/mail/list.h and list.c
- `ik_agent_ctx_t` has `ik_inbox_t *inbox` field and `char *agent_id` field
- `ik_agent_ctx_t` has `bool mail_notification_pending` field

## Task
Create the `ik_mail_read()` function that looks up a message by ID, formats it for display with relative timestamps, marks it as read in both memory and database, and clears the notification_pending flag.

**Function signature:**
```c
// Read a specific message by ID
//
// Complete workflow:
// 1. Look up message in agent's inbox by ID
// 2. If not found, return NOT_FOUND error
// 3. Format output with sender, relative time, and full body
// 4. Mark message as read (in-memory via ik_inbox_mark_read)
// 5. Mark message as read (database via ik_db_mail_mark_read)
// 6. Clear agent's mail_notification_pending flag
// 7. Return formatted output string
//
// @param ctx         Talloc context for output allocation
// @param repl        REPL context (provides db_ctx for database update)
// @param agent       Agent whose inbox to read from (must not be NULL)
// @param message_id  ID of message to read (must be positive)
// @param output      Receives formatted output string (must not be NULL)
// @return            OK on success, ERR on failure
//
// Error conditions:
// - Message not found: ERR(ctx, NOT_FOUND, "Message #%lld not found", message_id)
// - Database error: propagates error from ik_db_mail_mark_read()
res_t ik_mail_read(TALLOC_CTX *ctx, ik_repl_ctx_t *repl, ik_agent_ctx_t *agent,
                   int64_t message_id, char **output);
```

**Output format (from user story 31):**
```
From: 1/
Time: 2 minutes ago

Found 3 OAuth patterns worth considering:

1) Silent refresh - Refresh token in background before expiry. Requires
   background timer. Transparent to user.
...
```

**Key design decisions:**

1. **Relative time formatting**: Convert Unix timestamp to human-readable relative time:
   - "just now" (< 1 minute)
   - "N minutes ago" (1-59 minutes)
   - "N hours ago" (1-23 hours)
   - "yesterday" (24-48 hours)
   - "N days ago" (> 48 hours)

2. **Order of operations**: Format output BEFORE marking as read, so failures during formatting don't leave message marked read incorrectly.

3. **Atomic-ish update**: Mark in-memory first, then database. If database fails, message is still marked read in memory (acceptable - user already saw it).

4. **notification_pending flag**: Always clear this flag when reading ANY message. This ensures the agent won't be re-notified about mail after they've read at least one message.

5. **Full body**: No truncation of message body (unlike list preview which truncates at 50 chars).

6. **Error message format**: "Message #5 not found" (with hash prefix, matches list format)

**From user story 31 walkthrough:**
```
1. User types `/mail read 5` and presses Enter
2. REPL parses slash command, identifies `mail` command with `read` subcommand
3. Command handler extracts message ID (5)
4. Handler looks up message in current agent's inbox by ID
5. If message not found, displays error: "Message #5 not found"
6. If found, handler displays full message:
   - `From: {sender_agent_id}`
   - `Time: {relative_time}` (e.g., "2 minutes ago", "yesterday")
   - Blank line
   - Full message body (no truncation)
7. Handler marks message as read (msg->read = true)
8. Handler decrements agent's unread_count
9. Handler updates database (set read = 1 for this message)
10. Separator re-renders with updated unread count
11. If this was triggering message for pending notification, clear mail_notification_pending flag
```

This function implements steps 4-9 and 11. Command parsing (steps 1-3) and separator re-rendering (step 10) are handled by the command layer.

## TDD Cycle

### Red
1. Create `src/mail/read.h`:
   ```c
   #ifndef IK_MAIL_READ_H
   #define IK_MAIL_READ_H

   #include "../agent.h"
   #include "../error.h"
   #include "../repl.h"

   #include <stdint.h>
   #include <talloc.h>

   /**
    * Read a specific message by ID.
    *
    * Coordinates the complete read workflow:
    * 1. Look up message in agent's inbox by ID
    * 2. Format output with sender, relative time, and full body
    * 3. Mark message as read (in-memory and database)
    * 4. Clear agent's mail_notification_pending flag
    *
    * Output format:
    *   From: {sender_agent_id}
    *   Time: {relative_time}
    *
    *   {full_message_body}
    *
    * Relative time format:
    * - "just now" (< 1 minute ago)
    * - "N minutes ago" (1-59 minutes)
    * - "N hours ago" (1-23 hours)
    * - "yesterday" (24-48 hours)
    * - "N days ago" (> 48 hours)
    *
    * @param ctx         Talloc context for output allocation
    * @param repl        REPL context (provides db_ctx)
    * @param agent       Agent whose inbox to read from (must not be NULL)
    * @param message_id  ID of message to read (must be positive)
    * @param output      Receives formatted output string (must not be NULL)
    * @return            OK on success, ERR on failure
    *
    * Errors:
    * - NOT_FOUND: "Message #N not found" (message not in inbox)
    * - IO: Database update failure (propagated from ik_db_mail_mark_read)
    *
    * Side effects:
    * - Sets msg->read = true
    * - Decrements agent->inbox->unread_count
    * - Updates database read flag
    * - Clears agent->mail_notification_pending
    */
   res_t ik_mail_read(TALLOC_CTX *ctx, ik_repl_ctx_t *repl, ik_agent_ctx_t *agent,
                      int64_t message_id, char **output);

   /**
    * Format a Unix timestamp as relative time.
    *
    * Internal helper exposed for testing. Converts a Unix timestamp
    * to a human-readable relative time string like "2 minutes ago".
    *
    * @param ctx        Talloc context for string allocation
    * @param timestamp  Unix timestamp (seconds since epoch)
    * @param now        Current time (for testability, pass time_(NULL))
    * @return           Formatted relative time string (never NULL)
    *
    * Format rules:
    * - now - timestamp < 60:       "just now"
    * - now - timestamp < 3600:     "N minutes ago" (N = 1-59)
    * - now - timestamp < 86400:    "N hours ago" (N = 1-23)
    * - now - timestamp < 172800:   "yesterday"
    * - now - timestamp >= 172800:  "N days ago" (N = 2+)
    *
    * Edge cases:
    * - timestamp > now (future): treated as "just now"
    * - timestamp == now: "just now"
    */
   char *ik_mail_format_relative_time(TALLOC_CTX *ctx, int64_t timestamp, int64_t now);

   #endif // IK_MAIL_READ_H
   ```

2. Create `tests/unit/mail/read_test.c`:
   ```c
   /**
    * @file read_test.c
    * @brief Tests for mail read operation
    *
    * Tests the ik_mail_read() function which:
    * - Looks up message by ID
    * - Formats output with relative time
    * - Marks message as read (memory + DB)
    * - Clears notification_pending flag
    *
    * Also tests the relative time formatting helper.
    */

   #include "../../../src/mail/read.h"
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
   static int64_t mock_time_value = 1700000100;
   static bool mock_db_should_fail = false;
   static int mock_db_mark_read_call_count = 0;
   static int64_t mock_db_last_marked_id = 0;

   // Mock time wrapper
   time_t time_(time_t *tloc)
   {
       if (tloc != NULL) {
           *tloc = (time_t)mock_time_value;
       }
       return (time_t)mock_time_value;
   }

   // Mock database mark read
   res_t ik_db_mail_mark_read(ik_db_ctx_t *db, int64_t id)
   {
       (void)db;

       mock_db_mark_read_call_count++;
       mock_db_last_marked_id = id;

       if (mock_db_should_fail) {
           return ERR(NULL, IO, "Mock database error");
       }

       return OK(NULL);
   }

   // ========== Test Fixture ==========

   static TALLOC_CTX *ctx;
   static ik_repl_ctx_t *repl;
   static ik_agent_ctx_t *agent;

   static void reset_mocks(void)
   {
       mock_time_value = 1700000100;
       mock_db_should_fail = false;
       mock_db_mark_read_call_count = 0;
       mock_db_last_marked_id = 0;
   }

   // Helper to create a test message and add to inbox
   static ik_mail_msg_t *add_msg(int64_t id, const char *from, const char *body,
                                  int64_t ts, bool read)
   {
       ik_mail_msg_t *msg = NULL;
       res_t res = ik_mail_msg_create(ctx, id, from, agent->agent_id,
                                       body, ts, read, &msg);
       ck_assert(is_ok(&res));
       res = ik_inbox_add(agent->inbox, msg);
       ck_assert(is_ok(&res));
       return msg;
   }

   static void setup(void)
   {
       ctx = talloc_new(NULL);
       ck_assert_ptr_nonnull(ctx);

       reset_mocks();

       // Create mock REPL context
       repl = talloc_zero(ctx, ik_repl_ctx_t);
       ck_assert_ptr_nonnull(repl);
       repl->db_ctx = (ik_db_ctx_t *)0xDEADBEEF;  // Mock pointer

       // Create mock agent with inbox
       agent = talloc_zero(ctx, ik_agent_ctx_t);
       ck_assert_ptr_nonnull(agent);

       agent->agent_id = talloc_strdup(agent, "0/");
       ck_assert_ptr_nonnull(agent->agent_id);

       agent->inbox = ik_inbox_create(agent);
       ck_assert_ptr_nonnull(agent->inbox);

       agent->mail_notification_pending = false;
   }

   static void teardown(void)
   {
       reset_mocks();
       talloc_free(ctx);
       ctx = NULL;
       repl = NULL;
       agent = NULL;
   }

   // ========== Relative Time Formatting Tests ==========

   // Test: Just now (0 seconds ago)
   START_TEST(test_relative_time_just_now_zero)
   {
       int64_t now = 1700000100;
       char *result = ik_mail_format_relative_time(ctx, now, now);

       ck_assert_str_eq(result, "just now");
   }
   END_TEST

   // Test: Just now (59 seconds ago)
   START_TEST(test_relative_time_just_now_59s)
   {
       int64_t now = 1700000100;
       char *result = ik_mail_format_relative_time(ctx, now - 59, now);

       ck_assert_str_eq(result, "just now");
   }
   END_TEST

   // Test: 1 minute ago
   START_TEST(test_relative_time_1_minute)
   {
       int64_t now = 1700000100;
       char *result = ik_mail_format_relative_time(ctx, now - 60, now);

       ck_assert_str_eq(result, "1 minute ago");
   }
   END_TEST

   // Test: N minutes ago (singular vs plural)
   START_TEST(test_relative_time_minutes_plural)
   {
       int64_t now = 1700000100;

       char *r1 = ik_mail_format_relative_time(ctx, now - 60, now);
       ck_assert_str_eq(r1, "1 minute ago");

       char *r2 = ik_mail_format_relative_time(ctx, now - 120, now);
       ck_assert_str_eq(r2, "2 minutes ago");

       char *r59 = ik_mail_format_relative_time(ctx, now - 59 * 60, now);
       ck_assert_str_eq(r59, "59 minutes ago");
   }
   END_TEST

   // Test: 1 hour ago
   START_TEST(test_relative_time_1_hour)
   {
       int64_t now = 1700000100;
       char *result = ik_mail_format_relative_time(ctx, now - 3600, now);

       ck_assert_str_eq(result, "1 hour ago");
   }
   END_TEST

   // Test: N hours ago (singular vs plural)
   START_TEST(test_relative_time_hours_plural)
   {
       int64_t now = 1700000100;

       char *r1 = ik_mail_format_relative_time(ctx, now - 3600, now);
       ck_assert_str_eq(r1, "1 hour ago");

       char *r2 = ik_mail_format_relative_time(ctx, now - 2 * 3600, now);
       ck_assert_str_eq(r2, "2 hours ago");

       char *r23 = ik_mail_format_relative_time(ctx, now - 23 * 3600, now);
       ck_assert_str_eq(r23, "23 hours ago");
   }
   END_TEST

   // Test: Yesterday (24 hours ago)
   START_TEST(test_relative_time_yesterday_24h)
   {
       int64_t now = 1700000100;
       char *result = ik_mail_format_relative_time(ctx, now - 24 * 3600, now);

       ck_assert_str_eq(result, "yesterday");
   }
   END_TEST

   // Test: Yesterday (47 hours ago)
   START_TEST(test_relative_time_yesterday_47h)
   {
       int64_t now = 1700000100;
       char *result = ik_mail_format_relative_time(ctx, now - 47 * 3600, now);

       ck_assert_str_eq(result, "yesterday");
   }
   END_TEST

   // Test: 2 days ago (48 hours)
   START_TEST(test_relative_time_2_days)
   {
       int64_t now = 1700000100;
       char *result = ik_mail_format_relative_time(ctx, now - 48 * 3600, now);

       ck_assert_str_eq(result, "2 days ago");
   }
   END_TEST

   // Test: N days ago
   START_TEST(test_relative_time_days_plural)
   {
       int64_t now = 1700000100;

       char *r2 = ik_mail_format_relative_time(ctx, now - 48 * 3600, now);
       ck_assert_str_eq(r2, "2 days ago");

       char *r7 = ik_mail_format_relative_time(ctx, now - 7 * 24 * 3600, now);
       ck_assert_str_eq(r7, "7 days ago");

       char *r30 = ik_mail_format_relative_time(ctx, now - 30 * 24 * 3600, now);
       ck_assert_str_eq(r30, "30 days ago");
   }
   END_TEST

   // Test: Future timestamp treated as just now
   START_TEST(test_relative_time_future)
   {
       int64_t now = 1700000100;
       char *result = ik_mail_format_relative_time(ctx, now + 3600, now);

       ck_assert_str_eq(result, "just now");
   }
   END_TEST

   // Test: Very old timestamp (over a year)
   START_TEST(test_relative_time_very_old)
   {
       int64_t now = 1700000100;
       char *result = ik_mail_format_relative_time(ctx, now - 400 * 24 * 3600, now);

       ck_assert_str_eq(result, "400 days ago");
   }
   END_TEST

   // ========== Read Success Tests ==========

   // Test: Read existing unread message returns formatted output
   START_TEST(test_read_success_basic)
   {
       add_msg(5, "1/", "Hello world", 1700000000, false);

       char *output = NULL;
       res_t res = ik_mail_read(ctx, repl, agent, 5, &output);

       ck_assert(is_ok(&res));
       ck_assert_ptr_nonnull(output);
       ck_assert(strstr(output, "From: 1/") != NULL);
       ck_assert(strstr(output, "Time:") != NULL);
       ck_assert(strstr(output, "Hello world") != NULL);
   }
   END_TEST

   // Test: Output format matches user story 31
   START_TEST(test_read_output_format)
   {
       // Timestamp 100 seconds ago
       add_msg(5, "1/", "Message body here", 1700000000, false);
       mock_time_value = 1700000100;

       char *output = NULL;
       res_t res = ik_mail_read(ctx, repl, agent, 5, &output);

       ck_assert(is_ok(&res));

       // Check format: From line, Time line, blank line, body
       const char *expected_start = "From: 1/\nTime: ";
       ck_assert(strncmp(output, expected_start, strlen(expected_start)) == 0);

       // Should have blank line before body
       ck_assert(strstr(output, "\n\nMessage body here") != NULL);
   }
   END_TEST

   // Test: Full body included without truncation
   START_TEST(test_read_full_body_no_truncation)
   {
       // Create long body (over 50 chars which is list truncation limit)
       const char *long_body =
           "This is a very long message body that exceeds the fifty character "
           "preview limit used in the list view. It should be shown in full "
           "without any truncation when reading the message.";

       add_msg(5, "1/", long_body, 1700000000, false);

       char *output = NULL;
       res_t res = ik_mail_read(ctx, repl, agent, 5, &output);

       ck_assert(is_ok(&res));
       ck_assert(strstr(output, long_body) != NULL);
   }
   END_TEST

   // Test: Multiline body preserved
   START_TEST(test_read_multiline_body)
   {
       const char *body = "Line 1\nLine 2\nLine 3";
       add_msg(5, "1/", body, 1700000000, false);

       char *output = NULL;
       res_t res = ik_mail_read(ctx, repl, agent, 5, &output);

       ck_assert(is_ok(&res));
       ck_assert(strstr(output, body) != NULL);
   }
   END_TEST

   // Test: Sub-agent sender displayed correctly
   START_TEST(test_read_subagent_sender)
   {
       add_msg(5, "0/0", "From sub-agent", 1700000000, false);

       char *output = NULL;
       res_t res = ik_mail_read(ctx, repl, agent, 5, &output);

       ck_assert(is_ok(&res));
       ck_assert(strstr(output, "From: 0/0") != NULL);
   }
   END_TEST

   // Test: Relative time included in output
   START_TEST(test_read_relative_time_included)
   {
       // Timestamp 2 minutes ago
       mock_time_value = 1700000120;
       add_msg(5, "1/", "Test", 1700000000, false);

       char *output = NULL;
       res_t res = ik_mail_read(ctx, repl, agent, 5, &output);

       ck_assert(is_ok(&res));
       ck_assert(strstr(output, "Time: 2 minutes ago") != NULL);
   }
   END_TEST

   // ========== Mark Read Tests ==========

   // Test: Reading marks message as read in memory
   START_TEST(test_read_marks_read_memory)
   {
       ik_mail_msg_t *msg = add_msg(5, "1/", "Test", 1700000000, false);
       ck_assert(!msg->read);
       ck_assert_uint_eq(agent->inbox->unread_count, 1);

       char *output = NULL;
       res_t res = ik_mail_read(ctx, repl, agent, 5, &output);

       ck_assert(is_ok(&res));
       ck_assert(msg->read);
       ck_assert_uint_eq(agent->inbox->unread_count, 0);
   }
   END_TEST

   // Test: Reading marks message as read in database
   START_TEST(test_read_marks_read_database)
   {
       add_msg(5, "1/", "Test", 1700000000, false);

       char *output = NULL;
       res_t res = ik_mail_read(ctx, repl, agent, 5, &output);

       ck_assert(is_ok(&res));
       ck_assert_int_eq(mock_db_mark_read_call_count, 1);
       ck_assert_int_eq(mock_db_last_marked_id, 5);
   }
   END_TEST

   // Test: Reading already-read message doesn't double-decrement
   START_TEST(test_read_already_read_no_double_decrement)
   {
       ik_mail_msg_t *msg = add_msg(5, "1/", "Test", 1700000000, true);  // Already read
       ck_assert(msg->read);
       ck_assert_uint_eq(agent->inbox->unread_count, 0);

       char *output = NULL;
       res_t res = ik_mail_read(ctx, repl, agent, 5, &output);

       ck_assert(is_ok(&res));
       ck_assert(msg->read);
       ck_assert_uint_eq(agent->inbox->unread_count, 0);  // Still 0, not -1
   }
   END_TEST

   // Test: Database still updated for already-read message (idempotent)
   START_TEST(test_read_already_read_db_still_called)
   {
       add_msg(5, "1/", "Test", 1700000000, true);  // Already read

       char *output = NULL;
       res_t res = ik_mail_read(ctx, repl, agent, 5, &output);

       ck_assert(is_ok(&res));
       // DB update still called for consistency
       ck_assert_int_eq(mock_db_mark_read_call_count, 1);
   }
   END_TEST

   // ========== Notification Flag Tests ==========

   // Test: Reading clears mail_notification_pending flag
   START_TEST(test_read_clears_notification_pending)
   {
       agent->mail_notification_pending = true;
       add_msg(5, "1/", "Test", 1700000000, false);

       char *output = NULL;
       res_t res = ik_mail_read(ctx, repl, agent, 5, &output);

       ck_assert(is_ok(&res));
       ck_assert(!agent->mail_notification_pending);
   }
   END_TEST

   // Test: Reading clears flag even for already-read message
   START_TEST(test_read_clears_notification_pending_even_if_read)
   {
       agent->mail_notification_pending = true;
       add_msg(5, "1/", "Test", 1700000000, true);  // Already read

       char *output = NULL;
       res_t res = ik_mail_read(ctx, repl, agent, 5, &output);

       ck_assert(is_ok(&res));
       ck_assert(!agent->mail_notification_pending);
   }
   END_TEST

   // Test: Flag already false stays false
   START_TEST(test_read_flag_already_false)
   {
       agent->mail_notification_pending = false;
       add_msg(5, "1/", "Test", 1700000000, false);

       char *output = NULL;
       res_t res = ik_mail_read(ctx, repl, agent, 5, &output);

       ck_assert(is_ok(&res));
       ck_assert(!agent->mail_notification_pending);
   }
   END_TEST

   // ========== Not Found Tests ==========

   // Test: Non-existent message ID returns error
   START_TEST(test_read_not_found)
   {
       add_msg(5, "1/", "Test", 1700000000, false);

       char *output = (char *)0xDEADBEEF;  // Sentinel
       res_t res = ik_mail_read(ctx, repl, agent, 99, &output);

       ck_assert(is_err(&res));
       ck_assert_ptr_null(output);
   }
   END_TEST

   // Test: Error message format for not found
   START_TEST(test_read_not_found_error_message)
   {
       char *output = NULL;
       res_t res = ik_mail_read(ctx, repl, agent, 42, &output);

       ck_assert(is_err(&res));
       // Error should mention the message ID
       // Note: Actual error message format depends on error.h implementation
   }
   END_TEST

   // Test: Empty inbox returns not found
   START_TEST(test_read_empty_inbox)
   {
       char *output = NULL;
       res_t res = ik_mail_read(ctx, repl, agent, 1, &output);

       ck_assert(is_err(&res));
       ck_assert_ptr_null(output);
   }
   END_TEST

   // Test: Not found doesn't mark any message read
   START_TEST(test_read_not_found_no_side_effects)
   {
       ik_mail_msg_t *msg = add_msg(5, "1/", "Test", 1700000000, false);
       agent->mail_notification_pending = true;

       char *output = NULL;
       res_t res = ik_mail_read(ctx, repl, agent, 99, &output);

       ck_assert(is_err(&res));
       // No side effects
       ck_assert(!msg->read);
       ck_assert_uint_eq(agent->inbox->unread_count, 1);
       ck_assert(agent->mail_notification_pending);  // Not cleared
       ck_assert_int_eq(mock_db_mark_read_call_count, 0);
   }
   END_TEST

   // ========== Database Error Tests ==========

   // Test: Database error returns error
   START_TEST(test_read_db_error)
   {
       mock_db_should_fail = true;
       add_msg(5, "1/", "Test", 1700000000, false);

       char *output = NULL;
       res_t res = ik_mail_read(ctx, repl, agent, 5, &output);

       ck_assert(is_err(&res));
   }
   END_TEST

   // Test: Database error still marks in-memory (message was displayed)
   START_TEST(test_read_db_error_memory_still_marked)
   {
       mock_db_should_fail = true;
       ik_mail_msg_t *msg = add_msg(5, "1/", "Test", 1700000000, false);

       char *output = NULL;
       res_t res = ik_mail_read(ctx, repl, agent, 5, &output);

       ck_assert(is_err(&res));
       // Message was already displayed, so memory state updated
       ck_assert(msg->read);
       ck_assert_uint_eq(agent->inbox->unread_count, 0);
   }
   END_TEST

   // ========== Multiple Messages Tests ==========

   // Test: Reading one message doesn't affect others
   START_TEST(test_read_one_of_multiple)
   {
       ik_mail_msg_t *msg1 = add_msg(1, "1/", "First", 1700000001, false);
       ik_mail_msg_t *msg2 = add_msg(2, "2/", "Second", 1700000002, false);
       ik_mail_msg_t *msg3 = add_msg(3, "1/", "Third", 1700000003, false);

       ck_assert_uint_eq(agent->inbox->unread_count, 3);

       char *output = NULL;
       res_t res = ik_mail_read(ctx, repl, agent, 2, &output);

       ck_assert(is_ok(&res));
       ck_assert(!msg1->read);
       ck_assert(msg2->read);
       ck_assert(!msg3->read);
       ck_assert_uint_eq(agent->inbox->unread_count, 2);
   }
   END_TEST

   // Test: Can read multiple messages sequentially
   START_TEST(test_read_multiple_sequential)
   {
       add_msg(1, "1/", "First", 1700000001, false);
       add_msg(2, "2/", "Second", 1700000002, false);

       char *output1 = NULL;
       res_t res1 = ik_mail_read(ctx, repl, agent, 1, &output1);
       ck_assert(is_ok(&res1));
       ck_assert_uint_eq(agent->inbox->unread_count, 1);

       char *output2 = NULL;
       res_t res2 = ik_mail_read(ctx, repl, agent, 2, &output2);
       ck_assert(is_ok(&res2));
       ck_assert_uint_eq(agent->inbox->unread_count, 0);

       // DB called twice
       ck_assert_int_eq(mock_db_mark_read_call_count, 2);
   }
   END_TEST

   // ========== Memory Ownership Tests ==========

   // Test: Output string owned by provided context
   START_TEST(test_read_output_memory_ownership)
   {
       add_msg(5, "1/", "Test", 1700000000, false);

       TALLOC_CTX *child = talloc_new(ctx);
       char *output = NULL;
       res_t res = ik_mail_read(child, repl, agent, 5, &output);

       ck_assert(is_ok(&res));
       ck_assert_ptr_eq(talloc_parent(output), child);

       // Free child should free output (no crash)
       talloc_free(child);
   }
   END_TEST

   // Test: No memory leak on multiple reads
   START_TEST(test_read_no_memory_leak)
   {
       add_msg(5, "1/", "Test message body", 1700000000, false);

       for (int i = 0; i < 100; i++) {
           TALLOC_CTX *tmp = talloc_new(ctx);
           char *output = NULL;
           res_t res = ik_mail_read(tmp, repl, agent, 5, &output);
           ck_assert(is_ok(&res));
           ck_assert_ptr_nonnull(output);
           talloc_free(tmp);
       }
       // If we get here without crash or OOM, test passes
   }
   END_TEST

   // ========== Edge Cases ==========

   // Test: Empty body displays correctly
   START_TEST(test_read_empty_body)
   {
       add_msg(5, "1/", "", 1700000000, false);

       char *output = NULL;
       res_t res = ik_mail_read(ctx, repl, agent, 5, &output);

       ck_assert(is_ok(&res));
       ck_assert(strstr(output, "From: 1/") != NULL);
       ck_assert(strstr(output, "Time:") != NULL);
   }
   END_TEST

   // Test: Body with special characters
   START_TEST(test_read_special_chars)
   {
       const char *body = "Message with \"quotes\" and <brackets> and 'apostrophes'";
       add_msg(5, "1/", body, 1700000000, false);

       char *output = NULL;
       res_t res = ik_mail_read(ctx, repl, agent, 5, &output);

       ck_assert(is_ok(&res));
       ck_assert(strstr(output, body) != NULL);
   }
   END_TEST

   // Test: Very long sender ID
   START_TEST(test_read_long_sender_id)
   {
       add_msg(5, "0/0/0/0/0", "Deep sub-agent", 1700000000, false);

       char *output = NULL;
       res_t res = ik_mail_read(ctx, repl, agent, 5, &output);

       ck_assert(is_ok(&res));
       ck_assert(strstr(output, "From: 0/0/0/0/0") != NULL);
   }
   END_TEST

   // Test: Large message ID
   START_TEST(test_read_large_message_id)
   {
       int64_t large_id = 9999999999LL;
       add_msg(large_id, "1/", "Test", 1700000000, false);

       char *output = NULL;
       res_t res = ik_mail_read(ctx, repl, agent, large_id, &output);

       ck_assert(is_ok(&res));
       ck_assert_ptr_nonnull(output);
   }
   END_TEST

   // ========== Suite Configuration ==========

   static Suite *mail_read_suite(void)
   {
       Suite *s = suite_create("MailRead");

       TCase *tc_relative = tcase_create("RelativeTime");
       tcase_add_checked_fixture(tc_relative, setup, teardown);
       tcase_add_test(tc_relative, test_relative_time_just_now_zero);
       tcase_add_test(tc_relative, test_relative_time_just_now_59s);
       tcase_add_test(tc_relative, test_relative_time_1_minute);
       tcase_add_test(tc_relative, test_relative_time_minutes_plural);
       tcase_add_test(tc_relative, test_relative_time_1_hour);
       tcase_add_test(tc_relative, test_relative_time_hours_plural);
       tcase_add_test(tc_relative, test_relative_time_yesterday_24h);
       tcase_add_test(tc_relative, test_relative_time_yesterday_47h);
       tcase_add_test(tc_relative, test_relative_time_2_days);
       tcase_add_test(tc_relative, test_relative_time_days_plural);
       tcase_add_test(tc_relative, test_relative_time_future);
       tcase_add_test(tc_relative, test_relative_time_very_old);
       suite_add_tcase(s, tc_relative);

       TCase *tc_success = tcase_create("Success");
       tcase_add_checked_fixture(tc_success, setup, teardown);
       tcase_add_test(tc_success, test_read_success_basic);
       tcase_add_test(tc_success, test_read_output_format);
       tcase_add_test(tc_success, test_read_full_body_no_truncation);
       tcase_add_test(tc_success, test_read_multiline_body);
       tcase_add_test(tc_success, test_read_subagent_sender);
       tcase_add_test(tc_success, test_read_relative_time_included);
       suite_add_tcase(s, tc_success);

       TCase *tc_mark_read = tcase_create("MarkRead");
       tcase_add_checked_fixture(tc_mark_read, setup, teardown);
       tcase_add_test(tc_mark_read, test_read_marks_read_memory);
       tcase_add_test(tc_mark_read, test_read_marks_read_database);
       tcase_add_test(tc_mark_read, test_read_already_read_no_double_decrement);
       tcase_add_test(tc_mark_read, test_read_already_read_db_still_called);
       suite_add_tcase(s, tc_mark_read);

       TCase *tc_notification = tcase_create("NotificationFlag");
       tcase_add_checked_fixture(tc_notification, setup, teardown);
       tcase_add_test(tc_notification, test_read_clears_notification_pending);
       tcase_add_test(tc_notification, test_read_clears_notification_pending_even_if_read);
       tcase_add_test(tc_notification, test_read_flag_already_false);
       suite_add_tcase(s, tc_notification);

       TCase *tc_not_found = tcase_create("NotFound");
       tcase_add_checked_fixture(tc_not_found, setup, teardown);
       tcase_add_test(tc_not_found, test_read_not_found);
       tcase_add_test(tc_not_found, test_read_not_found_error_message);
       tcase_add_test(tc_not_found, test_read_empty_inbox);
       tcase_add_test(tc_not_found, test_read_not_found_no_side_effects);
       suite_add_tcase(s, tc_not_found);

       TCase *tc_db_error = tcase_create("DatabaseError");
       tcase_add_checked_fixture(tc_db_error, setup, teardown);
       tcase_add_test(tc_db_error, test_read_db_error);
       tcase_add_test(tc_db_error, test_read_db_error_memory_still_marked);
       suite_add_tcase(s, tc_db_error);

       TCase *tc_multiple = tcase_create("Multiple");
       tcase_add_checked_fixture(tc_multiple, setup, teardown);
       tcase_add_test(tc_multiple, test_read_one_of_multiple);
       tcase_add_test(tc_multiple, test_read_multiple_sequential);
       suite_add_tcase(s, tc_multiple);

       TCase *tc_memory = tcase_create("Memory");
       tcase_add_checked_fixture(tc_memory, setup, teardown);
       tcase_add_test(tc_memory, test_read_output_memory_ownership);
       tcase_add_test(tc_memory, test_read_no_memory_leak);
       suite_add_tcase(s, tc_memory);

       TCase *tc_edge = tcase_create("EdgeCases");
       tcase_add_checked_fixture(tc_edge, setup, teardown);
       tcase_add_test(tc_edge, test_read_empty_body);
       tcase_add_test(tc_edge, test_read_special_chars);
       tcase_add_test(tc_edge, test_read_long_sender_id);
       tcase_add_test(tc_edge, test_read_large_message_id);
       suite_add_tcase(s, tc_edge);

       return s;
   }

   int main(void)
   {
       Suite *s = mail_read_suite();
       SRunner *sr = srunner_create(s);

       srunner_run_all(sr, CK_NORMAL);
       int number_failed = srunner_ntests_failed(sr);
       srunner_free(sr);

       return (number_failed == 0) ? 0 : 1;
   }
   ```

3. Create stub `src/mail/read.c`:
   ```c
   #include "read.h"

   char *ik_mail_format_relative_time(TALLOC_CTX *ctx, int64_t timestamp, int64_t now)
   {
       (void)timestamp;
       (void)now;
       return talloc_strdup(ctx, "");
   }

   res_t ik_mail_read(TALLOC_CTX *ctx, ik_repl_ctx_t *repl, ik_agent_ctx_t *agent,
                      int64_t message_id, char **output)
   {
       (void)ctx;
       (void)repl;
       (void)agent;
       (void)message_id;
       *output = NULL;
       return OK(NULL);
   }
   ```

4. Update Makefile:
   - Add `src/mail/read.c` to `MODULE_SOURCES`
   - Verify `tests/unit/mail/read_test.c` is picked up by wildcard

5. Run `make check` - expect test failures (stubs return empty/incorrect values)

### Green
1. Implement `src/mail/read.c`:
   ```c
   #include "read.h"

   #include "inbox.h"
   #include "msg.h"

   #include "../db/mail.h"
   #include "../panic.h"
   #include "../wrapper.h"

   #include <assert.h>
   #include <inttypes.h>
   #include <talloc.h>

   // Time constants
   #define SECONDS_PER_MINUTE 60
   #define SECONDS_PER_HOUR   3600
   #define SECONDS_PER_DAY    86400

   char *ik_mail_format_relative_time(TALLOC_CTX *ctx, int64_t timestamp, int64_t now)
   {
       assert(ctx != NULL);  // LCOV_EXCL_BR_LINE

       // Handle future timestamps or same time
       if (timestamp >= now) {
           return talloc_strdup(ctx, "just now");
       }

       int64_t diff = now - timestamp;

       // Just now (< 1 minute)
       if (diff < SECONDS_PER_MINUTE) {
           return talloc_strdup(ctx, "just now");
       }

       // Minutes ago (< 1 hour)
       if (diff < SECONDS_PER_HOUR) {
           int64_t minutes = diff / SECONDS_PER_MINUTE;
           if (minutes == 1) {
               return talloc_strdup(ctx, "1 minute ago");
           }
           char *result = talloc_asprintf(ctx, "%" PRId64 " minutes ago", minutes);
           if (result == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
           return result;
       }

       // Hours ago (< 24 hours)
       if (diff < SECONDS_PER_DAY) {
           int64_t hours = diff / SECONDS_PER_HOUR;
           if (hours == 1) {
               return talloc_strdup(ctx, "1 hour ago");
           }
           char *result = talloc_asprintf(ctx, "%" PRId64 " hours ago", hours);
           if (result == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
           return result;
       }

       // Yesterday (24-48 hours)
       if (diff < 2 * SECONDS_PER_DAY) {
           return talloc_strdup(ctx, "yesterday");
       }

       // Days ago (>= 48 hours)
       int64_t days = diff / SECONDS_PER_DAY;
       char *result = talloc_asprintf(ctx, "%" PRId64 " days ago", days);
       if (result == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
       return result;
   }

   res_t ik_mail_read(TALLOC_CTX *ctx, ik_repl_ctx_t *repl, ik_agent_ctx_t *agent,
                      int64_t message_id, char **output)
   {
       // Preconditions
       assert(ctx != NULL);           // LCOV_EXCL_BR_LINE
       assert(repl != NULL);          // LCOV_EXCL_BR_LINE
       assert(agent != NULL);         // LCOV_EXCL_BR_LINE
       assert(agent->inbox != NULL);  // LCOV_EXCL_BR_LINE
       assert(message_id > 0);        // LCOV_EXCL_BR_LINE
       assert(output != NULL);        // LCOV_EXCL_BR_LINE

       // Initialize output
       *output = NULL;

       // Step 1: Look up message by ID
       ik_mail_msg_t *msg = ik_inbox_get_by_id(agent->inbox, message_id);
       if (msg == NULL) {
           return ERR(ctx, NOT_FOUND, "Message #%" PRId64 " not found", message_id);
       }

       // Step 2: Format output (before marking read, so failure doesn't leave inconsistent state)
       int64_t now = (int64_t)time_(NULL);
       char *relative_time = ik_mail_format_relative_time(ctx, msg->timestamp, now);

       char *formatted = talloc_asprintf(ctx, "From: %s\nTime: %s\n\n%s",
                                          msg->from_agent_id, relative_time, msg->body);
       if (formatted == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

       // Step 3: Mark message as read in memory
       // Note: ik_inbox_mark_read handles the case where message is already read
       res_t mark_res = ik_inbox_mark_read(agent->inbox, msg);
       if (is_err(&mark_res)) {  // LCOV_EXCL_BR_LINE
           // This should not fail since we just looked up the message
           PANIC("ik_inbox_mark_read failed for message we just found");  // LCOV_EXCL_LINE
       }

       // Step 4: Mark message as read in database
       res_t db_res = ik_db_mail_mark_read(repl->db_ctx, message_id);
       if (is_err(&db_res)) {
           // Memory already marked, but report DB error to caller
           // User already saw the message content, so this is acceptable
           return db_res;
       }

       // Step 5: Clear notification_pending flag
       agent->mail_notification_pending = false;

       *output = formatted;
       return OK(NULL);
   }
   ```

2. Run `make check` - expect all tests pass

### Refactor
1. Verify include order follows style guide:
   - Own header first (`read.h`)
   - Sibling headers next (`inbox.h`, `msg.h`)
   - Project headers next (`../db/mail.h`, `../panic.h`, `../wrapper.h`)
   - System headers last (`<assert.h>`, `<inttypes.h>`, `<talloc.h>`)

2. Verify `// comments` style used (not `/* */`)

3. Verify assert() statements have LCOV_EXCL_BR_LINE comments

4. Run `make lint` - verify clean

5. Run `make coverage` - verify 100% coverage on new code

6. Run `make check-valgrind` - verify no memory leaks

7. Review output format matches user story 31:
   - "From: {sender}"
   - "Time: {relative_time}"
   - Blank line
   - Full message body

8. Review relative time format:
   - "just now" (< 60 seconds)
   - "1 minute ago" / "N minutes ago" (1-59 minutes)
   - "1 hour ago" / "N hours ago" (1-23 hours)
   - "yesterday" (24-47 hours)
   - "N days ago" (>= 48 hours)

9. Consider edge cases:
   - Very old messages (years ago) - still shows "N days ago"
   - Future timestamps - treated as "just now"
   - Unicode in body - passed through unchanged

## Post-conditions
- `make check` passes
- `make lint` passes
- `make coverage` shows 100% on `src/mail/read.c`
- `ik_mail_read()` function implemented with:
  - Message lookup by ID
  - NOT_FOUND error for missing message
  - Formatted output with From, Time, and body
  - Relative time formatting
  - In-memory mark read (via ik_inbox_mark_read)
  - Database mark read (via ik_db_mail_mark_read)
  - notification_pending flag clearing
- `ik_mail_format_relative_time()` helper implemented with:
  - "just now" for < 1 minute
  - "N minutes ago" for 1-59 minutes
  - "N hours ago" for 1-23 hours
  - "yesterday" for 24-47 hours
  - "N days ago" for >= 48 hours
- Tests verify:
  - Relative time formatting (all ranges, singular/plural, edge cases)
  - Successful read returns formatted output
  - Output format matches user story
  - Full body without truncation
  - Multiline body preserved
  - Sub-agent sender ID displayed
  - Message marked read in memory
  - Message marked read in database
  - Already-read message doesn't double-decrement
  - notification_pending flag cleared
  - Not found returns error with no side effects
  - Empty inbox returns not found
  - Database error handling
  - Multiple message independence
  - Memory ownership
  - Edge cases (empty body, special chars, large IDs)
- src/mail/read.h exists with function declarations
- src/mail/read.c exists with implementation
- No changes to existing mail module files

## Notes

### Output Format Specification

**From user story 31:**
```
From: 1/
Time: 2 minutes ago

Found 3 OAuth patterns worth considering:
...
```

**Components:**
1. `From: {sender_agent_id}` - Sender's agent ID
2. `Time: {relative_time}` - Human-readable relative timestamp
3. Blank line separator
4. Full message body (no truncation)

**Format string:**
```c
"From: %s\nTime: %s\n\n%s"
```

### Relative Time Algorithm

```
diff = now - timestamp

if diff < 0:           "just now"       (future timestamp)
if diff < 60:          "just now"       (< 1 minute)
if diff < 3600:        "N minute(s) ago" (1-59 minutes)
if diff < 86400:       "N hour(s) ago"   (1-23 hours)
if diff < 172800:      "yesterday"       (24-47 hours)
else:                  "N days ago"      (>= 48 hours)
```

**Singular vs plural:**
- 1 minute ago vs N minutes ago
- 1 hour ago vs N hours ago
- N days ago (always plural, since minimum is 2)

### Order of Operations Rationale

**Why format before marking read?**

If formatting fails (OOM), we don't want the message to be marked as read when the user never saw it. By formatting first:
1. Failure leaves message unread
2. User can retry the read command
3. No inconsistent state

**Why mark memory before database?**

If database update fails:
1. User has already seen the formatted message
2. Marking in-memory prevents re-notification for this message
3. Database inconsistency is temporary (next session will see it as unread in DB)
4. Acceptable trade-off for better user experience

### notification_pending Flag

**From user story 38:**
> Reset `mail_notification_pending` when agent uses mail tool with `action: inbox` or `action: read`.

This flag prevents repeated notifications. Once an agent has been notified about unread mail and then reads a message, we clear the flag so they won't be notified again (until they go IDLE with new unread mail).

**Key behavior:**
- Cleared on ANY successful read (even if message was already read)
- NOT cleared on not-found errors
- NOT cleared on database errors (debatable, but keeps flag consistent with actual read state)

### Error Message Format

**Not found error:**
```
Message #5 not found
```

Uses `#` prefix to match the format in `/mail` list output (`#5 [unread] from 1/ - "..."`).

### Integration with Command Handler

This function will be called by the `/mail read <id>` command handler:

```c
// In command handler (future task)
static res_t handle_mail_read(ik_repl_ctx_t *repl, int64_t id)
{
    ik_agent_ctx_t *agent = repl->current_agent;

    char *output = NULL;
    res_t res = ik_mail_read(repl, repl, agent, id, &output);
    if (is_err(&res)) {
        // Display error to user
        display_error(repl, res.error);
        return res;
    }

    // Add to scrollback
    ik_scrollback_add_text(repl->scrollback, output);

    // Separator will auto-refresh to show updated unread count
    return OK(NULL);
}
```

### Integration with Tool Handler

The `mail` tool with `action: read` will also use this function:

```c
// In tool handler (future task)
static res_t handle_mail_tool_read(ik_repl_ctx_t *repl, int64_t id, cJSON *result_out)
{
    ik_agent_ctx_t *agent = repl->current_agent;

    char *output = NULL;
    res_t res = ik_mail_read(repl, repl, agent, id, &output);
    if (is_err(&res)) {
        return format_tool_error(res);
    }

    // Tool returns structured JSON with message fields
    ik_mail_msg_t *msg = ik_inbox_get_by_id(agent->inbox, id);
    cJSON_AddNumberToObject(result_out, "id", msg->id);
    cJSON_AddStringToObject(result_out, "from", msg->from_agent_id);
    cJSON_AddStringToObject(result_out, "timestamp", iso8601_format(msg->timestamp));
    cJSON_AddStringToObject(result_out, "body", msg->body);

    return OK(NULL);
}
```

### Testing Strategy

Tests organized by functionality:

1. **Relative time tests**: All time ranges, boundaries, singular/plural, edge cases
2. **Success tests**: Basic read, output format, full body, multiline, sub-agents
3. **Mark read tests**: Memory state, database call, idempotent behavior
4. **Notification flag tests**: Flag clearing behavior
5. **Not found tests**: Error case, no side effects
6. **Database error tests**: Error propagation, partial state update
7. **Multiple message tests**: Independence, sequential reads
8. **Memory tests**: Ownership, leak prevention
9. **Edge cases**: Empty body, special characters, large IDs

### Future Considerations

1. **Absolute timestamps**: Could add option to show absolute timestamp instead of relative
   - "2024-01-15 10:30 AM" vs "2 minutes ago"

2. **Timezone handling**: Current implementation uses server timezone. Could add user timezone support.

3. **Rich formatting**: Could add syntax highlighting for code blocks in message body.

4. **Pagination**: For very long messages, could add pagination support.

5. **Mark unread**: Could add ability to mark message as unread again.

### Dependency Chain

```
mail-msg-struct.md      (defines ik_mail_msg_t)
        |
        v
mail-inbox-struct.md    (defines ik_inbox_t, ik_inbox_get_by_id, ik_inbox_mark_read)
        |
        v
mail-db-operations.md   (defines ik_db_mail_mark_read)
        |
        v
mail-send.md            (high-level send operation)
        |
        v
mail-list.md            (high-level list operation)
        |
        v
mail-read.md            (this task - high-level read operation)
        |
        v
mail-command.md         (future - /mail command handler)
        |
        v
mail-tool.md            (future - mail tool for agents)
```
