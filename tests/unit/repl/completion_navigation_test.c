/**
 * @file completion_navigation_test.c
 * @brief Unit tests for completion navigation (TAB and arrow key interaction)
 */

#include <check.h>
#include <talloc.h>
#include "../../../src/repl.h"
#include "../../../src/repl_actions.h"
#include "../../../src/input.h"
#include "../../../src/completion.h"
#include "../../../src/input_buffer/core.h"
#include "../../test_utils.h"

/* Test: Typing "/" triggers completion automatically */
START_TEST(test_tab_triggers_completion)
{
    void *ctx = talloc_new(NULL);

    // Create input buffer
    ik_input_buffer_t *input_buf = ik_input_buffer_create(ctx);

    // Create REPL context
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->input_buffer = input_buf;
    repl->completion = NULL;
    repl->quit = false;

    // Type "/" (should trigger completion automatically)
    ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = '/'};
    res_t res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: Completion was created after typing "/"
    ck_assert_ptr_nonnull(repl->completion);
    ck_assert(repl->completion->count > 0);

    // Type "m" to filter
    action.codepoint = 'm';
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: Completion was updated with filtered matches
    ck_assert_ptr_nonnull(repl->completion);
    ck_assert(repl->completion->count > 0);

    // Verify: Prefix is stored
    ck_assert_str_eq(repl->completion->prefix, "/m");

    talloc_free(ctx);
}
END_TEST

/* Test: TAB accepts selection and dismisses completion */
START_TEST(test_tab_accepts_selection)
{
    void *ctx = talloc_new(NULL);

    // Create input buffer
    ik_input_buffer_t *input_buf = ik_input_buffer_create(ctx);

    // Create REPL context
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->input_buffer = input_buf;
    repl->quit = false;

    // Type "/m" - completion is created automatically
    ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = '/'};
    ik_repl_process_action(repl, &action);
    action.codepoint = 'm';
    ik_repl_process_action(repl, &action);

    // Verify completion is active
    ck_assert_ptr_nonnull(repl->completion);

    // Press TAB to accept selection
    action.type = IK_INPUT_TAB;
    res_t res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: Completion is dismissed after accepting
    ck_assert_ptr_null(repl->completion);

    // Verify: Input buffer was updated with the selection
    size_t text_len = 0;
    const char *text = ik_input_buffer_get_text(input_buf, &text_len);
    ck_assert_uint_gt(text_len, 2);  // At least "/" + something
    ck_assert_int_eq(text[0], '/');

    talloc_free(ctx);
}
END_TEST

/* Test: Arrow up changes selection to previous candidate */
START_TEST(test_arrow_up_changes_selection)
{
    void *ctx = talloc_new(NULL);

    // Create input buffer
    ik_input_buffer_t *input_buf = ik_input_buffer_create(ctx);

    // Create REPL context
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->input_buffer = input_buf;
    repl->quit = false;

    // Type "/m" - completion is created automatically
    ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = '/'};
    ik_repl_process_action(repl, &action);
    action.codepoint = 'm';
    ik_repl_process_action(repl, &action);
    // Completion should now be active (no need for TAB)
    ck_assert_ptr_nonnull(repl->completion);

    // Ensure we have multiple candidates
    ck_assert(repl->completion->count > 1);

    // Get initial selection (should be index 0)
    ck_assert_uint_eq(repl->completion->current, 0);
    const char *first_candidate = ik_completion_get_current(repl->completion);

    // Press Arrow Up (should wrap to last candidate)
    action.type = IK_INPUT_ARROW_UP;
    res_t res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: Selection changed to last candidate
    ck_assert_uint_eq(repl->completion->current, repl->completion->count - 1);
    const char *last_candidate = ik_completion_get_current(repl->completion);
    ck_assert_str_ne(last_candidate, first_candidate);

    // Verify: Completion still active
    ck_assert_ptr_nonnull(repl->completion);

    talloc_free(ctx);
}
END_TEST

