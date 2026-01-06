// Unit tests for REPL scrollback scrolling functionality

#include <check.h>
#include "../../../src/agent.h"
#include <talloc.h>
#include <string.h>
#include "../../../src/repl.h"
#include "../../../src/shared.h"
#include "../../../src/paths.h"
#include "../../../src/repl_actions.h"
#include "../../../src/scrollback.h"
#include "../../test_utils.h"
#include "../../../src/logger.h"
#include "../terminal/terminal_test_mocks.h"

static void suite_setup(void)
{
    ik_test_set_log_dir(__FILE__);
}

/* Test: Page Down scrolling decreases viewport_offset */
START_TEST(test_page_down_scrolling) {
    void *ctx = talloc_new(NULL);

    // Setup REPL with scrollback
    ik_repl_ctx_t *repl = NULL;
    ik_config_t *cfg = ik_test_create_config(ctx);
    // Create shared context
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    // Setup test paths
    test_paths_setup_env();
    ik_paths_t *paths = NULL;
    {
        res_t paths_res = ik_paths_init(ctx, &paths);
        ck_assert(is_ok(&paths_res));
    }

    res_t res = ik_shared_ctx_init(ctx, cfg, paths, logger, &shared);
    ck_assert(is_ok(&res));

    // Create REPL context
    res = ik_repl_init(ctx, shared, &repl);
    ck_assert(is_ok(&res));

    // Start scrolled up (viewport_offset = 48, i.e., 2 pages up)
    repl->current->viewport_offset = 48;

    // Simulate Page Down action
    ik_input_action_t action = {.type = IK_INPUT_PAGE_DOWN};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Should decrease by screen_rows (24)
    ck_assert_uint_eq(repl->current->viewport_offset, 24);

    // Cleanup
    talloc_free(ctx);
}

END_TEST

/* Test: Page Down at bottom stays at 0 */
START_TEST(test_page_down_at_bottom) {
    void *ctx = talloc_new(NULL);

    // Setup REPL with scrollback
    ik_repl_ctx_t *repl = NULL;
    ik_config_t *cfg = ik_test_create_config(ctx);
    // Create shared context
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    // Setup test paths
    test_paths_setup_env();
    ik_paths_t *paths = NULL;
    {
        res_t paths_res = ik_paths_init(ctx, &paths);
        ck_assert(is_ok(&paths_res));
    }

    res_t res = ik_shared_ctx_init(ctx, cfg, paths, logger, &shared);
    ck_assert(is_ok(&res));

    // Create REPL context
    res = ik_repl_init(ctx, shared, &repl);
    ck_assert(is_ok(&res));

    // Start at bottom (viewport_offset = 0)
    repl->current->viewport_offset = 0;

    // Simulate Page Down action
    ik_input_action_t action = {.type = IK_INPUT_PAGE_DOWN};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Should stay at 0
    ck_assert_uint_eq(repl->current->viewport_offset, 0);

    // Cleanup
    talloc_free(ctx);
}

END_TEST

/* Test: Page Down with small offset goes to 0 */
START_TEST(test_page_down_small_offset) {
    void *ctx = talloc_new(NULL);

    // Setup REPL with scrollback
    ik_repl_ctx_t *repl = NULL;
    ik_config_t *cfg = ik_test_create_config(ctx);
    // Create shared context
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    // Setup test paths
    test_paths_setup_env();
    ik_paths_t *paths = NULL;
    {
        res_t paths_res = ik_paths_init(ctx, &paths);
        ck_assert(is_ok(&paths_res));
    }

    res_t res = ik_shared_ctx_init(ctx, cfg, paths, logger, &shared);
    ck_assert(is_ok(&res));

    // Create REPL context
    res = ik_repl_init(ctx, shared, &repl);
    ck_assert(is_ok(&res));

    // Start with small offset (less than screen_rows)
    repl->current->viewport_offset = 10;

    // Simulate Page Down action
    ik_input_action_t action = {.type = IK_INPUT_PAGE_DOWN};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Should clamp to 0 (not go negative)
    ck_assert_uint_eq(repl->current->viewport_offset, 0);

    // Cleanup
    talloc_free(ctx);
}

END_TEST

/* Test: Page Up scrolling increases viewport_offset */
START_TEST(test_page_up_scrolling) {
    void *ctx = talloc_new(NULL);

    // Setup REPL with scrollback
    ik_repl_ctx_t *repl = NULL;
    ik_config_t *cfg = ik_test_create_config(ctx);
    // Create shared context
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    // Setup test paths
    test_paths_setup_env();
    ik_paths_t *paths = NULL;
    {
        res_t paths_res = ik_paths_init(ctx, &paths);
        ck_assert(is_ok(&paths_res));
    }

    res_t res = ik_shared_ctx_init(ctx, cfg, paths, logger, &shared);
    ck_assert(is_ok(&res));

    // Create REPL context
    res = ik_repl_init(ctx, shared, &repl);
    ck_assert(is_ok(&res));

    // Add some lines to scrollback to have content to scroll through
    for (int i = 0; i < 50; i++) {
        char line[100];
        snprintf(line, sizeof(line), "Line %d with some text content", i);
        res = ik_scrollback_append_line(repl->current->scrollback, line, strlen(line));
        ck_assert(is_ok(&res));
    }

    // Start at bottom (viewport_offset = 0)
    repl->current->viewport_offset = 0;

    // Simulate Page Up action
    ik_input_action_t action = {.type = IK_INPUT_PAGE_UP};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Should increase by screen_rows (24)
    ck_assert_uint_eq(repl->current->viewport_offset, 24);

    // Cleanup
    talloc_free(ctx);
}

