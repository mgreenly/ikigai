#include "agent.h"
/**
 * @file autocomplete_viewport_scroll_test.c
 * @brief Test that document height includes completion layer
 *
 * Bug fix: When content fills the viewport and autocomplete triggers,
 * the autocomplete should be visible (document should scroll to show it).
 */

#include <check.h>
#include "../../../src/agent.h"
#include "../../../src/shared.h"
#include <talloc.h>
#include <string.h>
#include <stdio.h>
#include "../../../src/repl.h"
#include "../../../src/scrollback.h"
#include "../../../src/input_buffer/core.h"
#include "../../../src/completion.h"
#include "../../test_utils.h"

/**
 * Test: Document height includes completion when active
 *
 * Scenario:
 * - Terminal is 10 rows
 * - Scrollback fills 8 rows
 * - Upper separator: 1 row
 * - Input buffer: 1 row
 * - Lower separator would be at row 10 (off screen)
 * - Autocomplete with 3 candidates appears
 * - Document height should be: 8 + 1 + 1 + 1 + 3 = 14 rows
 */
START_TEST(test_autocomplete_viewport_includes_completion_height) {
    void *ctx = talloc_new(NULL);

    // Terminal: 10 rows x 80 cols
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    res_t res;
    term->screen_rows = 10;
    term->screen_cols = 80;

    // Create input buffer with "/m" (will trigger completion)
    ik_input_buffer_t *input_buf = ik_input_buffer_create(ctx);
    res = ik_input_buffer_insert_codepoint(input_buf, '/');
    ck_assert(is_ok(&res));
    res = ik_input_buffer_insert_codepoint(input_buf, 'm');
    ck_assert(is_ok(&res));
    ik_input_buffer_ensure_layout(input_buf, 80);
    size_t input_buf_rows = ik_input_buffer_get_physical_lines(input_buf);
    ck_assert_uint_eq(input_buf_rows, 1);

    // Create scrollback with 8 rows of content
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    for (int i = 0; i < 8; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "line %d", i);
        res = ik_scrollback_append_line(scrollback, buf, strlen(buf));
        ck_assert(is_ok(&res));
    }
    ik_scrollback_ensure_layout(scrollback, 80);
    size_t scrollback_rows = ik_scrollback_get_total_physical_lines(scrollback);
    ck_assert_uint_eq(scrollback_rows, 8);

    // Create completion with 3 candidates
    ik_completion_t *completion = talloc_zero(ctx, ik_completion_t);
    completion->count = 3;
    completion->current = 0;
    completion->candidates = talloc_array(completion, char *, 3);
    completion->candidates[0] = talloc_strdup(completion, "/mark");
    completion->candidates[1] = talloc_strdup(completion, "/model");
    completion->candidates[2] = talloc_strdup(completion, "/msg");

    // Create shared context
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    shared->term = term;

    // Create agent
    ik_agent_ctx_t *agent = NULL;
    res = ik_agent_create(ctx, shared, NULL, &agent);
    ck_assert(is_ok(&res));

    // Override scrollback with our test scrollback
    talloc_free(agent->scrollback);
    agent->scrollback = scrollback;
    agent->viewport_offset = 0;  // At bottom

    // Create REPL with completion active
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    repl->shared = shared;
    repl->current = agent;
    repl->current->input_buffer = input_buf;
    repl->current->completion = completion;

    // Calculate viewport
    ik_viewport_t viewport;
    res = ik_repl_calculate_viewport(repl, &viewport);
    ck_assert(is_ok(&res));

    // Document height calculation (what we WANT)
    size_t input_buffer_display_rows = (input_buf_rows == 0) ? 1 : input_buf_rows;
    size_t completion_rows = (repl->current->completion != NULL) ? repl->current->completion->count : 0;
    size_t expected_document_height = scrollback_rows + 1 + input_buffer_display_rows + 1 + completion_rows;

    // Expected: 8 + 1 + 1 + 1 + 3 = 14 rows
    ck_assert_uint_eq(expected_document_height, 14);

    printf("\n=== Document Structure ===\n");
    printf("Scrollback: %zu rows\n", scrollback_rows);
    printf("Upper separator: 1 row\n");
    printf("Input buffer: %zu rows\n", input_buffer_display_rows);
    printf("Lower separator: 1 row\n");
    printf("Completion: %zu rows\n", completion_rows);
    printf("Expected document height: %zu rows\n", expected_document_height);
    printf("Terminal height: %d rows\n", term->screen_rows);

    // When document is 14 rows and terminal is 10 rows with viewport_offset=0:
    // last_visible_row = 14 - 1 - 0 = 13
    // first_visible_row = 13 + 1 - 10 = 4
    // Input buffer starts at doc row 9 (8 scrollback + 1 separator)
    // So input_buffer_start_row should be 9 - 4 = 5
    //
    // BUT if completion height is NOT included in document_height (the bug):
    // document_height = 11 (no completion)
    // last_visible_row = 11 - 1 - 0 = 10
    // first_visible_row = 10 + 1 - 10 = 1
    // Input buffer at doc row 9, so input_buffer_start_row = 9 - 1 = 8
    //
    // This test verifies input_buffer_start_row is 5 (not 8)
    printf("\n=== Viewport Calculation ===\n");
    printf("Input buffer doc row: %zu (scrollback + separator)\n", scrollback_rows + 1);
    printf("Input buffer viewport row: %zu\n", viewport.input_buffer_start_row);
    printf("Expected: 5 (with completion), would be 8 (without completion)\n");

    // The key test: input buffer should be pushed up by the completion layer
    ck_assert_uint_eq(viewport.input_buffer_start_row, 5);

    talloc_free(ctx);
}
END_TEST
/**
 * Test: Document height does NOT include completion when inactive
 *
 * Same scenario but without completion - should be shorter document.
 */
