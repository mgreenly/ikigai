/**
 * @file model_coverage_test.c
 * @brief Unit tests for coverage gaps in /model command
 */

#include "../../../src/agent.h"
#include "../../../src/commands.h"
#include "../../../src/commands_model.h"
#include "../../../src/config.h"
#include "../../../src/shared.h"
#include "../../../src/error.h"
#include "../../../src/repl.h"
#include "../../../src/scrollback.h"
#include "../../test_utils.h"

#include <check.h>
#include <talloc.h>

// Forward declaration for suite function
static Suite *commands_model_coverage_suite(void);

// Test fixture
static void *ctx;
static ik_repl_ctx_t *repl;

/**
 * Create a REPL context with config for model testing.
 */
static ik_repl_ctx_t *create_test_repl_with_config(void *parent, bool with_db)
{
    // Create scrollback buffer (80 columns is standard)
    ik_scrollback_t *scrollback = ik_scrollback_create(parent, 80);
    ck_assert_ptr_nonnull(scrollback);

    // Create config
    ik_config_t *cfg = talloc_zero(parent, ik_config_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->openai_model = talloc_strdup(cfg, "gpt-5-mini");
    ck_assert_ptr_nonnull(cfg->openai_model);

    // Create minimal REPL context
    ik_repl_ctx_t *r = talloc_zero(parent, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(r);

    // Create shared context
    ik_shared_ctx_t *shared = talloc_zero(parent, ik_shared_ctx_t);
    shared->cfg = cfg;
    shared->db_ctx = with_db ? (void *)0x1 : NULL; // Mock pointer or NULL

    // Create agent context
    ik_agent_ctx_t *agent = talloc_zero(r, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);
    agent->scrollback = scrollback;
    agent->uuid = talloc_strdup(agent, "test-agent-uuid");
    agent->model = talloc_strdup(agent, "gpt-5-mini");
    agent->provider = talloc_strdup(agent, "openai");
    agent->thinking_level = 0;  // IK_THINKING_NONE
    agent->shared = shared;
    r->current = agent;

    r->shared = shared;

    return r;
}

static void setup(void)
{
    ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    repl = create_test_repl_with_config(ctx, false);
    ck_assert_ptr_nonnull(repl);
}

static void teardown(void)
{
    talloc_free(ctx);
}

// Test: Model switch without database context (line 173 branch false)
// This covers the case where db_ctx is NULL, so the switch at line 175 is skipped
START_TEST(test_model_switch_without_db) {
    // Verify db_ctx is NULL
    ck_assert_ptr_null(repl->shared->db_ctx);

    // Switch model - should succeed without database persistence
    res_t res = ik_cmd_dispatch(ctx, repl, "/model gpt-4/high");
    ck_assert(is_ok(&res));

    // Verify model changed in memory
    ck_assert_str_eq(repl->current->model, "gpt-4");
    ck_assert_int_eq(repl->current->thinking_level, 3); // IK_THINKING_HIGH

    // Verify confirmation message
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 2);
}

END_TEST

// Test: Multiple model switches to trigger talloc_free branches (lines 151, 159)
START_TEST(test_model_multiple_switches_talloc_free) {
    // First switch - initial allocation
    res_t res = ik_cmd_dispatch(ctx, repl, "/model claude-sonnet-4-5");
    ck_assert(is_ok(&res));
    ck_assert_str_eq(repl->current->model, "claude-sonnet-4-5");
    ck_assert_str_eq(repl->current->provider, "anthropic");

    // Second switch - triggers talloc_free on lines 152 and 160
    res = ik_cmd_dispatch(ctx, repl, "/model gpt-4");
    ck_assert(is_ok(&res));
    ck_assert_str_eq(repl->current->model, "gpt-4");
    ck_assert_str_eq(repl->current->provider, "openai");

    // Third switch - again triggers talloc_free
    res = ik_cmd_dispatch(ctx, repl, "/model gemini-2.5-flash");
    ck_assert(is_ok(&res));
    ck_assert_str_eq(repl->current->model, "gemini-2.5-flash");
    ck_assert_str_eq(repl->current->provider, "google");

    // Verify all confirmations in scrollback
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 3);
}

