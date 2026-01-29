#include "agent.h"
/**
 * @file history_navigation_test.c
 * @brief Unit tests for arrow key cursor movement (history navigation disabled)
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

/* Test: Arrow up from empty input does nothing */
START_TEST(test_arrow_up_from_empty_input) {
    void *ctx = talloc_new(NULL);
    res_t res;

    // Create history
    ik_history_t *history = ik_history_create(ctx, 10);

    // Add some history entries (should not be loaded)
    res = ik_history_add(history, "first command");
    ck_assert(is_ok(&res));
    res = ik_history_add(history, "second command");
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

    // Press Arrow Up (cursor at position 0 in empty buffer - no-op)
    ik_input_action_t action = {.type = IK_INPUT_ARROW_UP};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: Input buffer still empty
    size_t text_len = 0;
    ik_input_buffer_get_text(input_buf, &text_len);
    ck_assert_uint_eq(text_len, 0);

    // Verify: Not browsing history
    ck_assert(!ik_history_is_browsing(history));

    talloc_free(ctx);
}
END_TEST
/* Test: Arrow up in multi-line text moves cursor between lines */
START_TEST(test_arrow_up_multiline_cursor_movement) {
    void *ctx = talloc_new(NULL);
    res_t res;

    // Create history
    ik_history_t *history = ik_history_create(ctx, 10);

    // Add history entry (should not be loaded)
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

    // Press Arrow Up - should move cursor to line1
    action.type = IK_INPUT_ARROW_UP;
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: Cursor moved to line1 (byte 5)
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
/* Test: Arrow down in single-line text does nothing */
START_TEST(test_arrow_down_single_line) {
    void *ctx = talloc_new(NULL);
    res_t res;

    // Create history
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

    // Type single line of text
    ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = 'h'};
    ik_repl_process_action(repl, &action);
    action.codepoint = 'e';
    ik_repl_process_action(repl, &action);
    action.codepoint = 'l';
    ik_repl_process_action(repl, &action);
    action.codepoint = 'l';
    ik_repl_process_action(repl, &action);
    action.codepoint = 'o';
    ik_repl_process_action(repl, &action);

    // Press Arrow Down - should do nothing (single line)
    action.type = IK_INPUT_ARROW_DOWN;
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: Input buffer unchanged
    size_t text_len = 0;
    const char *text = ik_input_buffer_get_text(input_buf, &text_len);
    ck_assert_uint_eq(text_len, 5);
    ck_assert_mem_eq(text, "hello", 5);

    // Verify: Not browsing history
    ck_assert(!ik_history_is_browsing(history));

    talloc_free(ctx);
}

END_TEST

static Suite *history_navigation_suite(void)
{
    Suite *s = suite_create("History_Navigation");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    tcase_add_test(tc_core, test_arrow_up_from_empty_input);
    tcase_add_test(tc_core, test_arrow_up_multiline_cursor_movement);
    tcase_add_test(tc_core, test_arrow_down_single_line);

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
