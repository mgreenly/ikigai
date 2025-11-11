/**
 * @file repl_run_test.c
 * @brief Unit tests for REPL event loop (ik_repl_run)
 */

#include <check.h>
#include <signal.h>
#include <talloc.h>
#include <string.h>
#include <unistd.h>
#include "../../../src/repl.h"
#include "../../../src/workspace.h"
#include "../../../src/input.h"
#include "../../../src/terminal.h"
#include "../../../src/render_direct.h"
#include "../../test_utils.h"

// Mock read tracking
static const char *mock_input = NULL;
static size_t mock_input_pos = 0;

// Mock read wrapper for testing
ssize_t ik_read_wrapper(int fd, void *buf, size_t count);

ssize_t ik_read_wrapper(int fd, void *buf, size_t count)
{
    (void)fd;

    if (!mock_input || mock_input_pos >= strlen(mock_input)) {
        return 0;  // EOF
    }

    size_t to_copy = 1;  // Read one byte at a time (simulating real terminal input)
    if (to_copy > count) {
        to_copy = count;
    }

    memcpy(buf, mock_input + mock_input_pos, to_copy);
    mock_input_pos += to_copy;

    return (ssize_t)to_copy;
}

// Mock write wrapper (suppress output during tests)
static bool mock_write_should_fail = false;
static int32_t mock_write_fail_after = -1;  // Fail after N successful writes (-1 = never fail)
static int32_t mock_write_count = 0;

ssize_t ik_write_wrapper(int fd, const void *buf, size_t count);

ssize_t ik_write_wrapper(int fd, const void *buf, size_t count)
{
    (void)fd;
    (void)buf;

    if (mock_write_should_fail) {
        return -1;  // Simulate write error
    }

    if (mock_write_fail_after >= 0 && mock_write_count >= mock_write_fail_after) {
        return -1;  // Fail after N writes
    }

    mock_write_count++;
    return (ssize_t)count;
}

