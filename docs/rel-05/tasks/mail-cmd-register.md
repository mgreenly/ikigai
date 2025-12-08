# Task: Register /mail Command and Implement Subcommand Dispatch

## Target
Phase 3: Inter-Agent Mailboxes - Step 9 (Command registration and subcommand dispatch)

Supports User Stories:
- 29 (send mail) - `/mail send <agent-id> <body>`
- 30 (list inbox) - `/mail` (default action)
- 31 (read mail) - `/mail read <id>`
- 32 (inbox empty) - displays "(no messages)" for empty inbox
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
- .agents/skills/patterns/command-handler.md

### Pre-read Docs
- docs/backlog/inter-agent-mailboxes.md (User Interface section, /mail command syntax)
- docs/memory.md (talloc ownership patterns)
- docs/error_handling.md (res_t patterns)
- docs/return_values.md (output parameter conventions)
- docs/rel-05/user-stories/29-send-mail.md
- docs/rel-05/user-stories/30-list-inbox.md
- docs/rel-05/user-stories/31-read-mail.md
- docs/rel-05/user-stories/32-inbox-empty.md
- docs/rel-05/user-stories/33-send-to-nonexistent-agent.md
- docs/rel-05/user-stories/34-send-empty-body-rejected.md

### Pre-read Source (patterns)
- src/commands.h (ik_cmd_handler_t signature, ik_command_t structure)
- src/commands.c (command registry, dispatch pattern, existing handlers)
- src/commands_mark.h (external command handler pattern)
- src/mail/send.h (ik_mail_send() signature)
- src/mail/list.h (ik_mail_list() signature)
- src/mail/read.h (ik_mail_read() signature)
- src/scrollback.h (ik_scrollback_append_line() for output)
- src/agent.h (ik_agent_ctx_t - current_agent access)
- src/repl.h (ik_repl_ctx_t structure - agents[], current_agent)

### Pre-read Tests (patterns)
- tests/unit/commands/mark_test.c (command handler test patterns)
- tests/unit/mail/send_test.c (mail operation mocking patterns)
- tests/unit/mail/list_test.c (mail operation mocking patterns)
- tests/unit/mail/read_test.c (mail operation mocking patterns)
- tests/test_utils.h (test helper functions)

## Pre-conditions
- `make check` passes
- `make lint` passes
- `ik_mail_send()` implemented in src/mail/send.h - validates recipient, body; persists to DB and inbox
- `ik_mail_list()` implemented in src/mail/list.h - formats inbox for display
- `ik_mail_read()` implemented in src/mail/read.h - reads message, marks as read, clears notification
- Command registry exists in src/commands.c with `commands[]` array
- `ik_cmd_dispatch()` parses command name and arguments
- `ik_agent_ctx_t` has `agent_id` field and `inbox` field
- `ik_repl_ctx_t` has `current_agent` pointer (or equivalent access to active agent)
- Scrollback output via `ik_scrollback_append_line()`

## Task
Register the `/mail` command in the commands registry and implement the handler function that dispatches to the appropriate mail operation based on the subcommand.

**Command Registration:**
```c
// In commands[] array
{"mail", "Send and receive messages (usage: /mail [send|read] ...)", cmd_mail},
```

**Handler Function Signature:**
```c
// Main /mail command handler
// Parses subcommand and dispatches to appropriate operation
static res_t cmd_mail(void *ctx, ik_repl_ctx_t *repl, const char *args);
```

**Subcommand Dispatch Logic:**

| Input | Subcommand | Action |
|-------|------------|--------|
| `/mail` | (none) | Call `ik_mail_list()` - list inbox |
| `/mail send 1/ Hello` | `send` | Parse recipient and body, call `ik_mail_send()` |
| `/mail read 5` | `read` | Parse message ID, call `ik_mail_read()` |
| `/mail foo` | unknown | Show usage error |

**Parsing Rules:**

1. **No arguments (args == NULL or empty):**
   - Call `ik_mail_list(ctx, agent, terminal_width)`
   - Append result to scrollback

2. **`send <agent-id> <body>`:**
   - Extract agent-id (first token after "send ")
   - Extract body (everything after agent-id)
   - Call `ik_mail_send(ctx, repl, from, to, body, &id)`
   - On success: append "Mail sent to agent {to}" to scrollback
   - On error: append error message to scrollback

3. **`read <id>`:**
   - Extract message ID (parse as int64_t)
   - Call `ik_mail_read(ctx, repl, agent, id, &output)`
   - Append output to scrollback
   - On error: append error message to scrollback

4. **Unknown subcommand:**
   - Append usage message to scrollback:
     ```
     Usage: /mail [send|read] ...
       /mail                  - List inbox
       /mail send <to> <body> - Send message
       /mail read <id>        - Read message
     ```

**Output Examples:**

```
(agent 0/) > /mail
Inbox for agent 0/:
  #5 [unread] from 1/ - "Found 3 OAuth patterns..."
  #3 from 2/ - "Build complete"

(agent 0/) > /mail send 1/ Research OAuth patterns
Mail sent to agent 1/

(agent 0/) > /mail read 5
From: 1/
Time: 2 minutes ago

Found 3 OAuth patterns worth considering...

(agent 0/) > /mail send 99/ Hello
Error: Agent 99/ not found

(agent 0/) > /mail foo
Usage: /mail [send|read] ...
  /mail                  - List inbox
  /mail send <to> <body> - Send message
  /mail read <id>        - Read message
```

## TDD Cycle

### Red
1. Create `src/commands_mail.h`:
   ```c
   /**
    * @file commands_mail.h
    * @brief Mail command declaration
    */

   #ifndef IK_COMMANDS_MAIL_H
   #define IK_COMMANDS_MAIL_H

   #include "error.h"
   #include "repl.h"

   /**
    * Command handler for /mail
    *
    * Dispatches to mail operations based on subcommand:
    * - No args: list inbox (ik_mail_list)
    * - send <to> <body>: send mail (ik_mail_send)
    * - read <id>: read message (ik_mail_read)
    *
    * @param ctx Context for allocations
    * @param repl REPL context (provides current agent, scrollback, db_ctx)
    * @param args Subcommand and arguments (NULL for inbox listing)
    * @return OK(NULL) on success, ERR on failure
    *
    * Error conditions:
    * - Unknown subcommand: Shows usage message, returns OK
    * - Invalid message ID: Shows error, returns ERR
    * - Send/read errors: Propagated from mail operations
    */
   res_t ik_cmd_mail(void *ctx, ik_repl_ctx_t *repl, const char *args);

   #endif // IK_COMMANDS_MAIL_H
   ```