/* Test: Arrow down changes selection to next candidate */
START_TEST(test_arrow_down_changes_selection)
{
    void *ctx = talloc_new(NULL);

    // Create input buffer
    ik_input_buffer_t *input_buf = ik_input_buffer_create(ctx);

    // Create REPL context
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->input_buffer = input_buf;
    repl->quit = false;

    // Type "/m" - completion is created automatically
    ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = '/'};
    ik_repl_process_action(repl, &action);
    action.codepoint = 'm';
    ik_repl_process_action(repl, &action);
    // Completion should now be active (no need for TAB)
    ck_assert_ptr_nonnull(repl->completion);

    // Ensure we have multiple candidates
    ck_assert(repl->completion->count > 1);

    // Get initial selection (index 0)
    ck_assert_uint_eq(repl->completion->current, 0);
    const char *first_candidate = ik_completion_get_current(repl->completion);

    // Press Arrow Down
    action.type = IK_INPUT_ARROW_DOWN;
    res_t res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: Selection moved to next (index 1)
    ck_assert_uint_eq(repl->completion->current, 1);
    const char *second_candidate = ik_completion_get_current(repl->completion);
    ck_assert_str_ne(second_candidate, first_candidate);

    // Verify: Completion still active
    ck_assert_ptr_nonnull(repl->completion);

    talloc_free(ctx);
}
END_TEST

/* Test: Escape dismisses completion without accepting */
START_TEST(test_escape_dismisses_completion)
{
    void *ctx = talloc_new(NULL);

    // Create input buffer
    ik_input_buffer_t *input_buf = ik_input_buffer_create(ctx);

    // Create REPL context
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->input_buffer = input_buf;
    repl->quit = false;

    // Type "/m" - completion is created automatically
    ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = '/'};
    ik_repl_process_action(repl, &action);
    action.codepoint = 'm';
    ik_repl_process_action(repl, &action);
    // Completion should now be active (no need for TAB)
    ck_assert_ptr_nonnull(repl->completion);

    // Verify original input before Escape
    size_t original_len = 0;
    ik_input_buffer_get_text(input_buf, &original_len);
    ck_assert_uint_eq(original_len, 2); // "/m"

    // Press Escape
    action.type = IK_INPUT_ESCAPE;
    res_t res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: Completion dismissed
    ck_assert_ptr_null(repl->completion);

    // Verify: Input buffer unchanged
    size_t text_len = 0;
    const char *text = ik_input_buffer_get_text(input_buf, &text_len);
    ck_assert_uint_eq(text_len, 2);
    ck_assert_mem_eq(text, "/m", 2);

    talloc_free(ctx);
}
END_TEST

/* Test: Typing updates completion dynamically */
START_TEST(test_typing_updates_completion)
{
    void *ctx = talloc_new(NULL);

    // Create input buffer
    ik_input_buffer_t *input_buf = ik_input_buffer_create(ctx);

    // Create REPL context
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->input_buffer = input_buf;
    repl->quit = false;

    // Type "/m" - completion is created automatically
    ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = '/'};
    ik_repl_process_action(repl, &action);
    action.codepoint = 'm';
    ik_repl_process_action(repl, &action);
    // Completion should now be active (no need for TAB)
    ck_assert_ptr_nonnull(repl->completion);

    size_t initial_count = repl->completion->count;

    // Type 'o' to narrow to "/mo" (should match "model" only)
    action.type = IK_INPUT_CHAR;
    action.codepoint = 'o';
    res_t res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: Completion was updated (new prefix)
    ck_assert_ptr_nonnull(repl->completion);
    ck_assert_str_eq(repl->completion->prefix, "/mo");

    // Verify: Candidate count changed (narrower match)
    ck_assert(repl->completion->count < initial_count);

    talloc_free(ctx);
}
END_TEST

/* Test: Typing dismisses completion on no match */
START_TEST(test_typing_dismisses_on_no_match)
{
    void *ctx = talloc_new(NULL);

    // Create input buffer
    ik_input_buffer_t *input_buf = ik_input_buffer_create(ctx);

    // Create REPL context
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->input_buffer = input_buf;
    repl->quit = false;

    // Type "/m" - completion is created automatically
    ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = '/'};
    ik_repl_process_action(repl, &action);
    action.codepoint = 'm';
    ik_repl_process_action(repl, &action);
    // Completion should now be active (no need for TAB)
    ck_assert_ptr_nonnull(repl->completion);

    // Type 'x' to create "/mx" (no matches)
    action.type = IK_INPUT_CHAR;
    action.codepoint = 'x';
    res_t res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: Completion dismissed (no matches)
    ck_assert_ptr_null(repl->completion);

    talloc_free(ctx);
}
END_TEST

