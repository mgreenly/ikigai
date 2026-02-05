/**
 * @file completion_args_test.c
 * @brief Completion argument matching integration tests
 */

#include <check.h>
#include "apps/ikigai/agent.h"
#include <inttypes.h>
#include "apps/ikigai/repl.h"
#include "apps/ikigai/shared.h"
#include "apps/ikigai/paths.h"
#include "shared/credentials.h"
#include "apps/ikigai/repl_actions.h"
#include "apps/ikigai/input.h"
#include "apps/ikigai/completion.h"
#include "apps/ikigai/history.h"
#include "apps/ikigai/input_buffer/core.h"
#include "tests/helpers/test_utils_helper.h"
#include "completion_test_mocks.h"

/* Test: Debug argument completion */
START_TEST(test_completion_debug_args) {
    cleanup_test_dir();
    void *ctx = talloc_new(NULL);
    ik_config_t *cfg = ik_test_create_config(ctx);
    cfg->history_size = 100;

    ik_repl_ctx_t *repl = NULL;
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

    res_t shared_res = ik_shared_ctx_init(ctx, cfg, creds, paths, logger, &shared);
    ck_assert(is_ok(&shared_res));
    res_t res = ik_repl_init(ctx, shared, &repl);
    ck_assert(is_ok(&res));

    type_str(repl, "/debug ");
    press_tab(repl);
    // Tab accepts first selection and dismisses completion
    ck_assert_ptr_null(repl->current->completion);

    // Tab selected one option (either "off" or "on"), now in input_buffer
    size_t len = 0;
    const char *text = ik_input_buffer_get_text(repl->current->input_buffer, &len);
    ck_assert(len > 7);  // "/debug " plus argument
    ck_assert_mem_eq(text, "/debug ", 7);

    ik_repl_cleanup(repl);
    talloc_free(ctx);
    cleanup_test_dir();
}
END_TEST
/* Test: Partial argument matching */
START_TEST(test_completion_partial_arg) {
    cleanup_test_dir();
    void *ctx = talloc_new(NULL);
    ik_config_t *cfg = ik_test_create_config(ctx);
    cfg->history_size = 100;

    ik_repl_ctx_t *repl = NULL;
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

    res_t shared_res = ik_shared_ctx_init(ctx, cfg, creds, paths, logger, &shared);
    ck_assert(is_ok(&shared_res));
    res_t res = ik_repl_init(ctx, shared, &repl);
    ck_assert(is_ok(&res));

    // Test argument completion - /model with any model
    type_str(repl, "/model ");
    press_tab(repl);
    // Tab accepts first selection and dismisses completion
    ck_assert_ptr_null(repl->current->completion);

    // Should have selected an argument
    size_t len = 0;
    const char *text = ik_input_buffer_get_text(repl->current->input_buffer, &len);
    ck_assert(len > 7);  // "/model " plus model name
    ck_assert_mem_eq(text, "/model ", 7);

    ik_repl_cleanup(repl);
    talloc_free(ctx);
    cleanup_test_dir();
}

END_TEST

static void suite_setup(void)
{
    ik_test_set_log_dir(__FILE__);
}

static Suite *completion_args_suite(void)
{
    Suite *s = suite_create("Completion Arguments");

    TCase *tc = tcase_create("Core");
    tcase_add_unchecked_fixture(tc, suite_setup, NULL);
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_test(tc, test_completion_debug_args);
    tcase_add_test(tc, test_completion_partial_arg);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    Suite *s = completion_args_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/integration/apps/ikigai/completion_args_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    int nf = srunner_ntests_failed(sr);
    srunner_free(sr);
    ik_test_reset_terminal();
    return (nf == 0) ? 0 : 1;
}