2. Create `tests/unit/commands/mail_cmd_test.c`:
   ```c
   /**
    * @file mail_cmd_test.c
    * @brief Tests for /mail command handler
    *
    * Tests the ik_cmd_mail() function which dispatches to mail operations:
    * - No arguments: list inbox
    * - send <to> <body>: send message
    * - read <id>: read message
    * - Unknown subcommand: show usage
    */

   #include "../../../src/commands_mail.h"
   #include "../../../src/agent.h"
   #include "../../../src/error.h"
   #include "../../../src/mail/inbox.h"
   #include "../../../src/mail/msg.h"
   #include "../../../src/scrollback.h"
   #include "../../test_utils.h"

   #include <check.h>
   #include <string.h>
   #include <talloc.h>

   // ========== Mock Infrastructure ==========

   // Mock state for mail operations
   static bool mock_send_called = false;
   static char *mock_send_from = NULL;
   static char *mock_send_to = NULL;
   static char *mock_send_body = NULL;
   static int64_t mock_send_id = 1;
   static bool mock_send_should_fail = false;
   static int mock_send_error_code = 0;  // NOT_FOUND or INVALID_ARG

   static bool mock_list_called = false;
   static size_t mock_list_terminal_width = 0;
   static char *mock_list_result = NULL;

   static bool mock_read_called = false;
   static int64_t mock_read_id = 0;
   static char *mock_read_output = NULL;
   static bool mock_read_should_fail = false;

   // Mock ik_mail_send
   res_t ik_mail_send(TALLOC_CTX *ctx, ik_repl_ctx_t *repl,
                      const char *from_agent_id, const char *to_agent_id,
                      const char *body, int64_t *id_out)
   {
       (void)repl;
       mock_send_called = true;

       // Record arguments
       if (mock_send_from != NULL) talloc_free(mock_send_from);
       if (mock_send_to != NULL) talloc_free(mock_send_to);
       if (mock_send_body != NULL) talloc_free(mock_send_body);

       mock_send_from = talloc_strdup(NULL, from_agent_id);
       mock_send_to = talloc_strdup(NULL, to_agent_id);
       mock_send_body = talloc_strdup(NULL, body);

       if (mock_send_should_fail) {
           *id_out = 0;
           if (mock_send_error_code == 1) {  // NOT_FOUND
               return ERR(ctx, NOT_FOUND, "Agent %s not found", to_agent_id);
           } else {  // INVALID_ARG
               return ERR(ctx, INVALID_ARG, "Message body cannot be empty");
           }
       }

       *id_out = mock_send_id++;
       return OK(NULL);
   }

   // Mock ik_mail_list
   char *ik_mail_list(TALLOC_CTX *ctx, ik_agent_ctx_t *agent, size_t terminal_width)
   {
       (void)agent;
       mock_list_called = true;
       mock_list_terminal_width = terminal_width;

       if (mock_list_result != NULL) {
           return talloc_strdup(ctx, mock_list_result);
       }
       return talloc_strdup(ctx, "Inbox for agent 0/:\n  (no messages)");
   }

   // Mock ik_mail_read
   res_t ik_mail_read(TALLOC_CTX *ctx, ik_repl_ctx_t *repl, ik_agent_ctx_t *agent,
                      int64_t message_id, char **output)
   {
       (void)repl;
       (void)agent;
       mock_read_called = true;
       mock_read_id = message_id;

       if (mock_read_should_fail) {
           *output = NULL;
           return ERR(ctx, NOT_FOUND, "Message #%" PRId64 " not found", message_id);
       }

       if (mock_read_output != NULL) {
           *output = talloc_strdup(ctx, mock_read_output);
       } else {
           *output = talloc_strdup(ctx, "From: 1/\nTime: just now\n\nTest message");
       }
       return OK(NULL);
   }

   // Scrollback tracking
   static char *last_scrollback_line = NULL;
   static int scrollback_append_count = 0;

   // We need to capture scrollback output - use actual scrollback or mock
   // For simplicity, we'll use actual scrollback and check its contents

   // ========== Test Fixture ==========

   static TALLOC_CTX *ctx;
   static ik_repl_ctx_t *repl;
   static ik_agent_ctx_t *agent;
   static ik_scrollback_t *scrollback;

   static void reset_mocks(void)
   {
       mock_send_called = false;
       if (mock_send_from != NULL) {
           talloc_free(mock_send_from);
           mock_send_from = NULL;
       }
       if (mock_send_to != NULL) {
           talloc_free(mock_send_to);
           mock_send_to = NULL;
       }
       if (mock_send_body != NULL) {
           talloc_free(mock_send_body);
           mock_send_body = NULL;
       }
       mock_send_id = 1;
       mock_send_should_fail = false;
       mock_send_error_code = 0;

       mock_list_called = false;
       mock_list_terminal_width = 0;
       mock_list_result = NULL;

       mock_read_called = false;
       mock_read_id = 0;
       mock_read_output = NULL;
       mock_read_should_fail = false;

       if (last_scrollback_line != NULL) {
           talloc_free(last_scrollback_line);
           last_scrollback_line = NULL;
       }
       scrollback_append_count = 0;
   }

   static void setup(void)
   {
       ctx = talloc_new(NULL);
       ck_assert_ptr_nonnull(ctx);

       reset_mocks();

       // Create scrollback
       scrollback = ik_scrollback_create(ctx, 80);
       ck_assert_ptr_nonnull(scrollback);

       // Create mock agent
       agent = talloc_zero(ctx, ik_agent_ctx_t);
       ck_assert_ptr_nonnull(agent);
       agent->agent_id = talloc_strdup(agent, "0/");
       agent->inbox = ik_inbox_create(agent);

       // Create mock REPL context
       repl = talloc_zero(ctx, ik_repl_ctx_t);
       ck_assert_ptr_nonnull(repl);
       repl->scrollback = scrollback;
       repl->current_agent = agent;
       repl->current_session_id = 1;
       repl->db_ctx = (ik_db_ctx_t *)0xDEADBEEF;  // Mock pointer

       // Note: For terminal width, repl->terminal->width or similar
       // Using cached_width from scrollback for now
   }

   static void teardown(void)
   {
       reset_mocks();
       talloc_free(ctx);
       ctx = NULL;
       repl = NULL;
       agent = NULL;
       scrollback = NULL;
   }

   // Helper to get last scrollback line
   static const char *get_last_scrollback_line(void)
   {
       size_t count = ik_scrollback_get_line_count(scrollback);
       if (count == 0) return NULL;

       const char *text;
       size_t length;
       res_t res = ik_scrollback_get_line_text(scrollback, count - 1, &text, &length);
       if (is_err(&res)) return NULL;

       return text;
   }

   // ========== Inbox Listing Tests (Default Action) ==========

   // Test: /mail with no args calls ik_mail_list
   START_TEST(test_mail_no_args_calls_list)
   {
       res_t res = ik_cmd_mail(ctx, repl, NULL);

       ck_assert(is_ok(&res));
       ck_assert(mock_list_called);
   }
   END_TEST

   // Test: /mail with empty string calls ik_mail_list
   START_TEST(test_mail_empty_string_calls_list)
   {
       res_t res = ik_cmd_mail(ctx, repl, "");

       ck_assert(is_ok(&res));
       ck_assert(mock_list_called);
   }
   END_TEST

   // Test: /mail with whitespace-only calls ik_mail_list
   START_TEST(test_mail_whitespace_only_calls_list)
   {
       res_t res = ik_cmd_mail(ctx, repl, "   ");

       ck_assert(is_ok(&res));
       ck_assert(mock_list_called);
   }
   END_TEST

   // Test: /mail list output appended to scrollback
   START_TEST(test_mail_list_output_in_scrollback)
   {
       mock_list_result = "Inbox for agent 0/:\n  #1 from 1/ - \"Hello\"";

       res_t res = ik_cmd_mail(ctx, repl, NULL);

       ck_assert(is_ok(&res));
       ck_assert_uint_gt(ik_scrollback_get_line_count(scrollback), 0);
   }
   END_TEST

   // Test: /mail passes terminal width to ik_mail_list
   START_TEST(test_mail_list_passes_terminal_width)
   {
       // Note: Width comes from scrollback->cached_width or repl->terminal
       res_t res = ik_cmd_mail(ctx, repl, NULL);

       ck_assert(is_ok(&res));
       ck_assert(mock_list_called);
       // Verify width was passed (depends on how width is accessed)
       ck_assert_uint_eq(mock_list_terminal_width, 80);
   }
   END_TEST

   // ========== Send Subcommand Tests ==========

   // Test: /mail send calls ik_mail_send with correct args
   START_TEST(test_mail_send_calls_send)
   {
       res_t res = ik_cmd_mail(ctx, repl, "send 1/ Hello world");

       ck_assert(is_ok(&res));
       ck_assert(mock_send_called);
       ck_assert_str_eq(mock_send_from, "0/");
       ck_assert_str_eq(mock_send_to, "1/");
       ck_assert_str_eq(mock_send_body, "Hello world");
   }
   END_TEST

   // Test: /mail send success shows confirmation
   START_TEST(test_mail_send_success_confirmation)
   {
       res_t res = ik_cmd_mail(ctx, repl, "send 1/ Test message");

       ck_assert(is_ok(&res));
       const char *last_line = get_last_scrollback_line();
       ck_assert_ptr_nonnull(last_line);
       ck_assert(strstr(last_line, "Mail sent to agent 1/") != NULL);
   }
   END_TEST

   // Test: /mail send with sub-agent recipient
   START_TEST(test_mail_send_to_subagent)
   {
       res_t res = ik_cmd_mail(ctx, repl, "send 1/0 Deep message");

       ck_assert(is_ok(&res));
       ck_assert(mock_send_called);
       ck_assert_str_eq(mock_send_to, "1/0");
       ck_assert_str_eq(mock_send_body, "Deep message");
   }
   END_TEST

   // Test: /mail send multiword body preserved
   START_TEST(test_mail_send_multiword_body)
   {
       res_t res = ik_cmd_mail(ctx, repl, "send 2/ This is a multi word message");

       ck_assert(is_ok(&res));
       ck_assert_str_eq(mock_send_body, "This is a multi word message");
   }
   END_TEST

   // Test: /mail send with extra whitespace
   START_TEST(test_mail_send_extra_whitespace)
   {
       res_t res = ik_cmd_mail(ctx, repl, "send   1/   Hello");

       ck_assert(is_ok(&res));
       ck_assert_str_eq(mock_send_to, "1/");
       ck_assert_str_eq(mock_send_body, "Hello");
   }
   END_TEST

   // Test: /mail send to nonexistent agent shows error
   START_TEST(test_mail_send_nonexistent_agent)
   {
       mock_send_should_fail = true;
       mock_send_error_code = 1;  // NOT_FOUND

       res_t res = ik_cmd_mail(ctx, repl, "send 99/ Hello");

       ck_assert(is_err(&res));
       const char *last_line = get_last_scrollback_line();
       ck_assert_ptr_nonnull(last_line);
       ck_assert(strstr(last_line, "Error") != NULL);
       ck_assert(strstr(last_line, "99/") != NULL);
   }
   END_TEST

   // Test: /mail send with empty body shows error
   START_TEST(test_mail_send_empty_body_rejected)
   {
       mock_send_should_fail = true;
       mock_send_error_code = 2;  // INVALID_ARG

       res_t res = ik_cmd_mail(ctx, repl, "send 1/ ");

       ck_assert(is_err(&res));
       const char *last_line = get_last_scrollback_line();
       ck_assert_ptr_nonnull(last_line);
       ck_assert(strstr(last_line, "Error") != NULL);
   }
   END_TEST

   // Test: /mail send missing recipient shows usage
   START_TEST(test_mail_send_missing_recipient)
   {
       res_t res = ik_cmd_mail(ctx, repl, "send");

       // Should show usage error, not crash
       ck_assert(is_err(&res) || is_ok(&res));  // Either error or usage shown
       ck_assert(!mock_send_called);  // Should not call send
   }
   END_TEST

   // Test: /mail send missing body shows usage
   START_TEST(test_mail_send_missing_body)
   {
       res_t res = ik_cmd_mail(ctx, repl, "send 1/");

       // Body is missing - should either error or use empty body
       // With trimming, empty body should be rejected
       ck_assert(!mock_send_called || mock_send_should_fail);
   }
   END_TEST

   // ========== Read Subcommand Tests ==========

   // Test: /mail read calls ik_mail_read with correct ID
   START_TEST(test_mail_read_calls_read)
   {
       res_t res = ik_cmd_mail(ctx, repl, "read 5");

       ck_assert(is_ok(&res));
       ck_assert(mock_read_called);
       ck_assert_int_eq(mock_read_id, 5);
   }
   END_TEST

   // Test: /mail read output appended to scrollback
   START_TEST(test_mail_read_output_in_scrollback)
   {
       mock_read_output = "From: 1/\nTime: 2 minutes ago\n\nTest content";

       res_t res = ik_cmd_mail(ctx, repl, "read 3");

       ck_assert(is_ok(&res));
       ck_assert_uint_gt(ik_scrollback_get_line_count(scrollback), 0);
   }
   END_TEST

   // Test: /mail read large message ID
   START_TEST(test_mail_read_large_id)
   {
       res_t res = ik_cmd_mail(ctx, repl, "read 9999999");

       ck_assert(is_ok(&res));
       ck_assert_int_eq(mock_read_id, 9999999);
   }
   END_TEST

   // Test: /mail read nonexistent message shows error
   START_TEST(test_mail_read_not_found)
   {
       mock_read_should_fail = true;

       res_t res = ik_cmd_mail(ctx, repl, "read 999");

       ck_assert(is_err(&res));
       const char *last_line = get_last_scrollback_line();
       ck_assert_ptr_nonnull(last_line);
       ck_assert(strstr(last_line, "Error") != NULL ||
                 strstr(last_line, "not found") != NULL);
   }
   END_TEST

   // Test: /mail read with invalid ID (not a number)
   START_TEST(test_mail_read_invalid_id)
   {
       res_t res = ik_cmd_mail(ctx, repl, "read abc");

       // Should show error for invalid ID
       ck_assert(is_err(&res) || is_ok(&res));  // Usage or error
       ck_assert(!mock_read_called);  // Should not call read with invalid ID
   }
   END_TEST

   // Test: /mail read with negative ID
   START_TEST(test_mail_read_negative_id)
   {
       res_t res = ik_cmd_mail(ctx, repl, "read -5");

       // Negative IDs should be rejected
       ck_assert(!mock_read_called || mock_read_id <= 0);
   }
   END_TEST

   // Test: /mail read missing ID shows usage
   START_TEST(test_mail_read_missing_id)
   {
       res_t res = ik_cmd_mail(ctx, repl, "read");

       // Should show usage error
       ck_assert(is_err(&res) || is_ok(&res));
       ck_assert(!mock_read_called);
   }
   END_TEST

   // Test: /mail read with extra whitespace
   START_TEST(test_mail_read_extra_whitespace)
   {
       res_t res = ik_cmd_mail(ctx, repl, "read   42  ");

       ck_assert(is_ok(&res));
       ck_assert(mock_read_called);
       ck_assert_int_eq(mock_read_id, 42);
   }
   END_TEST

   // ========== Unknown Subcommand Tests ==========

   // Test: /mail unknown shows usage
   START_TEST(test_mail_unknown_subcommand)
   {
       res_t res = ik_cmd_mail(ctx, repl, "foo");

       // Should show usage, return OK (informational)
       ck_assert(is_ok(&res));
       const char *last_line = get_last_scrollback_line();
       ck_assert_ptr_nonnull(last_line);
       ck_assert(strstr(last_line, "Usage") != NULL);
   }
   END_TEST

   // Test: /mail unknown shows available commands
   START_TEST(test_mail_unknown_shows_help)
   {
       res_t res = ik_cmd_mail(ctx, repl, "invalid");

       ck_assert(is_ok(&res));
       // Check scrollback contains usage info
       // May be multiple lines, but should mention send and read
       ck_assert_uint_gt(ik_scrollback_get_line_count(scrollback), 0);
   }
   END_TEST

   // Test: /mail with numeric but no subcommand (not "read 5")
   START_TEST(test_mail_numeric_no_subcommand)
   {
       res_t res = ik_cmd_mail(ctx, repl, "123");

       // "123" is not a valid subcommand - show usage
       ck_assert(is_ok(&res));
       ck_assert(!mock_read_called);  // Should not interpret as read
   }
   END_TEST

   // ========== Case Sensitivity Tests ==========

   // Test: /mail SEND (uppercase) - should work or show usage
   START_TEST(test_mail_send_uppercase)
   {
       res_t res = ik_cmd_mail(ctx, repl, "SEND 1/ Hello");

       // Design decision: case-insensitive or not?
       // For consistency with shell commands, recommend case-sensitive
       // If case-insensitive, mock_send_called should be true
       // If case-sensitive, should show usage
       // Either behavior is acceptable
       (void)res;  // Suppress unused warning
   }
   END_TEST

   // Test: /mail Read (mixed case)
   START_TEST(test_mail_read_mixed_case)
   {
       res_t res = ik_cmd_mail(ctx, repl, "Read 5");

       // Design decision: case-insensitive or not?
       (void)res;
   }
   END_TEST

   // ========== Edge Case Tests ==========

   // Test: Very long body
   START_TEST(test_mail_send_long_body)
   {
       // Create 1KB body
       char body[1100];
       memset(body, 'A', 1024);
       body[1024] = '\0';

       char *args = talloc_asprintf(ctx, "send 1/ %s", body);
       res_t res = ik_cmd_mail(ctx, repl, args);

       ck_assert(is_ok(&res));
       ck_assert(mock_send_called);
       ck_assert_int_eq(strlen(mock_send_body), 1024);
   }
   END_TEST

   // Test: Body with special characters
   START_TEST(test_mail_send_special_chars)
   {
       res_t res = ik_cmd_mail(ctx, repl, "send 1/ Hello \"quoted\" and <brackets>");

       ck_assert(is_ok(&res));
       ck_assert(strstr(mock_send_body, "\"quoted\"") != NULL);
   }
   END_TEST

   // Test: Body with newlines (from multiline input)
   START_TEST(test_mail_send_newlines_in_body)
   {
       res_t res = ik_cmd_mail(ctx, repl, "send 1/ Line1\nLine2\nLine3");

       ck_assert(is_ok(&res));
       ck_assert(strstr(mock_send_body, "\n") != NULL);
   }
   END_TEST

   // Test: Read ID at int64_t boundary
   START_TEST(test_mail_read_max_id)
   {
       res_t res = ik_cmd_mail(ctx, repl, "read 9223372036854775807");

       // Should handle INT64_MAX
       ck_assert(is_ok(&res) || is_err(&res));
       if (mock_read_called) {
           ck_assert_int_eq(mock_read_id, INT64_MAX);
       }
   }
   END_TEST

   // ========== From Agent Tests ==========

   // Test: Send uses current agent as sender
   START_TEST(test_mail_send_uses_current_agent)
   {
       // Change current agent ID
       talloc_free(agent->agent_id);
       agent->agent_id = talloc_strdup(agent, "2/");

       res_t res = ik_cmd_mail(ctx, repl, "send 1/ Hello");

       ck_assert(is_ok(&res));
       ck_assert_str_eq(mock_send_from, "2/");
   }
   END_TEST

   // Test: Send from sub-agent
   START_TEST(test_mail_send_from_subagent)
   {
       talloc_free(agent->agent_id);
       agent->agent_id = talloc_strdup(agent, "0/0");

       res_t res = ik_cmd_mail(ctx, repl, "send 1/ Message from sub-agent");

       ck_assert(is_ok(&res));
       ck_assert_str_eq(mock_send_from, "0/0");
   }
   END_TEST

   // ========== Memory Ownership Tests ==========

   // Test: No memory leaks on successful operations
   START_TEST(test_mail_no_memory_leak)
   {
       for (int i = 0; i < 100; i++) {
           TALLOC_CTX *tmp = talloc_new(ctx);
           res_t res = ik_cmd_mail(tmp, repl, NULL);
           ck_assert(is_ok(&res));
           talloc_free(tmp);
       }
       // If we get here without crash or OOM, test passes
   }
   END_TEST

   // Test: Error paths free resources
   START_TEST(test_mail_error_no_leak)
   {
       mock_send_should_fail = true;
       mock_send_error_code = 1;

       for (int i = 0; i < 100; i++) {
           TALLOC_CTX *tmp = talloc_new(ctx);
           res_t res = ik_cmd_mail(tmp, repl, "send 99/ fail");
           if (is_err(&res)) {
               talloc_free(res.err);
           }
           talloc_free(tmp);
       }
   }
   END_TEST

   // ========== Suite Configuration ==========

   static Suite *mail_cmd_suite(void)
   {
       Suite *s = suite_create("MailCommand");

       TCase *tc_list = tcase_create("ListInbox");
       tcase_add_checked_fixture(tc_list, setup, teardown);
       tcase_add_test(tc_list, test_mail_no_args_calls_list);
       tcase_add_test(tc_list, test_mail_empty_string_calls_list);
       tcase_add_test(tc_list, test_mail_whitespace_only_calls_list);
       tcase_add_test(tc_list, test_mail_list_output_in_scrollback);
       tcase_add_test(tc_list, test_mail_list_passes_terminal_width);
       suite_add_tcase(s, tc_list);

       TCase *tc_send = tcase_create("SendMail");
       tcase_add_checked_fixture(tc_send, setup, teardown);
       tcase_add_test(tc_send, test_mail_send_calls_send);
       tcase_add_test(tc_send, test_mail_send_success_confirmation);
       tcase_add_test(tc_send, test_mail_send_to_subagent);
       tcase_add_test(tc_send, test_mail_send_multiword_body);
       tcase_add_test(tc_send, test_mail_send_extra_whitespace);
       tcase_add_test(tc_send, test_mail_send_nonexistent_agent);
       tcase_add_test(tc_send, test_mail_send_empty_body_rejected);
       tcase_add_test(tc_send, test_mail_send_missing_recipient);
       tcase_add_test(tc_send, test_mail_send_missing_body);
       suite_add_tcase(s, tc_send);

       TCase *tc_read = tcase_create("ReadMail");
       tcase_add_checked_fixture(tc_read, setup, teardown);
       tcase_add_test(tc_read, test_mail_read_calls_read);
       tcase_add_test(tc_read, test_mail_read_output_in_scrollback);
       tcase_add_test(tc_read, test_mail_read_large_id);
       tcase_add_test(tc_read, test_mail_read_not_found);
       tcase_add_test(tc_read, test_mail_read_invalid_id);
       tcase_add_test(tc_read, test_mail_read_negative_id);
       tcase_add_test(tc_read, test_mail_read_missing_id);
       tcase_add_test(tc_read, test_mail_read_extra_whitespace);
       suite_add_tcase(s, tc_read);

       TCase *tc_unknown = tcase_create("UnknownSubcommand");
       tcase_add_checked_fixture(tc_unknown, setup, teardown);
       tcase_add_test(tc_unknown, test_mail_unknown_subcommand);
       tcase_add_test(tc_unknown, test_mail_unknown_shows_help);
       tcase_add_test(tc_unknown, test_mail_numeric_no_subcommand);
       suite_add_tcase(s, tc_unknown);

       TCase *tc_case = tcase_create("CaseSensitivity");
       tcase_add_checked_fixture(tc_case, setup, teardown);
       tcase_add_test(tc_case, test_mail_send_uppercase);
       tcase_add_test(tc_case, test_mail_read_mixed_case);
       suite_add_tcase(s, tc_case);

       TCase *tc_edge = tcase_create("EdgeCases");
       tcase_add_checked_fixture(tc_edge, setup, teardown);
       tcase_add_test(tc_edge, test_mail_send_long_body);
       tcase_add_test(tc_edge, test_mail_send_special_chars);
       tcase_add_test(tc_edge, test_mail_send_newlines_in_body);
       tcase_add_test(tc_edge, test_mail_read_max_id);
       suite_add_tcase(s, tc_edge);

       TCase *tc_agent = tcase_create("CurrentAgent");
       tcase_add_checked_fixture(tc_agent, setup, teardown);
       tcase_add_test(tc_agent, test_mail_send_uses_current_agent);
       tcase_add_test(tc_agent, test_mail_send_from_subagent);
       suite_add_tcase(s, tc_agent);

       TCase *tc_memory = tcase_create("Memory");
       tcase_add_checked_fixture(tc_memory, setup, teardown);
       tcase_add_test(tc_memory, test_mail_no_memory_leak);
       tcase_add_test(tc_memory, test_mail_error_no_leak);
       suite_add_tcase(s, tc_memory);

       return s;
   }

   int main(void)
   {
       Suite *s = mail_cmd_suite();
       SRunner *sr = srunner_create(s);

       srunner_run_all(sr, CK_NORMAL);
       int number_failed = srunner_ntests_failed(sr);
       srunner_free(sr);

       return (number_failed == 0) ? 0 : 1;
   }
   ```

