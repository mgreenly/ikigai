/**
 * @file repl_scroll_test.c
 * @brief Unit tests for REPL mouse scroll actions
 */

#include <check.h>
#include "../../../src/shared.h"
#include <talloc.h>
#include "../../../src/repl.h"
#include "../../../src/repl_actions.h"
#include "../../../src/scrollback.h"
#include "../../../src/input.h"
#include "../../test_utils.h"

// Test: Scroll up increases viewport_offset by 1
START_TEST(test_scroll_up_increases_offset)
{
    void *ctx = talloc_new(NULL);

    // Create REPL context with mocked terminal
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    res_t res;
    term->screen_rows = 10;
    term->screen_cols = 80;

    ik_input_buffer_t *input_buf = ik_input_buffer_create(ctx);
    res = ik_input_buffer_insert_codepoint(input_buf, 'h');
    ck_assert(is_ok(&res));

    // Create scrollback with 20 lines (more than terminal)
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    for (int32_t i = 0; i < 20; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "line %" PRId32, i);
        res = ik_scrollback_append_line(scrollback, buf, strlen(buf));
        ck_assert(is_ok(&res));
    }

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->term = term;
    repl->input_buffer = input_buf;
    repl->scrollback = scrollback;
    repl->viewport_offset = 5;  // Start at offset 5

    // Process scroll up action
    ik_input_action_t action = {.type = IK_INPUT_SCROLL_UP};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Viewport offset should increase by 1
    ck_assert_uint_eq(repl->viewport_offset, 6);

    talloc_free(ctx);
}
END_TEST

// Test: Scroll down decreases viewport_offset by 1
START_TEST(test_scroll_down_decreases_offset)
{
    void *ctx = talloc_new(NULL);

    // Create REPL context with mocked terminal
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    res_t res;
    term->screen_rows = 10;
    term->screen_cols = 80;

    ik_input_buffer_t *input_buf = ik_input_buffer_create(ctx);
    res = ik_input_buffer_insert_codepoint(input_buf, 'h');
    ck_assert(is_ok(&res));

    // Create scrollback with 20 lines
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    for (int32_t i = 0; i < 20; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "line %" PRId32, i);
        res = ik_scrollback_append_line(scrollback, buf, strlen(buf));
        ck_assert(is_ok(&res));
    }

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->term = term;
    repl->input_buffer = input_buf;
    repl->scrollback = scrollback;
    repl->viewport_offset = 5;  // Start at offset 5

    // Process scroll down action
    ik_input_action_t action = {.type = IK_INPUT_SCROLL_DOWN};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Viewport offset should decrease by 1
    ck_assert_uint_eq(repl->viewport_offset, 4);

    talloc_free(ctx);
}
END_TEST

// Test: Scroll up clamps at max offset
START_TEST(test_scroll_up_clamps_at_max)
{
    void *ctx = talloc_new(NULL);

    // Create REPL context with mocked terminal
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    res_t res;
    term->screen_rows = 10;
    term->screen_cols = 80;

    ik_input_buffer_t *input_buf = ik_input_buffer_create(ctx);
    res = ik_input_buffer_insert_codepoint(input_buf, 'h');
    ck_assert(is_ok(&res));

    // Create scrollback with 20 lines
    // Document: 20 scrollback + 1 separator + 1 input = 22 rows
    // Max offset = 22 - 10 = 12
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    for (int32_t i = 0; i < 20; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "line %" PRId32, i);
        res = ik_scrollback_append_line(scrollback, buf, strlen(buf));
        ck_assert(is_ok(&res));
    }

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->term = term;
    repl->input_buffer = input_buf;
    repl->scrollback = scrollback;
    repl->viewport_offset = 12;  // Already at max

    // Process scroll up action
    ik_input_action_t action = {.type = IK_INPUT_SCROLL_UP};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Should stay at max (12), not go to 13
    ck_assert_uint_eq(repl->viewport_offset, 12);

    talloc_free(ctx);
}
END_TEST

// Test: Scroll down clamps at 0
START_TEST(test_scroll_down_clamps_at_zero)
{
    void *ctx = talloc_new(NULL);

    // Create REPL context with mocked terminal
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    res_t res;
    term->screen_rows = 10;
    term->screen_cols = 80;

    ik_input_buffer_t *input_buf = ik_input_buffer_create(ctx);
    res = ik_input_buffer_insert_codepoint(input_buf, 'h');
    ck_assert(is_ok(&res));

    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    for (int32_t i = 0; i < 20; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "line %" PRId32, i);
        res = ik_scrollback_append_line(scrollback, buf, strlen(buf));
        ck_assert(is_ok(&res));
    }

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->term = term;
    repl->input_buffer = input_buf;
    repl->scrollback = scrollback;
    repl->viewport_offset = 0;  // Already at bottom

    // Process scroll down action
    ik_input_action_t action = {.type = IK_INPUT_SCROLL_DOWN};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Should stay at 0
    ck_assert_uint_eq(repl->viewport_offset, 0);

    talloc_free(ctx);
}
END_TEST

