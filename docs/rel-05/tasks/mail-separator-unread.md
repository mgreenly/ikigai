# Task: Display Unread Mail Count in Separator

## Target
Phase 3: Inter-Agent Mailboxes - Step 12 (Separator unread mail indicator)

Supports User Stories:
- 35 (separator shows unread count) - `[mail:N]` indicator displayed when unread_count > 0
- 36 (hides when zero) - No indicator when unread_count == 0

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/tdd.md
- .agents/skills/coverage.md
- .agents/skills/mocking.md
- .agents/skills/patterns/vtable.md

## Pre-read Docs
- docs/backlog/inter-agent-mailboxes.md (Status Bar Indicator section)
- docs/rel-05/user-stories/35-separator-shows-unread-count.md
- docs/rel-05/user-stories/36-separator-hides-mail-when-zero.md
- docs/rel-05/user-stories/28-separator-navigation-context.md (separator layout reference)

## Pre-read Source (patterns)
- src/layer_wrappers.h (separator layer interface)
- src/layer_wrappers.c (separator layer implementation)
- src/layer.h (layer vtable, output_buffer)
- src/repl.h (ik_repl_ctx_t - access path to current agent)
- src/agent.h (ik_agent_ctx_t - agent_id, inbox fields - from Phase 1)
- src/mail/inbox.h (ik_inbox_t - unread_count field)
- src/output_buffer.h (ik_output_buffer_t, ik_output_buffer_append)

## Pre-read Tests (patterns)
- tests/unit/layer/separator_layer_test.c (separator layer tests)
- tests/unit/mail/inbox_test.c (inbox fixture patterns)

## Pre-conditions
- `make check` passes
- `make lint` passes
- mail-tool-handler.md complete: All mail operations working
- separator-agent-id.md complete: Separator shows agent ID and navigation context
- mail-agent-inbox.md complete: Current agent accessible via `repl->current_agent` with `inbox` field
- `ik_inbox_t` has `unread_count` field
- Separator layer receives repl context (from separator-agent-id.md)
- Current separator format: `───────── ↑- ←- [0/] →1/ ↓- ─────────────────────────`

## Task
Update the separator layer to display an unread mail indicator `[mail:N]` after the navigation context when the current agent has unread messages. When unread_count is zero, no indicator is shown (clean separator).

**Visual specification (from user stories 35 & 36):**

With unread mail (count > 0):
```
───────── ↑- ←- [0/] →1/ ↓- ─────── [mail:2] ───────
```

Without unread mail (count == 0):
```
───────── ↑- ←- [0/] →1/ ↓- ─────────────────────────
```

**Format specification:**
- Indicator: `[mail:{count}]` - square brackets, literal "mail:", count
- Position: After navigation context section, before trailing dashes
- Padding: Single space before and after indicator ` [mail:N] `
- No special styling in v1 (same as rest of separator)
- Count is the agent's `inbox->unread_count` value