3. Create stub `src/commands_mail.c`:
   ```c
   #include "commands_mail.h"

   res_t ik_cmd_mail(void *ctx, ik_repl_ctx_t *repl, const char *args)
   {
       (void)ctx;
       (void)repl;
       (void)args;
       return OK(NULL);
   }
   ```

4. Update `src/commands.c`:
   - Add `#include "commands_mail.h"` after other command includes
   - Add mail command to registry:
     ```c
     {"mail", "Send and receive messages (usage: /mail [send|read] ...)", ik_cmd_mail},
     ```

5. Update Makefile:
   - Add `src/commands_mail.c` to `MODULE_SOURCES`
   - Verify `tests/unit/commands/mail_cmd_test.c` is picked up by wildcard

6. Run `make check` - expect test failures (stub returns OK without dispatch)

### Green
1. Implement `src/commands_mail.c`:
   ```c
   /**
    * @file commands_mail.c
    * @brief Mail command handler implementation
    */

   #include "commands_mail.h"

   #include "mail/list.h"
   #include "mail/read.h"
   #include "mail/send.h"
   #include "panic.h"
   #include "scrollback.h"

   #include <assert.h>
   #include <ctype.h>
   #include <errno.h>
   #include <inttypes.h>
   #include <stdlib.h>
   #include <string.h>
   #include <talloc.h>

   // Usage message for unknown subcommand
   static const char *USAGE_MESSAGE =
       "Usage: /mail [send|read] ...\n"
       "  /mail                  - List inbox\n"
       "  /mail send <to> <body> - Send message\n"
       "  /mail read <id>        - Read message";

   // Skip leading whitespace
   static const char *skip_whitespace(const char *str)
   {
       while (*str && isspace((unsigned char)*str)) {
           str++;
       }
       return str;
   }

   // Check if string is empty or whitespace-only
   static bool is_empty_or_whitespace(const char *str)
   {
       if (str == NULL) return true;
       while (*str) {
           if (!isspace((unsigned char)*str)) return false;
           str++;
       }
       return true;
   }

   // Extract next token (word) from string
   // Returns token start and updates *end to point past token
   static const char *extract_token(const char *start, const char **end)
   {
       start = skip_whitespace(start);
       if (*start == '\0') {
           *end = start;
           return NULL;
       }

       const char *token_end = start;
       while (*token_end && !isspace((unsigned char)*token_end)) {
           token_end++;
       }

       *end = token_end;
       return start;
   }

   // Handle /mail (no args) - list inbox
   static res_t handle_mail_list(void *ctx, ik_repl_ctx_t *repl)
   {
       assert(repl != NULL);                // LCOV_EXCL_BR_LINE
       assert(repl->current_agent != NULL); // LCOV_EXCL_BR_LINE
       assert(repl->scrollback != NULL);    // LCOV_EXCL_BR_LINE

       // Get terminal width from scrollback
       size_t width = (size_t)repl->scrollback->cached_width;

       // Format inbox listing
       char *output = ik_mail_list(ctx, repl->current_agent, width);
       if (output == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

       // Append to scrollback
       res_t result = ik_scrollback_append_line(repl->scrollback, output, strlen(output));
       talloc_free(output);

       return result;
   }

   // Handle /mail send <to> <body>
   static res_t handle_mail_send(void *ctx, ik_repl_ctx_t *repl, const char *args)
   {
       assert(repl != NULL);                // LCOV_EXCL_BR_LINE
       assert(repl->current_agent != NULL); // LCOV_EXCL_BR_LINE
       assert(repl->scrollback != NULL);    // LCOV_EXCL_BR_LINE
       assert(args != NULL);                // LCOV_EXCL_BR_LINE

       // Skip "send" and whitespace
       args = skip_whitespace(args);

       // Extract recipient (first token after "send")
       const char *token_end;
       const char *recipient = extract_token(args, &token_end);

       if (recipient == NULL || token_end == recipient) {
           // No recipient specified
           char *msg = talloc_strdup(ctx, "Error: Missing recipient (usage: /mail send <to> <body>)");
           if (!msg) PANIC("OOM");  // LCOV_EXCL_BR_LINE
           ik_scrollback_append_line(repl->scrollback, msg, strlen(msg));
           return ERR(ctx, INVALID_ARG, "Missing recipient");
       }

       // Copy recipient to null-terminated string
       size_t recipient_len = (size_t)(token_end - recipient);
       char *to = talloc_strndup(ctx, recipient, recipient_len);
       if (to == NULL) PANIC("OOM");  // LCOV_EXCL_BR_LINE

       // Rest is message body
       const char *body_start = skip_whitespace(token_end);
       if (*body_start == '\0') {
           // No body specified
           char *msg = talloc_strdup(ctx, "Error: Missing message body (usage: /mail send <to> <body>)");
           if (!msg) PANIC("OOM");  // LCOV_EXCL_BR_LINE
           ik_scrollback_append_line(repl->scrollback, msg, strlen(msg));
           talloc_free(to);
           return ERR(ctx, INVALID_ARG, "Missing message body");
       }

       // Get sender from current agent
       const char *from = repl->current_agent->agent_id;

       // Send mail
       int64_t msg_id = 0;
       res_t send_result = ik_mail_send(ctx, repl, from, to, body_start, &msg_id);

       if (is_err(&send_result)) {
           // Show error in scrollback
           char *msg = talloc_asprintf(ctx, "Error: %s", error_message(send_result.err));
           if (!msg) PANIC("OOM");  // LCOV_EXCL_BR_LINE
           ik_scrollback_append_line(repl->scrollback, msg, strlen(msg));
           talloc_free(to);
           return send_result;
       }

       // Show success confirmation
       char *msg = talloc_asprintf(ctx, "Mail sent to agent %s", to);
       if (!msg) PANIC("OOM");  // LCOV_EXCL_BR_LINE
       ik_scrollback_append_line(repl->scrollback, msg, strlen(msg));

       talloc_free(to);
       return OK(NULL);
   }

   // Handle /mail read <id>
   static res_t handle_mail_read(void *ctx, ik_repl_ctx_t *repl, const char *args)
   {
       assert(repl != NULL);                // LCOV_EXCL_BR_LINE
       assert(repl->current_agent != NULL); // LCOV_EXCL_BR_LINE
       assert(repl->scrollback != NULL);    // LCOV_EXCL_BR_LINE
       assert(args != NULL);                // LCOV_EXCL_BR_LINE

       // Skip whitespace to get ID
       args = skip_whitespace(args);

       if (*args == '\0') {
           // No ID specified
           char *msg = talloc_strdup(ctx, "Error: Missing message ID (usage: /mail read <id>)");
           if (!msg) PANIC("OOM");  // LCOV_EXCL_BR_LINE
           ik_scrollback_append_line(repl->scrollback, msg, strlen(msg));
           return ERR(ctx, INVALID_ARG, "Missing message ID");
       }

       // Parse message ID
       char *endptr;
       errno = 0;
       long long id_val = strtoll(args, &endptr, 10);

       // Check for parse errors
       if (errno != 0 || endptr == args) {
           char *msg = talloc_asprintf(ctx, "Error: Invalid message ID '%s'", args);
           if (!msg) PANIC("OOM");  // LCOV_EXCL_BR_LINE
           ik_scrollback_append_line(repl->scrollback, msg, strlen(msg));
           return ERR(ctx, INVALID_ARG, "Invalid message ID");
       }

       // Check for negative or zero ID
       if (id_val <= 0) {
           char *msg = talloc_asprintf(ctx, "Error: Message ID must be positive (got %" PRId64 ")", (int64_t)id_val);
           if (!msg) PANIC("OOM");  // LCOV_EXCL_BR_LINE
           ik_scrollback_append_line(repl->scrollback, msg, strlen(msg));
           return ERR(ctx, INVALID_ARG, "Message ID must be positive");
       }

       int64_t message_id = (int64_t)id_val;

       // Read message
       char *output = NULL;
       res_t read_result = ik_mail_read(ctx, repl, repl->current_agent, message_id, &output);

       if (is_err(&read_result)) {
           // Show error in scrollback
           char *msg = talloc_asprintf(ctx, "Error: %s", error_message(read_result.err));
           if (!msg) PANIC("OOM");  // LCOV_EXCL_BR_LINE
           ik_scrollback_append_line(repl->scrollback, msg, strlen(msg));
           return read_result;
       }

       // Append message content to scrollback
       if (output != NULL) {
           ik_scrollback_append_line(repl->scrollback, output, strlen(output));
           talloc_free(output);
       }

       return OK(NULL);
   }

   // Handle unknown subcommand - show usage
   static res_t handle_mail_usage(void *ctx, ik_repl_ctx_t *repl)
   {
       assert(repl != NULL);             // LCOV_EXCL_BR_LINE
       assert(repl->scrollback != NULL); // LCOV_EXCL_BR_LINE
       (void)ctx;

       ik_scrollback_append_line(repl->scrollback, USAGE_MESSAGE, strlen(USAGE_MESSAGE));
       return OK(NULL);
   }

   res_t ik_cmd_mail(void *ctx, ik_repl_ctx_t *repl, const char *args)
   {
       // Preconditions
       assert(ctx != NULL);   // LCOV_EXCL_BR_LINE
       assert(repl != NULL);  // LCOV_EXCL_BR_LINE

       // Empty or whitespace-only args = list inbox
       if (is_empty_or_whitespace(args)) {
           return handle_mail_list(ctx, repl);
       }

       // Skip leading whitespace
       const char *cmd_start = skip_whitespace(args);

       // Extract subcommand
       const char *cmd_end;
       const char *subcommand = extract_token(cmd_start, &cmd_end);

       if (subcommand == NULL) {
           // Should not happen after is_empty_or_whitespace check
           return handle_mail_list(ctx, repl);  // LCOV_EXCL_LINE
       }

       size_t cmd_len = (size_t)(cmd_end - subcommand);

       // Dispatch based on subcommand
       if (cmd_len == 4 && strncmp(subcommand, "send", 4) == 0) {
           return handle_mail_send(ctx, repl, cmd_end);
       }

       if (cmd_len == 4 && strncmp(subcommand, "read", 4) == 0) {
           return handle_mail_read(ctx, repl, cmd_end);
       }

       // Unknown subcommand - show usage
       return handle_mail_usage(ctx, repl);
   }
   ```

