#include "agent.h"
/**
 * @file history_navigation_test.c
 * @brief Unit tests for REPL history navigation with up/down arrows
 */

#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "../../../src/agent.h"
#include "../../../src/shared.h"
#include "../../../src/history.h"
#include "../../../src/input.h"
#include "../../../src/input_buffer/core.h"
#include "../../../src/repl.h"
#include "../../../src/repl_actions.h"
#include "../../test_utils_helper.h"

/* Test: Up arrow from empty input loads last entry */
START_TEST(test_history_up_from_empty_input) {
    void *ctx = talloc_new(NULL);
    res_t res;

    // Create history
    ik_history_t *history = ik_history_create(ctx, 10);

    // Add some history entries
    res = ik_history_add(history, "first command");
    ck_assert(is_ok(&res));
    res = ik_history_add(history, "second command");
    ck_assert(is_ok(&res));
    res = ik_history_add(history, "third command");
    ck_assert(is_ok(&res));

    // Create agent context (with input_buffer)
    ik_agent_ctx_t *agent = NULL;
    res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    // Create REPL context
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    repl->current = agent;
    ck_assert_ptr_nonnull(repl);
    repl->shared->history = history;
    repl->quit = false;
    ik_input_buffer_t *input_buf = repl->current->input_buffer;

    // Press Up arrow (cursor is at position 0 in empty buffer)
    ik_input_action_t action = {.type = IK_INPUT_ARROW_UP};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: Input buffer should contain "third command" (last entry)
    size_t text_len = 0;
    const char *text = ik_input_buffer_get_text(input_buf, &text_len);
    ck_assert_uint_eq(text_len, 13);
    ck_assert_mem_eq(text, "third command", 13);

    // Verify: Cursor is at position 0
    size_t byte_offset = 999;
    size_t grapheme_offset = 999;
    res = ik_input_buffer_get_cursor_position(input_buf, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 0);
    ck_assert_uint_eq(grapheme_offset, 0);

    // Verify: History is in browsing mode
    ck_assert(ik_history_is_browsing(history));

    talloc_free(ctx);
}
END_TEST
/* Test: Multiple Up arrows navigate backward through history */
START_TEST(test_history_up_multiple_times) {
    void *ctx = talloc_new(NULL);
    res_t res;

    // Create history
    ik_history_t *history = ik_history_create(ctx, 10);

    // Add history entries
    res = ik_history_add(history, "first");
    ck_assert(is_ok(&res));
    res = ik_history_add(history, "second");
    ck_assert(is_ok(&res));
    res = ik_history_add(history, "third");
    ck_assert(is_ok(&res));

    // Create agent context (with input_buffer)
    ik_agent_ctx_t *agent = NULL;
    res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    // Create REPL context
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    repl->current = agent;
    ck_assert_ptr_nonnull(repl);
    repl->shared->history = history;
    repl->quit = false;
    ik_input_buffer_t *input_buf = repl->current->input_buffer;

    // Press Up arrow first time
    ik_input_action_t action = {.type = IK_INPUT_ARROW_UP};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: "third"
    size_t text_len = 0;
    const char *text = ik_input_buffer_get_text(input_buf, &text_len);
    ck_assert_uint_eq(text_len, 5);
    ck_assert_mem_eq(text, "third", 5);

    // Press Up arrow second time
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: "second"
    text = ik_input_buffer_get_text(input_buf, &text_len);
    ck_assert_uint_eq(text_len, 6);
    ck_assert_mem_eq(text, "second", 6);

    // Press Up arrow third time
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: "first"
    text = ik_input_buffer_get_text(input_buf, &text_len);
    ck_assert_uint_eq(text_len, 5);
    ck_assert_mem_eq(text, "first", 5);

    // Press Up arrow fourth time (at beginning)
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: Still "first" (no change at beginning)
    text = ik_input_buffer_get_text(input_buf, &text_len);
    ck_assert_uint_eq(text_len, 5);
    ck_assert_mem_eq(text, "first", 5);

    talloc_free(ctx);
}

