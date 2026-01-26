/**
 * @file model_thinking_test.c
 * @brief Unit tests for /model command - thinking levels and edge cases
 */

#include "../../../src/agent.h"
#include "../../../src/commands.h"
#include "../../../src/config.h"
#include "../../../src/shared.h"
#include "../../../src/error.h"
#include "../../../src/repl.h"
#include "../../../src/scrollback.h"
#include "../../test_utils_helper.h"

#include <check.h>
#include <talloc.h>

// Forward declaration for suite function
static Suite *commands_model_thinking_suite(void);

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

    repl = create_test_repl_with_config(ctx);
    ck_assert_ptr_nonnull(repl);
}

static void teardown(void)
{
    talloc_free(ctx);
}

// Test: Thinking level - none
START_TEST(test_model_thinking_none) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/model claude-sonnet-4-5/none");
    ck_assert(is_ok(&res));
    ck_assert_str_eq(repl->current->model, "claude-sonnet-4-5");
    ck_assert_str_eq(repl->current->provider, "anthropic");
    ck_assert_int_eq(repl->current->thinking_level, 0); // IK_THINKING_NONE

    // Verify feedback shows "disabled" (line 2, after echo and blank)
    const char *line;
    size_t length;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 2, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert(strstr(line, "disabled") != NULL);
}

END_TEST
// Test: Thinking level - low (Anthropic extended thinking model)
START_TEST(test_model_thinking_low) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/model claude-sonnet-4-5/low");
    ck_assert(is_ok(&res));
    ck_assert_int_eq(repl->current->thinking_level, 1); // IK_THINKING_LOW

    // Verify feedback shows thinking budget with tokens for Anthropic (line 2, after echo and blank)
    const char *line;
    size_t length;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 2, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert(strstr(line, "low") != NULL);
    ck_assert(strstr(line, "tokens") != NULL);
}

END_TEST
// Test: Thinking level - med (Anthropic extended thinking model)
START_TEST(test_model_thinking_med) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/model claude-sonnet-4-5/med");
    ck_assert(is_ok(&res));
    ck_assert_int_eq(repl->current->thinking_level, 2); // IK_THINKING_MED

    // Verify feedback shows thinking budget with tokens for Anthropic (line 2, after echo and blank)
    const char *line;
    size_t length;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 2, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert(strstr(line, "medium") != NULL);
    ck_assert(strstr(line, "tokens") != NULL);
}

END_TEST
// Test: Thinking level - high (Anthropic extended thinking model)
START_TEST(test_model_thinking_high) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/model claude-sonnet-4-5/high");
    ck_assert(is_ok(&res));
    ck_assert_int_eq(repl->current->thinking_level, 3); // IK_THINKING_HIGH

    // Verify feedback shows thinking budget with tokens for Anthropic (line 2, after echo and blank)
    const char *line;
    size_t length;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 2, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert(strstr(line, "high") != NULL);
    ck_assert(strstr(line, "tokens") != NULL);
}

END_TEST
// Test: Invalid thinking level
START_TEST(test_model_thinking_invalid) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/model claude-3-5-sonnet-20241022/invalid");
    ck_assert(is_err(&res));

    // Verify error message in scrollback (line 2, after echo and blank)
    const char *line;
    size_t length;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 2, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert(strstr(line, "Invalid thinking level") != NULL);
}

END_TEST
// Test: Google provider with thinking (budget-based model)
START_TEST(test_model_google_thinking) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/model gemini-2.5-flash/high");
    ck_assert(is_ok(&res));
    ck_assert_str_eq(repl->current->provider, "google");

    // Verify feedback shows thinking budget with tokens for Gemini 2.5 (line 2, after echo and blank)
    const char *line;
    size_t length;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 2, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert(strstr(line, "high") != NULL);
    ck_assert(strstr(line, "tokens") != NULL);
}

END_TEST
// Test: OpenAI GPT-5 with high thinking effort
START_TEST(test_model_openai_thinking) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/model gpt-5/high");
    ck_assert(is_ok(&res));
    ck_assert_str_eq(repl->current->provider, "openai");

    // Verify feedback shows thinking effort for GPT-5 (line 2, after echo and blank)
    const char *line;
    size_t length;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 2, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert(strstr(line, "high") != NULL);
    ck_assert(strstr(line, "effort") != NULL);
}

END_TEST
// Test: OpenAI GPT-5 with low thinking effort
START_TEST(test_model_openai_thinking_low) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/model gpt-5/low");
    ck_assert(is_ok(&res));
    ck_assert_str_eq(repl->current->provider, "openai");

    // Verify feedback shows low effort for GPT-5 (line 2, after echo and blank)
    const char *line;
    size_t length;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 2, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert(strstr(line, "low") != NULL);
    ck_assert(strstr(line, "effort") != NULL);
}

END_TEST
// Test: OpenAI GPT-5 with medium thinking effort
START_TEST(test_model_openai_thinking_med) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/model gpt-5/med");
    ck_assert(is_ok(&res));
    ck_assert_str_eq(repl->current->provider, "openai");

    // Verify feedback shows medium effort for GPT-5 (line 2, after echo and blank)
    const char *line;
    size_t length;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 2, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert(strstr(line, "medium") != NULL);
    ck_assert(strstr(line, "effort") != NULL);
}