END_TEST

// Test: Anthropic model with low thinking level (line 32, 58-61)
START_TEST(test_anthropic_model_low_thinking) {
    // Switch to Anthropic model with low thinking
    res_t res = ik_cmd_dispatch(ctx, repl, "/model claude-3-7-sonnet/low");
    ck_assert(is_ok(&res));
    ck_assert_str_eq(repl->current->model, "claude-3-7-sonnet");
    ck_assert_str_eq(repl->current->provider, "anthropic");
    ck_assert_int_eq(repl->current->thinking_level, 1); // IK_THINKING_LOW
}

END_TEST

// Test: Anthropic model with medium thinking level (line 34, 58-61)
START_TEST(test_anthropic_model_med_thinking) {
    // Switch to Anthropic model with medium thinking
    res_t res = ik_cmd_dispatch(ctx, repl, "/model claude-3-7-sonnet/med");
    ck_assert(is_ok(&res));
    ck_assert_str_eq(repl->current->model, "claude-3-7-sonnet");
    ck_assert_str_eq(repl->current->provider, "anthropic");
    ck_assert_int_eq(repl->current->thinking_level, 2); // IK_THINKING_MED
}

END_TEST

// Test: Google model with low thinking level (line 32, 62-65)
START_TEST(test_google_model_low_thinking) {
    // Switch to Google model with low thinking
    res_t res = ik_cmd_dispatch(ctx, repl, "/model gemini-2.5-flash/low");
    ck_assert(is_ok(&res));
    ck_assert_str_eq(repl->current->model, "gemini-2.5-flash");
    ck_assert_str_eq(repl->current->provider, "google");
    ck_assert_int_eq(repl->current->thinking_level, 1); // IK_THINKING_LOW
}

END_TEST

// Test: Google model with medium thinking level (line 34, 62-65)
START_TEST(test_google_model_med_thinking) {
    // Switch to Google model with medium thinking
    res_t res = ik_cmd_dispatch(ctx, repl, "/model gemini-2.5-flash/med");
    ck_assert(is_ok(&res));
    ck_assert_str_eq(repl->current->model, "gemini-2.5-flash");
    ck_assert_str_eq(repl->current->provider, "google");
    ck_assert_int_eq(repl->current->thinking_level, 2); // IK_THINKING_MED
}

END_TEST

// Test: OpenAI model with low effort (line 68-69)
START_TEST(test_openai_model_low_thinking) {
    // Switch to OpenAI model with low thinking
    res_t res = ik_cmd_dispatch(ctx, repl, "/model o1/low");
    ck_assert(is_ok(&res));
    ck_assert_str_eq(repl->current->model, "o1");
    ck_assert_str_eq(repl->current->provider, "openai");
    ck_assert_int_eq(repl->current->thinking_level, 1); // IK_THINKING_LOW
}

END_TEST

// Test: OpenAI model with medium effort (line 68-69)
START_TEST(test_openai_model_med_thinking) {
    // Switch to OpenAI model with medium thinking
    res_t res = ik_cmd_dispatch(ctx, repl, "/model o1/med");
    ck_assert(is_ok(&res));
    ck_assert_str_eq(repl->current->model, "o1");
    ck_assert_str_eq(repl->current->provider, "openai");
    ck_assert_int_eq(repl->current->thinking_level, 2); // IK_THINKING_MED
}

END_TEST

// Test: Model switch with none thinking level (line 130)
START_TEST(test_model_thinking_none) {
    // Switch to model with none thinking
    res_t res = ik_cmd_dispatch(ctx, repl, "/model gpt-4/none");
    ck_assert(is_ok(&res));
    ck_assert_str_eq(repl->current->model, "gpt-4");
    ck_assert_int_eq(repl->current->thinking_level, 0); // IK_THINKING_NONE
}

END_TEST

