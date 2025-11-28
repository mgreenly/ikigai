/**
 * @file repl_exact_user_scenario_test.c
 * @brief Test exact user scenario: 5-row terminal with A, B, C, D in scrollback
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include <unistd.h>
#include "../../../src/repl.h"
#include "../../../src/scrollback.h"
#include "../../../src/render.h"
#include "../../../src/input_buffer/core.h"
#include "../../test_utils.h"

/**
 * Test: Exact user scenario
 *
 * Terminal: 5 rows
 * Initial scrollback: A, B, C, D (4 lines)
 * At bottom: shows B, C, D, separator, input buffer
 * After Page Up: should show A, B, C, D, separator (input buffer off-screen)
 */
START_TEST(test_exact_user_scenario) {
    void *ctx = talloc_new(NULL);

    // Terminal: 5 rows x 80 cols
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    res_t res;
    term->screen_rows = 5;
    term->screen_cols = 80;

    // Create empty input buffer
    ik_input_buffer_t *input_buf = NULL;
    input_buf = ik_input_buffer_create(ctx);
    ik_input_buffer_ensure_layout(input_buf, 80);

    // Create scrollback with A, B, C, D
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    res = ik_scrollback_append_line(scrollback, "A", 1);
    ck_assert(is_ok(&res));
    res = ik_scrollback_append_line(scrollback, "B", 1);
    ck_assert(is_ok(&res));
    res = ik_scrollback_append_line(scrollback, "C", 1);
    ck_assert(is_ok(&res));
    res = ik_scrollback_append_line(scrollback, "D", 1);
    ck_assert(is_ok(&res));

    // Create render context
    ik_render_ctx_t *render_ctx = NULL;
    res = ik_render_create(ctx, 5, 80, 1, &render_ctx);
    ck_assert(is_ok(&res));

    // Create REPL at bottom (offset=0)
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->term = term;
    repl->input_buffer = input_buf;
    repl->scrollback = scrollback;
    repl->render = render_ctx;
    repl->viewport_offset = 0;

    // Document: 4 scrollback + 1 separator + 1 input buffer = 6 rows
    // (input buffer always occupies 1 row even when empty, for cursor visibility)
    // Terminal: 5 rows
    // At bottom (offset=0), showing rows 1-5:
    //   Row 0: A (off-screen)
    //   Row 1: B
    //   Row 2: C
    //   Row 3: D
    //   Row 4: separator
    //   Row 5: input buffer

    fprintf(stderr, "\n=== User Scenario: At Bottom ===\n");

    // Capture initial render
    int pipefd1[2];
    ck_assert_int_eq(pipe(pipefd1), 0);
    int saved_stdout1 = dup(1);
    dup2(pipefd1[1], 1);

    res = ik_repl_render_frame(repl);
    ck_assert(is_ok(&res));

    fflush(stdout);
    dup2(saved_stdout1, 1);
    close(pipefd1[1]);

    char output1[8192] = {0};
    ssize_t bytes_read1 = read(pipefd1[0], output1, sizeof(output1) - 1);
    ck_assert(bytes_read1 > 0);
    close(pipefd1[0]);
    close(saved_stdout1);

    fprintf(stderr, "Output at bottom:\n%s\n", output1);
    fprintf(stderr, "Contains B: %s\n", strstr(output1, "B") ? "YES" : "NO");
    fprintf(stderr, "Contains A: %s\n", strstr(output1, "A") ? "YES" : "NO");

    // At bottom: should see B, C, D, separator, input buffer (A is off-screen top)
    ck_assert_ptr_ne(strstr(output1, "B"), NULL);
    ck_assert_ptr_ne(strstr(output1, "C"), NULL);
    ck_assert_ptr_ne(strstr(output1, "D"), NULL);

    // Now press Page Up
    repl->viewport_offset = 5;

    fprintf(stderr, "\n=== After Page Up ===\n");

    // Capture render after Page Up
    int pipefd2[2];
    ck_assert_int_eq(pipe(pipefd2), 0);
    int saved_stdout2 = dup(1);
    dup2(pipefd2[1], 1);

    res = ik_repl_render_frame(repl);
    ck_assert(is_ok(&res));

    fflush(stdout);
    dup2(saved_stdout2, 1);
    close(pipefd2[1]);

    char output2[8192] = {0};
    ssize_t bytes_read2 = read(pipefd2[0], output2, sizeof(output2) - 1);
    ck_assert(bytes_read2 > 0);
    close(pipefd2[0]);
    close(saved_stdout2);

    fprintf(stderr, "Output after Page Up:\n%s\n", output2);
    fprintf(stderr, "Contains A: %s\n", strstr(output2, "A") ? "YES" : "NO");
    fprintf(stderr, "Contains B: %s\n", strstr(output2, "B") ? "YES" : "NO");
    fprintf(stderr, "Contains D: %s\n", strstr(output2, "D") ? "YES" : "NO");

    // After Page Up, should show A, B, C, D, separator (rows 0-4)
    // Input buffer is off-screen (row 5)
    ck_assert_ptr_ne(strstr(output2, "A"), NULL);
    ck_assert_ptr_ne(strstr(output2, "B"), NULL);
    ck_assert_ptr_ne(strstr(output2, "C"), NULL);
    ck_assert_ptr_ne(strstr(output2, "D"), NULL);

    talloc_free(ctx);
}
END_TEST

/* Create test suite */
static Suite *exact_scenario_suite(void)
{
    Suite *s = suite_create("Exact User Scenario");

    TCase *tc_scenario = tcase_create("Scenario");
    tcase_set_timeout(tc_scenario, 30);
    tcase_add_test(tc_scenario, test_exact_user_scenario);
    suite_add_tcase(s, tc_scenario);

    return s;
}

int main(void)
{
    Suite *s = exact_scenario_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
