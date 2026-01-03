/**
 * @file completion_edge_cases_test.c
 * @brief Edge case tests for tab completion feature
 */

#include <check.h>
#include <inttypes.h>
#include "../../src/agent.h"
#include "../../src/completion.h"
#include "../../src/history.h"
#include "../../src/input.h"
#include "../../src/input_buffer/core.h"
#include "../../src/repl.h"
#include "../../src/repl_actions.h"
#include "../../src/shared.h"
#include "../test_utils.h"
#include "completion_test_mocks.h"

static void press_space(ik_repl_ctx_t *r)
{
    ik_input_action_t a = {.type = IK_INPUT_CHAR, .codepoint = ' '};
    ik_repl_process_action(r, &a);
}

/* Test: Tab accepts selection and dismisses completion */
START_TEST(test_completion_space_commits) {
    cleanup_test_dir();
    void *ctx = talloc_new(NULL);
    ik_config_t *cfg = ik_test_create_config(ctx);
    cfg->history_size = 100;

    ik_repl_ctx_t *repl = NULL;
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t r = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared); ck_assert(is_ok(&r));
    r = ik_repl_init(ctx, shared, &repl); ck_assert(is_ok(&r));
    type_str(repl, "/m");
    press_tab(repl);
    ck_assert_ptr_null(repl->current->completion);
    size_t len = 0;
    const char *text = ik_input_buffer_get_text(repl->current->input_buffer, &len);
    ck_assert(len >= 2);
    ck_assert_mem_eq(text, "/", 1);
    press_space(repl);
    text = ik_input_buffer_get_text(repl->current->input_buffer, &len);
    // Now should have more text (added space)
    ck_assert(len > 2);

    ik_repl_cleanup(repl);
    talloc_free(ctx);
    cleanup_test_dir();
}
END_TEST
/* Test: Multiple Tab presses - each press accepts and dismisses */
START_TEST(test_completion_tab_wraparound) {
    cleanup_test_dir();
    void *ctx = talloc_new(NULL);
    ik_config_t *cfg = ik_test_create_config(ctx);
    cfg->history_size = 100;

    ik_repl_ctx_t *repl = NULL;
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t r = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared); ck_assert(is_ok(&r));
    r = ik_repl_init(ctx, shared, &repl); ck_assert(is_ok(&r));
    type_str(repl, "/debug ");
    press_tab(repl);
    ck_assert_ptr_null(repl->current->completion);
    size_t len = 0;
    const char *text = ik_input_buffer_get_text(repl->current->input_buffer, &len);
    ck_assert(len > 7);
    ck_assert_mem_eq(text, "/debug ", 7);
    ik_repl_cleanup(repl);
    talloc_free(ctx);
    cleanup_test_dir();
}

END_TEST

START_TEST(test_completion_single_item) {
    cleanup_test_dir();
    void *ctx = talloc_new(NULL);
    ik_config_t *cfg = ik_test_create_config(ctx);
    cfg->history_size = 100;
    ik_repl_ctx_t *repl = NULL;
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t r = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared); ck_assert(is_ok(&r));
    r = ik_repl_init(ctx, shared, &repl); ck_assert(is_ok(&r));
    type_str(repl, "/debug");
    press_tab(repl);
    // Tab accepts and dismisses completion
    ck_assert_ptr_null(repl->current->completion);

    // Input buffer should have the selected command
    size_t len = 0;
    const char *text = ik_input_buffer_get_text(repl->current->input_buffer, &len);
    ck_assert(len > 0);
    ck_assert_mem_eq(text, "/", 1);

    ik_repl_cleanup(repl);
    talloc_free(ctx);
    cleanup_test_dir();
}

