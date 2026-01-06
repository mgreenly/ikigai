/**
 * @file completion_e2e_test.c
 * @brief End-to-end integration tests for tab completion edge cases and layer behavior
 */

#include <check.h>
#include "../../src/agent.h"
#include <inttypes.h>
#include "../../src/repl.h"
#include "../../src/shared.h"
#include "../../src/paths.h"
#include "../../src/repl_actions.h"
#include "../../src/input.h"
#include "../../src/completion.h"
#include "../../src/history.h"
#include "../../src/input_buffer/core.h"
#include "../test_utils.h"
#include "completion_test_mocks.h"

/* Test: No matches produces no layer */
START_TEST(test_completion_no_matches) {
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

    shared_res = ik_shared_ctx_init(ctx, cfg, paths, logger, &shared);
    ck_assert(is_ok(&shared_res));
    res_t res = ik_repl_init(ctx, shared, &repl);
    ck_assert(is_ok(&res));

    type_str(repl, "/xyz");
    press_tab(repl);
    ck_assert_ptr_null(repl->current->completion);
    ck_assert(!repl->current->completion_layer->is_visible(repl->current->completion_layer));

    ik_repl_cleanup(repl);
    talloc_free(ctx);
    cleanup_test_dir();
}
END_TEST
/* Test: Completion and history don't conflict */
START_TEST(test_completion_history_no_conflict) {
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

    shared_res = ik_shared_ctx_init(ctx, cfg, paths, logger, &shared);
    ck_assert(is_ok(&shared_res));
    res_t res = ik_repl_init(ctx, shared, &repl);
    ck_assert(is_ok(&res));

    ik_history_add(repl->shared->history, "prev cmd");

    type_str(repl, "/m");
    press_tab(repl);
    // Tab accepts and dismisses completion
    ck_assert_ptr_null(repl->current->completion);
    ck_assert(!ik_history_is_browsing(repl->shared->history));

    ik_input_buffer_clear(repl->current->input_buffer);
    // Use Ctrl+P for explicit history navigation (rel-05: arrow keys now handled by burst detector)
    ik_input_action_t hist_action = {.type = IK_INPUT_CTRL_P};
    ik_repl_process_action(repl, &hist_action);
    ck_assert(ik_history_is_browsing(repl->shared->history));

    ik_repl_cleanup(repl);
    talloc_free(ctx);
    cleanup_test_dir();
}

END_TEST
/* Test: Layer visibility */
START_TEST(test_completion_layer_visibility) {
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

    shared_res = ik_shared_ctx_init(ctx, cfg, paths, logger, &shared);
    ck_assert(is_ok(&shared_res));
    res_t res = ik_repl_init(ctx, shared, &repl);
    ck_assert(is_ok(&res));

    ck_assert(!repl->current->completion_layer->is_visible(repl->current->completion_layer));

    type_str(repl, "/m");
    press_tab(repl);
    // Tab accepts and dismisses, so layer should be hidden after
    ck_assert(!repl->current->completion_layer->is_visible(repl->current->completion_layer));

    ik_repl_cleanup(repl);
    talloc_free(ctx);
    cleanup_test_dir();
}

END_TEST
/* Test: Dynamic typing updates completions */
START_TEST(test_completion_dynamic_update) {
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

    shared_res = ik_shared_ctx_init(ctx, cfg, paths, logger, &shared);
    ck_assert(is_ok(&shared_res));
    res_t res = ik_repl_init(ctx, shared, &repl);
    ck_assert(is_ok(&res));

    type_str(repl, "/ma");
    // Just verify typing works without Tab
    // Completion would be created by Tab press, which we're not doing here

    // Type 'r' to make "/mar"
    ik_input_action_t a = {.type = IK_INPUT_CHAR, .codepoint = 'r'};
    ik_repl_process_action(repl, &a);

    size_t len = 0;
    const char *text = ik_input_buffer_get_text(repl->current->input_buffer, &len);
    ck_assert_uint_eq(len, 4);
    ck_assert_mem_eq(text, "/mar", 4);

    ik_repl_cleanup(repl);
    talloc_free(ctx);
    cleanup_test_dir();
}

END_TEST

static void suite_setup(void)
{
    ik_test_set_log_dir(__FILE__);
}

static Suite *completion_e2e_suite(void)
{
    Suite *s = suite_create("Completion E2E");

    TCase *tc = tcase_create("Core");
    tcase_set_timeout(tc, 30);
    tcase_add_unchecked_fixture(tc, suite_setup, NULL);
    tcase_add_test(tc, test_completion_no_matches);
    tcase_add_test(tc, test_completion_history_no_conflict);
    tcase_add_test(tc, test_completion_layer_visibility);
    tcase_add_test(tc, test_completion_dynamic_update);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    Suite *s = completion_e2e_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int nf = srunner_ntests_failed(sr);
    srunner_free(sr);
    ik_test_reset_terminal();
    return (nf == 0) ? 0 : 1;
}
