/**
 * @file model_test.c
 * @brief Unit tests for /model command
 */

#include "../../../src/commands.h"
#include "../../../src/config.h"
#include "../../../src/error.h"
#include "../../../src/repl.h"
#include "../../../src/scrollback.h"
#include "../../test_utils.h"

#include <check.h>
#include <talloc.h>

// Forward declaration for suite function
static Suite *commands_model_suite(void);

// Test fixture
static void *ctx;
static ik_repl_ctx_t *repl;

/**
 * Create a REPL context with config for model testing.
 */
static ik_repl_ctx_t *create_test_repl_with_config(void *parent)
{
    // Create scrollback buffer (80 columns is standard)
    ik_scrollback_t *scrollback = ik_scrollback_create(parent, 80);
    ck_assert_ptr_nonnull(scrollback);

    // Create config
    ik_cfg_t *cfg = talloc_zero(parent, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->openai_model = talloc_strdup(cfg, "gpt-5-mini");
    ck_assert_ptr_nonnull(cfg->openai_model);

    // Create minimal REPL context
    ik_repl_ctx_t *r = talloc_zero(parent, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(r);
    r->scrollback = scrollback;
    r->cfg = cfg;

    return r;
}

static void setup(void)
{
    ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    repl = create_test_repl_with_config(ctx);
    ck_assert_ptr_nonnull(repl);
}

static void teardown(void)
{
    talloc_free(ctx);
}

// Test: Switch to valid model
START_TEST(test_model_switch_gpt4) {
    // Verify initial model
    ck_assert_str_eq(repl->cfg->openai_model, "gpt-5-mini");

    // Execute /model gpt-4
    res_t res = ik_cmd_dispatch(ctx, repl, "/model gpt-4");
    ck_assert(is_ok(&res));

    // Verify model changed
    ck_assert_str_eq(repl->cfg->openai_model, "gpt-4");

    // Verify confirmation message in scrollback
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->scrollback), 1);
    const char *line;
    size_t length;
    res = ik_scrollback_get_line_text(repl->scrollback, 0, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(line);
    ck_assert_str_eq(line, "Switched to model: gpt-4");
}
END_TEST
// Test: Switch to gpt-4-turbo
START_TEST(test_model_switch_gpt4_turbo)
{
    res_t res = ik_cmd_dispatch(ctx, repl, "/model gpt-4-turbo");
    ck_assert(is_ok(&res));
    ck_assert_str_eq(repl->cfg->openai_model, "gpt-4-turbo");

    const char *line;
    size_t length;
    res = ik_scrollback_get_line_text(repl->scrollback, 0, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(line);
    ck_assert_str_eq(line, "Switched to model: gpt-4-turbo");
}

END_TEST
// Test: Switch to gpt-4o
START_TEST(test_model_switch_gpt4o)
{
    res_t res = ik_cmd_dispatch(ctx, repl, "/model gpt-4o");
    ck_assert(is_ok(&res));
    ck_assert_str_eq(repl->cfg->openai_model, "gpt-4o");
}

END_TEST
// Test: Switch to gpt-3.5-turbo
START_TEST(test_model_switch_gpt35_turbo)
{
    res_t res = ik_cmd_dispatch(ctx, repl, "/model gpt-3.5-turbo");
    ck_assert(is_ok(&res));
    ck_assert_str_eq(repl->cfg->openai_model, "gpt-3.5-turbo");
}

END_TEST
// Test: Switch to o1-mini
START_TEST(test_model_switch_o1_mini)
{
    res_t res = ik_cmd_dispatch(ctx, repl, "/model o1-mini");
    ck_assert(is_ok(&res));
    ck_assert_str_eq(repl->cfg->openai_model, "o1-mini");
}

END_TEST
// Test: Missing model name
START_TEST(test_model_missing_name)
{
    res_t res = ik_cmd_dispatch(ctx, repl, "/model");
    ck_assert(is_err(&res));

    // Verify error message in scrollback
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->scrollback), 1);
    const char *line;
    size_t length;
    res = ik_scrollback_get_line_text(repl->scrollback, 0, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(line);
    ck_assert_str_eq(line, "Error: Model name required (usage: /model <name>)");

    // Verify model unchanged
    ck_assert_str_eq(repl->cfg->openai_model, "gpt-5-mini");
}

END_TEST
// Test: Invalid model name
START_TEST(test_model_invalid_name)
{
    res_t res = ik_cmd_dispatch(ctx, repl, "/model invalid-model-xyz");
    ck_assert(is_err(&res));

    // Verify error message in scrollback
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->scrollback), 1);
    const char *line;
    size_t length;
    res = ik_scrollback_get_line_text(repl->scrollback, 0, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(line);
    ck_assert_str_eq(line, "Error: Unknown model 'invalid-model-xyz'");

    // Verify model unchanged
    ck_assert_str_eq(repl->cfg->openai_model, "gpt-5-mini");
}

