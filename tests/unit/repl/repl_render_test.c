/**
 * @file repl_render_test.c
 * @brief Unit tests for REPL render_frame function (basic rendering)
 */

#include <check.h>
#include <signal.h>
#include <talloc.h>
#include <string.h>
#include <stdio.h>
#include "../../../src/repl.h"
#include "../../../src/render.h"
#include "../../../src/layer.h"
#include "../../../src/layer_wrappers.h"
#include "../../../src/byte_array.h"
#include "../../test_utils.h"

// Mock write tracking
static int32_t mock_write_calls = 0;
static char mock_write_buffer[4096];
static size_t mock_write_buffer_len = 0;
static bool mock_write_should_fail = false;

// Mock write wrapper declaration
ssize_t posix_write_(int fd, const void *buf, size_t count);

// Mock write wrapper for testing
ssize_t posix_write_(int fd, const void *buf, size_t count)
{
    (void)fd;
    mock_write_calls++;

    if (mock_write_should_fail) {
        return -1;  // Simulate write failure
    }

    if (mock_write_buffer_len + count < sizeof(mock_write_buffer)) {
        memcpy(mock_write_buffer + mock_write_buffer_len, buf, count);
        mock_write_buffer_len += count;
    }
    return (ssize_t)count;
}

/* Test: Render frame with empty input buffer */
START_TEST(test_repl_render_frame_empty_input_buffer) {
    void *ctx = talloc_new(NULL);

    // Manually construct REPL context components
    ik_input_buffer_t *input_buf = NULL;
    res_t res;
    input_buf = ik_input_buffer_create(ctx);

    ik_render_ctx_t *render = NULL;
    res = ik_render_create(ctx, 24, 80, 1, &render);  // Mock terminal: 24x80, fd=1
    ck_assert(is_ok(&res));

    // Create term context (required by ik_repl_render_frame)
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    term->screen_rows = 24;
    term->screen_cols = 80;

    // Create scrollback (required by ik_repl_render_frame)
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    // Create minimal REPL context
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->input_buffer = input_buf;
    repl->render = render;
    repl->term = term;
    repl->scrollback = scrollback;
    repl->viewport_offset = 0;

    // Reset mock state
    mock_write_calls = 0;
    mock_write_buffer_len = 0;

    /* Call render_frame - this should succeed with empty input buffer */
    res = ik_repl_render_frame(repl);
    ck_assert(is_ok(&res));

    /* Verify write was called (rendering happened) */
    ck_assert_int_gt(mock_write_calls, 0);

    /* Cleanup */
    talloc_free(ctx);
}
END_TEST
/* Test: Render frame with multi-line text */
START_TEST(test_repl_render_frame_multiline)
{
    void *ctx = talloc_new(NULL);

    // Manually construct REPL context components
    ik_input_buffer_t *input_buf = NULL;
    res_t res;
    input_buf = ik_input_buffer_create(ctx);

    // Insert multi-line text
    res = ik_input_buffer_insert_codepoint(input_buf, 'h');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'i');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_newline(input_buf);
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'b');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'y');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'e');
    ck_assert(is_ok(&res));

    ik_render_ctx_t *render = NULL;
    res = ik_render_create(ctx, 24, 80, 1, &render);
    ck_assert(is_ok(&res));

    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    term->screen_rows = 24;
    term->screen_cols = 80;

    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->input_buffer = input_buf;
    repl->render = render;
    repl->term = term;
    repl->scrollback = scrollback;
    repl->viewport_offset = 0;

    mock_write_calls = 0;
    mock_write_buffer_len = 0;

    /* Call render_frame - should succeed with multi-line text */
    res = ik_repl_render_frame(repl);
    ck_assert(is_ok(&res));
    ck_assert_int_gt(mock_write_calls, 0);

    talloc_free(ctx);
}