END_TEST
/* Test: Down arrow restores pending input */
START_TEST(test_history_down_restores_pending) {
    void *ctx = talloc_new(NULL);
    res_t res;

    // Create history
    ik_history_t *history = ik_history_create(ctx, 10);

    // Add history entries
    res = ik_history_add(history, "first");
    ck_assert(is_ok(&res));
    res = ik_history_add(history, "second");
    ck_assert(is_ok(&res));

    // Create agent context (with input_buffer)
    ik_agent_ctx_t *agent = NULL;
    res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    // Create REPL context
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    repl->current = agent;
    ck_assert_ptr_nonnull(repl);
    repl->shared->history = history;
    repl->quit = false;
    ik_input_buffer_t *input_buf = repl->current->input_buffer;

    // Type some text (pending input)
    ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = 'h'};
    ik_repl_process_action(repl, &action);
    action.codepoint = 'i';
    ik_repl_process_action(repl, &action);

    // Move cursor to position 0
    input_buf->cursor_byte_offset = 0;
    size_t text_len;
    const char *text = ik_input_buffer_get_text(input_buf, &text_len);
    ik_input_buffer_cursor_set_position(input_buf->cursor, text, text_len, 0);

    // Press Up arrow (should start browsing and save "hi")
    action.type = IK_INPUT_ARROW_UP;
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: "second" is loaded
    text = ik_input_buffer_get_text(input_buf, &text_len);
    ck_assert_uint_eq(text_len, 6);
    ck_assert_mem_eq(text, "second", 6);

    // Press Down arrow (should move forward)
    action.type = IK_INPUT_ARROW_DOWN;
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: Pending input "hi" is restored
    text = ik_input_buffer_get_text(input_buf, &text_len);
    ck_assert_uint_eq(text_len, 2);
    ck_assert_mem_eq(text, "hi", 2);

    // Verify: No longer browsing
    ck_assert(!ik_history_is_browsing(history));

    // Verify: Cursor is at position 0
    size_t byte_offset = 999;
    size_t grapheme_offset = 999;
    res = ik_input_buffer_get_cursor_position(input_buf, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 0);
    ck_assert_uint_eq(grapheme_offset, 0);

    talloc_free(ctx);
}

END_TEST
/* Test: Up arrow with cursor not at zero does normal cursor movement */
START_TEST(test_history_up_with_cursor_not_at_zero) {
    void *ctx = talloc_new(NULL);
    res_t res;

    // Create history
    ik_history_t *history = ik_history_create(ctx, 10);

    // Add history entry
    res = ik_history_add(history, "history entry");
    ck_assert(is_ok(&res));

    // Create agent context (with input_buffer)
    ik_agent_ctx_t *agent = NULL;
    res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    // Create REPL context
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    repl->current = agent;
    ck_assert_ptr_nonnull(repl);
    repl->shared->history = history;
    repl->quit = false;
    ik_input_buffer_t *input_buf = repl->current->input_buffer;

    // Type multi-line text: "line1\nline2"
    ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = 'l'};
    ik_repl_process_action(repl, &action);
    action.codepoint = 'i';
    ik_repl_process_action(repl, &action);
    action.codepoint = 'n';
    ik_repl_process_action(repl, &action);
    action.codepoint = 'e';
    ik_repl_process_action(repl, &action);
    action.codepoint = '1';
    ik_repl_process_action(repl, &action);
    action.type = IK_INPUT_INSERT_NEWLINE;
    ik_repl_process_action(repl, &action);
    action.type = IK_INPUT_CHAR;
    action.codepoint = 'l';
    ik_repl_process_action(repl, &action);
    action.codepoint = 'i';
    ik_repl_process_action(repl, &action);
    action.codepoint = 'n';
    ik_repl_process_action(repl, &action);
    action.codepoint = 'e';
    ik_repl_process_action(repl, &action);
    action.codepoint = '2';
    ik_repl_process_action(repl, &action);

    // Cursor is now at end of line2 (byte 11)
    size_t byte_offset = 999;
    size_t grapheme_offset = 999;
    res = ik_input_buffer_get_cursor_position(input_buf, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 11);

    // Press Up arrow (should do normal cursor up, not history)
    action.type = IK_INPUT_ARROW_UP;
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: Cursor moved up to line1 (byte 5)
    res = ik_input_buffer_get_cursor_position(input_buf, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 5);

    // Verify: Input buffer text unchanged
    size_t text_len = 0;
    const char *text = ik_input_buffer_get_text(input_buf, &text_len);
    ck_assert_uint_eq(text_len, 11);
    ck_assert_mem_eq(text, "line1\nline2", 11);

    // Verify: Not browsing history
    ck_assert(!ik_history_is_browsing(history));

    talloc_free(ctx);
}