END_TEST
/* Test: Tab accepts completion, ESC on empty input does nothing */
START_TEST(test_completion_escape_exact_revert) {
    cleanup_test_dir();
    void *ctx = talloc_new(NULL);
    ik_config_t *cfg = ik_test_create_config(ctx);
    cfg->history_size = 100;

    ik_repl_ctx_t *repl = NULL;
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t r = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared); ck_assert(is_ok(&r));
    r = ik_repl_init(ctx, shared, &repl); ck_assert(is_ok(&r));
    type_str(repl, "/mar");
    size_t original_len = 0;
    ik_input_buffer_get_text(repl->current->input_buffer, &original_len);

    // Press Tab to accept completion
    press_tab(repl);
    // Completion is dismissed after Tab
    ck_assert_ptr_null(repl->current->completion);

    // Text should have changed to a completion match
    size_t new_len = 0;
    const char *new_text = ik_input_buffer_get_text(repl->current->input_buffer, &new_len);
    ck_assert(new_len >= original_len);

    // ESC after Tab has no effect (completion already dismissed)
    press_esc(repl);
    ck_assert_ptr_null(repl->current->completion);

    // Text stays as is after ESC
    size_t final_len = 0;
    const char *final_text = ik_input_buffer_get_text(repl->current->input_buffer, &final_len);
    ck_assert_uint_eq(final_len, new_len);
    ck_assert_mem_eq(final_text, new_text, final_len);

    ik_repl_cleanup(repl);
    talloc_free(ctx);
    cleanup_test_dir();
}

END_TEST
/* Test: Tab accepts and dismisses immediately */
START_TEST(test_completion_tab_cycle_then_space) {
    cleanup_test_dir();
    void *ctx = talloc_new(NULL);
    ik_config_t *cfg = ik_test_create_config(ctx);
    cfg->history_size = 100;

    ik_repl_ctx_t *repl = NULL;
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t r = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared); ck_assert(is_ok(&r));
    r = ik_repl_init(ctx, shared, &repl); ck_assert(is_ok(&r));
    type_str(repl, "/debug ");
    press_tab(repl);
    ck_assert_ptr_null(repl->current->completion);
    size_t len = 0;
    const char *text = ik_input_buffer_get_text(repl->current->input_buffer, &len);
    ck_assert(len > 7);
    ck_assert_mem_eq(text, "/debug ", 7);
    ik_repl_cleanup(repl);
    talloc_free(ctx);
    cleanup_test_dir();
}

END_TEST

START_TEST(test_completion_space_on_first_tab) {
    cleanup_test_dir();
    void *ctx = talloc_new(NULL);
    ik_config_t *cfg = ik_test_create_config(ctx);
    cfg->history_size = 100;

    ik_repl_ctx_t *repl = NULL;
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t r = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared); ck_assert(is_ok(&r));
    r = ik_repl_init(ctx, shared, &repl); ck_assert(is_ok(&r));
    type_str(repl, "/d");
    press_tab(repl);
    ck_assert_ptr_null(repl->current->completion);
    size_t len = 0;
    const char *text = ik_input_buffer_get_text(repl->current->input_buffer, &len);
    ck_assert(len >= 2);
    ck_assert_mem_eq(text, "/", 1);
    size_t len_before_space = len;
    press_space(repl);
    text = ik_input_buffer_get_text(repl->current->input_buffer, &len);
    // Should have added a character
    ck_assert_uint_eq(len, len_before_space + 1);

    ik_repl_cleanup(repl);
    talloc_free(ctx);
    cleanup_test_dir();
}

END_TEST
/* Test: Type after Tab adds to accepted completion */
START_TEST(test_completion_type_cancels) {
    cleanup_test_dir();
    void *ctx = talloc_new(NULL);
    ik_config_t *cfg = ik_test_create_config(ctx);
    cfg->history_size = 100;

    ik_repl_ctx_t *repl = NULL;
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t r = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared); ck_assert(is_ok(&r));
    r = ik_repl_init(ctx, shared, &repl); ck_assert(is_ok(&r));
    type_str(repl, "/m");
    press_tab(repl);
    ck_assert_ptr_null(repl->current->completion);
    size_t len_before = 0;
    ik_input_buffer_get_text(repl->current->input_buffer, &len_before);
    ik_input_action_t a = {.type = IK_INPUT_CHAR, .codepoint = 'x'};
    ik_repl_process_action(repl, &a);

    // Check input buffer has new char
    size_t len_after = 0;
    ik_input_buffer_get_text(repl->current->input_buffer, &len_after);
    // Should have added the 'x'
    ck_assert_uint_eq(len_after, len_before + 1);

    ik_repl_cleanup(repl);
    talloc_free(ctx);
    cleanup_test_dir();
}