// Test: cmd_model_parse with trailing slash (line 276)
START_TEST(test_parse_trailing_slash) {
    char *model = NULL;
    char *thinking = NULL;
    res_t res = cmd_model_parse(ctx, "gpt-4/", &model, &thinking);
    ck_assert(is_err(&res));
    ck_assert_ptr_nonnull(res.err);
    ck_assert_str_eq(error_message(res.err), "Malformed input: trailing '/' with no thinking level");
    talloc_free(res.err);
}

END_TEST

// Test: cmd_model_parse with empty model name (line 282)
START_TEST(test_parse_empty_model) {
    char *model = NULL;
    char *thinking = NULL;
    res_t res = cmd_model_parse(ctx, "/low", &model, &thinking);
    ck_assert(is_err(&res));
    ck_assert_ptr_nonnull(res.err);
    ck_assert_str_eq(error_message(res.err), "Malformed input: empty model name");
    talloc_free(res.err);
}

END_TEST

// Test: Error path - switch model during active LLM request (line 94)
START_TEST(test_model_switch_during_llm_request) {
    // Set agent state to waiting for LLM
    repl->current->state = IK_AGENT_STATE_WAITING_FOR_LLM;

    // Attempt to switch model - should fail
    res_t res = ik_cmd_dispatch(ctx, repl, "/model gpt-4");
    ck_assert(is_err(&res));
    ck_assert_ptr_nonnull(res.err);
    ck_assert_str_eq(error_message(res.err), "Cannot switch models during active request");
    talloc_free(res.err);

    // Reset state
    repl->current->state = IK_AGENT_STATE_IDLE;
}

END_TEST

// Test: Error path - unknown model (line 118)
START_TEST(test_model_unknown_model) {
    // Attempt to switch to unknown model
    res_t res = ik_cmd_dispatch(ctx, repl, "/model unknown-model-xyz");
    ck_assert(is_err(&res));
    ck_assert_ptr_nonnull(res.err);
}

END_TEST

// Test: Error path - invalid thinking level (line 136-146)
START_TEST(test_model_invalid_thinking_level) {
    // Attempt to use invalid thinking level
    res_t res = ik_cmd_dispatch(ctx, repl, "/model gpt-4/invalid");
    ck_assert(is_err(&res));
    ck_assert_ptr_nonnull(res.err);
}

END_TEST

// Test: Model switch with NULL provider and model (lines 151, 159 false branches)
START_TEST(test_model_switch_null_provider_model) {
    // Set provider and model to NULL
    if (repl->current->provider != NULL) {
        talloc_free(repl->current->provider);
        repl->current->provider = NULL;
    }
    if (repl->current->model != NULL) {
        talloc_free(repl->current->model);
        repl->current->model = NULL;
    }

    // Switch model - should allocate new strings without freeing NULL
    res_t res = ik_cmd_dispatch(ctx, repl, "/model gpt-4");
    ck_assert(is_ok(&res));
    ck_assert_str_eq(repl->current->model, "gpt-4");
    ck_assert_str_eq(repl->current->provider, "openai");
}

END_TEST

// Suite definition
static Suite *commands_model_coverage_suite(void)
{
    Suite *s = suite_create("commands_model_coverage");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_core, setup, teardown);

    tcase_add_test(tc_core, test_model_switch_without_db);
    tcase_add_test(tc_core, test_model_multiple_switches_talloc_free);
    tcase_add_test(tc_core, test_anthropic_model_low_thinking);
    tcase_add_test(tc_core, test_anthropic_model_med_thinking);
    tcase_add_test(tc_core, test_google_model_low_thinking);
    tcase_add_test(tc_core, test_google_model_med_thinking);
    tcase_add_test(tc_core, test_openai_model_low_thinking);
    tcase_add_test(tc_core, test_openai_model_med_thinking);
    tcase_add_test(tc_core, test_model_thinking_none);
    tcase_add_test(tc_core, test_parse_trailing_slash);
    tcase_add_test(tc_core, test_parse_empty_model);
    tcase_add_test(tc_core, test_model_switch_during_llm_request);
    tcase_add_test(tc_core, test_model_unknown_model);
    tcase_add_test(tc_core, test_model_invalid_thinking_level);
    tcase_add_test(tc_core, test_model_switch_null_provider_model);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = commands_model_coverage_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
