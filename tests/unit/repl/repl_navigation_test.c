#include "agent.h"
/**
 * @file repl_navigation_test.c
 * @brief Unit tests for REPL navigation and control actions
 */

#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "../../../src/agent.h"
#include "../../../src/repl.h"
#include "../../../src/repl_actions.h"
#include "../../../src/input.h"
#include "../../../src/shared.h"
#include "../../test_utils.h"

/* Test: Process ARROW_LEFT action */
START_TEST(test_repl_process_action_arrow_left) {
    void *ctx = talloc_new(NULL);

    ik_input_buffer_t *input_buf = NULL;
    res_t res;
    input_buf = ik_input_buffer_create(ctx);

    res = ik_input_buffer_insert_codepoint(input_buf, 'a');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'b');
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(repl);

    // Create agent context for display state
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    repl->current = agent;
    repl->current->input_buffer = input_buf;
    repl->quit = false;

    ik_input_action_t action = {.type = IK_INPUT_ARROW_LEFT};

    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_input_buffer_get_cursor_position(input_buf, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 1);
    ck_assert_uint_eq(grapheme_offset, 1);

    talloc_free(ctx);
}

END_TEST
/* Test: Process ARROW_RIGHT action */
START_TEST(test_repl_process_action_arrow_right)
{
    void *ctx = talloc_new(NULL);

    ik_input_buffer_t *input_buf = NULL;
    res_t res;
    input_buf = ik_input_buffer_create(ctx);

    res = ik_input_buffer_insert_codepoint(input_buf, 'a');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'b');
    ck_assert(is_ok(&res));

    res = ik_input_buffer_cursor_left(input_buf);
    ck_assert(is_ok(&res));
    res = ik_input_buffer_cursor_left(input_buf);
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(repl);

    // Create agent context for display state
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    repl->current = agent;
    repl->current->input_buffer = input_buf;
    repl->quit = false;

    ik_input_action_t action = {.type = IK_INPUT_ARROW_RIGHT};

    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_input_buffer_get_cursor_position(input_buf, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 1);
    ck_assert_uint_eq(grapheme_offset, 1);

    talloc_free(ctx);
}

END_TEST
/* Test: Process CTRL_C action sets quit flag */
START_TEST(test_repl_process_action_ctrl_c)
{
    void *ctx = talloc_new(NULL);

    ik_input_buffer_t *input_buf = NULL;
    res_t res;
    input_buf = ik_input_buffer_create(ctx);

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(repl);

    // Create agent context for display state
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    repl->current = agent;
    repl->current->input_buffer = input_buf;
    repl->quit = false;

    ik_input_action_t action = {.type = IK_INPUT_CTRL_C};

    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    ck_assert_int_eq(repl->quit, true);

    talloc_free(ctx);
}

END_TEST
/* Test: Process ARROW_LEFT at start (no-op) */
START_TEST(test_repl_process_action_left_at_start)
{
    void *ctx = talloc_new(NULL);

    ik_input_buffer_t *input_buf = NULL;
    res_t res;
    input_buf = ik_input_buffer_create(ctx);

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(repl);

    // Create agent context for display state
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    repl->current = agent;
    repl->current->input_buffer = input_buf;

    ik_input_action_t action = {.type = IK_INPUT_ARROW_LEFT};

    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_input_buffer_get_cursor_position(input_buf, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 0);
    ck_assert_uint_eq(grapheme_offset, 0);

    talloc_free(ctx);
}

END_TEST
/* Test: Process ARROW_RIGHT at end (no-op) */
START_TEST(test_repl_process_action_right_at_end)
{
    void *ctx = talloc_new(NULL);

    ik_input_buffer_t *input_buf = NULL;
    res_t res;
    input_buf = ik_input_buffer_create(ctx);

    res = ik_input_buffer_insert_codepoint(input_buf, 'a');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'b');
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(repl);

    // Create agent context for display state
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    repl->current = agent;
    repl->current->input_buffer = input_buf;

    ik_input_action_t action = {.type = IK_INPUT_ARROW_RIGHT};

    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res = ik_input_buffer_get_cursor_position(input_buf, &byte_offset, &grapheme_offset);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(byte_offset, 2);
    ck_assert_uint_eq(grapheme_offset, 2);

    talloc_free(ctx);
}

END_TEST
/* Test: Process UNKNOWN action (no-op) */
START_TEST(test_repl_process_action_unknown)
{
    void *ctx = talloc_new(NULL);

    ik_input_buffer_t *input_buf = NULL;
    res_t res;
    input_buf = ik_input_buffer_create(ctx);

    res = ik_input_buffer_insert_codepoint(input_buf, 'a');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'b');
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(repl);

    // Create agent context for display state
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    repl->current = agent;
    repl->current->input_buffer = input_buf;

    ik_input_action_t action = {.type = IK_INPUT_UNKNOWN};

    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    size_t len = 0;
    const char *text = ik_input_buffer_get_text(input_buf, &len);
    ck_assert_uint_eq(len, 2);
    ck_assert_int_eq(text[0], 'a');
    ck_assert_int_eq(text[1], 'b');

    talloc_free(ctx);
}

END_TEST

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
/* Test: NULL parameter assertions */
START_TEST(test_repl_process_action_null_repl_asserts)
{
    ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = 'a'};
    ik_repl_process_action(NULL, &action);
}

