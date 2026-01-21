#include "agent.h"
/**
 * @file completion_advanced_test.c
 * @brief Unit tests for advanced completion scenarios
 */

#include <check.h>
#include <talloc.h>
#include "../../../src/agent.h"
#include "../../../src/shared.h"
#include "../../../src/repl.h"
#include "../../../src/repl_actions.h"
#include "../../../src/input.h"
#include "../../../src/completion.h"
#include "../../../src/input_buffer/core.h"
#include "../../test_utils_helper.h"

/* Test: Typing dismisses completion on no match */
START_TEST(test_typing_dismisses_on_no_match) {
    void *ctx = talloc_new(NULL);

    // Create agent
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    // Create REPL context
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    ck_assert_ptr_nonnull(repl);
    repl->current = agent;
    repl->quit = false;

    // Create minimal shared context for test
    repl->shared = talloc_zero(repl, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(repl->shared);
    repl->shared->history = NULL;  /* No history for this test */

    // Type "/m" - completion is created automatically
    ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = '/'};
    ik_repl_process_action(repl, &action);
    action.codepoint = 'm';
    ik_repl_process_action(repl, &action);
    // Completion should now be active (no need for TAB)
    ck_assert_ptr_nonnull(repl->current->completion);

    // Type 'x' to create "/mx" (no matches)
    action.type = IK_INPUT_CHAR;
    action.codepoint = 'x';
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: Completion dismissed (no matches)
    ck_assert_ptr_null(repl->current->completion);

    talloc_free(ctx);
}
END_TEST
/* Test: Left/Right arrow dismisses completion */
START_TEST(test_left_right_arrow_dismisses) {
    void *ctx = talloc_new(NULL);

    // Create agent
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    // Create REPL context
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    ck_assert_ptr_nonnull(repl);
    repl->current = agent;
    repl->quit = false;

    // Create minimal shared context for test
    repl->shared = talloc_zero(repl, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(repl->shared);
    repl->shared->history = NULL;  /* No history for this test */

    // Type "/m" - completion is created automatically
    ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = '/'};
    ik_repl_process_action(repl, &action);
    action.codepoint = 'm';
    ik_repl_process_action(repl, &action);
    // Completion should now be active (no need for TAB)
    ck_assert_ptr_nonnull(repl->current->completion);

    // Press Left arrow
    action.type = IK_INPUT_ARROW_LEFT;
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: Completion dismissed
    ck_assert_ptr_null(repl->current->completion);

    // Clear the input and re-type to get completion back
    action.type = IK_INPUT_CTRL_U;
    ik_repl_process_action(repl, &action);
    action.type = IK_INPUT_CHAR;
    action.codepoint = '/';
    ik_repl_process_action(repl, &action);
    ck_assert_ptr_nonnull(repl->current->completion);

    // Press Right arrow
    action.type = IK_INPUT_ARROW_RIGHT;
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: Completion dismissed
    ck_assert_ptr_null(repl->current->completion);

    talloc_free(ctx);
}

END_TEST
/* Test: TAB on empty input does nothing */
START_TEST(test_tab_on_empty_input_no_op) {
    void *ctx = talloc_new(NULL);

    // Create agent
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    // Create REPL context
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    ck_assert_ptr_nonnull(repl);
    repl->current = agent;
    repl->current->completion = NULL;
    repl->quit = false;

    // Create minimal shared context for test
    repl->shared = talloc_zero(repl, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(repl->shared);
    repl->shared->history = NULL;  /* No history for this test */

    // Press TAB on empty input
    ik_input_action_t action = {.type = IK_INPUT_TAB};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: No completion created
    ck_assert_ptr_null(repl->current->completion);

    talloc_free(ctx);
}

END_TEST
/* Test: TAB on non-slash input does nothing */
START_TEST(test_tab_on_non_slash_no_op) {
    void *ctx = talloc_new(NULL);

    // Create agent
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    // Create REPL context
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    ck_assert_ptr_nonnull(repl);
    repl->current = agent;
    repl->current->completion = NULL;
    repl->quit = false;

    // Create minimal shared context for test
    repl->shared = talloc_zero(repl, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(repl->shared);
    repl->shared->history = NULL;  /* No history for this test */

    // Type "hello" (no leading slash)
    ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = 'h'};
    ik_repl_process_action(repl, &action);
    action.codepoint = 'e';
    ik_repl_process_action(repl, &action);

    // Press TAB
    action.type = IK_INPUT_TAB;
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: No completion created
    ck_assert_ptr_null(repl->current->completion);

    talloc_free(ctx);
}

END_TEST
/* Test: Cursor is at end of completed text after TAB acceptance */
START_TEST(test_cursor_at_end_after_tab_completion) {
    void *ctx = talloc_new(NULL);

    // Create agent
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    // Create REPL context
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    ck_assert_ptr_nonnull(repl);
    repl->current = agent;
    repl->quit = false;

    // Create minimal shared context for test
    repl->shared = talloc_zero(repl, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(repl->shared);
    repl->shared->history = NULL;  /* No history for this test */

    // Type "/m" - completion is created automatically
    ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = '/'};
    ik_repl_process_action(repl, &action);
    action.codepoint = 'm';
    ik_repl_process_action(repl, &action);

    // Verify completion is active
    ck_assert_ptr_nonnull(repl->current->completion);

    // Press TAB to accept selection
    action.type = IK_INPUT_TAB;
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: Completion is dismissed after accepting
    ck_assert_ptr_null(repl->current->completion);

    // Verify: Input buffer was updated with the selection
    size_t text_len = 0;
    const char *text = ik_input_buffer_get_text(repl->current->input_buffer, &text_len);
    ck_assert_uint_gt(text_len, 2);  // At least "/" + something
    ck_assert_int_eq(text[0], '/');

    // Verify: Cursor is at end of text (not at position 0)
    size_t cursor_byte = 0;
    size_t cursor_grapheme = 0;
    res = ik_input_buffer_get_cursor_position(repl->current->input_buffer, &cursor_byte, &cursor_grapheme);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_byte, text_len);  // Cursor should be at end of text

    talloc_free(ctx);
}

END_TEST

static Suite *completion_navigation_suite(void)
{
    Suite *s = suite_create("Completion Advanced");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    tcase_add_test(tc_core, test_typing_dismisses_on_no_match);
    tcase_add_test(tc_core, test_left_right_arrow_dismisses);
    tcase_add_test(tc_core, test_tab_on_empty_input_no_op);
    tcase_add_test(tc_core, test_tab_on_non_slash_no_op);
    tcase_add_test(tc_core, test_cursor_at_end_after_tab_completion);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = completion_navigation_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/repl/completion_advanced_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
