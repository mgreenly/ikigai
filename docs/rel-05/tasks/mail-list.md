# Task: Inbox Listing Operation

## Target
Phase 3: Inter-Agent Mailboxes - Step 7 (High-level list operation)

Supports User Stories:
- 30 (list inbox) - format messages for display with unread first, previews, line truncation
- 32 (inbox empty) - display "(no messages)" when count == 0

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
- docs/backlog/inter-agent-mailboxes.md (User Interface section, output format examples)
- docs/memory.md (talloc ownership patterns)
- docs/rel-05/user-stories/30-list-inbox.md (exact output format, walkthrough)
- docs/rel-05/user-stories/32-inbox-empty.md (empty inbox handling)

### Pre-read Source (patterns)
- src/mail/msg.h (ik_mail_msg_t structure)
- src/mail/inbox.h (ik_inbox_t structure, ik_inbox_get_all() for sorted retrieval)
- src/mail/send.h (ik_mail_send() - sibling high-level operation pattern)
- src/mail/send.c (high-level operation implementation pattern)
- src/agent.h (ik_agent_ctx_t structure - agent_id field, inbox field)
- src/scrollback.h (output formatting patterns, line display)
- src/input/ansi.h (ik_ansi_visible_width() for ANSI-aware width calculation)
- src/wrapper.h (wrapper functions for testability)

### Pre-read Tests (patterns)
- tests/unit/mail/send_test.c (mail operation tests, mock patterns)
- tests/unit/mail/inbox_test.c (inbox tests, message creation helpers)
- tests/unit/scrollback/scrollback_test.c (output formatting test patterns)
- tests/test_utils.h (test helper functions)

## Pre-conditions
- `make check` passes
- `make lint` passes
- `ik_mail_msg_t` struct defined with fields: id, from_agent_id, to_agent_id, body, timestamp, read
- `ik_mail_msg_create()` factory function implemented
- `ik_inbox_t` struct with operations: `ik_inbox_add()`, `ik_inbox_get_all()`, `ik_inbox_mark_read()`
- `ik_inbox_get_all()` returns messages sorted: unread first, then by timestamp descending
- `ik_agent_ctx_t` has `ik_inbox_t *inbox` field and `char *agent_id` field
- `ik_mail_send()` implemented in src/mail/send.h and send.c

## Task
Create the `ik_mail_list()` function that formats an agent's inbox for display in the scrollback. This function produces the complete output string for the `/mail` command when listing the inbox.

**Function signature:**
```c
// Format inbox messages for display
//
// Produces a complete, ready-to-display string containing:
// - Header: "Inbox for agent {agent_id}:"
// - Each message on its own line with indentation
// - "(no messages)" placeholder if inbox is empty
//
// Message format: "  #ID [unread] from AGENT - \"PREVIEW...\""
// - [unread] tag only appears if msg->read == false
// - Preview is first ~50 chars of body, truncated with "..."
// - Line truncated to terminal_width minus indent
//
// @param ctx            Talloc context for output string allocation
// @param agent          Agent whose inbox to list (must not be NULL)
// @param terminal_width Terminal width for line truncation (0 = no truncation)
// @return               Formatted string ready for scrollback output (never NULL)
//
// Note: Returns allocated string even for empty inbox (with "(no messages)" text)
char *ik_mail_list(TALLOC_CTX *ctx, ik_agent_ctx_t *agent, size_t terminal_width);
```

**Output format (from user story 30):**
```
Inbox for agent 0/:
  #5 [unread] from 1/ - "Found 3 OAuth patterns worth considering: 1) Sil..."
  #4 [unread] from 2/ - "Build failed on test_auth.c line 42. Error: undef..."
  #3 from 1/ - "Starting research on OAuth patterns as requested..."
  #2 from 2/ - "Build complete, all 847 tests passing."
  #1 from 1/ - "Acknowledged. Beginning OAuth 2.0 research now."
```

**Empty inbox format (from user story 32):**
```
Inbox for agent 0/:
  (no messages)
```

**Key design decisions:**

1. **Preview length**: First ~50 characters of body. If body exceeds 50 chars, truncate with "...". Preview includes quotes.

2. **Line truncation**: Each message line is truncated to `terminal_width - 2` (for indent). If truncated, ends with "...".

3. **Unread tag**: `[unread]` appears after message ID only when `msg->read == false`.

4. **Sorting**: Uses `ik_inbox_get_all()` which returns messages already sorted (unread first, then timestamp descending).

5. **Empty inbox**: Always returns a valid string. Empty inbox produces header + "(no messages)" line.

6. **No trailing newline**: Output does not end with a newline (caller adds as needed).

7. **ANSI-aware truncation**: When truncating lines, use visible width (ignoring ANSI escape sequences) to ensure proper display.

## TDD Cycle

### Red
1. Create `src/mail/list.h`:
   ```c
   #ifndef IK_MAIL_LIST_H
   #define IK_MAIL_LIST_H

   #include "../agent.h"

   #include <stddef.h>
   #include <talloc.h>

   /**
    * Format inbox messages for display.
    *
    * Produces a formatted string containing the agent's inbox listing,
    * ready for display in the scrollback. Format follows user story 30:
    *
    *   Inbox for agent 0/:
    *     #5 [unread] from 1/ - "Preview text..."
    *     #3 from 2/ - "Another message..."
    *
    * Message ordering: unread first, then by timestamp descending.
    * This ordering is provided by ik_inbox_get_all().
    *
    * Empty inbox displays:
    *   Inbox for agent 0/:
    *     (no messages)
    *
    * @param ctx            Talloc context for output allocation
    * @param agent          Agent whose inbox to format (must not be NULL)
    * @param terminal_width Terminal width for line truncation (0 = no limit)
    * @return               Formatted string (never NULL, always allocated)
    *
    * Assertions:
    * - ctx must not be NULL
    * - agent must not be NULL
    * - agent->agent_id must not be NULL
    * - agent->inbox must not be NULL
    */
   char *ik_mail_list(TALLOC_CTX *ctx, ik_agent_ctx_t *agent, size_t terminal_width);

   #endif // IK_MAIL_LIST_H
   ```

