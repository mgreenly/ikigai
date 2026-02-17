// Unit tests for REPL scrollback submit line functionality

#include <check.h>
#include "apps/ikigai/agent.h"
#include <talloc.h>
#include <string.h>
#include "apps/ikigai/repl.h"
#include "apps/ikigai/shared.h"
#include "apps/ikigai/paths.h"
#include "shared/credentials.h"
#include "apps/ikigai/repl_actions.h"
#include "apps/ikigai/scrollback.h"
#include "tests/helpers/test_utils_helper.h"
#include "shared/logger.h"
#include "tests/unit/shared/terminal/terminal_test_mocks.h"

static void suite_setup(void)
{
    ik_test_set_log_dir(__FILE__);
}

/* Test: Submit line adds to scrollback and clears input buffer */
START_TEST(test_submit_line_to_scrollback) {
    void *ctx = talloc_new(NULL);

    // Setup REPL
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

    ik_credentials_t *creds = talloc_zero_(ctx, sizeof(ik_credentials_t));
    if (creds == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    res_t res = ik_shared_ctx_init(ctx, cfg, creds, paths, logger, false, &shared);
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
START_TEST(test_submit_line_auto_scroll) {
    void *ctx = talloc_new(NULL);

    // Setup REPL
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

    ik_credentials_t *creds = talloc_zero_(ctx, sizeof(ik_credentials_t));
    if (creds == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    res_t res = ik_shared_ctx_init(ctx, cfg, creds, paths, logger, false, &shared);
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
START_TEST(test_submit_empty_line) {
    void *ctx = talloc_new(NULL);

    // Setup REPL
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

    ik_credentials_t *creds = talloc_zero_(ctx, sizeof(ik_credentials_t));
    if (creds == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    res_t res = ik_shared_ctx_init(ctx, cfg, creds, paths, logger, false, &shared);
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
static Suite *repl_scrollback_submit_suite(void)
{
    Suite *s = suite_create("REPL Scrollback Submit Line");

    TCase *tc_submit = tcase_create("Submit Line");
    tcase_set_timeout(tc_submit, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_submit, suite_setup, NULL);
    tcase_add_test(tc_submit, test_submit_line_to_scrollback);
    tcase_add_test(tc_submit, test_submit_line_auto_scroll);
    tcase_add_test(tc_submit, test_submit_empty_line);
    suite_add_tcase(s, tc_submit);

    return s;
}

int main(void)
{
    Suite *s = repl_scrollback_submit_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/repl/repl_scrollback_submit_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