END_TEST
// Test: Multiple switches (verify proper memory cleanup)
START_TEST(test_model_multiple_switches)
{
    // First switch
    res_t res = ik_cmd_dispatch(ctx, repl, "/model gpt-4");
    ck_assert(is_ok(&res));
    ck_assert_str_eq(repl->cfg->openai_model, "gpt-4");

    // Second switch
    res = ik_cmd_dispatch(ctx, repl, "/model gpt-3.5-turbo");
    ck_assert(is_ok(&res));
    ck_assert_str_eq(repl->cfg->openai_model, "gpt-3.5-turbo");

    // Third switch
    res = ik_cmd_dispatch(ctx, repl, "/model o1-mini");
    ck_assert(is_ok(&res));
    ck_assert_str_eq(repl->cfg->openai_model, "o1-mini");

    // Verify all three messages in scrollback
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->scrollback), 3);
}

END_TEST
// Test: Switch with extra whitespace before model name
START_TEST(test_model_with_whitespace)
{
    res_t res = ik_cmd_dispatch(ctx, repl, "/model   gpt-4");
    ck_assert(is_ok(&res));
    ck_assert_str_eq(repl->cfg->openai_model, "gpt-4");
}

END_TEST
// Test: All supported models
START_TEST(test_model_all_valid_models)
{
    const char *valid_models[] = {
        "gpt-4", "gpt-4-turbo", "gpt-4o", "gpt-4o-mini",
        "gpt-3.5-turbo", "gpt-5", "gpt-5-mini", "o1", "o1-mini", "o1-preview"
    };
    const size_t model_count = sizeof(valid_models) / sizeof(valid_models[0]);

    for (size_t i = 0; i < model_count; i++) {
        char *cmd = talloc_asprintf(ctx, "/model %s", valid_models[i]);
        res_t res = ik_cmd_dispatch(ctx, repl, cmd);
        ck_assert(is_ok(&res));
        ck_assert_str_eq(repl->cfg->openai_model, valid_models[i]);
        talloc_free(cmd);
    }

    // Verify all confirmations in scrollback
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->scrollback), model_count);
}

END_TEST

// Suite definition
static Suite *commands_model_suite(void)
{
    Suite *s = suite_create("commands_model");

    TCase *tc_core = tcase_create("Core");
    tcase_add_checked_fixture(tc_core, setup, teardown);

    tcase_add_test(tc_core, test_model_switch_gpt4);
    tcase_add_test(tc_core, test_model_switch_gpt4_turbo);
    tcase_add_test(tc_core, test_model_switch_gpt4o);
    tcase_add_test(tc_core, test_model_switch_gpt35_turbo);
    tcase_add_test(tc_core, test_model_switch_o1_mini);
    tcase_add_test(tc_core, test_model_missing_name);
    tcase_add_test(tc_core, test_model_invalid_name);
    tcase_add_test(tc_core, test_model_multiple_switches);
    tcase_add_test(tc_core, test_model_with_whitespace);
    tcase_add_test(tc_core, test_model_all_valid_models);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = commands_model_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
