/**
 * @file history_arrow_viewport_test.c
 * @brief Tests for arrow key viewport scrolling behavior
 *
 * When viewport_offset > 0, arrow up/down keys scroll the viewport
 * instead of navigating history. This allows scroll wheel (which sends
 * arrow sequences in alternate scroll mode) to scroll naturally.
 */

#include <check.h>
#include <inttypes.h>
#include <string.h>
#include <talloc.h>
#include "../../../src/shared.h"
#include "../../../src/history.h"
#include "../../../src/input.h"
#include "../../../src/input_buffer/core.h"
#include "../../../src/repl.h"
#include "../../../src/repl_actions.h"
#include "../../../src/scrollback.h"
#include "../../test_utils.h"

/* Test: Arrow up with viewport_offset > 0 scrolls viewport instead of history */
START_TEST(test_arrow_up_with_viewport_offset_scrolls)
{
    void *ctx = talloc_new(NULL);

    // Create REPL context with mocked terminal
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    term->screen_rows = 10;
    term->screen_cols = 80;

    ik_input_buffer_t *input_buf = ik_input_buffer_create(ctx);
    res_t res = ik_input_buffer_insert_codepoint(input_buf, 'h');
    ck_assert(is_ok(&res));

    // Create scrollback with enough content
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    for (int32_t i = 0; i < 20; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "line %" PRId32, i);
        res = ik_scrollback_append_line(scrollback, buf, strlen(buf));
        ck_assert(is_ok(&res));
    }

    ik_history_t *history = ik_history_create(ctx, 10);
    res = ik_history_add(history, "history entry");
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->term = term;
    repl->input_buffer = input_buf;
    repl->scrollback = scrollback;
    repl->history = history;
    repl->viewport_offset = 5;  // Already scrolled up

    // Press Arrow Up - should scroll viewport, not navigate history
    ik_input_action_t action = {.type = IK_INPUT_ARROW_UP};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: viewport_offset increased by 1
    ck_assert_uint_eq(repl->viewport_offset, 6);

    // Verify: Input buffer unchanged (still "h")
    size_t text_len = 0;
    const char *text = ik_input_buffer_get_text(input_buf, &text_len);
    ck_assert_uint_eq(text_len, 1);
    ck_assert_mem_eq(text, "h", 1);

    // Verify: Not browsing history
    ck_assert(!ik_history_is_browsing(history));

    talloc_free(ctx);
}
END_TEST

/* Test: Arrow down with viewport_offset > 0 scrolls viewport instead of history */
START_TEST(test_arrow_down_with_viewport_offset_scrolls)
{
    void *ctx = talloc_new(NULL);

    // Create REPL context with mocked terminal
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    term->screen_rows = 10;
    term->screen_cols = 80;

    ik_input_buffer_t *input_buf = ik_input_buffer_create(ctx);
    res_t res = ik_input_buffer_insert_codepoint(input_buf, 'h');
    ck_assert(is_ok(&res));

    // Create scrollback with enough content
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    for (int32_t i = 0; i < 20; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "line %" PRId32, i);
        res = ik_scrollback_append_line(scrollback, buf, strlen(buf));
        ck_assert(is_ok(&res));
    }

    ik_history_t *history = ik_history_create(ctx, 10);
    res = ik_history_add(history, "history entry");
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->term = term;
    repl->input_buffer = input_buf;
    repl->scrollback = scrollback;
    repl->history = history;
    repl->viewport_offset = 5;  // Already scrolled up

    // Press Arrow Down - should scroll viewport, not navigate history
    ik_input_action_t action = {.type = IK_INPUT_ARROW_DOWN};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: viewport_offset decreased by 1
    ck_assert_uint_eq(repl->viewport_offset, 4);

    // Verify: Input buffer unchanged (still "h")
    size_t text_len = 0;
    const char *text = ik_input_buffer_get_text(input_buf, &text_len);
    ck_assert_uint_eq(text_len, 1);
    ck_assert_mem_eq(text, "h", 1);

    // Verify: Not browsing history
    ck_assert(!ik_history_is_browsing(history));

    talloc_free(ctx);
}
END_TEST