END_TEST
/* Test: NULL parameter assertions */
START_TEST(test_repl_process_action_null_action_asserts)
{
    void *ctx = talloc_new(NULL);
    ik_input_buffer_t *input_buf = NULL;
    input_buf = ik_input_buffer_create(ctx);
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    repl->current = agent;
    repl->current->input_buffer = input_buf;
    ik_repl_process_action(repl, NULL);
    talloc_free(ctx);
}

END_TEST
#endif

/* Test: Arrow up */
START_TEST(test_repl_process_action_arrow_up)
{
    void *ctx = talloc_new(NULL);

    ik_input_buffer_t *input_buf = NULL;
    res_t res;
    input_buf = ik_input_buffer_create(ctx);

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(repl);

    // Create agent context for display state
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    repl->current = agent;
    repl->current->input_buffer = input_buf;
    repl->quit = false;

    // Create minimal shared context for history access
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    shared->history = NULL;  // No history for this test
    repl->shared = shared;

    // Insert "a\nb" (line1\nline2)
    ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = 'a'};
    ik_repl_process_action(repl, &action);
    action.type = IK_INPUT_INSERT_NEWLINE;
    ik_repl_process_action(repl, &action);
    action.type = IK_INPUT_CHAR;
    action.codepoint = 'b';
    ik_repl_process_action(repl, &action);

    // Cursor is at byte 3 (after "a\nb"), grapheme 3
    size_t byte_offset = 999;
    size_t grapheme_offset = 999;
    ik_input_buffer_get_cursor_position(input_buf, &byte_offset, &grapheme_offset);
    ck_assert_uint_eq(byte_offset, 3);
    ck_assert_uint_eq(grapheme_offset, 3);

    // Move up
    action.type = IK_INPUT_ARROW_UP;
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Cursor should be at byte 1 (after "a"), grapheme 1
    ik_input_buffer_get_cursor_position(input_buf, &byte_offset, &grapheme_offset);
    ck_assert_uint_eq(byte_offset, 1);
    ck_assert_uint_eq(grapheme_offset, 1);

    talloc_free(ctx);
}

END_TEST
/* Test: Arrow down */
START_TEST(test_repl_process_action_arrow_down)
{
    void *ctx = talloc_new(NULL);

    ik_input_buffer_t *input_buf = NULL;
    res_t res;
    input_buf = ik_input_buffer_create(ctx);

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(repl);

    // Create agent context for display state
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    repl->current = agent;
    repl->current->input_buffer = input_buf;
    repl->quit = false;

    // Create minimal shared context for history access
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    shared->history = NULL;  // No history for this test
    repl->shared = shared;

    // Insert "a\nb" (line1\nline2)
    ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = 'a'};
    ik_repl_process_action(repl, &action);
    action.type = IK_INPUT_INSERT_NEWLINE;
    ik_repl_process_action(repl, &action);
    action.type = IK_INPUT_CHAR;
    action.codepoint = 'b';
    ik_repl_process_action(repl, &action);

    // Move cursor to start (byte 0)
    input_buf->cursor_byte_offset = 0;
    size_t text_len;
    const char *text = ik_input_buffer_get_text(input_buf, &text_len);
    ik_input_buffer_cursor_set_position(input_buf->cursor, text, text_len, 0);

    // Move down
    action.type = IK_INPUT_ARROW_DOWN;
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Cursor should be at byte 2 (after "\n", start of line2), grapheme 2
    size_t byte_offset = 999;
    size_t grapheme_offset = 999;
    ik_input_buffer_get_cursor_position(input_buf, &byte_offset, &grapheme_offset);
    ck_assert_uint_eq(byte_offset, 2);
    ck_assert_uint_eq(grapheme_offset, 2);

    talloc_free(ctx);
}

END_TEST

static Suite *repl_navigation_suite(void)
{
    Suite *s = suite_create("REPL_Navigation");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);
    tcase_set_timeout(tc_core, 30);
    tcase_set_timeout(tc_core, 30);
    tcase_set_timeout(tc_core, 30);

    tcase_add_test(tc_core, test_repl_process_action_arrow_left);
    tcase_add_test(tc_core, test_repl_process_action_arrow_right);
    tcase_add_test(tc_core, test_repl_process_action_arrow_up);
    tcase_add_test(tc_core, test_repl_process_action_arrow_down);
    tcase_add_test(tc_core, test_repl_process_action_ctrl_c);
    tcase_add_test(tc_core, test_repl_process_action_left_at_start);
    tcase_add_test(tc_core, test_repl_process_action_right_at_end);
    tcase_add_test(tc_core, test_repl_process_action_unknown);

    suite_add_tcase(s, tc_core);

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
    TCase *tc_assertions = tcase_create("Assertions");
    tcase_set_timeout(tc_assertions, 30);
    tcase_set_timeout(tc_assertions, 30);
    tcase_set_timeout(tc_assertions, 30);
    tcase_set_timeout(tc_assertions, 30);
    tcase_set_timeout(tc_assertions, 30); // Longer timeout for valgrind
    tcase_add_test_raise_signal(tc_assertions, test_repl_process_action_null_repl_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_repl_process_action_null_action_asserts, SIGABRT);
    suite_add_tcase(s, tc_assertions);
#endif

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = repl_navigation_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