/* Test: Left/Right arrow dismisses completion */
START_TEST(test_left_right_arrow_dismisses)
{
    void *ctx = talloc_new(NULL);

    // Create input buffer
    ik_input_buffer_t *input_buf = ik_input_buffer_create(ctx);

    // Create REPL context
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->input_buffer = input_buf;
    repl->quit = false;

    // Type "/m" - completion is created automatically
    ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = '/'};
    ik_repl_process_action(repl, &action);
    action.codepoint = 'm';
    ik_repl_process_action(repl, &action);
    // Completion should now be active (no need for TAB)
    ck_assert_ptr_nonnull(repl->completion);

    // Press Left arrow
    action.type = IK_INPUT_ARROW_LEFT;
    res_t res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: Completion dismissed
    ck_assert_ptr_null(repl->completion);

    // Clear the input and re-type to get completion back
    action.type = IK_INPUT_CTRL_U;
    ik_repl_process_action(repl, &action);
    action.type = IK_INPUT_CHAR;
    action.codepoint = '/';
    ik_repl_process_action(repl, &action);
    ck_assert_ptr_nonnull(repl->completion);

    // Press Right arrow
    action.type = IK_INPUT_ARROW_RIGHT;
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: Completion dismissed
    ck_assert_ptr_null(repl->completion);

    talloc_free(ctx);
}
END_TEST

/* Test: TAB on empty input does nothing */
START_TEST(test_tab_on_empty_input_no_op)
{
    void *ctx = talloc_new(NULL);

    // Create input buffer (empty)
    ik_input_buffer_t *input_buf = ik_input_buffer_create(ctx);

    // Create REPL context
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->input_buffer = input_buf;
    repl->completion = NULL;
    repl->quit = false;

    // Press TAB on empty input
    ik_input_action_t action = {.type = IK_INPUT_TAB};
    res_t res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: No completion created
    ck_assert_ptr_null(repl->completion);

    talloc_free(ctx);
}
END_TEST

/* Test: TAB on non-slash input does nothing */
START_TEST(test_tab_on_non_slash_no_op)
{
    void *ctx = talloc_new(NULL);

    // Create input buffer
    ik_input_buffer_t *input_buf = ik_input_buffer_create(ctx);

    // Create REPL context
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->input_buffer = input_buf;
    repl->completion = NULL;
    repl->quit = false;

    // Type "hello" (no leading slash)
    ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = 'h'};
    ik_repl_process_action(repl, &action);
    action.codepoint = 'e';
    ik_repl_process_action(repl, &action);

    // Press TAB
    action.type = IK_INPUT_TAB;
    res_t res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: No completion created
    ck_assert_ptr_null(repl->completion);

    talloc_free(ctx);
}
END_TEST

/* Test: Cursor is at end of completed text after TAB acceptance */
START_TEST(test_cursor_at_end_after_tab_completion)
{
    void *ctx = talloc_new(NULL);

    // Create input buffer
    ik_input_buffer_t *input_buf = ik_input_buffer_create(ctx);

    // Create REPL context
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->input_buffer = input_buf;
    repl->quit = false;

    // Type "/m" - completion is created automatically
    ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = '/'};
    ik_repl_process_action(repl, &action);
    action.codepoint = 'm';
    ik_repl_process_action(repl, &action);

    // Verify completion is active
    ck_assert_ptr_nonnull(repl->completion);

    // Press TAB to accept selection
    action.type = IK_INPUT_TAB;
    res_t res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: Completion is dismissed after accepting
    ck_assert_ptr_null(repl->completion);

    // Verify: Input buffer was updated with the selection
    size_t text_len = 0;
    const char *text = ik_input_buffer_get_text(input_buf, &text_len);
    ck_assert_uint_gt(text_len, 2);  // At least "/" + something
    ck_assert_int_eq(text[0], '/');

    // Verify: Cursor is at end of text (not at position 0)
    size_t cursor_byte = 0;
    size_t cursor_grapheme = 0;
    res = ik_input_buffer_get_cursor_position(input_buf, &cursor_byte, &cursor_grapheme);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(cursor_byte, text_len);  // Cursor should be at end of text

    talloc_free(ctx);
}
END_TEST

static Suite *completion_navigation_suite(void)
{
    Suite *s = suite_create("Completion_Navigation");
    TCase *tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_tab_triggers_completion);
    tcase_add_test(tc_core, test_tab_accepts_selection);
    tcase_add_test(tc_core, test_arrow_up_changes_selection);
    tcase_add_test(tc_core, test_arrow_down_changes_selection);
    tcase_add_test(tc_core, test_escape_dismisses_completion);
    tcase_add_test(tc_core, test_typing_updates_completion);
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

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