**Update triggers (immediate re-render):**
1. New mail arrives (unread_count increases)
2. Message marked as read (unread_count decreases)
3. Agent switch (different agent's unread_count)

**Rendering logic:**
```c
void separator_render(ik_layer_t *layer, ik_output_buffer_t *output,
                      size_t width, size_t start_row, size_t row_count)
{
    ik_repl_ctx_t *repl = layer->user_ctx;
    ik_agent_ctx_t *agent = repl->current_agent;

    // Build navigation context string: "↑- ←- [0/] →1/ ↓-"
    char nav_label[64];
    build_navigation_label(repl, nav_label, sizeof(nav_label));
    size_t nav_len = strlen(nav_label);

    // Build mail indicator (empty if unread_count == 0)
    char mail_indicator[32] = "";
    size_t mail_len = 0;
    if (agent->inbox->unread_count > 0) {
        snprintf(mail_indicator, sizeof(mail_indicator),
                 " [mail:%zu] ", agent->inbox->unread_count);
        mail_len = strlen(mail_indicator);
    }

    // Calculate dash counts
    size_t content_len = nav_len + mail_len;
    size_t dash_space = (width > content_len) ? width - content_len : 0;
    size_t left_dashes = dash_space / 2;
    size_t right_dashes = dash_space - left_dashes;

    // Render: dashes + nav_label + dashes + mail_indicator + dashes
    // (mail indicator positioned after nav section)
    render_dashes(output, left_dashes);
    render_string(output, nav_label);
    if (mail_len > 0) {
        // Position mail indicator ~2/3 across for visual balance
        size_t mid_dashes = right_dashes / 2;
        render_dashes(output, mid_dashes);
        render_string(output, mail_indicator);
        render_dashes(output, right_dashes - mid_dashes);
    } else {
        render_dashes(output, right_dashes);
    }
}
```

**Edge cases:**
- Very narrow terminal: Prioritize agent ID, then nav, then mail indicator
- Very high unread count (>999): Display actual number, truncate dashes
- No current agent: Skip mail indicator entirely
- Agent has inbox but unread_count is 0: No indicator (story 36)

**Width calculation adjustment:**
The mail indicator consumes width: ` [mail:N] ` = 9 + digits(N) characters
- 1 unread: 10 chars (` [mail:1] `)
- 99 unread: 12 chars (` [mail:99] `)
- 999 unread: 13 chars (` [mail:999] `)

## TDD Cycle

### Red
1. Update `src/layer_wrappers.h` - Add mail indicator helper (internal):
   No changes needed to public interface (separator already receives repl context)

2. Create/update tests in `tests/unit/layer/separator_mail_test.c`:
   ```c
   /**
    * @file separator_mail_test.c
    * @brief Tests for separator mail indicator
    *
    * Tests the [mail:N] indicator in separator layer:
    * - Shown when unread_count > 0
    * - Hidden when unread_count == 0
    * - Updates on mail arrival and read
    * - Width calculations with indicator
    */

   #include "../../../src/layer_wrappers.h"
   #include "../../../src/mail/inbox.h"
   #include "../../../src/error.h"
   #include "../../test_utils.h"

   #include <check.h>
   #include <string.h>
   #include <talloc.h>

   // ========== Test Fixture ==========

   static TALLOC_CTX *ctx;
   static ik_repl_ctx_t *mock_repl;
   static ik_agent_ctx_t *mock_agent;
   static ik_layer_t *separator;
   static bool visible;

   // Helper to create mock agent with inbox
   static ik_agent_ctx_t *create_mock_agent(TALLOC_CTX *parent, const char *agent_id)
   {
       ik_agent_ctx_t *a = talloc_zero(parent, ik_agent_ctx_t);
       if (a == NULL) return NULL;

       a->agent_id = talloc_strdup(a, agent_id);
       a->inbox = ik_inbox_create(a);

       return a;
   }

   // Helper to add unread messages to inbox
   static void add_unread(ik_agent_ctx_t *a, size_t count)
   {
       for (size_t i = 0; i < count; i++) {
           ik_mail_msg_t *msg = NULL;
           res_t res = ik_mail_msg_create(ctx, (int64_t)i + 1, "1/", a->agent_id,
                                           "Test message", 1700000000 + (int64_t)i,
                                           false, &msg);
           ck_assert(is_ok(&res));
           ik_inbox_add(a->inbox, msg);
       }
   }

   // Helper to find string in output buffer
   static bool output_contains(ik_output_buffer_t *output, const char *needle)
   {
       // Search within output data
       if (output->size < strlen(needle)) return false;
       return memmem(output->data, output->size, needle, strlen(needle)) != NULL;
   }

   static void setup(void)
   {
       ctx = talloc_new(NULL);
       ck_assert_ptr_nonnull(ctx);

       // Create mock repl with single agent
       mock_repl = talloc_zero(ctx, ik_repl_ctx_t);
       ck_assert_ptr_nonnull(mock_repl);

       mock_agent = create_mock_agent(ctx, "0/");
       ck_assert_ptr_nonnull(mock_agent);

       mock_repl->current_agent = mock_agent;
       mock_repl->agents = talloc_array(ctx, ik_agent_ctx_t *, 1);
       mock_repl->agents[0] = mock_agent;
       mock_repl->agent_count = 1;

       visible = true;
       separator = ik_separator_layer_create(ctx, "sep", &visible);
       ck_assert_ptr_nonnull(separator);

       // Set repl as user context for separator
       separator->user_ctx = mock_repl;
   }

   static void teardown(void)
   {
       talloc_free(ctx);
       ctx = NULL;
       mock_repl = NULL;
       mock_agent = NULL;
       separator = NULL;
   }

   // ========== No Unread Mail Tests (Story 36) ==========

   // Test: No indicator when inbox is empty
   START_TEST(test_mail_indicator_hidden_empty_inbox)
   {
       ck_assert_uint_eq(mock_agent->inbox->unread_count, 0);

       ik_output_buffer_t *output = ik_output_buffer_create(ctx, 200);
       separator->render(separator, output, 60, 0, 1);

       // Should NOT contain [mail:
       ck_assert(!output_contains(output, "[mail:"));
   }
   END_TEST

   // Test: No indicator when all messages are read
   START_TEST(test_mail_indicator_hidden_all_read)
   {
       // Add messages as read
       ik_mail_msg_t *msg = NULL;
       res_t res = ik_mail_msg_create(ctx, 1, "1/", "0/", "Test", 1700000000, true, &msg);
       ck_assert(is_ok(&res));
       ik_inbox_add(mock_agent->inbox, msg);

       ck_assert_uint_eq(mock_agent->inbox->unread_count, 0);

       ik_output_buffer_t *output = ik_output_buffer_create(ctx, 200);
       separator->render(separator, output, 60, 0, 1);

       // Should NOT contain [mail:
       ck_assert(!output_contains(output, "[mail:"));
   }
   END_TEST

   // Test: No "[mail:0]" ever displayed
   START_TEST(test_mail_indicator_no_zero)
   {
       ck_assert_uint_eq(mock_agent->inbox->unread_count, 0);

       ik_output_buffer_t *output = ik_output_buffer_create(ctx, 200);
       separator->render(separator, output, 60, 0, 1);

       // Should NOT contain [mail:0]
       ck_assert(!output_contains(output, "[mail:0]"));
   }
   END_TEST

   // ========== Unread Mail Present Tests (Story 35) ==========

   // Test: Indicator shown with 1 unread message
   START_TEST(test_mail_indicator_shown_one_unread)
   {
       add_unread(mock_agent, 1);
       ck_assert_uint_eq(mock_agent->inbox->unread_count, 1);

       ik_output_buffer_t *output = ik_output_buffer_create(ctx, 200);
       separator->render(separator, output, 60, 0, 1);

       ck_assert(output_contains(output, "[mail:1]"));
   }
   END_TEST

   // Test: Indicator shows correct count with multiple unread
   START_TEST(test_mail_indicator_multiple_unread)
   {
       add_unread(mock_agent, 5);
       ck_assert_uint_eq(mock_agent->inbox->unread_count, 5);

       ik_output_buffer_t *output = ik_output_buffer_create(ctx, 200);
       separator->render(separator, output, 60, 0, 1);

       ck_assert(output_contains(output, "[mail:5]"));
   }
   END_TEST

   // Test: Indicator shows high count
   START_TEST(test_mail_indicator_high_count)
   {
       add_unread(mock_agent, 99);
       ck_assert_uint_eq(mock_agent->inbox->unread_count, 99);

       ik_output_buffer_t *output = ik_output_buffer_create(ctx, 200);
       separator->render(separator, output, 80, 0, 1);

       ck_assert(output_contains(output, "[mail:99]"));
   }
   END_TEST

   // Test: Indicator format is exactly [mail:N]
   START_TEST(test_mail_indicator_format)
   {
       add_unread(mock_agent, 2);

       ik_output_buffer_t *output = ik_output_buffer_create(ctx, 200);
       separator->render(separator, output, 60, 0, 1);

       // Must contain exact format with square brackets
       ck_assert(output_contains(output, "[mail:2]"));
       // Must have space before (per spec)
       ck_assert(output_contains(output, " [mail:2]"));
   }
   END_TEST

   // ========== Dynamic Update Tests ==========

   // Test: Indicator updates when count increases
   START_TEST(test_mail_indicator_updates_on_receive)
   {
       add_unread(mock_agent, 1);

       ik_output_buffer_t *output1 = ik_output_buffer_create(ctx, 200);
       separator->render(separator, output1, 60, 0, 1);
       ck_assert(output_contains(output1, "[mail:1]"));

       // Receive another message
       add_unread(mock_agent, 1);  // Now 2 total

       ik_output_buffer_t *output2 = ik_output_buffer_create(ctx, 200);
       separator->render(separator, output2, 60, 0, 1);
       ck_assert(output_contains(output2, "[mail:2]"));
   }
   END_TEST

   // Test: Indicator updates when message read
   START_TEST(test_mail_indicator_updates_on_read)
   {
       add_unread(mock_agent, 2);

       ik_output_buffer_t *output1 = ik_output_buffer_create(ctx, 200);
       separator->render(separator, output1, 60, 0, 1);
       ck_assert(output_contains(output1, "[mail:2]"));

       // Mark one as read
       ik_mail_msg_t *msg = ik_inbox_get_by_id(mock_agent->inbox, 1);
       ck_assert_ptr_nonnull(msg);
       ik_inbox_mark_read(mock_agent->inbox, msg);

       ik_output_buffer_t *output2 = ik_output_buffer_create(ctx, 200);
       separator->render(separator, output2, 60, 0, 1);
       ck_assert(output_contains(output2, "[mail:1]"));
   }
   END_TEST

   // Test: Indicator disappears when last message read
   START_TEST(test_mail_indicator_disappears_when_all_read)
   {
       add_unread(mock_agent, 1);

       ik_output_buffer_t *output1 = ik_output_buffer_create(ctx, 200);
       separator->render(separator, output1, 60, 0, 1);
       ck_assert(output_contains(output1, "[mail:1]"));

       // Mark as read
       ik_mail_msg_t *msg = ik_inbox_get_by_id(mock_agent->inbox, 1);
       ik_inbox_mark_read(mock_agent->inbox, msg);

       ik_output_buffer_t *output2 = ik_output_buffer_create(ctx, 200);
       separator->render(separator, output2, 60, 0, 1);
       ck_assert(!output_contains(output2, "[mail:"));
   }
   END_TEST

   // ========== Agent Switch Tests ==========

   // Test: Indicator reflects switched agent's count
   START_TEST(test_mail_indicator_on_agent_switch)
   {
       // Agent 0/ has 2 unread
       add_unread(mock_agent, 2);

       // Create agent 1/ with 0 unread
       ik_agent_ctx_t *agent1 = create_mock_agent(ctx, "1/");
       mock_repl->agents = talloc_realloc(ctx, mock_repl->agents, ik_agent_ctx_t *, 2);
       mock_repl->agents[1] = agent1;
       mock_repl->agent_count = 2;

       // Render with agent 0/ current
       ik_output_buffer_t *output1 = ik_output_buffer_create(ctx, 200);
       separator->render(separator, output1, 60, 0, 1);
       ck_assert(output_contains(output1, "[mail:2]"));

       // Switch to agent 1/
       mock_repl->current_agent = agent1;

       ik_output_buffer_t *output2 = ik_output_buffer_create(ctx, 200);
       separator->render(separator, output2, 60, 0, 1);
       ck_assert(!output_contains(output2, "[mail:"));
   }
   END_TEST

   // Test: Shows new agent's unread count after switch
   START_TEST(test_mail_indicator_new_agent_has_mail)
   {
       // Agent 0/ has 0 unread
       ck_assert_uint_eq(mock_agent->inbox->unread_count, 0);

       // Create agent 1/ with 3 unread
       ik_agent_ctx_t *agent1 = create_mock_agent(ctx, "1/");
       mock_repl->agents = talloc_realloc(ctx, mock_repl->agents, ik_agent_ctx_t *, 2);
       mock_repl->agents[1] = agent1;
       mock_repl->agent_count = 2;

       // Add messages to agent 1/
       for (int i = 0; i < 3; i++) {
           ik_mail_msg_t *msg = NULL;
           ik_mail_msg_create(ctx, i + 1, "0/", "1/", "Test", 1700000000 + i, false, &msg);
           ik_inbox_add(agent1->inbox, msg);
       }

       // Render with agent 0/ current
       ik_output_buffer_t *output1 = ik_output_buffer_create(ctx, 200);
       separator->render(separator, output1, 60, 0, 1);
       ck_assert(!output_contains(output1, "[mail:"));

       // Switch to agent 1/
       mock_repl->current_agent = agent1;

       ik_output_buffer_t *output2 = ik_output_buffer_create(ctx, 200);
       separator->render(separator, output2, 60, 0, 1);
       ck_assert(output_contains(output2, "[mail:3]"));
   }
   END_TEST

   // ========== Width Calculation Tests ==========

   // Test: Separator still fits in narrow terminal
   START_TEST(test_mail_indicator_narrow_terminal)
   {
       add_unread(mock_agent, 5);

       // Very narrow - 20 chars
       ik_output_buffer_t *output = ik_output_buffer_create(ctx, 200);
       separator->render(separator, output, 20, 0, 1);

       // Should still render something reasonable
       // (implementation may truncate dashes, prioritize content)
       ck_assert_uint_gt(output->size, 0);
   }
   END_TEST

   // Test: Indicator positioned after navigation context
   START_TEST(test_mail_indicator_position_after_nav)
   {
       add_unread(mock_agent, 1);

       ik_output_buffer_t *output = ik_output_buffer_create(ctx, 200);
       separator->render(separator, output, 80, 0, 1);

       // Find positions
       const char *agent_id_pos = memmem(output->data, output->size, "[0/]", 4);
       const char *mail_pos = memmem(output->data, output->size, "[mail:1]", 8);

       ck_assert_ptr_nonnull(agent_id_pos);
       ck_assert_ptr_nonnull(mail_pos);

       // Mail indicator should come AFTER agent ID
       ck_assert(mail_pos > agent_id_pos);
   }
   END_TEST

   // Test: Width consumed by indicator is accounted for
   START_TEST(test_mail_indicator_width_accounting)
   {
       add_unread(mock_agent, 1);

       // Render at width 60 with mail indicator
       ik_output_buffer_t *output = ik_output_buffer_create(ctx, 200);
       separator->render(separator, output, 60, 0, 1);

       // Output should be width + CRLF
       // Account for possible multibyte characters in dashes
       ck_assert_uint_ge(output->size, 60);  // At least width chars
   }
   END_TEST

   // ========== Edge Case Tests ==========

   // Test: No current agent (should not crash)
   START_TEST(test_mail_indicator_no_current_agent)
   {
       mock_repl->current_agent = NULL;

       ik_output_buffer_t *output = ik_output_buffer_create(ctx, 200);
       separator->render(separator, output, 60, 0, 1);

       // Should render without crash, no mail indicator
       ck_assert(!output_contains(output, "[mail:"));
   }
   END_TEST

   // Test: Agent without inbox (should not crash)
   START_TEST(test_mail_indicator_null_inbox)
   {
       // Temporarily null out inbox
       ik_inbox_t *saved = mock_agent->inbox;
       mock_agent->inbox = NULL;

       ik_output_buffer_t *output = ik_output_buffer_create(ctx, 200);
       separator->render(separator, output, 60, 0, 1);

       // Should render without crash, no mail indicator
       ck_assert(!output_contains(output, "[mail:"));

       mock_agent->inbox = saved;
   }
   END_TEST

   // Test: Very large unread count
   START_TEST(test_mail_indicator_large_count)
   {
       // Manually set high count (avoid creating 1000+ messages)
       mock_agent->inbox->unread_count = 999;

       ik_output_buffer_t *output = ik_output_buffer_create(ctx, 200);
       separator->render(separator, output, 100, 0, 1);

       ck_assert(output_contains(output, "[mail:999]"));
   }
   END_TEST

   // ========== Suite Configuration ==========

   static Suite *separator_mail_suite(void)
   {
       Suite *s = suite_create("SeparatorMail");

       TCase *tc_hidden = tcase_create("HiddenWhenZero");
       tcase_add_checked_fixture(tc_hidden, setup, teardown);
       tcase_add_test(tc_hidden, test_mail_indicator_hidden_empty_inbox);
       tcase_add_test(tc_hidden, test_mail_indicator_hidden_all_read);
       tcase_add_test(tc_hidden, test_mail_indicator_no_zero);
       suite_add_tcase(s, tc_hidden);

       TCase *tc_shown = tcase_create("ShownWhenUnread");
       tcase_add_checked_fixture(tc_shown, setup, teardown);
       tcase_add_test(tc_shown, test_mail_indicator_shown_one_unread);
       tcase_add_test(tc_shown, test_mail_indicator_multiple_unread);
       tcase_add_test(tc_shown, test_mail_indicator_high_count);
       tcase_add_test(tc_shown, test_mail_indicator_format);
       suite_add_tcase(s, tc_shown);

       TCase *tc_update = tcase_create("DynamicUpdate");
       tcase_add_checked_fixture(tc_update, setup, teardown);
       tcase_add_test(tc_update, test_mail_indicator_updates_on_receive);
       tcase_add_test(tc_update, test_mail_indicator_updates_on_read);
       tcase_add_test(tc_update, test_mail_indicator_disappears_when_all_read);
       suite_add_tcase(s, tc_update);

       TCase *tc_switch = tcase_create("AgentSwitch");
       tcase_add_checked_fixture(tc_switch, setup, teardown);
       tcase_add_test(tc_switch, test_mail_indicator_on_agent_switch);
       tcase_add_test(tc_switch, test_mail_indicator_new_agent_has_mail);
       suite_add_tcase(s, tc_switch);

       TCase *tc_width = tcase_create("WidthCalculation");
       tcase_add_checked_fixture(tc_width, setup, teardown);
       tcase_add_test(tc_width, test_mail_indicator_narrow_terminal);
       tcase_add_test(tc_width, test_mail_indicator_position_after_nav);
       tcase_add_test(tc_width, test_mail_indicator_width_accounting);
       suite_add_tcase(s, tc_width);

       TCase *tc_edge = tcase_create("EdgeCases");
       tcase_add_checked_fixture(tc_edge, setup, teardown);
       tcase_add_test(tc_edge, test_mail_indicator_no_current_agent);
       tcase_add_test(tc_edge, test_mail_indicator_null_inbox);
       tcase_add_test(tc_edge, test_mail_indicator_large_count);
       suite_add_tcase(s, tc_edge);

       return s;
   }

   int main(void)
   {
       Suite *s = separator_mail_suite();
       SRunner *sr = srunner_create(s);

       srunner_run_all(sr, CK_NORMAL);
       int number_failed = srunner_ntests_failed(sr);
       srunner_free(sr);

       return (number_failed == 0) ? 0 : 1;
   }
   ```

3. Update Makefile:
   - Verify `tests/unit/layer/separator_mail_test.c` is picked up by wildcard

4. Run `make check` - expect test failures (mail indicator not implemented)

### Green
1. Update `src/layer_wrappers.c` separator render function:

   ```c
   // Add includes at top
   #include "mail/inbox.h"

   // Helper to build mail indicator string
   // Returns empty string if no unread, otherwise " [mail:N] "
   static size_t build_mail_indicator(char *buf, size_t buf_size,
                                       const ik_agent_ctx_t *agent)
   {
       if (agent == NULL || agent->inbox == NULL) {
           buf[0] = '\0';
           return 0;
       }

       size_t unread = agent->inbox->unread_count;
       if (unread == 0) {
           buf[0] = '\0';
           return 0;
       }

       int written = snprintf(buf, buf_size, " [mail:%zu] ", unread);
       if (written < 0 || (size_t)written >= buf_size) {
           buf[0] = '\0';
           return 0;
       }

       return (size_t)written;
   }

   // Update separator_render() - add after navigation label
   static void separator_render(const ik_layer_t *layer,
                                ik_output_buffer_t *output,
                                size_t width,
                                size_t start_row,
                                size_t row_count)
   {
       (void)start_row;
       (void)row_count;

       ik_repl_ctx_t *repl = layer->user_ctx;

       // Build navigation label (existing code)
       char nav_label[64] = "";
       size_t nav_len = 0;
       if (repl != NULL && repl->current_agent != NULL) {
           nav_len = build_navigation_label(repl, nav_label, sizeof(nav_label));
       }

       // Build mail indicator
       char mail_indicator[32] = "";
       size_t mail_len = 0;
       if (repl != NULL && repl->current_agent != NULL) {
           mail_len = build_mail_indicator(mail_indicator, sizeof(mail_indicator),
                                            repl->current_agent);
       }

       // Calculate total content width
       size_t content_width = nav_len + mail_len;

       // Calculate dash distribution
       size_t available_for_dashes = 0;
       if (width > content_width) {
           available_for_dashes = width - content_width;
       }

       size_t left_dashes = available_for_dashes / 2;
       size_t right_dashes = available_for_dashes - left_dashes;

       // If we have a mail indicator, split right dashes around it
       size_t mid_dashes = 0;
       size_t trailing_dashes = right_dashes;
       if (mail_len > 0 && right_dashes > 0) {
           // Position mail indicator roughly in middle of right section
           mid_dashes = right_dashes / 2;
           trailing_dashes = right_dashes - mid_dashes;
       }

       // Render: left_dashes + nav_label + mid_dashes + mail + trailing_dashes
       for (size_t i = 0; i < left_dashes; i++) {
           ik_output_buffer_append(output, "-", 1);
       }

       if (nav_len > 0) {
           ik_output_buffer_append(output, nav_label, nav_len);
       }

       if (mail_len > 0) {
           for (size_t i = 0; i < mid_dashes; i++) {
               ik_output_buffer_append(output, "-", 1);
           }
           ik_output_buffer_append(output, mail_indicator, mail_len);
       }

       for (size_t i = 0; i < trailing_dashes; i++) {
           ik_output_buffer_append(output, "-", 1);
       }

       // End with CRLF
       ik_output_buffer_append(output, "\r\n", 2);
   }
   ```

2. Ensure inbox.h is included properly:
   - Add `#include "mail/inbox.h"` to layer_wrappers.c (after other project includes)

3. Run `make check` - expect all tests pass

### Refactor
1. Verify include order follows style guide:
   - Own header first (`layer_wrappers.h`)
   - Project headers next (alphabetically: `agent.h`, `mail/inbox.h`, `output_buffer.h`, `repl.h`)
   - System headers last

2. Verify `// comments` style used (not `/* */`)

3. Verify assert() statements have LCOV_EXCL_BR_LINE comments where used

4. Consider: Should indicator use Unicode box characters for brackets? (Decision: ASCII for v1)

5. Consider: Should very high counts use abbreviations (e.g., "99+")? (Decision: No, show actual count)

6. Run `make lint` - verify clean

7. Run `make coverage` - verify 100% coverage on new code

8. Run `make check-valgrind` - verify no memory leaks

9. Manual verification:
   - Start ikigai with debug mode
   - Send mail to current agent
   - Verify `[mail:N]` appears in separator
   - Read mail, verify count decreases
   - Read all mail, verify indicator disappears

## Post-conditions
- `make check` passes
- `make lint` passes
- `make coverage` shows 100% on modified code in `src/layer_wrappers.c`
- Separator displays `[mail:N]` when current agent has unread mail
- Format is exactly `[mail:{count}]` with surrounding spaces
- Indicator positioned after navigation context
- No indicator when unread_count == 0 (never shows `[mail:0]`)
- Indicator updates immediately when:
  - New mail received (count increases)
  - Message read (count decreases to 0 = disappears)
  - Agent switch (shows new agent's count)
- Width calculation accounts for indicator length
- Edge cases handled:
  - No current agent: no crash, no indicator
  - Null inbox: no crash, no indicator
  - Very narrow terminal: graceful degradation
  - High unread count: shows actual number
- Tests cover:
  - Hidden when zero (empty inbox, all read, no zero display)
  - Shown when unread (1, multiple, high count, format)
  - Dynamic updates (receive, read, disappear)
  - Agent switch (reflects new agent's count)
  - Width calculations (narrow, positioning)
  - Edge cases (null agent, null inbox, large count)

## Notes

### Separator Layout

The separator has several content sections in Phase 2:

```
───────── ↑- ←- [0/] →1/ ↓- ─────── [mail:2] ───────
|         |                |        |         |
left      navigation       mid      mail      trailing
dashes    context          dashes   indicator dashes
```

The mail indicator is positioned in the right portion, roughly centered between the navigation context and the end of the line.

### Render Trigger

The separator re-renders as part of the normal REPL render cycle. When the unread count changes, the next render will reflect the new count. Key triggers:
- `ik_inbox_add()` - Called when mail received
- `ik_inbox_mark_read()` - Called when message read
- Agent switch - Repl sets new current_agent, triggers render

No special notification or callback is needed; the separator reads the current state each render.

### Width Priority

When terminal is very narrow, content priority:
1. Agent ID `[0/]` - Most critical
2. Navigation arrows - User orientation
3. Mail indicator - Notification (can be omitted if no space)
4. Dashes - Filler (reduced as needed)

If width is less than essential content, some content may be truncated or omitted.

### Testing Strategy

Tests are organized by user story requirements:
1. **Story 36 tests**: Verify indicator hidden when unread_count == 0
2. **Story 35 tests**: Verify indicator shown with correct format
3. **Dynamic tests**: Verify immediate updates
4. **Switch tests**: Verify per-agent counts
5. **Edge cases**: Robust handling of unusual conditions

### Accessibility Considerations

For v1, the mail indicator uses the same visual style as the rest of the separator. Future enhancements might include:
- Color highlighting for mail indicator
- Screen reader annotations
- Configurable styling

### Relationship to Notification System

The mail indicator in the separator is separate from the notification injection system (mail-notification.md):
- **Separator indicator**: Passive visual status - always shows current count
- **Notification injection**: Active prompting - triggers agent to check mail

Both use `inbox->unread_count` but serve different purposes.

### Dependency Chain

```
mail-inbox-struct.md    (defines ik_inbox_t with unread_count)
        |
mail-agent-inbox.md     (adds inbox to agent context)
        |
separator-agent-id.md   (separator accesses agent via repl)
        |
mail-tool-handler.md    (all mail operations working)
        |
mail-separator-unread.md (this task - display unread indicator)
        |
mail-notification.md    (future - injects "you have mail" on IDLE)
```