// Test: Scroll actions don't affect input buffer content
START_TEST(test_scroll_preserves_input_buffer)
{
    void *ctx = talloc_new(NULL);

    // Create REPL context with mocked terminal
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    res_t res;
    term->screen_rows = 10;
    term->screen_cols = 80;

    ik_input_buffer_t *input_buf = ik_input_buffer_create(ctx);
    res = ik_input_buffer_insert_codepoint(input_buf, 'h');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'e');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'l');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'l');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'o');
    ck_assert(is_ok(&res));

    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    for (int32_t i = 0; i < 20; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "line %" PRId32, i);
        res = ik_scrollback_append_line(scrollback, buf, strlen(buf));
        ck_assert(is_ok(&res));
    }

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->term = term;
    repl->input_buffer = input_buf;
    repl->scrollback = scrollback;
    repl->viewport_offset = 5;

    // Get original input buffer content
    size_t original_len = 0;
    const char *original_text = ik_input_buffer_get_text(input_buf, &original_len);
    ck_assert_uint_eq(original_len, 5);
    ck_assert_mem_eq(original_text, "hello", 5);

    // Process scroll up action
    ik_input_action_t action_up = {.type = IK_INPUT_SCROLL_UP};
    res = ik_repl_process_action(repl, &action_up);
    ck_assert(is_ok(&res));

    // Check input buffer is unchanged
    size_t new_len = 0;
    const char *new_text = ik_input_buffer_get_text(input_buf, &new_len);
    ck_assert_uint_eq(new_len, 5);
    ck_assert_mem_eq(new_text, "hello", 5);

    // Process scroll down action
    ik_input_action_t action_down = {.type = IK_INPUT_SCROLL_DOWN};
    res = ik_repl_process_action(repl, &action_down);
    ck_assert(is_ok(&res));

    // Check input buffer is still unchanged
    new_text = ik_input_buffer_get_text(input_buf, &new_len);
    ck_assert_uint_eq(new_len, 5);
    ck_assert_mem_eq(new_text, "hello", 5);

    talloc_free(ctx);
}
END_TEST

// Test: Scroll up with empty input buffer (covers input_buffer_rows == 0 branch)
START_TEST(test_scroll_up_empty_input_buffer)
{
    void *ctx = talloc_new(NULL);

    // Create REPL context with mocked terminal
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    res_t res;
    term->screen_rows = 10;
    term->screen_cols = 80;

    // Create empty input buffer (input_buffer_rows will be 0)
    ik_input_buffer_t *input_buf = ik_input_buffer_create(ctx);

    // Create scrollback with 20 lines
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    for (int32_t i = 0; i < 20; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "line %" PRId32, i);
        res = ik_scrollback_append_line(scrollback, buf, strlen(buf));
        ck_assert(is_ok(&res));
    }

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->term = term;
    repl->input_buffer = input_buf;
    repl->scrollback = scrollback;
    repl->viewport_offset = 5;

    // Process scroll up action
    ik_input_action_t action = {.type = IK_INPUT_SCROLL_UP};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Viewport offset should increase by 1
    ck_assert_uint_eq(repl->viewport_offset, 6);

    talloc_free(ctx);
}
END_TEST

// Test: Scroll up when document fits entirely on screen (covers document_height <= screen_rows branch)
START_TEST(test_scroll_up_small_document)
{
    void *ctx = talloc_new(NULL);

    // Create REPL context with large terminal
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    res_t res;
    term->screen_rows = 50;  // Large terminal
    term->screen_cols = 80;

    ik_input_buffer_t *input_buf = ik_input_buffer_create(ctx);
    res = ik_input_buffer_insert_codepoint(input_buf, 'h');
    ck_assert(is_ok(&res));

    // Create small scrollback (only 3 lines)
    // Document: 3 scrollback + 1 separator + 1 input = 5 rows total
    // Screen: 50 rows
    // Since document (5) < screen (50), max_offset should be 0
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    for (int32_t i = 0; i < 3; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "line %" PRId32, i);
        res = ik_scrollback_append_line(scrollback, buf, strlen(buf));
        ck_assert(is_ok(&res));
    }

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->term = term;
    repl->input_buffer = input_buf;
    repl->scrollback = scrollback;
    repl->viewport_offset = 0;

    // Process scroll up action
    ik_input_action_t action = {.type = IK_INPUT_SCROLL_UP};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Offset should stay at 0 since document fits entirely on screen
    ck_assert_uint_eq(repl->viewport_offset, 0);

    talloc_free(ctx);
}
END_TEST

// Create test suite
static Suite *repl_scroll_suite(void)
{
    Suite *s = suite_create("REPL Scroll Actions");

    TCase *tc_scroll = tcase_create("Scroll");
    tcase_set_timeout(tc_scroll, 30);
    tcase_add_test(tc_scroll, test_scroll_up_increases_offset);
    tcase_add_test(tc_scroll, test_scroll_down_decreases_offset);
    tcase_add_test(tc_scroll, test_scroll_up_clamps_at_max);
    tcase_add_test(tc_scroll, test_scroll_down_clamps_at_zero);
    tcase_add_test(tc_scroll, test_scroll_preserves_input_buffer);
    tcase_add_test(tc_scroll, test_scroll_up_empty_input_buffer);
    tcase_add_test(tc_scroll, test_scroll_up_small_document);
    suite_add_tcase(s, tc_scroll);

    return s;
}

int main(void)
{
    Suite *s = repl_scroll_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