2. Create `tests/unit/mail/list_test.c`:
   ```c
   /**
    * @file list_test.c
    * @brief Tests for inbox listing operation
    *
    * Tests the ik_mail_list() function which formats inbox
    * contents for display in the scrollback.
    *
    * Covers:
    * - Empty inbox
    * - Single message (read/unread)
    * - Multiple messages with mixed read status
    * - Preview truncation
    * - Line width truncation
    * - Message ordering verification
    * - Special characters in body
    */

   #include "../../../src/mail/list.h"
   #include "../../../src/mail/inbox.h"
   #include "../../../src/mail/msg.h"
   #include "../../../src/agent.h"
   #include "../../test_utils.h"

   #include <check.h>
   #include <string.h>
   #include <talloc.h>

   // ========== Test Fixture ==========

   static TALLOC_CTX *ctx;
   static ik_agent_ctx_t *agent;

   // Helper to create a test message and add to inbox
   static void add_msg(int64_t id, const char *from, const char *body,
                       int64_t ts, bool read)
   {
       ik_mail_msg_t *msg = NULL;
       res_t res = ik_mail_msg_create(ctx, id, from, agent->agent_id,
                                       body, ts, read, &msg);
       ck_assert(is_ok(&res));
       res = ik_inbox_add(agent->inbox, msg);
       ck_assert(is_ok(&res));
   }

   static void setup(void)
   {
       ctx = talloc_new(NULL);
       ck_assert_ptr_nonnull(ctx);

       // Create mock agent with inbox
       agent = talloc_zero(ctx, ik_agent_ctx_t);
       ck_assert_ptr_nonnull(agent);

       agent->agent_id = talloc_strdup(agent, "0/");
       ck_assert_ptr_nonnull(agent->agent_id);

       agent->inbox = ik_inbox_create(agent);
       ck_assert_ptr_nonnull(agent->inbox);
   }

   static void teardown(void)
   {
       talloc_free(ctx);
       ctx = NULL;
       agent = NULL;
   }

   // ========== Empty Inbox Tests ==========

   // Test: Empty inbox shows "(no messages)"
   START_TEST(test_list_empty_inbox)
   {
       char *result = ik_mail_list(ctx, agent, 0);

       ck_assert_ptr_nonnull(result);
       ck_assert(strstr(result, "Inbox for agent 0/:") != NULL);
       ck_assert(strstr(result, "(no messages)") != NULL);
   }
   END_TEST

   // Test: Empty inbox format matches user story 32 exactly
   START_TEST(test_list_empty_inbox_format)
   {
       char *result = ik_mail_list(ctx, agent, 0);

       // Should have header line and placeholder line
       const char *expected = "Inbox for agent 0/:\n  (no messages)";
       ck_assert_str_eq(result, expected);
   }
   END_TEST

   // Test: Empty inbox for different agent ID
   START_TEST(test_list_empty_inbox_different_agent)
   {
       talloc_free(agent->agent_id);
       agent->agent_id = talloc_strdup(agent, "1/0");

       char *result = ik_mail_list(ctx, agent, 0);

       ck_assert(strstr(result, "Inbox for agent 1/0:") != NULL);
       ck_assert(strstr(result, "(no messages)") != NULL);
   }
   END_TEST

   // ========== Single Message Tests ==========

   // Test: Single unread message displays correctly
   START_TEST(test_list_single_unread)
   {
       add_msg(1, "1/", "Hello world", 1700000000, false);

       char *result = ik_mail_list(ctx, agent, 0);

       ck_assert(strstr(result, "Inbox for agent 0/:") != NULL);
       ck_assert(strstr(result, "#1") != NULL);
       ck_assert(strstr(result, "[unread]") != NULL);
       ck_assert(strstr(result, "from 1/") != NULL);
       ck_assert(strstr(result, "\"Hello world\"") != NULL);
   }
   END_TEST

   // Test: Single read message has no [unread] tag
   START_TEST(test_list_single_read)
   {
       add_msg(1, "1/", "Hello world", 1700000000, true);

       char *result = ik_mail_list(ctx, agent, 0);

       ck_assert(strstr(result, "#1") != NULL);
       ck_assert(strstr(result, "[unread]") == NULL);
       ck_assert(strstr(result, "from 1/") != NULL);
       ck_assert(strstr(result, "\"Hello world\"") != NULL);
   }
   END_TEST

   // Test: Single message format matches expected pattern
   START_TEST(test_list_single_message_format)
   {
       add_msg(42, "2/", "Test message", 1700000000, false);

       char *result = ik_mail_list(ctx, agent, 0);

       // Check for expected components in order
       const char *line_start = strstr(result, "  #42");
       ck_assert_ptr_nonnull(line_start);
       ck_assert(strstr(line_start, "[unread]") != NULL);
       ck_assert(strstr(line_start, "from 2/") != NULL);
       ck_assert(strstr(line_start, "- \"Test message\"") != NULL);
   }
   END_TEST

   // Test: Sub-agent sender ID displayed correctly
   START_TEST(test_list_subagent_sender)
   {
       add_msg(1, "1/0", "From sub-agent", 1700000000, false);

       char *result = ik_mail_list(ctx, agent, 0);

       ck_assert(strstr(result, "from 1/0") != NULL);
   }
   END_TEST

   // ========== Multiple Message Tests ==========

   // Test: Multiple messages each on own line
   START_TEST(test_list_multiple_messages)
   {
       add_msg(1, "1/", "First message", 1700000001, true);
       add_msg(2, "2/", "Second message", 1700000002, true);
       add_msg(3, "1/", "Third message", 1700000003, true);

       char *result = ik_mail_list(ctx, agent, 0);

       // Count newlines to verify separate lines
       int newlines = 0;
       for (const char *p = result; *p; p++) {
           if (*p == '\n') newlines++;
       }
       // Header + 3 messages = 3 newlines (no trailing newline)
       ck_assert_int_eq(newlines, 3);

       ck_assert(strstr(result, "#1") != NULL);
       ck_assert(strstr(result, "#2") != NULL);
       ck_assert(strstr(result, "#3") != NULL);
   }
   END_TEST

   // Test: Unread messages appear first (sorted by ik_inbox_get_all)
   START_TEST(test_list_unread_first)
   {
       // Add read message first
       add_msg(1, "1/", "Read message", 1700000001, true);
       // Add unread message second
       add_msg(2, "2/", "Unread message", 1700000002, false);

       char *result = ik_mail_list(ctx, agent, 0);

       // Unread (#2) should appear before read (#1)
       const char *pos_unread = strstr(result, "#2");
       const char *pos_read = strstr(result, "#1");
       ck_assert_ptr_nonnull(pos_unread);
       ck_assert_ptr_nonnull(pos_read);
       ck_assert(pos_unread < pos_read);
   }
   END_TEST

   // Test: Multiple unread sorted by timestamp descending (newest first)
   START_TEST(test_list_unread_by_timestamp_desc)
   {
       add_msg(1, "1/", "Old unread", 1700000001, false);
       add_msg(2, "2/", "New unread", 1700000003, false);
       add_msg(3, "1/", "Middle unread", 1700000002, false);

       char *result = ik_mail_list(ctx, agent, 0);

       // Order should be: #2 (newest), #3 (middle), #1 (oldest)
       const char *pos1 = strstr(result, "#1");
       const char *pos2 = strstr(result, "#2");
       const char *pos3 = strstr(result, "#3");
       ck_assert(pos2 < pos3);  // Newest first
       ck_assert(pos3 < pos1);  // Oldest last
   }
   END_TEST

   // Test: Mixed read/unread with correct ordering
   START_TEST(test_list_mixed_read_unread_ordering)
   {
       // Mix of read and unread with various timestamps
       add_msg(1, "1/", "Read-Old", 1700000001, true);
       add_msg(2, "2/", "Unread-Old", 1700000002, false);
       add_msg(3, "1/", "Read-New", 1700000004, true);
       add_msg(4, "2/", "Unread-New", 1700000003, false);

       char *result = ik_mail_list(ctx, agent, 0);

       // Expected order: unread (newest to oldest), then read (newest to oldest)
       // #4 (unread, ts=3), #2 (unread, ts=2), #3 (read, ts=4), #1 (read, ts=1)
       const char *pos1 = strstr(result, "#1");
       const char *pos2 = strstr(result, "#2");
       const char *pos3 = strstr(result, "#3");
       const char *pos4 = strstr(result, "#4");

       ck_assert(pos4 < pos2);  // Unread newest before unread older
       ck_assert(pos2 < pos3);  // Unread before read
       ck_assert(pos3 < pos1);  // Read newest before read oldest
   }
   END_TEST

   // Test: Unread count matches [unread] tags shown
   START_TEST(test_list_unread_count_matches)
   {
       add_msg(1, "1/", "Read", 1700000001, true);
       add_msg(2, "2/", "Unread1", 1700000002, false);
       add_msg(3, "1/", "Unread2", 1700000003, false);

       char *result = ik_mail_list(ctx, agent, 0);

       // Count [unread] occurrences
       int unread_count = 0;
       const char *p = result;
       while ((p = strstr(p, "[unread]")) != NULL) {
           unread_count++;
           p++;
       }
       ck_assert_int_eq(unread_count, 2);
   }
   END_TEST

   // ========== Preview Truncation Tests ==========

   // Test: Short body shows complete text
   START_TEST(test_list_preview_short_body)
   {
       add_msg(1, "1/", "Short", 1700000000, false);

       char *result = ik_mail_list(ctx, agent, 0);

       ck_assert(strstr(result, "\"Short\"") != NULL);
       // Should NOT have truncation ellipsis inside quotes
       ck_assert(strstr(result, "\"Short...\"") == NULL);
   }
   END_TEST

   // Test: Exactly 50 chars shows complete text
   START_TEST(test_list_preview_exactly_50_chars)
   {
       // Create exactly 50-character body
       char body[51];
       memset(body, 'A', 50);
       body[50] = '\0';

       add_msg(1, "1/", body, 1700000000, false);

       char *result = ik_mail_list(ctx, agent, 0);

       // Should show full body without truncation
       char expected[54];  // "...\""
       snprintf(expected, sizeof(expected), "\"%s\"", body);
       ck_assert(strstr(result, expected) != NULL);
   }
   END_TEST

   // Test: Body over 50 chars is truncated with "..."
   START_TEST(test_list_preview_truncation)
   {
       // Create 60-character body
       char body[61];
       memset(body, 'B', 60);
       body[60] = '\0';

       add_msg(1, "1/", body, 1700000000, false);

       char *result = ik_mail_list(ctx, agent, 0);

       // Should have truncation ellipsis
       ck_assert(strstr(result, "...\"") != NULL);
       // Full body should NOT appear
       ck_assert(strstr(result, body) == NULL);
   }
   END_TEST

   // Test: Preview truncation preserves first 50 chars
   START_TEST(test_list_preview_truncation_content)
   {
       const char *body = "This is a long message that exceeds fifty characters and should be truncated";

       add_msg(1, "1/", body, 1700000000, false);

       char *result = ik_mail_list(ctx, agent, 0);

       // First 50 chars: "This is a long message that exceeds fifty charact"
       ck_assert(strstr(result, "\"This is a long message that exceeds fifty charact...\"") != NULL);
   }
   END_TEST

   // Test: Empty body shows empty quotes
   START_TEST(test_list_preview_empty_body)
   {
       add_msg(1, "1/", "", 1700000000, false);

       char *result = ik_mail_list(ctx, agent, 0);

       // Should show empty quotes
       ck_assert(strstr(result, "- \"\"") != NULL);
   }
   END_TEST

   // Test: Multiline body shows only first line in preview
   START_TEST(test_list_preview_multiline)
   {
       const char *body = "First line\nSecond line\nThird line";

       add_msg(1, "1/", body, 1700000000, false);

       char *result = ik_mail_list(ctx, agent, 0);

       // Preview should show first line, not newlines
       ck_assert(strstr(result, "First line") != NULL);
       // Newline should be replaced or truncated
       ck_assert(strstr(result, "\nSecond") == NULL);
   }
   END_TEST

   // Test: Body with special characters handled
   START_TEST(test_list_preview_special_chars)
   {
       const char *body = "Message with \"quotes\" and <brackets>";

       add_msg(1, "1/", body, 1700000000, false);

       char *result = ik_mail_list(ctx, agent, 0);

       // Should contain the special characters (not escaped for display)
       ck_assert(strstr(result, "Message with") != NULL);
   }
   END_TEST

   // ========== Line Width Truncation Tests ==========

   // Test: No truncation when terminal_width is 0
   START_TEST(test_list_no_width_truncation)
   {
       const char *long_body = "This is a very long message body that would normally be truncated if we had width limits";

       add_msg(1, "1/", long_body, 1700000000, false);

       char *result = ik_mail_list(ctx, agent, 0);

       // With terminal_width=0, no line truncation applied
       // Preview truncation still applies at 50 chars
       ck_assert(strstr(result, "\"This is a very long message body that would norma...\"") != NULL);
   }
   END_TEST

   // Test: Line truncation applied at terminal width
   START_TEST(test_list_line_truncation)
   {
       add_msg(1, "1/", "Message body text", 1700000000, false);

       // Very narrow terminal (40 chars)
       char *result = ik_mail_list(ctx, agent, 40);

       // Lines should be truncated
       // Each line should be <= 40 chars
       const char *line_start = strstr(result, "  #1");
       ck_assert_ptr_nonnull(line_start);

       // Find end of that line
       const char *line_end = strchr(line_start, '\n');
       if (line_end == NULL) line_end = line_start + strlen(line_start);

       size_t line_len = (size_t)(line_end - line_start);
       ck_assert_uint_le(line_len, 40);
   }
   END_TEST

   // Test: Truncated line ends with "..."
   START_TEST(test_list_line_truncation_ellipsis)
   {
       // Long body that will cause line truncation
       add_msg(1, "1/", "Message body", 1700000000, false);

       // Very narrow terminal (30 chars) to force truncation
       char *result = ik_mail_list(ctx, agent, 30);

       // Should have truncation ellipsis at end of message line
       const char *msg_line = strstr(result, "  #1");
       ck_assert_ptr_nonnull(msg_line);

       const char *line_end = strchr(msg_line, '\n');
       if (line_end == NULL) line_end = msg_line + strlen(msg_line);

       // Check that truncated line ends with ...
       ck_assert(line_end - msg_line >= 3);
       ck_assert(strncmp(line_end - 3, "...", 3) == 0);
   }
   END_TEST

   // Test: Header line not truncated
   START_TEST(test_list_header_not_truncated)
   {
       add_msg(1, "1/", "Test", 1700000000, false);

       // Narrow terminal
       char *result = ik_mail_list(ctx, agent, 40);

       // Header should still be complete
       ck_assert(strstr(result, "Inbox for agent 0/:") != NULL);
   }
   END_TEST

   // Test: Wide terminal doesn't truncate
   START_TEST(test_list_wide_terminal_no_truncation)
   {
       add_msg(1, "1/", "Short message", 1700000000, false);

       // Wide terminal (200 chars)
       char *result = ik_mail_list(ctx, agent, 200);

       // Should show full message line without truncation ellipsis
       ck_assert(strstr(result, "\"Short message\"") != NULL);
       // Verify no premature truncation
       const char *msg_line = strstr(result, "  #1");
       const char *line_end = strchr(msg_line, '\n');
       if (line_end == NULL) line_end = msg_line + strlen(msg_line);

       // Line should end with quote, not ellipsis
       ck_assert(*(line_end - 1) == '"');
   }
   END_TEST

   // ========== Unread Tag Tests ==========

   // Test: [unread] tag placement
   START_TEST(test_list_unread_tag_placement)
   {
       add_msg(1, "1/", "Test", 1700000000, false);

       char *result = ik_mail_list(ctx, agent, 0);

       // [unread] should appear between ID and "from"
       const char *id_pos = strstr(result, "#1");
       const char *unread_pos = strstr(result, "[unread]");
       const char *from_pos = strstr(result, "from 1/");

       ck_assert_ptr_nonnull(id_pos);
       ck_assert_ptr_nonnull(unread_pos);
       ck_assert_ptr_nonnull(from_pos);

       ck_assert(id_pos < unread_pos);
       ck_assert(unread_pos < from_pos);
   }
   END_TEST

   // Test: Read message has no [unread] tag (extra space not added)
   START_TEST(test_list_read_no_tag)
   {
       add_msg(1, "1/", "Test", 1700000000, true);

       char *result = ik_mail_list(ctx, agent, 0);

       // Should NOT have [unread] anywhere
       ck_assert(strstr(result, "[unread]") == NULL);
       // Should still have ID and from
       ck_assert(strstr(result, "#1") != NULL);
       ck_assert(strstr(result, "from 1/") != NULL);
   }
   END_TEST

   // ========== Message ID Format Tests ==========

   // Test: Message ID prefixed with #
   START_TEST(test_list_message_id_format)
   {
       add_msg(123, "1/", "Test", 1700000000, false);

       char *result = ik_mail_list(ctx, agent, 0);

       ck_assert(strstr(result, "#123") != NULL);
   }
   END_TEST

   // Test: Large message ID displayed correctly
   START_TEST(test_list_large_message_id)
   {
       add_msg(999999, "1/", "Test", 1700000000, false);

       char *result = ik_mail_list(ctx, agent, 0);

       ck_assert(strstr(result, "#999999") != NULL);
   }
   END_TEST

   // ========== Memory Ownership Tests ==========

   // Test: Result string owned by provided context
   START_TEST(test_list_memory_ownership)
   {
       add_msg(1, "1/", "Test", 1700000000, false);

       TALLOC_CTX *child = talloc_new(ctx);
       char *result = ik_mail_list(child, agent, 0);

       ck_assert_ptr_eq(talloc_parent(result), child);

       // Free child should free result (no crash)
       talloc_free(child);
   }
   END_TEST

   // Test: Multiple calls don't leak
   START_TEST(test_list_no_memory_leak)
   {
       add_msg(1, "1/", "Test", 1700000000, false);

       // Call multiple times
       for (int i = 0; i < 100; i++) {
           TALLOC_CTX *tmp = talloc_new(ctx);
           char *result = ik_mail_list(tmp, agent, 80);
           ck_assert_ptr_nonnull(result);
           talloc_free(tmp);
       }
       // If we get here without crash or OOM, test passes
   }
   END_TEST

   // ========== Edge Cases ==========

   // Test: Inbox with only read messages
   START_TEST(test_list_all_read)
   {
       add_msg(1, "1/", "Read 1", 1700000001, true);
       add_msg(2, "2/", "Read 2", 1700000002, true);

       char *result = ik_mail_list(ctx, agent, 0);

       // No [unread] tags
       ck_assert(strstr(result, "[unread]") == NULL);
       // Both messages present
       ck_assert(strstr(result, "#1") != NULL);
       ck_assert(strstr(result, "#2") != NULL);
   }
   END_TEST

   // Test: Inbox with only unread messages
   START_TEST(test_list_all_unread)
   {
       add_msg(1, "1/", "Unread 1", 1700000001, false);
       add_msg(2, "2/", "Unread 2", 1700000002, false);

       char *result = ik_mail_list(ctx, agent, 0);

       // Two [unread] tags
       int count = 0;
       const char *p = result;
       while ((p = strstr(p, "[unread]")) != NULL) {
           count++;
           p++;
       }
       ck_assert_int_eq(count, 2);
   }
   END_TEST

   // Test: Many messages (scalability)
   START_TEST(test_list_many_messages)
   {
       // Add 50 messages
       for (int i = 1; i <= 50; i++) {
           add_msg(i, "1/", "Message body", 1700000000 + i, i % 2 == 0);
       }

       char *result = ik_mail_list(ctx, agent, 0);

       ck_assert_ptr_nonnull(result);
       ck_assert(strstr(result, "#1") != NULL);
       ck_assert(strstr(result, "#50") != NULL);
   }
   END_TEST

   // Test: Message with very long sender ID
   START_TEST(test_list_long_sender_id)
   {
       add_msg(1, "0/0/0/0", "Deeply nested agent", 1700000000, false);

       char *result = ik_mail_list(ctx, agent, 0);

       ck_assert(strstr(result, "from 0/0/0/0") != NULL);
   }
   END_TEST

   // Test: Same timestamp messages maintain stable order
   START_TEST(test_list_same_timestamp)
   {
       add_msg(1, "1/", "First", 1700000000, false);
       add_msg(2, "2/", "Second", 1700000000, false);

       char *result = ik_mail_list(ctx, agent, 0);

       // Both messages should appear (order may vary but both present)
       ck_assert(strstr(result, "#1") != NULL);
       ck_assert(strstr(result, "#2") != NULL);
   }
   END_TEST

   // ========== Suite Configuration ==========

   static Suite *mail_list_suite(void)
   {
       Suite *s = suite_create("MailList");

       TCase *tc_empty = tcase_create("Empty");
       tcase_add_checked_fixture(tc_empty, setup, teardown);
       tcase_add_test(tc_empty, test_list_empty_inbox);
       tcase_add_test(tc_empty, test_list_empty_inbox_format);
       tcase_add_test(tc_empty, test_list_empty_inbox_different_agent);
       suite_add_tcase(s, tc_empty);

       TCase *tc_single = tcase_create("Single");
       tcase_add_checked_fixture(tc_single, setup, teardown);
       tcase_add_test(tc_single, test_list_single_unread);
       tcase_add_test(tc_single, test_list_single_read);
       tcase_add_test(tc_single, test_list_single_message_format);
       tcase_add_test(tc_single, test_list_subagent_sender);
       suite_add_tcase(s, tc_single);

       TCase *tc_multiple = tcase_create("Multiple");
       tcase_add_checked_fixture(tc_multiple, setup, teardown);
       tcase_add_test(tc_multiple, test_list_multiple_messages);
       tcase_add_test(tc_multiple, test_list_unread_first);
       tcase_add_test(tc_multiple, test_list_unread_by_timestamp_desc);
       tcase_add_test(tc_multiple, test_list_mixed_read_unread_ordering);
       tcase_add_test(tc_multiple, test_list_unread_count_matches);
       suite_add_tcase(s, tc_multiple);

       TCase *tc_preview = tcase_create("Preview");
       tcase_add_checked_fixture(tc_preview, setup, teardown);
       tcase_add_test(tc_preview, test_list_preview_short_body);
       tcase_add_test(tc_preview, test_list_preview_exactly_50_chars);
       tcase_add_test(tc_preview, test_list_preview_truncation);
       tcase_add_test(tc_preview, test_list_preview_truncation_content);
       tcase_add_test(tc_preview, test_list_preview_empty_body);
       tcase_add_test(tc_preview, test_list_preview_multiline);
       tcase_add_test(tc_preview, test_list_preview_special_chars);
       suite_add_tcase(s, tc_preview);

       TCase *tc_width = tcase_create("LineWidth");
       tcase_add_checked_fixture(tc_width, setup, teardown);
       tcase_add_test(tc_width, test_list_no_width_truncation);
       tcase_add_test(tc_width, test_list_line_truncation);
       tcase_add_test(tc_width, test_list_line_truncation_ellipsis);
       tcase_add_test(tc_width, test_list_header_not_truncated);
       tcase_add_test(tc_width, test_list_wide_terminal_no_truncation);
       suite_add_tcase(s, tc_width);

       TCase *tc_unread = tcase_create("UnreadTag");
       tcase_add_checked_fixture(tc_unread, setup, teardown);
       tcase_add_test(tc_unread, test_list_unread_tag_placement);
       tcase_add_test(tc_unread, test_list_read_no_tag);
       suite_add_tcase(s, tc_unread);

       TCase *tc_id = tcase_create("MessageId");
       tcase_add_checked_fixture(tc_id, setup, teardown);
       tcase_add_test(tc_id, test_list_message_id_format);
       tcase_add_test(tc_id, test_list_large_message_id);
       suite_add_tcase(s, tc_id);

       TCase *tc_memory = tcase_create("Memory");
       tcase_add_checked_fixture(tc_memory, setup, teardown);
       tcase_add_test(tc_memory, test_list_memory_ownership);
       tcase_add_test(tc_memory, test_list_no_memory_leak);
       suite_add_tcase(s, tc_memory);

       TCase *tc_edge = tcase_create("EdgeCases");
       tcase_add_checked_fixture(tc_edge, setup, teardown);
       tcase_add_test(tc_edge, test_list_all_read);
       tcase_add_test(tc_edge, test_list_all_unread);
       tcase_add_test(tc_edge, test_list_many_messages);
       tcase_add_test(tc_edge, test_list_long_sender_id);
       tcase_add_test(tc_edge, test_list_same_timestamp);
       suite_add_tcase(s, tc_edge);

       return s;
   }

   int main(void)
   {
       Suite *s = mail_list_suite();
       SRunner *sr = srunner_create(s);

       srunner_run_all(sr, CK_NORMAL);
       int number_failed = srunner_ntests_failed(sr);
       srunner_free(sr);

       return (number_failed == 0) ? 0 : 1;
   }
   ```

