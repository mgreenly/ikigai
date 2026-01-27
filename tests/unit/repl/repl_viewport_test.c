#include "agent.h"
/**
 * @file repl_viewport_test.c
 * @brief Unit tests for REPL viewport calculation (Phase 4 Task 4.2)
 */

#include <check.h>
#include "../../../src/agent.h"
#include "../../../src/shared.h"
#include <talloc.h>
#include "../../../src/repl.h"
#include "../../../src/scrollback.h"
#include "../../test_utils_helper.h"

#ifdef IKIGAI_DEV
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

/* Test: Viewport with empty scrollback (input buffer fills screen) */
START_TEST(test_viewport_empty_scrollback) {
    void *ctx = talloc_new(NULL);

    // Create REPL context with mocked terminal (24 rows)
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    res_t res;
    term->screen_rows = 24;
    term->screen_cols = 80;

    ik_input_buffer_t *input_buf = NULL;
    input_buf = ik_input_buffer_create(ctx);

    // Add a few lines to input buffer
    res = ik_input_buffer_insert_codepoint(input_buf, 'h');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'i');
    ck_assert(is_ok(&res));

    // Ensure input buffer layout
    ik_input_buffer_ensure_layout(input_buf, 80);
    size_t input_buf_rows = ik_input_buffer_get_physical_lines(input_buf);
    ck_assert_uint_eq(input_buf_rows, 1);  // "hi" is 1 line

    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->term = term;

    // Create agent context for display state
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    repl->current = agent;
    repl->current->input_buffer = input_buf;
    repl->current->scrollback = scrollback;
    repl->current->viewport_offset = 0;

    // Calculate viewport
    ik_viewport_t viewport;
    res = ik_repl_calculate_viewport(repl, &viewport);
    ck_assert(is_ok(&res));

    // With empty scrollback, all rows go to input buffer
    ck_assert_uint_eq(viewport.scrollback_start_line, 0);
    ck_assert_uint_eq(viewport.scrollback_lines_count, 0);
    // Input buffer starts at row 1 (after separator at row 0)
    ck_assert_uint_eq(viewport.input_buffer_start_row, 1);

    talloc_free(ctx);
}
END_TEST
/* Test: Viewport with small scrollback (both visible) */
START_TEST(test_viewport_small_scrollback) {
    void *ctx = talloc_new(NULL);

    // Create REPL context with mocked terminal (24 rows)
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    res_t res;
    term->screen_rows = 24;
    term->screen_cols = 80;

    ik_input_buffer_t *input_buf = NULL;
    input_buf = ik_input_buffer_create(ctx);

    // Add single line to input buffer
    res = ik_input_buffer_insert_codepoint(input_buf, 'h');
    ck_assert(is_ok(&res));
    ik_input_buffer_ensure_layout(input_buf, 80);
    size_t input_buf_rows = ik_input_buffer_get_physical_lines(input_buf);
    ck_assert_uint_eq(input_buf_rows, 1);

    // Create scrollback with 3 lines
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    res = ik_scrollback_append_line(scrollback, "line 1", 6);
    ck_assert(is_ok(&res));
    res = ik_scrollback_append_line(scrollback, "line 2", 6);
    ck_assert(is_ok(&res));
    res = ik_scrollback_append_line(scrollback, "line 3", 6);
    ck_assert(is_ok(&res));

    size_t scrollback_rows = ik_scrollback_get_total_physical_lines(scrollback);
    ck_assert_uint_eq(scrollback_rows, 3);

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->term = term;

    // Create agent context for display state
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    repl->current = agent;
    repl->current->input_buffer = input_buf;
    repl->current->scrollback = scrollback;
    repl->current->viewport_offset = 0;  // At bottom

    // Calculate viewport
    ik_viewport_t viewport;
    res = ik_repl_calculate_viewport(repl, &viewport);
    ck_assert(is_ok(&res));

    // Total: 3 scrollback rows + 1 separator + 1 input buffer row = 5 rows (fits in 24)
    // All scrollback lines should be visible
    ck_assert_uint_eq(viewport.scrollback_start_line, 0);
    ck_assert_uint_eq(viewport.scrollback_lines_count, 3);
    // Input buffer starts at row 4 (3 scrollback + 1 separator)
    ck_assert_uint_eq(viewport.input_buffer_start_row, 4);

    talloc_free(ctx);
}