END_TEST

/* Test: Page Up with empty scrollback stays at 0 */
START_TEST(test_page_up_empty_scrollback) {
    void *ctx = talloc_new(NULL);

    // Setup REPL with empty scrollback
    ik_repl_ctx_t *repl = NULL;
    ik_config_t *cfg = ik_test_create_config(ctx);
    // Create shared context
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    // Setup test paths
    test_paths_setup_env();
    ik_paths_t *paths = NULL;
    {
        res_t paths_res = ik_paths_init(ctx, &paths);
        ck_assert(is_ok(&paths_res));
    }

    res_t res = ik_shared_ctx_init(ctx, cfg, paths, logger, &shared);
    ck_assert(is_ok(&res));

    // Create REPL context
    res = ik_repl_init(ctx, shared, &repl);
    ck_assert(is_ok(&res));

    // Verify scrollback is empty
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 0);

    // Start at bottom (viewport_offset = 0)
    repl->current->viewport_offset = 0;

    // Simulate Page Up action
    ik_input_action_t action = {.type = IK_INPUT_PAGE_UP};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Should clamp to 0 (can't scroll up with no content)
    ck_assert_uint_eq(repl->current->viewport_offset, 0);

    // Cleanup
    talloc_free(ctx);
}

END_TEST

/* Test: Page Up clamping at max scrollback */
START_TEST(test_page_up_clamping) {
    void *ctx = talloc_new(NULL);

    // Setup REPL with scrollback (terminal is 24 rows from ik_repl_init)
    ik_repl_ctx_t *repl = NULL;
    ik_config_t *cfg = ik_test_create_config(ctx);
    // Create shared context
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    // Setup test paths
    test_paths_setup_env();
    ik_paths_t *paths = NULL;
    {
        res_t paths_res = ik_paths_init(ctx, &paths);
        ck_assert(is_ok(&paths_res));
    }

    res_t res = ik_shared_ctx_init(ctx, cfg, paths, logger, &shared);
    ck_assert(is_ok(&res));

    // Create REPL context
    res = ik_repl_init(ctx, shared, &repl);
    ck_assert(is_ok(&res));

    // Add enough lines to overflow terminal (30 lines > 24 terminal rows)
    for (int i = 0; i < 30; i++) {
        char line[100];
        snprintf(line, sizeof(line), "Line %d", i);
        res = ik_scrollback_append_line(repl->current->scrollback, line, strlen(line));
        ck_assert(is_ok(&res));
    }

    // With unified document model:
    // document_height = scrollback (30) + upper_separator (1) + MAX(input buffer, 1) + lower_separator (1) = 33 rows
    // input buffer always occupies at least 1 row (for cursor visibility when empty)
    // max_offset = 33 - 24 = 9
    size_t scrollback_rows = ik_scrollback_get_total_physical_lines(repl->current->scrollback);
    ik_input_buffer_ensure_layout(repl->current->input_buffer, repl->shared->term->screen_cols);
    size_t input_rows = ik_input_buffer_get_physical_lines(repl->current->input_buffer);
    size_t input_display_rows = (input_rows == 0) ? 1 : input_rows;
    size_t document_height = scrollback_rows + 1 + input_display_rows + 1;
    size_t expected_max = document_height - (size_t)repl->shared->term->screen_rows;

    // Start near top
    repl->current->viewport_offset = (expected_max > 10) ? expected_max - 10 : 0;

    // Simulate Page Up action (should hit ceiling)
    ik_input_action_t action = {.type = IK_INPUT_PAGE_UP};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Should clamp to max offset (document model)
    ck_assert_uint_eq(repl->current->viewport_offset, expected_max);

    // Cleanup
    talloc_free(ctx);
}

END_TEST

/* Create test suite */
static Suite *repl_scrollback_scroll_suite(void)
{
    Suite *s = suite_create("REPL Scrollback Scrolling");

    TCase *tc_page_down = tcase_create("Page Down");
    tcase_set_timeout(tc_page_down, 30);
    tcase_add_unchecked_fixture(tc_page_down, suite_setup, NULL);
    tcase_add_test(tc_page_down, test_page_down_scrolling);
    tcase_add_test(tc_page_down, test_page_down_at_bottom);
    tcase_add_test(tc_page_down, test_page_down_small_offset);
    suite_add_tcase(s, tc_page_down);

    TCase *tc_page_up = tcase_create("Page Up");
    tcase_set_timeout(tc_page_up, 30);
    tcase_add_unchecked_fixture(tc_page_up, suite_setup, NULL);
    tcase_add_test(tc_page_up, test_page_up_scrolling);
    tcase_add_test(tc_page_up, test_page_up_empty_scrollback);
    tcase_add_test(tc_page_up, test_page_up_clamping);
    suite_add_tcase(s, tc_page_up);

    return s;
}

int main(void)
{
    Suite *s = repl_scrollback_scroll_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