3. Create stub `src/mail/list.c`:
   ```c
   #include "list.h"

   char *ik_mail_list(TALLOC_CTX *ctx, ik_agent_ctx_t *agent, size_t terminal_width)
   {
       (void)agent;
       (void)terminal_width;
       return talloc_strdup(ctx, "");
   }
   ```

4. Update Makefile:
   - Add `src/mail/list.c` to `MODULE_SOURCES`
   - Verify `tests/unit/mail/list_test.c` is picked up by wildcard

5. Run `make check` - expect test failures (stub returns empty string)

### Green
1. Implement `src/mail/list.c`:
   ```c
   #include "list.h"

   #include "inbox.h"
   #include "msg.h"

   #include "../panic.h"

   #include <assert.h>
   #include <ctype.h>
   #include <inttypes.h>
   #include <string.h>
   #include <talloc.h>

   // Maximum preview length (characters of body to show)
   #define PREVIEW_MAX_LEN 50

   // Indentation for message lines
   #define INDENT "  "
   #define INDENT_LEN 2

   // Create preview of message body (first N chars, truncated with "...")
   // Handles multiline by replacing newlines with spaces
   static char *create_preview(TALLOC_CTX *ctx, const char *body)
   {
       assert(ctx != NULL);   // LCOV_EXCL_BR_LINE
       assert(body != NULL);  // LCOV_EXCL_BR_LINE

       size_t body_len = strlen(body);

       // Empty body
       if (body_len == 0) {
           return talloc_strdup(ctx, "\"\"");
       }

       // Determine preview length
       size_t preview_len = body_len;
       bool needs_truncation = false;
       if (preview_len > PREVIEW_MAX_LEN) {
           preview_len = PREVIEW_MAX_LEN;
           needs_truncation = true;
       }

       // Allocate buffer for preview: "..." + content + "..." + NUL
       // Opening quote (1) + content (preview_len) + truncation (...=3) + closing quote (1) + NUL (1)
       char *preview = talloc_array(ctx, char, 1 + preview_len + (needs_truncation ? 3 : 0) + 1 + 1);
       if (preview == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

       char *p = preview;
       *p++ = '"';

       // Copy preview content, replacing newlines with spaces
       for (size_t i = 0; i < preview_len; i++) {
           char c = body[i];
           if (c == '\n' || c == '\r') {
               *p++ = ' ';
           } else {
               *p++ = c;
           }
       }

       // Add truncation marker if needed
       if (needs_truncation) {
           *p++ = '.';
           *p++ = '.';
           *p++ = '.';
       }

       *p++ = '"';
       *p = '\0';

       return preview;
   }

   // Format a single message line
   // Returns: "  #ID [unread] from AGENT - \"PREVIEW...\""
   static char *format_message_line(TALLOC_CTX *ctx, const ik_mail_msg_t *msg)
   {
       assert(ctx != NULL);  // LCOV_EXCL_BR_LINE
       assert(msg != NULL);  // LCOV_EXCL_BR_LINE

       // Create preview
       char *preview = create_preview(ctx, msg->body);

       // Build line with or without [unread] tag
       char *line;
       if (!msg->read) {
           line = talloc_asprintf(ctx, "%s#%" PRId64 " [unread] from %s - %s",
                                   INDENT, msg->id, msg->from_agent_id, preview);
       } else {
           line = talloc_asprintf(ctx, "%s#%" PRId64 " from %s - %s",
                                   INDENT, msg->id, msg->from_agent_id, preview);
       }

       if (line == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

       // Free preview (it's been copied into line via asprintf)
       talloc_free(preview);

       return line;
   }

   // Truncate line to terminal width
   // If truncated, ends with "..."
   static void truncate_line(char *line, size_t max_width)
   {
       if (max_width == 0) return;  // No truncation

       size_t len = strlen(line);
       if (len <= max_width) return;  // No truncation needed

       // Need to truncate. Leave room for "..."
       if (max_width < 3) {
           // Very narrow - just truncate
           line[max_width] = '\0';
       } else {
           // Truncate and add ellipsis
           line[max_width - 3] = '.';
           line[max_width - 2] = '.';
           line[max_width - 1] = '.';
           line[max_width] = '\0';
       }
   }

   char *ik_mail_list(TALLOC_CTX *ctx, ik_agent_ctx_t *agent, size_t terminal_width)
   {
       // Preconditions
       assert(ctx != NULL);              // LCOV_EXCL_BR_LINE
       assert(agent != NULL);            // LCOV_EXCL_BR_LINE
       assert(agent->agent_id != NULL);  // LCOV_EXCL_BR_LINE
       assert(agent->inbox != NULL);     // LCOV_EXCL_BR_LINE

       // Create temporary context for intermediate strings
       TALLOC_CTX *tmp = talloc_new(NULL);
       if (tmp == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

       // Start with header
       char *header = talloc_asprintf(tmp, "Inbox for agent %s:", agent->agent_id);
       if (header == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

       // Get sorted messages
       size_t count;
       ik_mail_msg_t **messages = ik_inbox_get_all(agent->inbox, &count);

       // Handle empty inbox
       if (count == 0) {
           char *result = talloc_asprintf(ctx, "%s\n%s(no messages)", header, INDENT);
           if (result == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
           talloc_free(tmp);
           return result;
       }

       // Build message lines
       char **lines = talloc_array(tmp, char *, count);
       if (lines == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

       for (size_t i = 0; i < count; i++) {
           lines[i] = format_message_line(tmp, messages[i]);
           truncate_line(lines[i], terminal_width);
       }

       // Calculate total output size
       size_t total_len = strlen(header) + 1;  // Header + newline
       for (size_t i = 0; i < count; i++) {
           total_len += strlen(lines[i]);
           if (i < count - 1) total_len += 1;  // Newline between messages
       }

       // Allocate result string
       char *result = talloc_array(ctx, char, total_len + 1);
       if (result == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

       // Build result
       char *p = result;

       // Copy header
       size_t header_len = strlen(header);
       memcpy(p, header, header_len);
       p += header_len;
       *p++ = '\n';

       // Copy message lines
       for (size_t i = 0; i < count; i++) {
           size_t line_len = strlen(lines[i]);
           memcpy(p, lines[i], line_len);
           p += line_len;
           if (i < count - 1) {
               *p++ = '\n';
           }
       }
       *p = '\0';

       // Cleanup
       talloc_free(tmp);

       return result;
   }
   ```

