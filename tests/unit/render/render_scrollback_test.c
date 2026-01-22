/**
 * @file render_scrollback_test.c
 * @brief Unit tests for scrollback rendering (Phase 4 Task 4.3)
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "../../../src/render.h"
#include "../../../src/scrollback.h"
#include "../../test_utils_helper.h"

// Mock write tracking
static int32_t mock_write_calls = 0;
static char mock_write_buffer[8192];
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

/* Test: Render empty scrollback */
START_TEST(test_render_empty_scrollback) {
    void *ctx = talloc_new(NULL);

    // Create render context
    ik_render_ctx_t *render = NULL;
    res_t res = ik_render_create(ctx, 24, 80, 1, &render);
    ck_assert(is_ok(&res));

    // Create empty scrollback
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    // Reset mock state
    mock_write_calls = 0;
    mock_write_buffer_len = 0;

    // Render empty scrollback
    int32_t rows_used = 0;
    res = ik_render_scrollback(render, scrollback, 0, 0, &rows_used);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(rows_used, 0);

    // Should not write anything for empty scrollback
    ck_assert_int_eq(mock_write_calls, 0);

    talloc_free(ctx);
}
END_TEST
/* Test: Render single line of scrollback */
START_TEST(test_render_single_line) {
    void *ctx = talloc_new(NULL);

    // Create render context
    ik_render_ctx_t *render = NULL;
    res_t res = ik_render_create(ctx, 24, 80, 1, &render);
    ck_assert(is_ok(&res));

    // Create scrollback with one line
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    res = ik_scrollback_append_line(scrollback, "Hello, world!", 13);
    ck_assert(is_ok(&res));

    // Reset mock state
    mock_write_calls = 0;
    mock_write_buffer_len = 0;

    // Render the single line
    int32_t rows_used = 0;
    res = ik_render_scrollback(render, scrollback, 0, 1, &rows_used);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(rows_used, 1);  // One line = 1 row

    // Should have written to terminal
    ck_assert_int_gt(mock_write_calls, 0);

    // Verify "Hello, world!" appears in output
    ck_assert_ptr_nonnull(strstr(mock_write_buffer, "Hello, world!"));

    talloc_free(ctx);
}

END_TEST
/* Test: Render multiple lines */
START_TEST(test_render_multiple_lines) {
    void *ctx = talloc_new(NULL);

    // Create render context
    ik_render_ctx_t *render = NULL;
    res_t res = ik_render_create(ctx, 24, 80, 1, &render);
    ck_assert(is_ok(&res));

    // Create scrollback with 3 lines
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    res = ik_scrollback_append_line(scrollback, "Line 1", 6);
    ck_assert(is_ok(&res));
    res = ik_scrollback_append_line(scrollback, "Line 2", 6);
    ck_assert(is_ok(&res));
    res = ik_scrollback_append_line(scrollback, "Line 3", 6);
    ck_assert(is_ok(&res));

    // Reset mock state
    mock_write_calls = 0;
    mock_write_buffer_len = 0;

    // Render all 3 lines
    int32_t rows_used = 0;
    res = ik_render_scrollback(render, scrollback, 0, 3, &rows_used);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(rows_used, 3);  // 3 lines = 3 rows

    // Verify all lines appear in output
    ck_assert_ptr_nonnull(strstr(mock_write_buffer, "Line 1"));
    ck_assert_ptr_nonnull(strstr(mock_write_buffer, "Line 2"));
    ck_assert_ptr_nonnull(strstr(mock_write_buffer, "Line 3"));

    talloc_free(ctx);
}

END_TEST
/* Test: Render partial scrollback (subset of lines) */
START_TEST(test_render_partial_scrollback) {
    void *ctx = talloc_new(NULL);

    // Create render context
    ik_render_ctx_t *render = NULL;
    res_t res = ik_render_create(ctx, 24, 80, 1, &render);
    ck_assert(is_ok(&res));

    // Create scrollback with 5 lines
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    for (int32_t i = 0; i < 5; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Line %" PRId32, i);
        res = ik_scrollback_append_line(scrollback, buf, strlen(buf));
        ck_assert(is_ok(&res));
    }

    // Reset mock state
    mock_write_calls = 0;
    mock_write_buffer_len = 0;

    // Render lines 2-4 (3 lines total)
    int32_t rows_used = 0;
    res = ik_render_scrollback(render, scrollback, 2, 3, &rows_used);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(rows_used, 3);

    // Verify only lines 2, 3, 4 appear
    ck_assert_ptr_null(strstr(mock_write_buffer, "Line 0"));
    ck_assert_ptr_null(strstr(mock_write_buffer, "Line 1"));
    ck_assert_ptr_nonnull(strstr(mock_write_buffer, "Line 2"));
    ck_assert_ptr_nonnull(strstr(mock_write_buffer, "Line 3"));
    ck_assert_ptr_nonnull(strstr(mock_write_buffer, "Line 4"));

    talloc_free(ctx);
}

