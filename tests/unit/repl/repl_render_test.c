/**
 * @file repl_render_test.c
 * @brief Unit tests for REPL render_frame function
 */

#include <check.h>
#include <signal.h>
#include <talloc.h>
#include <string.h>
#include "../../../src/repl.h"
#include "../../../src/render_direct.h"
#include "../../test_utils.h"

// Mock write tracking
static int32_t mock_write_calls = 0;
static char mock_write_buffer[4096];
static size_t mock_write_buffer_len = 0;

// Mock write wrapper declaration
ssize_t ik_write_wrapper(int fd, const void *buf, size_t count);

// Mock write wrapper for testing
ssize_t ik_write_wrapper(int fd, const void *buf, size_t count)
{
    (void)fd;
    mock_write_calls++;
    if (mock_write_buffer_len + count < sizeof(mock_write_buffer)) {
        memcpy(mock_write_buffer + mock_write_buffer_len, buf, count);
        mock_write_buffer_len += count;
    }
    return (ssize_t)count;
}

/* Test: Render frame with empty workspace */
START_TEST(test_repl_render_frame_empty_workspace) {
    void *ctx = talloc_new(NULL);

    // Manually construct REPL context components
    ik_workspace_t *workspace = NULL;
    res_t res = ik_workspace_create(ctx, &workspace);
    ck_assert(is_ok(&res));

    ik_render_direct_ctx_t *render = NULL;
    res = ik_render_direct_create(ctx, 24, 80, 1, &render);  // Mock terminal: 24x80, fd=1
    ck_assert(is_ok(&res));

    // Create minimal REPL context
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->workspace = workspace;
    repl->render = render;

    // Reset mock state
    mock_write_calls = 0;
    mock_write_buffer_len = 0;

    /* Call render_frame - this should succeed with empty workspace */
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
    ik_workspace_t *workspace = NULL;
    res_t res = ik_workspace_create(ctx, &workspace);
    ck_assert(is_ok(&res));

    // Insert multi-line text
    res = ik_workspace_insert_codepoint(workspace, 'h');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'i');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_newline(workspace);
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'b');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'y');
    ck_assert(is_ok(&res));
    res = ik_workspace_insert_codepoint(workspace, 'e');
    ck_assert(is_ok(&res));

    ik_render_direct_ctx_t *render = NULL;
    res = ik_render_direct_create(ctx, 24, 80, 1, &render);
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->workspace = workspace;
    repl->render = render;

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

    ik_workspace_t *workspace = NULL;
    res_t res = ik_workspace_create(ctx, &workspace);
    ck_assert(is_ok(&res));

    // Insert text: "hello"
    const char *text = "hello";
    for (size_t i = 0; i < 5; i++) {
        res = ik_workspace_insert_codepoint(workspace, (uint32_t)text[i]);
        ck_assert(is_ok(&res));
    }

    ik_render_direct_ctx_t *render = NULL;
    res = ik_render_direct_create(ctx, 24, 80, 1, &render);
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->workspace = workspace;
    repl->render = render;

    // Test cursor at end
    mock_write_calls = 0;
    res = ik_repl_render_frame(repl);
    ck_assert(is_ok(&res));
    ck_assert_int_gt(mock_write_calls, 0);

    // Move cursor to start and test again
    res = ik_workspace_cursor_left(workspace);
    ck_assert(is_ok(&res));
    res = ik_workspace_cursor_left(workspace);
    ck_assert(is_ok(&res));
    res = ik_workspace_cursor_left(workspace);
    ck_assert(is_ok(&res));
    res = ik_workspace_cursor_left(workspace);
    ck_assert(is_ok(&res));
    res = ik_workspace_cursor_left(workspace);
    ck_assert(is_ok(&res));

    mock_write_calls = 0;
    res = ik_repl_render_frame(repl);
    ck_assert(is_ok(&res));
    ck_assert_int_gt(mock_write_calls, 0);

    // Move cursor to middle
    res = ik_workspace_cursor_right(workspace);
    ck_assert(is_ok(&res));
    res = ik_workspace_cursor_right(workspace);
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

    ik_workspace_t *workspace = NULL;
    res_t res = ik_workspace_create(ctx, &workspace);
    ck_assert(is_ok(&res));

    // Insert UTF-8 emoji
    res = ik_workspace_insert_codepoint(workspace, 0x1F600);  // 😀
    ck_assert(is_ok(&res));

    ik_render_direct_ctx_t *render = NULL;
    res = ik_render_direct_create(ctx, 24, 80, 1, &render);
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->workspace = workspace;
    repl->render = render;

    mock_write_calls = 0;
    res = ik_repl_render_frame(repl);
    ck_assert(is_ok(&res));
    ck_assert_int_gt(mock_write_calls, 0);

    talloc_free(ctx);
}

END_TEST
/* Test: NULL parameter assertions */
START_TEST(test_repl_render_frame_null_repl_asserts)
{
    /* repl cannot be NULL - should abort */
    ik_repl_render_frame(NULL);
}

END_TEST

static Suite *repl_render_suite(void)
{
    Suite *s = suite_create("REPL_Render");
    TCase *tc_core = tcase_create("Core");
    TCase *tc_assertions = tcase_create("Assertions");

    /* Normal tests */
    tcase_add_test(tc_core, test_repl_render_frame_empty_workspace);
    tcase_add_test(tc_core, test_repl_render_frame_multiline);
    tcase_add_test(tc_core, test_repl_render_frame_cursor_positions);
    tcase_add_test(tc_core, test_repl_render_frame_utf8);

    /* Assertion tests */
    tcase_add_test_raise_signal(tc_assertions, test_repl_render_frame_null_repl_asserts, SIGABRT);

    suite_add_tcase(s, tc_core);
    suite_add_tcase(s, tc_assertions);
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