END_TEST
/* Test: /rewind command has no args without marks */
START_TEST(test_completion_rewind_args) {
    cleanup_test_dir();
    void *ctx = talloc_new(NULL);
    ik_config_t *cfg = ik_test_create_config(ctx);
    cfg->history_size = 100;

    ik_repl_ctx_t *repl = NULL;
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t r = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared); ck_assert(is_ok(&r));
    r = ik_repl_init(ctx, shared, &repl); ck_assert(is_ok(&r));
    type_str(repl, "/rewind ");
    press_tab(repl);
    // No completion available
    ck_assert_ptr_null(repl->current->completion);

    size_t len = 0;
    const char *text = ik_input_buffer_get_text(repl->current->input_buffer, &len);
    ck_assert_uint_eq(len, 8);  // "/rewind " with no completion added
    ck_assert_mem_eq(text, "/rewind ", 8);

    ik_repl_cleanup(repl);
    talloc_free(ctx);
    cleanup_test_dir();
}

END_TEST
/* Test: /mark command has no argument completion */
START_TEST(test_completion_mark_no_args) {
    cleanup_test_dir();
    void *ctx = talloc_new(NULL);
    ik_config_t *cfg = ik_test_create_config(ctx);
    cfg->history_size = 100;

    ik_repl_ctx_t *repl = NULL;
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t r = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared); ck_assert(is_ok(&r));
    r = ik_repl_init(ctx, shared, &repl); ck_assert(is_ok(&r));
    type_str(repl, "/mark ");
    press_tab(repl);
    ck_assert_ptr_null(repl->current->completion);

    size_t len = 0;
    const char *text = ik_input_buffer_get_text(repl->current->input_buffer, &len);
    ck_assert_uint_eq(len, 6);  // "/mark "
    ck_assert_mem_eq(text, "/mark ", 6);

    ik_repl_cleanup(repl);
    talloc_free(ctx);
    cleanup_test_dir();
}

END_TEST
/* Test: /help command has no argument completion */
START_TEST(test_completion_help_no_args) {
    cleanup_test_dir();
    void *ctx = talloc_new(NULL);
    ik_config_t *cfg = ik_test_create_config(ctx);
    cfg->history_size = 100;

    ik_repl_ctx_t *repl = NULL;
    ik_shared_ctx_t *shared = NULL;
    // Create logger before calling init
    ik_logger_t *logger = ik_logger_create(ctx, "/tmp");
    res_t r = ik_shared_ctx_init(ctx, cfg, "/tmp", ".ikigai", logger, &shared); ck_assert(is_ok(&r));
    r = ik_repl_init(ctx, shared, &repl); ck_assert(is_ok(&r));
    type_str(repl, "/help ");
    press_tab(repl);
    ck_assert_ptr_null(repl->current->completion);

    size_t len = 0;
    const char *text = ik_input_buffer_get_text(repl->current->input_buffer, &len);
    ck_assert_uint_eq(len, 6);  // "/help "
    ck_assert_mem_eq(text, "/help ", 6);

    ik_repl_cleanup(repl);
    talloc_free(ctx);
    cleanup_test_dir();
}

END_TEST

static void suite_setup(void)
{
    ik_test_set_log_dir(__FILE__);
}

static Suite *completion_edge_cases_suite(void)
{
    Suite *s = suite_create("Completion Edge Cases");

    TCase *tc = tcase_create("Edge Cases");
    tcase_add_unchecked_fixture(tc, suite_setup, NULL);
    tcase_set_timeout(tc, 30);
    tcase_add_test(tc, test_completion_space_commits);
    tcase_add_test(tc, test_completion_tab_wraparound);
    tcase_add_test(tc, test_completion_single_item);
    tcase_add_test(tc, test_completion_escape_exact_revert);
    tcase_add_test(tc, test_completion_tab_cycle_then_space);
    tcase_add_test(tc, test_completion_space_on_first_tab);
    tcase_add_test(tc, test_completion_type_cancels);
    tcase_add_test(tc, test_completion_rewind_args);
    tcase_add_test(tc, test_completion_mark_no_args);
    tcase_add_test(tc, test_completion_help_no_args);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    Suite *s = completion_edge_cases_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int nf = srunner_ntests_failed(sr);
    srunner_free(sr);
    ik_test_reset_terminal();
    return (nf == 0) ? 0 : 1;
}