END_TEST
// Test: OpenAI GPT-5 with none thinking effort (early return line 50-52)
START_TEST(test_model_openai_thinking_none) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/model gpt-5/none");
    ck_assert(is_ok(&res));
    ck_assert_str_eq(repl->current->provider, "openai");
    ck_assert_int_eq(repl->current->thinking_level, 0); // IK_THINKING_NONE

    // Verify feedback shows "disabled" (not "none effort") (line 2, after echo and blank)
    const char *line;
    size_t length;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 2, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert(strstr(line, "disabled") != NULL);
}

END_TEST
// Test: Warning for non-thinking model with thinking level
START_TEST(test_model_nothinking_with_level) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/model gpt-4/high");
    ck_assert(is_ok(&res));

    // Should have 4 lines: echo + blank + confirmation + warning
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 5);

    // Verify warning message (line 3, after echo + blank + confirmation)
    const char *line;
    size_t length;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 3, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert(strstr(line, "Warning") != NULL);
    ck_assert(strstr(line, "does not support thinking") != NULL);
}

END_TEST
// Test: Model switch during active LLM request
START_TEST(test_model_switch_during_request) {
    // Set agent state to waiting for LLM
    repl->current->state = 1; // IK_AGENT_STATE_WAITING_FOR_LLM

    res_t res = ik_cmd_dispatch(ctx, repl, "/model gpt-4");
    ck_assert(is_err(&res));

    // Verify error message (line 2, after echo and blank)
    const char *line;
    size_t length;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 2, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert(strstr(line, "Cannot switch models during active request") != NULL);

    // Reset state for other tests
    repl->current->state = 0;
}

END_TEST
// Test: Malformed input - trailing slash
START_TEST(test_model_parse_trailing_slash) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/model gpt-4/");
    ck_assert(is_err(&res));

    // Verify error message (line 2, after echo and blank)
    const char *line;
    size_t length;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 2, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert(strstr(line, "Malformed") != NULL);
    ck_assert(strstr(line, "trailing '/'") != NULL);
}

END_TEST
// Test: Malformed input - empty model name
START_TEST(test_model_parse_empty_model) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/model /high");
    ck_assert(is_err(&res));

    // Verify error message (line 2, after echo and blank)
    const char *line;
    size_t length;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 2, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert(strstr(line, "Malformed") != NULL);
    ck_assert(strstr(line, "empty model name") != NULL);
}

END_TEST
// Test: Google model with budget=0 (gemini-3.0-flash - level-based)
START_TEST(test_model_google_level_based) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/model gemini-3.0-flash/high");
    ck_assert(is_ok(&res));
    ck_assert_str_eq(repl->current->provider, "google");

    // Verify feedback shows "level" instead of tokens for Gemini 3.x (line 2, after echo and blank)
    const char *line;
    size_t length;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 2, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert(strstr(line, "high") != NULL);
    ck_assert(strstr(line, "level") != NULL);
}

END_TEST
// Test: Anthropic model with budget=0 (non-budget model)
START_TEST(test_model_anthropic_no_budget) {
    // Use claude-3-5-sonnet-20241022 which is not in capability table (budget=0)
    res_t res = ik_cmd_dispatch(ctx, repl, "/model claude-3-5-sonnet-20241022/high");
    ck_assert(is_ok(&res));
    ck_assert_str_eq(repl->current->provider, "anthropic");

    // Verify feedback shows "level" instead of tokens when budget=0 (line 2, after echo and blank)
    const char *line;
    size_t length;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 2, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert(strstr(line, "high") != NULL);
    ck_assert(strstr(line, "level") != NULL);
}

END_TEST

// Suite definition
static Suite *commands_model_thinking_suite(void)
{
    Suite *s = suite_create("commands_model_thinking");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_core, setup, teardown);

    tcase_add_test(tc_core, test_model_thinking_none);
    tcase_add_test(tc_core, test_model_thinking_low);
    tcase_add_test(tc_core, test_model_thinking_med);
    tcase_add_test(tc_core, test_model_thinking_high);
    tcase_add_test(tc_core, test_model_thinking_invalid);
    tcase_add_test(tc_core, test_model_google_thinking);
    tcase_add_test(tc_core, test_model_openai_thinking);
    tcase_add_test(tc_core, test_model_openai_thinking_low);
    tcase_add_test(tc_core, test_model_openai_thinking_med);
    tcase_add_test(tc_core, test_model_openai_thinking_none);
    tcase_add_test(tc_core, test_model_nothinking_with_level);
    tcase_add_test(tc_core, test_model_switch_during_request);
    tcase_add_test(tc_core, test_model_parse_trailing_slash);
    tcase_add_test(tc_core, test_model_parse_empty_model);
    tcase_add_test(tc_core, test_model_google_level_based);
    tcase_add_test(tc_core, test_model_anthropic_no_budget);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = commands_model_thinking_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/commands/model_thinking_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