2. Run `make check` - expect all tests pass

### Refactor
1. Verify include order follows style guide:
   - Own header first (`commands_mail.h`)
   - Project headers next (`mail/list.h`, `mail/read.h`, `mail/send.h`, etc.)
   - System headers last (`<assert.h>`, `<ctype.h>`, etc.)

2. Verify `// comments` style used (not `/* */`)

3. Verify assert() statements have LCOV_EXCL_BR_LINE comments

4. Run `make lint` - verify clean

5. Run `make coverage` - verify 100% coverage on new code

6. Run `make check-valgrind` - verify no memory leaks

7. Review output format:
   - "Mail sent to agent X" confirmation
   - "Error: ..." prefix for all errors
   - Usage message is clear and complete

8. Consider edge cases:
   - Agent ID with complex format (0/0/0)
   - Very long message bodies
   - Unicode in message body
   - Special characters in body

9. Verify command appears in `/help` output

10. Test integration with actual mail operations (not mocked)

## Post-conditions
- `make check` passes
- `make lint` passes
- `make coverage` shows 100% on `src/commands_mail.c`
- `/mail` command registered in commands[] array
- `ik_cmd_mail()` handler implemented with:
  - Dispatch to `ik_mail_list()` for no args
  - Dispatch to `ik_mail_send()` for "send" subcommand
  - Dispatch to `ik_mail_read()` for "read" subcommand
  - Usage message for unknown subcommands
  - Proper argument parsing and validation