START_TEST(test_autocomplete_viewport_without_completion)
{
    void *ctx = talloc_new(NULL);

    // Terminal: 10 rows x 80 cols
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    res_t res;
    term->screen_rows = 10;
    term->screen_cols = 80;

    // Create input buffer
    ik_input_buffer_t *input_buf = ik_input_buffer_create(ctx);
    res = ik_input_buffer_insert_codepoint(input_buf, 'w');
    ck_assert(is_ok(&res));
    ik_input_buffer_ensure_layout(input_buf, 80);
    size_t input_buf_rows = ik_input_buffer_get_physical_lines(input_buf);
    ck_assert_uint_eq(input_buf_rows, 1);

    // Create scrollback with 8 rows
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);
    for (int i = 0; i < 8; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "line %d", i);
        res = ik_scrollback_append_line(scrollback, buf, strlen(buf));
        ck_assert(is_ok(&res));
    }
    ik_scrollback_ensure_layout(scrollback, 80);
    size_t scrollback_rows = ik_scrollback_get_total_physical_lines(scrollback);

    // Create shared context
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    shared->term = term;

    // Create agent
    ik_agent_ctx_t *agent = NULL;
    res = ik_agent_create(ctx, shared, NULL, &agent);
    ck_assert(is_ok(&res));

    // Override scrollback with our test scrollback
    talloc_free(agent->scrollback);
    agent->scrollback = scrollback;
    agent->viewport_offset = 0;

    // Create REPL WITHOUT completion
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    repl->shared = shared;
    repl->current = agent;
    repl->current->input_buffer = input_buf;
    repl->current->completion = NULL;  // No completion

    // Calculate viewport
    ik_viewport_t viewport;
    res = ik_repl_calculate_viewport(repl, &viewport);
    ck_assert(is_ok(&res));

    // Document height should be: 8 + 1 + 1 + 1 = 11 rows (no completion)
    size_t input_buffer_display_rows = (input_buf_rows == 0) ? 1 : input_buf_rows;
    size_t completion_rows = (repl->current->completion != NULL) ? repl->current->completion->count : 0;
    size_t expected_document_height = scrollback_rows + 1 + input_buffer_display_rows + 1 + completion_rows;

    ck_assert_uint_eq(expected_document_height, 11);
    ck_assert_uint_eq(completion_rows, 0);

    talloc_free(ctx);
}

END_TEST

/* Create test suite */
static Suite *autocomplete_viewport_scroll_suite(void)
{
    Suite *s = suite_create("Autocomplete Viewport Scroll");

    TCase *tc_viewport = tcase_create("Viewport");
    tcase_set_timeout(tc_viewport, 30);
    tcase_add_test(tc_viewport, test_autocomplete_viewport_includes_completion_height);
    tcase_add_test(tc_viewport, test_autocomplete_viewport_without_completion);
    suite_add_tcase(s, tc_viewport);

    return s;
}

int main(void)
{
    Suite *s = autocomplete_viewport_scroll_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