END_TEST
/* Test: Invalid start_line (beyond scrollback) */
START_TEST(test_render_invalid_start_line) {
    void *ctx = talloc_new(NULL);

    // Create render context
    ik_render_ctx_t *render = NULL;
    res_t res = ik_render_create(ctx, 24, 80, 1, &render);
    ck_assert(is_ok(&res));

    // Create scrollback with 3 lines
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    res = ik_scrollback_append_line(scrollback, "Line 1", 6);
    ck_assert(is_ok(&res));
    res = ik_scrollback_append_line(scrollback, "Line 2", 6);
    ck_assert(is_ok(&res));
    res = ik_scrollback_append_line(scrollback, "Line 3", 6);
    ck_assert(is_ok(&res));

    // Try to render starting at line 10 (beyond end)
    int32_t rows_used = 0;
    res = ik_render_scrollback(render, scrollback, 10, 5, &rows_used);
    ck_assert(is_err(&res));

    talloc_free(ctx);
}

END_TEST
/* Test: Line count clamping (request more lines than available) */
START_TEST(test_render_line_count_clamping) {
    void *ctx = talloc_new(NULL);

    // Create render context
    ik_render_ctx_t *render = NULL;
    res_t res = ik_render_create(ctx, 24, 80, 1, &render);
    ck_assert(is_ok(&res));

    // Create scrollback with 3 lines
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    res = ik_scrollback_append_line(scrollback, "Line 1", 6);
    ck_assert(is_ok(&res));
    res = ik_scrollback_append_line(scrollback, "Line 2", 6);
    ck_assert(is_ok(&res));
    res = ik_scrollback_append_line(scrollback, "Line 3", 6);
    ck_assert(is_ok(&res));

    // Reset mock state
    mock_write_calls = 0;
    mock_write_buffer_len = 0;

    // Request 100 lines starting at line 1 (should clamp to lines 1-2)
    int32_t rows_used = 0;
    res = ik_render_scrollback(render, scrollback, 1, 100, &rows_used);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(rows_used, 2);  // Only 2 lines available (1 and 2)

    // Verify lines 2 and 3 appear (0-indexed: lines 1 and 2)
    ck_assert_ptr_nonnull(strstr(mock_write_buffer, "Line 2"));
    ck_assert_ptr_nonnull(strstr(mock_write_buffer, "Line 3"));

    talloc_free(ctx);
}

END_TEST
/* Test: Render text with embedded newlines */
START_TEST(test_render_with_newlines) {
    void *ctx = talloc_new(NULL);

    // Create render context
    ik_render_ctx_t *render = NULL;
    res_t res = ik_render_create(ctx, 24, 80, 1, &render);
    ck_assert(is_ok(&res));

    // Create scrollback with line containing newlines
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    res = ik_scrollback_append_line(scrollback, "Line\nwith\nnewlines", 17);
    ck_assert(is_ok(&res));

    // Reset mock state
    mock_write_calls = 0;
    mock_write_buffer_len = 0;

    // Render the line with newlines
    int32_t rows_used = 0;
    res = ik_render_scrollback(render, scrollback, 0, 1, &rows_used);
    ck_assert(is_ok(&res));

    // Should have written to terminal
    ck_assert_int_gt(mock_write_calls, 0);

    // Verify newlines were converted to \r\n
    ck_assert_ptr_nonnull(strstr(mock_write_buffer, "\r\n"));

    talloc_free(ctx);
}

END_TEST
/* Test: Write failure during scrollback render */
START_TEST(test_render_write_failure) {
    void *ctx = talloc_new(NULL);

    // Create render context
    ik_render_ctx_t *render = NULL;
    res_t res = ik_render_create(ctx, 24, 80, 1, &render);
    ck_assert(is_ok(&res));

    // Create scrollback with a line
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    res = ik_scrollback_append_line(scrollback, "Test line", 9);
    ck_assert(is_ok(&res));

    // Enable write failure
    mock_write_should_fail = true;

    // Attempt to render - should fail
    int32_t rows_used = 0;
    res = ik_render_scrollback(render, scrollback, 0, 1, &rows_used);
    ck_assert(is_err(&res));

    // Cleanup mock state
    mock_write_should_fail = false;

    talloc_free(ctx);
}

END_TEST

/* Create test suite */
static Suite *render_scrollback_suite(void)
{
    Suite *s = suite_create("Render Scrollback");

    TCase *tc_render = tcase_create("Rendering");
    tcase_set_timeout(tc_render, IK_TEST_TIMEOUT);
    tcase_add_test(tc_render, test_render_empty_scrollback);
    tcase_add_test(tc_render, test_render_single_line);
    tcase_add_test(tc_render, test_render_multiple_lines);
    tcase_add_test(tc_render, test_render_partial_scrollback);
    tcase_add_test(tc_render, test_render_invalid_start_line);
    tcase_add_test(tc_render, test_render_line_count_clamping);
    tcase_add_test(tc_render, test_render_with_newlines);
    tcase_add_test(tc_render, test_render_write_failure);
    suite_add_tcase(s, tc_render);

    return s;
}

int main(void)
{
    Suite *s = render_scrollback_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/render/render_scrollback_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