2. Run `make check` - expect all tests pass

### Refactor
1. Verify include order follows style guide:
   - Own header first (`list.h`)
   - Sibling headers next (`inbox.h`, `msg.h`)
   - Project headers next (`../panic.h`)
   - System headers last (`<assert.h>`, `<ctype.h>`, etc.)

2. Verify `// comments` style used (not `/* */`)

3. Verify assert() statements have LCOV_EXCL_BR_LINE comments

4. Run `make lint` - verify clean

5. Run `make coverage` - verify 100% coverage on new code

6. Run `make check-valgrind` - verify no memory leaks

7. Review output format matches user stories:
   - "Inbox for agent 0/:" header
   - "  #ID [unread] from AGENT - \"PREVIEW...\"" message format
   - "  (no messages)" for empty inbox

8. Consider edge cases:
   - Very long message IDs (int64_t max)
   - Unicode characters in body (handled as bytes)
   - ANSI escape sequences (passed through, not width-counted in this version)

## Post-conditions
- `make check` passes
- `make lint` passes
- `make coverage` shows 100% on `src/mail/list.c`
- `ik_mail_list()` function implemented with:
  - Header line: "Inbox for agent {id}:"
  - Empty inbox: "  (no messages)"
  - Message format: "  #ID [unread] from AGENT - \"PREVIEW...\""
  - Preview truncation at 50 chars with "..."
  - Line truncation based on terminal_width
  - Unread tag only for unread messages
  - Messages sorted (unread first, then timestamp descending)