END_TEST
/* Test: Viewport with large scrollback (scrollback overflows) */
START_TEST(test_viewport_large_scrollback) {
    void *ctx = talloc_new(NULL);

    // Create REPL context with small terminal (10 rows)
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    res_t res;
    term->screen_rows = 10;
    term->screen_cols = 80;

    ik_input_buffer_t *input_buf = NULL;
    input_buf = ik_input_buffer_create(ctx);

    // Add 2 lines to input buffer
    res = ik_input_buffer_insert_codepoint(input_buf, 'h');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_newline(input_buf);
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'i');
    ck_assert(is_ok(&res));
    ik_input_buffer_ensure_layout(input_buf, 80);
    size_t input_buf_rows = ik_input_buffer_get_physical_lines(input_buf);
    ck_assert_uint_eq(input_buf_rows, 2);

    // Create scrollback with 20 lines (more than terminal)
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    for (int32_t i = 0; i < 20; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "line %" PRId32, i);
        res = ik_scrollback_append_line(scrollback, buf, strlen(buf));
        ck_assert(is_ok(&res));
    }

    size_t scrollback_rows = ik_scrollback_get_total_physical_lines(scrollback);
    ck_assert_uint_eq(scrollback_rows, 20);

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->term = term;

    // Create agent context for display state
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    repl->current = agent;
    repl->current->input_buffer = input_buf;
    repl->current->scrollback = scrollback;
    repl->current->viewport_offset = 0;  // At bottom

    // Calculate viewport
    ik_viewport_t viewport;
    res = ik_repl_calculate_viewport(repl, &viewport);
    ck_assert(is_ok(&res));

    // Terminal: 10 rows, input buffer: 2 rows, upper_separator: 1 row, lower_separator: 1 row
    // Document: 20 scrollback + 1 upper_separator + 2 input buffer + 1 lower_separator = 24 rows
    // Viewport shows last 10 rows: scrollback 14-19 (6 rows) + upper_separator (1) + input buffer (2) + lower_separator (1)
    ck_assert_uint_eq(viewport.scrollback_start_line, 14);
    ck_assert_uint_eq(viewport.scrollback_lines_count, 6);
    // Input buffer starts at row 7 (6 scrollback + 1 upper_separator)
    ck_assert_uint_eq(viewport.input_buffer_start_row, 7);

    talloc_free(ctx);
}

END_TEST
/* Test: Viewport offset clamping (don't scroll past top) */
START_TEST(test_viewport_offset_clamping) {
    void *ctx = talloc_new(NULL);

    // Create REPL context with terminal (10 rows)
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    res_t res;
    term->screen_rows = 10;
    term->screen_cols = 80;

    ik_input_buffer_t *input_buf = NULL;
    input_buf = ik_input_buffer_create(ctx);

    // Add 1 line to input buffer
    res = ik_input_buffer_insert_codepoint(input_buf, 'h');
    ck_assert(is_ok(&res));
    ik_input_buffer_ensure_layout(input_buf, 80);
    size_t input_buf_rows = ik_input_buffer_get_physical_lines(input_buf);
    ck_assert_uint_eq(input_buf_rows, 1);

    // Create scrollback with 20 lines (more than available space)
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    for (int32_t i = 0; i < 20; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "line %" PRId32, i);
        res = ik_scrollback_append_line(scrollback, buf, strlen(buf));
        ck_assert(is_ok(&res));
    }

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->term = term;

    // Create agent context for display state
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    repl->current = agent;
    repl->current->input_buffer = input_buf;
    repl->current->scrollback = scrollback;
    repl->current->viewport_offset = 100;  // Try to scroll way past top

    // Calculate viewport - should clamp to valid range
    ik_viewport_t viewport;
    res = ik_repl_calculate_viewport(repl, &viewport);
    ck_assert(is_ok(&res));

    // Document: 20 scrollback + 1 separator + 1 input buffer = 22 rows
    // Viewport shows 10 rows, max offset = 22 - 10 = 12
    // offset=100 clamped to 12, showing rows 0-9 of document (first 10 scrollback lines)
    ck_assert_uint_eq(viewport.scrollback_start_line, 0);
    ck_assert_uint_eq(viewport.scrollback_lines_count, 10);

    talloc_free(ctx);
}