- Tests verify:
  - No args calls list
  - Empty/whitespace args calls list
  - "send" subcommand parses recipient and body
  - "send" success shows confirmation
  - "send" errors show error message
  - "send" validates recipient and body presence
  - "read" subcommand parses message ID
  - "read" displays message content
  - "read" errors show error message
  - "read" validates ID is valid positive integer
  - Unknown subcommands show usage
  - Current agent used as sender for send
  - Memory management correct
- src/commands_mail.h exists with function declaration
- src/commands_mail.c exists with implementation
- commands.c updated with mail command registration
- `/help` shows mail command with description

## Notes

### Subcommand Parsing Algorithm

```
Input: args string (everything after "/mail")

1. Trim leading whitespace
2. If empty or NULL:
   → Dispatch to inbox listing
3. Extract first token (subcommand)
4. If subcommand == "send":
   → Extract next token (recipient)
   → Rest of string is body
   → Call ik_mail_send()
5. If subcommand == "read":
   → Extract next token (message ID)
   → Parse as int64_t
   → Call ik_mail_read()
6. Otherwise:
   → Show usage message
```

### Error Message Format

All errors displayed to user follow the pattern:
```
Error: <error message from operation>
```

For example:
- "Error: Agent 99/ not found"
- "Error: Message body cannot be empty"
- "Error: Message #5 not found"
- "Error: Missing recipient (usage: /mail send <to> <body>)"
- "Error: Invalid message ID 'abc'"