END_TEST
/* Test: History navigation takes precedence over multi-line at position 0 */
START_TEST(test_history_navigation_with_multiline) {
    void *ctx = talloc_new(NULL);
    res_t res;

    // Create history
    ik_history_t *history = ik_history_create(ctx, 10);

    // Add history entry
    res = ik_history_add(history, "from history");
    ck_assert(is_ok(&res));

    // Create agent context (with input_buffer)
    ik_agent_ctx_t *agent = NULL;
    res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    // Create REPL context
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    repl->current = agent;
    ck_assert_ptr_nonnull(repl);
    repl->shared->history = history;
    repl->quit = false;
    ik_input_buffer_t *input_buf = repl->current->input_buffer;

    // Type multi-line text: "l1\nl2"
    ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = 'l'};
    ik_repl_process_action(repl, &action);
    action.codepoint = '1';
    ik_repl_process_action(repl, &action);
    action.type = IK_INPUT_INSERT_NEWLINE;
    ik_repl_process_action(repl, &action);
    action.type = IK_INPUT_CHAR;
    action.codepoint = 'l';
    ik_repl_process_action(repl, &action);
    action.codepoint = '2';
    ik_repl_process_action(repl, &action);

    // Move cursor to position 0
    input_buf->cursor_byte_offset = 0;
    size_t text_len;
    const char *text = ik_input_buffer_get_text(input_buf, &text_len);
    ik_input_buffer_cursor_set_position(input_buf->cursor, text, text_len, 0);

    // Press Up arrow at position 0 (should do history, not multi-line cursor up)
    action.type = IK_INPUT_ARROW_UP;
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: History entry loaded
    text = ik_input_buffer_get_text(input_buf, &text_len);
    ck_assert_uint_eq(text_len, 12);
    ck_assert_mem_eq(text, "from history", 12);

    // Verify: Browsing history
    ck_assert(ik_history_is_browsing(history));

    talloc_free(ctx);
}

END_TEST
/* Test: Empty history handled gracefully */
START_TEST(test_history_empty) {
    void *ctx = talloc_new(NULL);
    res_t res;

    // Create empty history
    ik_history_t *history = ik_history_create(ctx, 10);

    // Create agent context (with input_buffer)
    ik_agent_ctx_t *agent = NULL;
    res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    // Create REPL context
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    repl->current = agent;
    ck_assert_ptr_nonnull(repl);
    repl->shared->history = history;
    repl->quit = false;
    ik_input_buffer_t *input_buf = repl->current->input_buffer;

    // Press Up arrow in empty history
    ik_input_action_t action = {.type = IK_INPUT_ARROW_UP};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: Input buffer still empty
    size_t text_len = 0;
    ik_input_buffer_get_text(input_buf, &text_len);
    ck_assert_uint_eq(text_len, 0);

    // Verify: Not browsing (empty history)
    ck_assert(!ik_history_is_browsing(history));

    talloc_free(ctx);
}

END_TEST

static Suite *history_navigation_suite(void)
{
    Suite *s = suite_create("History_Navigation");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    tcase_add_test(tc_core, test_history_up_from_empty_input);
    tcase_add_test(tc_core, test_history_up_multiple_times);
    tcase_add_test(tc_core, test_history_down_restores_pending);
    tcase_add_test(tc_core, test_history_up_with_cursor_not_at_zero);
    tcase_add_test(tc_core, test_history_navigation_with_multiline);
    tcase_add_test(tc_core, test_history_empty);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = history_navigation_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/repl/history_navigation_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