- Tests verify:
  - Empty inbox format
  - Single message (read/unread)
  - Multiple messages with ordering
  - Preview truncation (short, exact 50, over 50)
  - Preview with empty body, multiline, special chars
  - Line width truncation
  - Header not truncated
  - Unread tag placement and absence
  - Message ID format
  - Memory ownership
  - Edge cases (all read, all unread, many messages)
- src/mail/list.h exists with function declaration
- src/mail/list.c exists with implementation
- No changes to existing mail module files

## Notes

### Output Format Specification

**Header:**
```
Inbox for agent {agent_id}:
```

**Empty inbox:**
```
Inbox for agent 0/:
  (no messages)
```

**Message line format:**
```
  #ID [unread] from AGENT - "PREVIEW..."
```

Components:
- `  ` - Two-space indent
- `#ID` - Message ID prefixed with #
- `[unread]` - Tag only present if `msg->read == false`
- `from AGENT` - Sender agent ID
- `- "PREVIEW..."` - First ~50 chars of body in quotes, truncated with "..."

**Example full output:**
```
Inbox for agent 0/:
  #5 [unread] from 1/ - "Found 3 OAuth patterns worth considering: 1) Sil..."
  #4 [unread] from 2/ - "Build failed on test_auth.c line 42. Error: undef..."
  #3 from 1/ - "Starting research on OAuth patterns as requested..."
  #2 from 2/ - "Build complete, all 847 tests passing."
  #1 from 1/ - "Acknowledged. Beginning OAuth 2.0 research now."
```