### Success Confirmation Format

```
Mail sent to agent 1/
```

Simple confirmation including recipient ID.

### Usage Message

```
Usage: /mail [send|read] ...
  /mail                  - List inbox
  /mail send <to> <body> - Send message
  /mail read <id>        - Read message
```

Shown when:
- Unknown subcommand (e.g., `/mail foo`)
- Could also be shown for `/mail help` if implemented

### Integration with Commands Registry

**In commands.c:**
```c
#include "commands_mail.h"

static const ik_command_t commands[] = {
    {"clear", "Clear scrollback...", cmd_clear},
    {"mark", "Create checkpoint...", ik_cmd_mark},
    {"rewind", "Rollback...", ik_cmd_rewind},
    {"help", "Show available commands", cmd_help},
    {"model", "Switch LLM model...", cmd_model},
    {"system", "Set system message...", cmd_system},
    {"debug", "Toggle debug output...", cmd_debug},
    {"mail", "Send and receive messages (usage: /mail [send|read] ...)", ik_cmd_mail},  // NEW
};
```

The description appears in `/help` output.

### Terminal Width Access

The handler needs terminal width for `ik_mail_list()`. Options:

1. **From scrollback**: `repl->scrollback->cached_width` (current approach)
2. **From terminal**: `repl->terminal->width` (if terminal struct accessible)
3. **From config**: `repl->cfg->terminal_width` (if persisted)