/* Test: Arrow up with viewport_offset == 0 navigates history normally */
START_TEST(test_arrow_up_with_zero_offset_navigates_history)
{
    void *ctx = talloc_new(NULL);

    // Create REPL context with mocked terminal
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    term->screen_rows = 10;
    term->screen_cols = 80;

    ik_input_buffer_t *input_buf = ik_input_buffer_create(ctx);

    // Create scrollback
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    ik_history_t *history = ik_history_create(ctx, 10);
    res_t res = ik_history_add(history, "history entry");
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->term = term;
    repl->input_buffer = input_buf;
    repl->scrollback = scrollback;
    repl->history = history;
    repl->viewport_offset = 0;  // At bottom

    // Press Arrow Up - should navigate history
    ik_input_action_t action = {.type = IK_INPUT_ARROW_UP};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: Input buffer contains history entry
    size_t text_len = 0;
    const char *text = ik_input_buffer_get_text(input_buf, &text_len);
    ck_assert_uint_eq(text_len, 13);
    ck_assert_mem_eq(text, "history entry", 13);

    // Verify: Browsing history
    ck_assert(ik_history_is_browsing(history));

    // Verify: viewport_offset still 0
    ck_assert_uint_eq(repl->viewport_offset, 0);

    talloc_free(ctx);
}
END_TEST

/* Test: Arrow down when scrolled to bottom then returns to offset 0, next arrow down triggers history */
START_TEST(test_arrow_down_to_bottom_then_history)
{
    void *ctx = talloc_new(NULL);

    // Create REPL context with mocked terminal
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    term->screen_rows = 10;
    term->screen_cols = 80;

    ik_input_buffer_t *input_buf = ik_input_buffer_create(ctx);
    res_t res = ik_input_buffer_insert_codepoint(input_buf, 'h');
    ck_assert(is_ok(&res));

    // Create scrollback with enough content
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    for (int32_t i = 0; i < 20; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "line %" PRId32, i);
        res = ik_scrollback_append_line(scrollback, buf, strlen(buf));
        ck_assert(is_ok(&res));
    }

    ik_history_t *history = ik_history_create(ctx, 10);
    res = ik_history_add(history, "first");
    ck_assert(is_ok(&res));
    res = ik_history_add(history, "second");
    ck_assert(is_ok(&res));

    // Start with history browsing and viewport offset
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->term = term;
    repl->input_buffer = input_buf;
    repl->scrollback = scrollback;
    repl->history = history;
    repl->viewport_offset = 1;  // Scrolled up by 1

    // Press Arrow Down - should scroll viewport down to 0
    ik_input_action_t action = {.type = IK_INPUT_ARROW_DOWN};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: viewport_offset is now 0
    ck_assert_uint_eq(repl->viewport_offset, 0);

    // Verify: Input buffer unchanged (still "h")
    size_t text_len = 0;
    const char *text = ik_input_buffer_get_text(input_buf, &text_len);
    ck_assert_uint_eq(text_len, 1);
    ck_assert_mem_eq(text, "h", 1);

    // Now move cursor to position 0 to enable history navigation
    input_buf->cursor_byte_offset = 0;
    ik_input_buffer_cursor_set_position(input_buf->cursor, text, text_len, 0);

    // Press Arrow Down again - now should do nothing (not browsing history)
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: Input buffer still "h" (cursor down in single line does nothing)
    text = ik_input_buffer_get_text(input_buf, &text_len);
    ck_assert_uint_eq(text_len, 1);
    ck_assert_mem_eq(text, "h", 1);

    talloc_free(ctx);
}
END_TEST

static Suite *history_arrow_viewport_suite(void)
{
    Suite *s = suite_create("History_Arrow_Viewport");
    TCase *tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_arrow_up_with_viewport_offset_scrolls);
    tcase_add_test(tc_core, test_arrow_down_with_viewport_offset_scrolls);
    tcase_add_test(tc_core, test_arrow_up_with_zero_offset_navigates_history);
    tcase_add_test(tc_core, test_arrow_down_to_bottom_then_history);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = history_arrow_viewport_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
