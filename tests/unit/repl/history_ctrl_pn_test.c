/**
 * @file history_ctrl_pn_test.c
 * @brief Tests for Ctrl+P/Ctrl+N history navigation
 *
 * Tests for dedicated history navigation keys (Ctrl+P = previous, Ctrl+N = next)
 * that work regardless of cursor position.
 */

#include <check.h>
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

/* Test: Ctrl+P starts browsing with empty input */
START_TEST(test_ctrl_p_starts_browsing_empty)
{
    void *ctx = talloc_new(NULL);

    // Create REPL context
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    term->screen_rows = 10;
    term->screen_cols = 80;

    ik_input_buffer_t *input_buf = ik_input_buffer_create(ctx);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    ik_history_t *history = ik_history_create(ctx, 10);
    res_t res = ik_history_add(history, "first entry");
    ck_assert(is_ok(&res));
    res = ik_history_add(history, "second entry");
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->term = term;
    repl->input_buffer = input_buf;
    repl->scrollback = scrollback;
    repl->shared->history = history;
    repl->viewport_offset = 0;

    // Press Ctrl+P - should start browsing and show most recent entry
    ik_input_action_t action = {.type = IK_INPUT_CTRL_P};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: Input buffer contains "second entry"
    size_t text_len = 0;
    const char *text = ik_input_buffer_get_text(input_buf, &text_len);
    ck_assert_uint_eq(text_len, 12);
    ck_assert_mem_eq(text, "second entry", 12);

    // Verify: Browsing history
    ck_assert(ik_history_is_browsing(history));

    talloc_free(ctx);
}
END_TEST

/* Test: Ctrl+P starts browsing with non-empty input */
START_TEST(test_ctrl_p_starts_browsing_with_text)
{
    void *ctx = talloc_new(NULL);

    // Create REPL context
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    term->screen_rows = 10;
    term->screen_cols = 80;

    ik_input_buffer_t *input_buf = ik_input_buffer_create(ctx);
    res_t res = ik_input_buffer_insert_codepoint(input_buf, 'h');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'e');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'l');
    ck_assert(is_ok(&res));

    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    ik_history_t *history = ik_history_create(ctx, 10);
    res = ik_history_add(history, "first entry");
    ck_assert(is_ok(&res));
    res = ik_history_add(history, "second entry");
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->term = term;
    repl->input_buffer = input_buf;
    repl->scrollback = scrollback;
    repl->shared->history = history;
    repl->viewport_offset = 0;

    // Press Ctrl+P - should save "hel" as pending and start browsing
    ik_input_action_t action = {.type = IK_INPUT_CTRL_P};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: Input buffer contains most recent entry
    size_t text_len = 0;
    const char *text = ik_input_buffer_get_text(input_buf, &text_len);
    ck_assert_uint_eq(text_len, 12);
    ck_assert_mem_eq(text, "second entry", 12);

    // Verify: Browsing history
    ck_assert(ik_history_is_browsing(history));

    talloc_free(ctx);
}
END_TEST

/* Test: Ctrl+P while already browsing moves to previous entry */
START_TEST(test_ctrl_p_moves_to_previous)
{
    void *ctx = talloc_new(NULL);

    // Create REPL context
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    term->screen_rows = 10;
    term->screen_cols = 80;

    ik_input_buffer_t *input_buf = ik_input_buffer_create(ctx);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    ik_history_t *history = ik_history_create(ctx, 10);
    res_t res = ik_history_add(history, "first entry");
    ck_assert(is_ok(&res));
    res = ik_history_add(history, "second entry");
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->term = term;
    repl->input_buffer = input_buf;
    repl->scrollback = scrollback;
    repl->shared->history = history;
    repl->viewport_offset = 0;

    // Press Ctrl+P once - should show "second entry"
    ik_input_action_t action = {.type = IK_INPUT_CTRL_P};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Press Ctrl+P again - should show "first entry"
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: Input buffer contains "first entry"
    size_t text_len = 0;
    const char *text = ik_input_buffer_get_text(input_buf, &text_len);
    ck_assert_uint_eq(text_len, 11);
    ck_assert_mem_eq(text, "first entry", 11);

    talloc_free(ctx);
}
END_TEST

/* Test: Ctrl+P at oldest entry returns NULL (no change) */
START_TEST(test_ctrl_p_at_oldest_entry)
{
    void *ctx = talloc_new(NULL);

    // Create REPL context
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    term->screen_rows = 10;
    term->screen_cols = 80;

    ik_input_buffer_t *input_buf = ik_input_buffer_create(ctx);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    ik_history_t *history = ik_history_create(ctx, 10);
    res_t res = ik_history_add(history, "only entry");
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->term = term;
    repl->input_buffer = input_buf;
    repl->scrollback = scrollback;
    repl->shared->history = history;
    repl->viewport_offset = 0;

    // Press Ctrl+P once - should show "only entry"
    ik_input_action_t action = {.type = IK_INPUT_CTRL_P};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    size_t text_len = 0;
    const char *text = ik_input_buffer_get_text(input_buf, &text_len);
    ck_assert_uint_eq(text_len, 10);
    ck_assert_mem_eq(text, "only entry", 10);

    // Press Ctrl+P again - should do nothing (already at oldest)
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: Input buffer unchanged
    text = ik_input_buffer_get_text(input_buf, &text_len);
    ck_assert_uint_eq(text_len, 10);
    ck_assert_mem_eq(text, "only entry", 10);

    talloc_free(ctx);
}
END_TEST