END_TEST
/* Test: Render frame with cursor at various positions */
START_TEST(test_repl_render_frame_cursor_positions)
{
    void *ctx = talloc_new(NULL);

    ik_input_buffer_t *input_buf = NULL;
    res_t res;
    input_buf = ik_input_buffer_create(ctx);

    // Insert text: "hello"
    const char *text = "hello";
    for (size_t i = 0; i < 5; i++) {
        res = ik_input_buffer_insert_codepoint(input_buf, (uint32_t)text[i]);
        ck_assert(is_ok(&res));
    }

    ik_render_ctx_t *render = NULL;
    res = ik_render_create(ctx, 24, 80, 1, &render);
    ck_assert(is_ok(&res));

    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    term->screen_rows = 24;
    term->screen_cols = 80;

    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->input_buffer = input_buf;
    repl->render = render;
    repl->term = term;
    repl->scrollback = scrollback;
    repl->viewport_offset = 0;

    // Test cursor at end
    mock_write_calls = 0;
    res = ik_repl_render_frame(repl);
    ck_assert(is_ok(&res));
    ck_assert_int_gt(mock_write_calls, 0);

    // Move cursor to start and test again
    res = ik_input_buffer_cursor_left(input_buf);
    ck_assert(is_ok(&res));
    res = ik_input_buffer_cursor_left(input_buf);
    ck_assert(is_ok(&res));
    res = ik_input_buffer_cursor_left(input_buf);
    ck_assert(is_ok(&res));
    res = ik_input_buffer_cursor_left(input_buf);
    ck_assert(is_ok(&res));
    res = ik_input_buffer_cursor_left(input_buf);
    ck_assert(is_ok(&res));

    mock_write_calls = 0;
    res = ik_repl_render_frame(repl);
    ck_assert(is_ok(&res));
    ck_assert_int_gt(mock_write_calls, 0);

    // Move cursor to middle
    res = ik_input_buffer_cursor_right(input_buf);
    ck_assert(is_ok(&res));
    res = ik_input_buffer_cursor_right(input_buf);
    ck_assert(is_ok(&res));

    mock_write_calls = 0;
    res = ik_repl_render_frame(repl);
    ck_assert(is_ok(&res));
    ck_assert_int_gt(mock_write_calls, 0);

    talloc_free(ctx);
}

END_TEST
/* Test: Render frame with UTF-8 multi-byte characters */
START_TEST(test_repl_render_frame_utf8)
{
    void *ctx = talloc_new(NULL);

    ik_input_buffer_t *input_buf = NULL;
    res_t res;
    input_buf = ik_input_buffer_create(ctx);

    // Insert UTF-8 emoji
    res = ik_input_buffer_insert_codepoint(input_buf, 0x1F600);  // 😀
    ck_assert(is_ok(&res));

    ik_render_ctx_t *render = NULL;
    res = ik_render_create(ctx, 24, 80, 1, &render);
    ck_assert(is_ok(&res));

    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    term->screen_rows = 24;
    term->screen_cols = 80;

    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->input_buffer = input_buf;
    repl->render = render;
    repl->term = term;
    repl->scrollback = scrollback;
    repl->viewport_offset = 0;

    mock_write_calls = 0;
    res = ik_repl_render_frame(repl);
    ck_assert(is_ok(&res));
    ck_assert_int_gt(mock_write_calls, 0);

    talloc_free(ctx);
}

END_TEST

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
/* Test: NULL parameter assertions */
START_TEST(test_repl_render_frame_null_repl_asserts)
{
    /* repl cannot be NULL - should abort */
    ik_repl_render_frame(NULL);
}

END_TEST
#endif

static Suite *repl_render_suite(void)
{
    Suite *s = suite_create("REPL_Render");
    TCase *tc_core = tcase_create("Core");

    /* Basic rendering tests */
    tcase_add_test(tc_core, test_repl_render_frame_empty_input_buffer);
    tcase_add_test(tc_core, test_repl_render_frame_multiline);
    tcase_add_test(tc_core, test_repl_render_frame_cursor_positions);
    tcase_add_test(tc_core, test_repl_render_frame_utf8);

    suite_add_tcase(s, tc_core);

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
    /* Assertion tests */
    TCase *tc_assertions = tcase_create("Assertions");
    tcase_set_timeout(tc_assertions, 30); // Longer timeout for valgrind
    tcase_add_test_raise_signal(tc_assertions, test_repl_render_frame_null_repl_asserts, SIGABRT);
    suite_add_tcase(s, tc_assertions);
#endif

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = repl_render_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
