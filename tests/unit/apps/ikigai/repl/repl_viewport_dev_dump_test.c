/**
 * @file repl_viewport_dev_dump_test.c
 * @brief Unit tests for REPL viewport dev framebuffer dump
 */

#include <check.h>
#include "apps/ikigai/agent.h"
#include "apps/ikigai/shared.h"
#include <talloc.h>
#include "apps/ikigai/repl.h"
#include "tests/helpers/test_utils_helper.h"

#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

/* Test: Dev dump with NULL framebuffer */
START_TEST(test_dev_dump_null_framebuffer) {
    void *ctx = talloc_new(NULL);

    // Create minimal repl context
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->framebuffer = NULL;
    repl->framebuffer_len = 0;

    // Should return early without crashing
    ik_repl_dev_dump_framebuffer(repl);

    talloc_free(ctx);
}
END_TEST

/* Test: Dev dump with empty framebuffer */
START_TEST(test_dev_dump_empty_framebuffer) {
    void *ctx = talloc_new(NULL);

    // Create minimal repl context
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    char buffer[100];
    repl->framebuffer = buffer;
    repl->framebuffer_len = 0;  // Empty

    // Should return early without crashing
    ik_repl_dev_dump_framebuffer(repl);

    talloc_free(ctx);
}
END_TEST

/* Test: Dev dump without debug directory */
START_TEST(test_dev_dump_no_debug_dir) {
    void *ctx = talloc_new(NULL);

    // Ensure debug directory doesn't exist
    rmdir(".ikigai/debug");
    rmdir(".ikigai");

    // Create minimal repl context with framebuffer
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    ik_term_ctx_t *term = talloc_zero(shared, ik_term_ctx_t);
    repl->shared = shared;
    shared->term = term;
    term->screen_rows = 24;
    term->screen_cols = 80;

    char buffer[100] = "test data";
    repl->framebuffer = buffer;
    repl->framebuffer_len = 9;
    repl->cursor_row = 0;
    repl->cursor_col = 0;

    // Should return early without crashing (no debug dir)
    ik_repl_dev_dump_framebuffer(repl);

    talloc_free(ctx);
}
END_TEST

/* Test: Dev dump when .ikigai/debug is a file (not directory) */
START_TEST(test_dev_dump_debug_is_file) {
    void *ctx = talloc_new(NULL);

    // Force cleanup any existing .ikigai directory structure
    int result = system("rm -rf .ikigai");
    (void)result;

    // Create .ikigai directory and .ikigai/debug as a FILE (not directory)
    mkdir(".ikigai", 0755);
    int fd = open(".ikigai/debug", O_WRONLY | O_CREAT | O_EXCL, 0644);
    ck_assert_int_ge(fd, 0);  // Ensure file was created
    close(fd);

    // Create minimal repl context with framebuffer
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    ik_term_ctx_t *term = talloc_zero(shared, ik_term_ctx_t);
    repl->shared = shared;
    shared->term = term;
    term->screen_rows = 24;
    term->screen_cols = 80;

    char buffer[100] = "test data";
    repl->framebuffer = buffer;
    repl->framebuffer_len = 9;
    repl->cursor_row = 0;
    repl->cursor_col = 0;

    // Should return early without crashing (.ikigai/debug is not a directory)
    ik_repl_dev_dump_framebuffer(repl);

    // Clean up
    unlink(".ikigai/debug");
    rmdir(".ikigai");

    talloc_free(ctx);
}
END_TEST

/* Test: Dev dump with debug directory - successful write */
START_TEST(test_dev_dump_success) {
    void *ctx = talloc_new(NULL);

    // Create debug directory
    mkdir(".ikigai", 0755);
    mkdir(".ikigai/debug", 0755);

    // Create minimal repl context with framebuffer
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    ik_term_ctx_t *term = talloc_zero(shared, ik_term_ctx_t);
    repl->shared = shared;
    shared->term = term;
    term->screen_rows = 24;
    term->screen_cols = 80;

    char buffer[100] = "test framebuffer data";
    repl->framebuffer = buffer;
    repl->framebuffer_len = 21;
    repl->cursor_row = 5;
    repl->cursor_col = 10;

    // Should write the file
    ik_repl_dev_dump_framebuffer(repl);

    // Verify file was created
    struct stat st;
    int result = stat(".ikigai/debug/repl_viewport.framebuffer", &st);
    ck_assert_int_eq(result, 0);
    ck_assert(S_ISREG(st.st_mode));

    // Clean up
    unlink(".ikigai/debug/repl_viewport.framebuffer");
    rmdir(".ikigai/debug");
    rmdir(".ikigai");

    talloc_free(ctx);
}
END_TEST

/* Test: Dev dump with read-only debug directory - file open fails */
START_TEST(test_dev_dump_readonly_dir) {
    void *ctx = talloc_new(NULL);

    // Create debug directory
    mkdir(".ikigai", 0755);
    mkdir(".ikigai/debug", 0755);

    // Make directory read-only
    chmod(".ikigai/debug", 0444);

    // Create minimal repl context with framebuffer
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    ik_term_ctx_t *term = talloc_zero(shared, ik_term_ctx_t);
    repl->shared = shared;
    shared->term = term;
    term->screen_rows = 24;
    term->screen_cols = 80;

    char buffer[100] = "test data";
    repl->framebuffer = buffer;
    repl->framebuffer_len = 9;
    repl->cursor_row = 0;
    repl->cursor_col = 0;

    // Should return early without crashing (can't open file)
    ik_repl_dev_dump_framebuffer(repl);

    // Clean up
    chmod(".ikigai/debug", 0755);
    rmdir(".ikigai/debug");
    rmdir(".ikigai");

    talloc_free(ctx);
}
END_TEST

/* Create test suite */
static Suite *repl_viewport_dev_dump_suite(void)
{
    Suite *s = suite_create("REPL Viewport Dev Framebuffer Dump");

    TCase *tc_dev_dump = tcase_create("Dev Framebuffer Dump");
    tcase_set_timeout(tc_dev_dump, IK_TEST_TIMEOUT);
    tcase_add_test(tc_dev_dump, test_dev_dump_null_framebuffer);
    tcase_add_test(tc_dev_dump, test_dev_dump_empty_framebuffer);
    tcase_add_test(tc_dev_dump, test_dev_dump_no_debug_dir);
    tcase_add_test(tc_dev_dump, test_dev_dump_debug_is_file);
    tcase_add_test(tc_dev_dump, test_dev_dump_success);
    tcase_add_test(tc_dev_dump, test_dev_dump_readonly_dir);
    suite_add_tcase(s, tc_dev_dump);

    return s;
}

int main(void)
{
    Suite *s = repl_viewport_dev_dump_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/repl/repl_viewport_dev_dump_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