END_TEST
/* Test: Viewport when terminal height equals input buffer height (no room for scrollback) */
START_TEST(test_viewport_no_scrollback_room) {
    void *ctx = talloc_new(NULL);

    // Create REPL context with terminal that exactly matches input buffer height
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    res_t res;
    term->screen_rows = 3;  // Exactly 3 rows
    term->screen_cols = 80;

    ik_input_buffer_t *input_buf = NULL;
    input_buf = ik_input_buffer_create(ctx);

    // Add content that results in 3 physical lines (equals terminal height)
    res = ik_input_buffer_insert_codepoint(input_buf, 'a');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_newline(input_buf);
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'b');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_newline(input_buf);
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'c');
    ck_assert(is_ok(&res));
    ik_input_buffer_ensure_layout(input_buf, 80);
    size_t input_buf_rows = ik_input_buffer_get_physical_lines(input_buf);
    ck_assert_uint_eq(input_buf_rows, 3);  // 3 lines exactly

    // Create scrollback with some lines (but there's no room to show them)
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    res = ik_scrollback_append_line(scrollback, "scrollback line 1", 17);
    ck_assert(is_ok(&res));
    res = ik_scrollback_append_line(scrollback, "scrollback line 2", 17);
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->term = term;

    // Create agent context for display state
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    repl->current = agent;
    repl->current->input_buffer = input_buf;
    repl->current->scrollback = scrollback;
    repl->current->viewport_offset = 0;

    // Calculate viewport
    ik_viewport_t viewport;
    res = ik_repl_calculate_viewport(repl, &viewport);
    ck_assert(is_ok(&res));

    // Terminal has 3 rows, input buffer needs 3 rows
    // available_for_scrollback = 3 - 3 = 0
    // The false branch of "if (available_for_scrollback > 0)" executes
    // No scrollback should be shown (no room for separator or scrollback)
    ck_assert_uint_eq(viewport.scrollback_start_line, 0);
    ck_assert_uint_eq(viewport.scrollback_lines_count, 0);
    ck_assert_uint_eq(viewport.input_buffer_start_row, 0);

    talloc_free(ctx);
}

END_TEST

#ifdef IKIGAI_DEV
/* Test: Dev dump with NULL framebuffer */
START_TEST(test_dev_dump_null_framebuffer) {
    void *ctx = talloc_new(NULL);

    // Create minimal repl context
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->dev_framebuffer = NULL;
    repl->dev_framebuffer_len = 0;

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
    repl->dev_framebuffer = buffer;
    repl->dev_framebuffer_len = 0;  // Empty

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
    repl->dev_framebuffer = buffer;
    repl->dev_framebuffer_len = 9;
    repl->dev_cursor_row = 0;
    repl->dev_cursor_col = 0;

    // Should return early without crashing (no debug dir)
    ik_repl_dev_dump_framebuffer(repl);

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
    repl->dev_framebuffer = buffer;
    repl->dev_framebuffer_len = 21;
    repl->dev_cursor_row = 5;
    repl->dev_cursor_col = 10;

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
    repl->dev_framebuffer = buffer;
    repl->dev_framebuffer_len = 9;
    repl->dev_cursor_row = 0;
    repl->dev_cursor_col = 0;

    // Should return early without crashing (can't open file)
    ik_repl_dev_dump_framebuffer(repl);

    // Clean up
    chmod(".ikigai/debug", 0755);
    rmdir(".ikigai/debug");
    rmdir(".ikigai");

    talloc_free(ctx);
}
END_TEST
#endif

/* Create test suite */
static Suite *repl_viewport_suite(void)
{
    Suite *s = suite_create("REPL Viewport Calculation");

    TCase *tc_viewport = tcase_create("Viewport");
    tcase_set_timeout(tc_viewport, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_viewport, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_viewport, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_viewport, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_viewport, IK_TEST_TIMEOUT);
    tcase_add_test(tc_viewport, test_viewport_empty_scrollback);
    tcase_add_test(tc_viewport, test_viewport_small_scrollback);
    tcase_add_test(tc_viewport, test_viewport_large_scrollback);
    tcase_add_test(tc_viewport, test_viewport_offset_clamping);
    tcase_add_test(tc_viewport, test_viewport_no_scrollback_room);
    suite_add_tcase(s, tc_viewport);

#ifdef IKIGAI_DEV
    TCase *tc_dev_dump = tcase_create("Dev Framebuffer Dump");
    tcase_set_timeout(tc_dev_dump, IK_TEST_TIMEOUT);
    tcase_add_test(tc_dev_dump, test_dev_dump_null_framebuffer);
    tcase_add_test(tc_dev_dump, test_dev_dump_empty_framebuffer);
    tcase_add_test(tc_dev_dump, test_dev_dump_no_debug_dir);
    tcase_add_test(tc_dev_dump, test_dev_dump_success);
    tcase_add_test(tc_dev_dump, test_dev_dump_readonly_dir);
    suite_add_tcase(s, tc_dev_dump);
#endif

    return s;
}

int main(void)
{
    Suite *s = repl_viewport_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/repl/repl_viewport_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