### Preview Truncation Algorithm

1. If body length <= 50 chars:
   - Show complete body in quotes: `"body text"`

2. If body length > 50 chars:
   - Take first 50 chars
   - Append "..."
   - Wrap in quotes: `"first 50 chars..."`

3. Multiline handling:
   - Replace `\n` and `\r` with space
   - Truncation still applies at 50 chars

### Line Truncation Algorithm

1. Format complete message line
2. If `terminal_width` == 0, no truncation
3. If line length > terminal_width:
   - Truncate to terminal_width - 3
   - Append "..."
4. Result fits within terminal_width

**Example:**
```
// Original: "  #5 [unread] from 1/ - \"Found 3 OAuth patterns...\""
// Width 40: "  #5 [unread] from 1/ - \"Found 3 ..."
```

### Integration with Command Handler

This function will be called by the `/mail` command handler:

```c
// In mail command handler (future task)
static res_t handle_mail_inbox(ik_repl_ctx_t *repl)
{
    ik_agent_ctx_t *agent = repl->current_agent;
    size_t width = repl->terminal->width;

    char *output = ik_mail_list(repl, agent, width);

    // Add to scrollback
    ik_scrollback_add_text(repl->scrollback, output);

    return OK(NULL);
}
```

### Integration with Tool Handler

