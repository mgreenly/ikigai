// Unit tests for REPL scrollback integration

#include <check.h>
#include "../../../src/agent.h"
#include <talloc.h>
#include <string.h>
#include "../../../src/repl.h"
#include "../../../src/shared.h"
#include "../../../src/repl_actions.h"
#include "../../../src/scrollback.h"
#include "../../test_utils.h"
#include "../../../src/logger.h"
#include "../terminal/terminal_test_mocks.h"

static void suite_setup(void)
{
    ik_test_set_log_dir(__FILE__);
}

/* Test: REPL context can hold scrollback buffer */
START_TEST(test_repl_context_with_scrollback) {
    void *ctx = talloc_new(NULL);

    // Manually construct REPL context (like other tests do)
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(repl);

    // Create agent context for display state
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    repl->current = agent;

    // Create scrollback with terminal width of 80
    ik_scrollback_t *scrollback = ik_scrollback_create(repl, 80);
    ck_assert_ptr_nonnull(scrollback);

    // Assign to REPL context
    repl->current->scrollback = scrollback;
    repl->current->viewport_offset = 0;

    // Verify scrollback is accessible through REPL
    ck_assert_ptr_nonnull(repl->current->scrollback);
    ck_assert_uint_eq(repl->current->viewport_offset, 0);

    // Verify scrollback is empty initially
    size_t line_count = ik_scrollback_get_line_count(repl->current->scrollback);
    ck_assert_uint_eq(line_count, 0);

    // Cleanup
    talloc_free(ctx);
}
END_TEST
/* Test: REPL scrollback integration with terminal width */
START_TEST(test_repl_scrollback_terminal_width)
{
    void *ctx = talloc_new(NULL);

    // Create REPL context with mocked terminal
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    ck_assert_ptr_nonnull(term);
    term->screen_rows = 24;
    term->screen_cols = 120;

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(repl);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->term = term;

    // Create agent context for display state
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    repl->current = agent;

    // Create scrollback with terminal width
    ik_scrollback_t *scrollback = ik_scrollback_create(repl, term->screen_cols);
    ck_assert_ptr_nonnull(scrollback);

    repl->current->scrollback = scrollback;
    repl->current->viewport_offset = 0;

    // Verify scrollback uses correct terminal width
    ck_assert_int_eq(repl->current->scrollback->cached_width, 120);

    // Cleanup
    talloc_free(ctx);
}

END_TEST
/* Test: Page Down scrolling decreases viewport_offset */
START_TEST(test_page_down_scrolling)
{
    void *ctx = talloc_new(NULL);

    // Setup REPL with scrollback
    ik_repl_ctx_t *repl = NULL;
    ik_config_t *cfg = ik_test_create_config(ctx);
    // Create shared context
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t res = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared);
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
START_TEST(test_page_down_at_bottom)
{
    void *ctx = talloc_new(NULL);

    // Setup REPL with scrollback
    ik_repl_ctx_t *repl = NULL;
    ik_config_t *cfg = ik_test_create_config(ctx);
    // Create shared context
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t res = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared);
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
START_TEST(test_page_down_small_offset)
{
    void *ctx = talloc_new(NULL);

    // Setup REPL with scrollback
    ik_repl_ctx_t *repl = NULL;
    ik_config_t *cfg = ik_test_create_config(ctx);
    // Create shared context
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t res = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared);
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
START_TEST(test_page_up_scrolling)
{
    void *ctx = talloc_new(NULL);

    // Setup REPL with scrollback
    ik_repl_ctx_t *repl = NULL;
    ik_config_t *cfg = ik_test_create_config(ctx);
    // Create shared context
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t res = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared);
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
START_TEST(test_page_up_empty_scrollback)
{
    void *ctx = talloc_new(NULL);

    // Setup REPL with empty scrollback
    ik_repl_ctx_t *repl = NULL;
    ik_config_t *cfg = ik_test_create_config(ctx);
    // Create shared context
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t res = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared);
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
START_TEST(test_page_up_clamping)
{
    void *ctx = talloc_new(NULL);

    // Setup REPL with scrollback (terminal is 24 rows from ik_repl_init)
    ik_repl_ctx_t *repl = NULL;
    ik_config_t *cfg = ik_test_create_config(ctx);
    // Create shared context
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t res = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared);
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
/* Test: Submit line adds to scrollback and clears input buffer */
START_TEST(test_submit_line_to_scrollback)
{
    void *ctx = talloc_new(NULL);

    // Setup REPL
    ik_repl_ctx_t *repl = NULL;
    ik_config_t *cfg = ik_test_create_config(ctx);
    // Create shared context
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t res = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared);
    ck_assert(is_ok(&res));

    // Create REPL context
    res = ik_repl_init(ctx, shared, &repl);
    ck_assert(is_ok(&res));

    // Add some text to input buffer
    const char *test_text = "Hello, world!";
    for (size_t i = 0; test_text[i] != '\0'; i++) {
        ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = (uint32_t)test_text[i]};
        res = ik_repl_process_action(repl, &action);
        ck_assert(is_ok(&res));
    }

    // Verify input buffer has content
    size_t ws_len = ik_byte_array_size(repl->current->input_buffer->text);
    ck_assert_uint_gt(ws_len, 0);

    // Submit line
    res = ik_repl_submit_line(repl);
    ck_assert(is_ok(&res));

    // Verify scrollback has two lines (content + blank line)
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 2);

    // Verify input buffer is cleared
    ck_assert_uint_eq(ik_byte_array_size(repl->current->input_buffer->text), 0);

    // Cleanup
    talloc_free(ctx);
}