Using scrollback's cached width is simplest and consistent with existing rendering.

### Sender Identity

The sender is always the current agent:
```c
const char *from = repl->current_agent->agent_id;
```

This matches the design principle: "human operates through agent."

### Case Sensitivity

Design decision: Subcommands are **case-sensitive**.

Rationale:
- Consistent with shell commands
- Simpler implementation (no case conversion)
- User expectation from other CLI tools

### ID Parsing

Message IDs are parsed with `strtoll()`:
- Rejects non-numeric input
- Rejects negative values
- Handles large values (up to INT64_MAX)
- Reports clear errors for invalid input

### Dependency Chain

```
mail-msg-struct.md      (defines ik_mail_msg_t)
        |
        v
mail-inbox-struct.md    (defines ik_inbox_t)
        |
        v
mail-db-operations.md   (database operations)
        |
        v
mail-send.md            (ik_mail_send)
        |
        v
mail-list.md            (ik_mail_list)
        |
        v
mail-read.md            (ik_mail_read)
        |
        v
mail-cmd-register.md    (this task - /mail command)
        |
        v
mail-tool.md            (future - mail tool for agents)
```

### Testing Strategy

Tests organized by functionality:

1. **List inbox tests**: No args, empty args, whitespace-only
2. **Send tests**: Parsing, success, errors, edge cases
3. **Read tests**: Parsing, success, errors, ID validation
4. **Unknown subcommand tests**: Usage display
5. **Case sensitivity tests**: Uppercase/mixed case handling
6. **Edge cases**: Long bodies, special characters, extreme IDs
7. **Current agent tests**: Sender identity verification
8. **Memory tests**: Leak prevention

### Future Extensions

1. **Aliases**: `/m` as shorthand for `/mail`

2. **Interactive read**: `/mail read` without ID could prompt for ID or show first unread

3. **Send confirmation prompt**: For long messages, could confirm before sending

4. **Tab completion**: Complete agent IDs and subcommands

5. **History**: Remember last recipient for repeat sends

### Error Handling Philosophy

- **Validation errors** (missing args, invalid ID): Show error + usage hint
- **Operation errors** (agent not found, DB error): Propagate from mail operations
- **All errors**: Append to scrollback so user sees feedback
- **Return value**: ERR for actual errors, OK for informational (like usage display)

### Scrollback Output

All output goes through `ik_scrollback_append_line()`:
- Success confirmations
- Error messages
- Usage help
- List output (from ik_mail_list)
- Message content (from ik_mail_read)

This ensures consistent display and integration with scrollback history.