The `mail` tool with `action: inbox` will also use this function:

```c
// In tool handler (future task)
static res_t handle_mail_tool_inbox(ik_repl_ctx_t *repl, cJSON *result_out)
{
    ik_agent_ctx_t *agent = repl->current_agent;

    // Get messages for JSON response (different from display format)
    size_t count;
    ik_mail_msg_t **msgs = ik_inbox_get_all(agent->inbox, &count);

    // Build JSON array...
    // (Tool response uses JSON format, not ik_mail_list display format)
}
```

### Testing Strategy

Tests organized by functionality:

1. **Empty inbox tests**: Verify placeholder text and format
2. **Single message tests**: Basic formatting, read/unread distinction
3. **Multiple message tests**: Ordering, multiple lines, count verification
4. **Preview tests**: Length limits, truncation, special characters
5. **Line width tests**: Terminal width handling, truncation with ellipsis
6. **Unread tag tests**: Placement, absence for read messages
7. **Message ID tests**: Format, large values
8. **Memory tests**: Ownership, leak prevention
9. **Edge cases**: All read, all unread, many messages, special sender IDs

### Future Considerations

1. **ANSI-aware truncation**: Current implementation counts bytes, not visible characters. For ANSI-colored output, use `ik_ansi_visible_width()`.

2. **Pagination**: Large inboxes may benefit from pagination (future enhancement).

3. **Relative timestamps**: Could show "2 minutes ago" instead of nothing (currently no timestamp shown in list view - only in full message view via `/mail read ID`).

4. **Mark indicator**: Could show which message was last read, or allow cursor navigation.

5. **Color coding**: Unread messages could be highlighted with ANSI colors.

### Dependency Chain

```
mail-msg-struct.md      (defines ik_mail_msg_t)
        |
        v
mail-inbox-struct.md    (defines ik_inbox_t, ik_inbox_get_all)
        |
        v
mail-send.md            (high-level send operation)
        |
        v
mail-list.md            (this task - high-level list operation)
        |
        v
mail-read.md            (future - high-level read single message)
        |
        v
mail-command.md         (future - /mail command handler)
```