END_TEST
/* Test: Submit line resets viewport_offset (auto-scroll) */
START_TEST(test_submit_line_auto_scroll)
{
    void *ctx = talloc_new(NULL);

    // Setup REPL
    ik_repl_ctx_t *repl = NULL;
    ik_config_t *cfg = ik_test_create_config(ctx);
    // Create shared context
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t res = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared);
    ck_assert(is_ok(&res));

    // Create REPL context
    res = ik_repl_init(ctx, shared, &repl);
    ck_assert(is_ok(&res));

    // Scroll up (viewport_offset > 0)
    repl->current->viewport_offset = 100;

    // Add text to input buffer
    const char *test_text = "Test line";
    for (size_t i = 0; test_text[i] != '\0'; i++) {
        ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = (uint32_t)test_text[i]};
        res = ik_repl_process_action(repl, &action);
        ck_assert(is_ok(&res));
    }

    // Submit line
    res = ik_repl_submit_line(repl);
    ck_assert(is_ok(&res));

    // Verify viewport_offset is reset to 0 (auto-scroll to bottom)
    ck_assert_uint_eq(repl->current->viewport_offset, 0);

    // Cleanup
    talloc_free(ctx);
}

END_TEST
/* Test: Submit empty input buffer does not add to scrollback */
START_TEST(test_submit_empty_line)
{
    void *ctx = talloc_new(NULL);

    // Setup REPL
    ik_repl_ctx_t *repl = NULL;
    ik_config_t *cfg = ik_test_create_config(ctx);
    // Create shared context
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t res = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared);
    ck_assert(is_ok(&res));

    // Create REPL context
    res = ik_repl_init(ctx, shared, &repl);
    ck_assert(is_ok(&res));

    // Verify input buffer is empty
    ck_assert_uint_eq(ik_byte_array_size(repl->current->input_buffer->text), 0);

    // Submit empty line
    res = ik_repl_submit_line(repl);
    ck_assert(is_ok(&res));

    // Verify scrollback is still empty (no line added)
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 0);

    // Cleanup
    talloc_free(ctx);
}

END_TEST

/* Create test suite */
static Suite *repl_scrollback_suite(void)
{
    Suite *s = suite_create("REPL Scrollback Integration");

    TCase *tc_init = tcase_create("Initialization");
    tcase_set_timeout(tc_init, 30);
    tcase_set_timeout(tc_init, 30);
    tcase_set_timeout(tc_init, 30);
    tcase_set_timeout(tc_init, 30);
    tcase_add_unchecked_fixture(tc_init, suite_setup, NULL);
    tcase_set_timeout(tc_init, 30);
    tcase_add_test(tc_init, test_repl_context_with_scrollback);
    tcase_add_test(tc_init, test_repl_scrollback_terminal_width);
    suite_add_tcase(s, tc_init);

    TCase *tc_scroll = tcase_create("Scrolling");
    tcase_set_timeout(tc_scroll, 30);
    tcase_set_timeout(tc_scroll, 30);
    tcase_set_timeout(tc_scroll, 30);
    tcase_set_timeout(tc_scroll, 30);
    tcase_add_unchecked_fixture(tc_scroll, suite_setup, NULL);
    tcase_set_timeout(tc_scroll, 30);
    tcase_add_test(tc_scroll, test_page_down_scrolling);
    tcase_add_test(tc_scroll, test_page_down_at_bottom);
    tcase_add_test(tc_scroll, test_page_down_small_offset);
    tcase_add_test(tc_scroll, test_page_up_scrolling);
    tcase_add_test(tc_scroll, test_page_up_empty_scrollback);
    tcase_add_test(tc_scroll, test_page_up_clamping);
    suite_add_tcase(s, tc_scroll);

    TCase *tc_submit = tcase_create("Submit Line");
    tcase_set_timeout(tc_submit, 30);
    tcase_set_timeout(tc_submit, 30);
    tcase_set_timeout(tc_submit, 30);
    tcase_set_timeout(tc_submit, 30);
    tcase_add_unchecked_fixture(tc_submit, suite_setup, NULL);
    tcase_set_timeout(tc_submit, 30);
    tcase_add_test(tc_submit, test_submit_line_to_scrollback);
    tcase_add_test(tc_submit, test_submit_line_auto_scroll);
    tcase_add_test(tc_submit, test_submit_empty_line);
    suite_add_tcase(s, tc_submit);

    return s;
}

int main(void)
{
    Suite *s = repl_scrollback_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