/* Test: Ctrl+N when not browsing does nothing */
START_TEST(test_ctrl_n_when_not_browsing)
{
    void *ctx = talloc_new(NULL);

    // Create REPL context
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    term->screen_rows = 10;
    term->screen_cols = 80;

    ik_input_buffer_t *input_buf = ik_input_buffer_create(ctx);
    res_t res = ik_input_buffer_insert_codepoint(input_buf, 'h');
    ck_assert(is_ok(&res));

    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    ik_history_t *history = ik_history_create(ctx, 10);
    res = ik_history_add(history, "entry");
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->term = term;
    repl->input_buffer = input_buf;
    repl->scrollback = scrollback;
    repl->shared->history = history;
    repl->viewport_offset = 0;

    // Press Ctrl+N - should do nothing
    ik_input_action_t action = {.type = IK_INPUT_CTRL_N};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: Input buffer unchanged
    size_t text_len = 0;
    const char *text = ik_input_buffer_get_text(input_buf, &text_len);
    ck_assert_uint_eq(text_len, 1);
    ck_assert_mem_eq(text, "h", 1);

    // Verify: Not browsing
    ck_assert(!ik_history_is_browsing(history));

    talloc_free(ctx);
}
END_TEST

/* Test: Ctrl+N while browsing moves to next entry */
START_TEST(test_ctrl_n_moves_to_next)
{
    void *ctx = talloc_new(NULL);

    // Create REPL context
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    term->screen_rows = 10;
    term->screen_cols = 80;

    ik_input_buffer_t *input_buf = ik_input_buffer_create(ctx);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    ik_history_t *history = ik_history_create(ctx, 10);
    res_t res = ik_history_add(history, "first entry");
    ck_assert(is_ok(&res));
    res = ik_history_add(history, "second entry");
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->term = term;
    repl->input_buffer = input_buf;
    repl->scrollback = scrollback;
    repl->shared->history = history;
    repl->viewport_offset = 0;

    // Press Ctrl+P twice to get to "first entry"
    ik_input_action_t action_p = {.type = IK_INPUT_CTRL_P};
    res = ik_repl_process_action(repl, &action_p);
    ck_assert(is_ok(&res));
    res = ik_repl_process_action(repl, &action_p);
    ck_assert(is_ok(&res));

    // Verify we're at "first entry"
    size_t text_len = 0;
    const char *text = ik_input_buffer_get_text(input_buf, &text_len);
    ck_assert_uint_eq(text_len, 11);
    ck_assert_mem_eq(text, "first entry", 11);

    // Press Ctrl+N - should move to "second entry"
    ik_input_action_t action_n = {.type = IK_INPUT_CTRL_N};
    res = ik_repl_process_action(repl, &action_n);
    ck_assert(is_ok(&res));

    // Verify: Input buffer contains "second entry"
    text = ik_input_buffer_get_text(input_buf, &text_len);
    ck_assert_uint_eq(text_len, 12);
    ck_assert_mem_eq(text, "second entry", 12);

    talloc_free(ctx);
}
END_TEST

/* Test: Ctrl+N at newest entry returns pending (empty if started empty) */
START_TEST(test_ctrl_n_at_newest_returns_pending)
{
    void *ctx = talloc_new(NULL);

    // Create REPL context
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    term->screen_rows = 10;
    term->screen_cols = 80;

    ik_input_buffer_t *input_buf = ik_input_buffer_create(ctx);
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    ik_history_t *history = ik_history_create(ctx, 10);
    res_t res = ik_history_add(history, "first entry");
    ck_assert(is_ok(&res));
    res = ik_history_add(history, "second entry");
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->term = term;
    repl->input_buffer = input_buf;
    repl->scrollback = scrollback;
    repl->shared->history = history;
    repl->viewport_offset = 0;

    // Press Ctrl+P twice to go back to "first entry"
    ik_input_action_t action_p = {.type = IK_INPUT_CTRL_P};
    res = ik_repl_process_action(repl, &action_p);
    ck_assert(is_ok(&res));
    res = ik_repl_process_action(repl, &action_p);
    ck_assert(is_ok(&res));

    // Verify we're at "first entry"
    size_t text_len = 0;
    const char *text = ik_input_buffer_get_text(input_buf, &text_len);
    ck_assert_uint_eq(text_len, 11);
    ck_assert_mem_eq(text, "first entry", 11);

    // Press Ctrl+N twice - first gets "second entry", second gets pending (empty string)
    ik_input_action_t action_n = {.type = IK_INPUT_CTRL_N};
    res = ik_repl_process_action(repl, &action_n);
    ck_assert(is_ok(&res));

    text = ik_input_buffer_get_text(input_buf, &text_len);
    ck_assert_uint_eq(text_len, 12);
    ck_assert_mem_eq(text, "second entry", 12);

    res = ik_repl_process_action(repl, &action_n);
    ck_assert(is_ok(&res));

    // Verify: Input buffer contains pending (empty string since we started browsing with empty input)
    text = ik_input_buffer_get_text(input_buf, &text_len);
    ck_assert_uint_eq(text_len, 0);

    talloc_free(ctx);
}
END_TEST

static Suite *history_ctrl_pn_suite(void)
{
    Suite *s = suite_create("History_Ctrl_PN");
    TCase *tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_ctrl_p_starts_browsing_empty);
    tcase_add_test(tc_core, test_ctrl_p_starts_browsing_with_text);
    tcase_add_test(tc_core, test_ctrl_p_moves_to_previous);
    tcase_add_test(tc_core, test_ctrl_p_at_oldest_entry);
    tcase_add_test(tc_core, test_ctrl_n_when_not_browsing);
    tcase_add_test(tc_core, test_ctrl_n_moves_to_next);
    tcase_add_test(tc_core, test_ctrl_n_at_newest_returns_pending);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = history_ctrl_pn_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