/* Test: Simple character input followed by Ctrl+C */
START_TEST(test_repl_run_simple_char_input) {
    void *ctx = talloc_new(NULL);

    // Manually construct REPL context components (avoid terminal init in tests)
    ik_workspace_t *workspace = NULL;
    res_t res = ik_workspace_create(ctx, &workspace);
    ck_assert(is_ok(&res));

    ik_input_parser_t *parser = NULL;
    res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    // Create mock terminal context
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    ck_assert_ptr_nonnull(term);
    term->tty_fd = 0;  // Mock stdin
    term->screen_rows = 24;
    term->screen_cols = 80;

    ik_render_direct_ctx_t *render = NULL;
    res = ik_render_direct_create(ctx, 24, 80, 1, &render);
    ck_assert(is_ok(&res));

    // Create REPL context
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->workspace = workspace;
    repl->input_parser = parser;
    repl->term = term;
    repl->render = render;
    repl->quit = false;

    // Mock input: "a" + Ctrl+C
    mock_input = "a\x03";
    mock_input_pos = 0;

    // Run REPL event loop
    res = ik_repl_run(repl);
    ck_assert(is_ok(&res));

    // Verify workspace contains "a"
    char *text = NULL;
    size_t len = 0;
    res = ik_workspace_get_text(repl->workspace, &text, &len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(len, 1);
    ck_assert_int_eq(text[0], 'a');
    talloc_free(text);

    // Verify quit flag is set
    ck_assert(repl->quit);

    talloc_free(ctx);
}
END_TEST
/* Test: Multiple character input */
START_TEST(test_repl_run_multiple_chars)
{
    void *ctx = talloc_new(NULL);

    ik_workspace_t *workspace = NULL;
    res_t res = ik_workspace_create(ctx, &workspace);
    ck_assert(is_ok(&res));

    ik_input_parser_t *parser = NULL;
    res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    ck_assert_ptr_nonnull(term);
    term->tty_fd = 0;
    term->screen_rows = 24;
    term->screen_cols = 80;

    ik_render_direct_ctx_t *render = NULL;
    res = ik_render_direct_create(ctx, 24, 80, 1, &render);
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->workspace = workspace;
    repl->input_parser = parser;
    repl->term = term;
    repl->render = render;
    repl->quit = false;

    // Mock input: "abc" + Ctrl+C
    mock_input = "abc\x03";
    mock_input_pos = 0;

    res = ik_repl_run(repl);
    ck_assert(is_ok(&res));

    char *text = NULL;
    size_t len = 0;
    res = ik_workspace_get_text(workspace, &text, &len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(len, 3);
    ck_assert_int_eq(text[0], 'a');
    ck_assert_int_eq(text[1], 'b');
    ck_assert_int_eq(text[2], 'c');
    talloc_free(text);

    ck_assert(repl->quit);

    talloc_free(ctx);
}

END_TEST
/* Test: Input with newline */
START_TEST(test_repl_run_with_newline)
{
    void *ctx = talloc_new(NULL);

    ik_workspace_t *workspace = NULL;
    res_t res = ik_workspace_create(ctx, &workspace);
    ck_assert(is_ok(&res));

    ik_input_parser_t *parser = NULL;
    res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    ck_assert_ptr_nonnull(term);
    term->tty_fd = 0;
    term->screen_rows = 24;
    term->screen_cols = 80;

    ik_render_direct_ctx_t *render = NULL;
    res = ik_render_direct_create(ctx, 24, 80, 1, &render);
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->workspace = workspace;
    repl->input_parser = parser;
    repl->term = term;
    repl->render = render;
    repl->quit = false;

    // Mock input: "hi\n" + Ctrl+C
    mock_input = "hi\n\x03";
    mock_input_pos = 0;

    res = ik_repl_run(repl);
    ck_assert(is_ok(&res));

    char *text = NULL;
    size_t len = 0;
    res = ik_workspace_get_text(workspace, &text, &len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(len, 3);
    ck_assert_int_eq(text[0], 'h');
    ck_assert_int_eq(text[1], 'i');
    ck_assert_int_eq(text[2], '\n');
    talloc_free(text);

    talloc_free(ctx);
}

END_TEST
/* Test: Input with backspace */
START_TEST(test_repl_run_with_backspace)
{
    void *ctx = talloc_new(NULL);

    ik_workspace_t *workspace = NULL;
    res_t res = ik_workspace_create(ctx, &workspace);
    ck_assert(is_ok(&res));

    ik_input_parser_t *parser = NULL;
    res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    ck_assert_ptr_nonnull(term);
    term->tty_fd = 0;
    term->screen_rows = 24;
    term->screen_cols = 80;

    ik_render_direct_ctx_t *render = NULL;
    res = ik_render_direct_create(ctx, 24, 80, 1, &render);
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->workspace = workspace;
    repl->input_parser = parser;
    repl->term = term;
    repl->render = render;
    repl->quit = false;

    // Mock input: "ab" + backspace + Ctrl+C
    mock_input = "ab\x7f\x03";
    mock_input_pos = 0;

    res = ik_repl_run(repl);
    ck_assert(is_ok(&res));

    char *text = NULL;
    size_t len = 0;
    res = ik_workspace_get_text(workspace, &text, &len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(len, 1);
    ck_assert_int_eq(text[0], 'a');
    talloc_free(text);

    talloc_free(ctx);
}

END_TEST
/* Test: Read EOF */
START_TEST(test_repl_run_read_eof)
{
    void *ctx = talloc_new(NULL);

    ik_workspace_t *workspace = NULL;
    res_t res = ik_workspace_create(ctx, &workspace);
    ck_assert(is_ok(&res));

    ik_input_parser_t *parser = NULL;
    res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    ck_assert_ptr_nonnull(term);
    term->tty_fd = 0;
    term->screen_rows = 24;
    term->screen_cols = 80;

    ik_render_direct_ctx_t *render = NULL;
    res = ik_render_direct_create(ctx, 24, 80, 1, &render);
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->workspace = workspace;
    repl->input_parser = parser;
    repl->term = term;
    repl->render = render;
    repl->quit = false;

    // Mock input: immediate EOF (empty string)
    mock_input = "";
    mock_input_pos = 0;

    res = ik_repl_run(repl);
    ck_assert(is_ok(&res));

    // Workspace should be empty
    char *text = NULL;
    size_t len = 0;
    res = ik_workspace_get_text(workspace, &text, &len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(len, 0);
    talloc_free(text);

    // quit flag should still be false (natural EOF, not Ctrl+C)
    ck_assert_int_eq(repl->quit, false);

    talloc_free(ctx);
}

END_TEST
/* Test: REPL handles incomplete escape sequence at EOF */
START_TEST(test_repl_run_unknown_action)
{
    void *ctx = talloc_new(NULL);

    ik_workspace_t *workspace = NULL;
    res_t res = ik_workspace_create(ctx, &workspace);
    ck_assert(is_ok(&res));

    ik_input_parser_t *parser = NULL;
    res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    ck_assert_ptr_nonnull(term);
    term->tty_fd = 0;
    term->screen_rows = 24;
    term->screen_cols = 80;

    ik_render_direct_ctx_t *render = NULL;
    res = ik_render_direct_create(ctx, 24, 80, 1, &render);
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->workspace = workspace;
    repl->input_parser = parser;
    repl->term = term;
    repl->render = render;
    repl->quit = false;

    // Mock input: 'a' + incomplete ESC sequence (ESC then EOF)
    // This tests that the REPL doesn't hang or crash on incomplete input
    mock_input = "a\x1b";
    mock_input_pos = 0;

    res = ik_repl_run(repl);
    ck_assert(is_ok(&res));

    // Should exit cleanly (EOF breaks the loop)
    ck_assert_int_eq(repl->quit, false);

    // Verify 'a' was processed before the incomplete sequence
    char *text = NULL;
    size_t len = 0;
    res = ik_workspace_get_text(workspace, &text, &len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(len, 1);
    ck_assert_int_eq(text[0], 'a');
    talloc_free(text);

    talloc_free(ctx);
}

END_TEST

/* Test: Initial render error */
START_TEST(test_repl_run_initial_render_error)
{
    void *ctx = talloc_new(NULL);

    ik_workspace_t *workspace = NULL;
    res_t res = ik_workspace_create(ctx, &workspace);
    ck_assert(is_ok(&res));

    ik_input_parser_t *parser = NULL;
    res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    ck_assert_ptr_nonnull(term);
    term->tty_fd = 0;
    term->screen_rows = 24;
    term->screen_cols = 80;

    ik_render_direct_ctx_t *render = NULL;
    res = ik_render_direct_create(ctx, 24, 80, 1, &render);
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->workspace = workspace;
    repl->input_parser = parser;
    repl->term = term;
    repl->render = render;
    repl->quit = false;

    // Cause initial render to fail
    mock_write_should_fail = true;

    // Run should fail on initial render
    res = ik_repl_run(repl);
    ck_assert(is_err(&res));

    // Reset
    mock_write_should_fail = false;

    talloc_free(ctx);
}
END_TEST

/* Test: Render error during event loop */
START_TEST(test_repl_run_render_error_in_loop)
{
    void *ctx = talloc_new(NULL);

    // Reset mock state
    mock_write_should_fail = false;
    mock_write_fail_after = 1;  // Fail after 1 successful write (initial render succeeds, loop render fails)
    mock_write_count = 0;

    ik_workspace_t *workspace = NULL;
    res_t res = ik_workspace_create(ctx, &workspace);
    ck_assert(is_ok(&res));

    ik_input_parser_t *parser = NULL;
    res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    ck_assert_ptr_nonnull(term);
    term->tty_fd = 0;
    term->screen_rows = 24;
    term->screen_cols = 80;

    ik_render_direct_ctx_t *render = NULL;
    res = ik_render_direct_create(ctx, 24, 80, 1, &render);
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->workspace = workspace;
    repl->input_parser = parser;
    repl->term = term;
    repl->render = render;
    repl->quit = false;

    // Mock input: 'a' (which will trigger re-render that fails)
    mock_input = "a";
    mock_input_pos = 0;

    // Initial render succeeds (write count = 1), then render after 'a' fails
    res = ik_repl_run(repl);
    ck_assert(is_err(&res));

    // Reset
    mock_write_fail_after = -1;
    mock_write_count = 0;

    talloc_free(ctx);
}
END_TEST

static Suite *repl_run_suite(void)
{
    Suite *s = suite_create("REPL_Run");
    TCase *tc_core = tcase_create("Core");

    /* Normal tests */
    tcase_add_test(tc_core, test_repl_run_simple_char_input);
    tcase_add_test(tc_core, test_repl_run_multiple_chars);
    tcase_add_test(tc_core, test_repl_run_with_newline);
    tcase_add_test(tc_core, test_repl_run_with_backspace);
    tcase_add_test(tc_core, test_repl_run_read_eof);
    tcase_add_test(tc_core, test_repl_run_unknown_action);
    tcase_add_test(tc_core, test_repl_run_initial_render_error);
    tcase_add_test(tc_core, test_repl_run_render_error_in_loop);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = repl_run_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
