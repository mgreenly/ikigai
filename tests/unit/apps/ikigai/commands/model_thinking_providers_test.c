/**
 * @file model_thinking_providers_test.c
 * @brief Unit tests for /model command - provider-specific thinking feedback
 */

#include "apps/ikigai/agent.h"
#include "apps/ikigai/commands.h"
#include "apps/ikigai/config.h"
#include "apps/ikigai/shared.h"
#include "shared/error.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/scrollback.h"
#include "tests/helpers/test_utils_helper.h"

#include <check.h>
#include <stdatomic.h>
#include <talloc.h>

// Forward declaration for suite function
static Suite *commands_model_thinking_providers_suite(void);

// Test fixture
static void *ctx;
static ik_repl_ctx_t *repl;

/**
 * Create a REPL context with config for model testing.
 */
static ik_repl_ctx_t *create_test_repl_with_config(void *parent)
{
    ik_scrollback_t *scrollback = ik_scrollback_create(parent, 80);
    ck_assert_ptr_nonnull(scrollback);

    ik_config_t *cfg = talloc_zero(parent, ik_config_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->openai_model = talloc_strdup(cfg, "gpt-5-mini");
    ck_assert_ptr_nonnull(cfg->openai_model);

    ik_repl_ctx_t *r = talloc_zero(parent, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(r);

    ik_shared_ctx_t *shared = talloc_zero(parent, ik_shared_ctx_t);
    shared->cfg = cfg;

    ik_agent_ctx_t *agent = talloc_zero(r, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);
    agent->scrollback = scrollback;
    agent->uuid = talloc_strdup(agent, "test-agent-uuid");
    agent->model = talloc_strdup(agent, "gpt-5-mini");
    agent->provider = talloc_strdup(agent, "openai");
    agent->thinking_level = 0;  // IK_THINKING_MIN
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

// Test: Google provider with thinking (budget-based model)
START_TEST(test_model_google_thinking) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/model gemini-2.5-flash/high");
    ck_assert(is_ok(&res));
    ck_assert_str_eq(repl->current->provider, "google");

    const char *line;
    size_t length;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 2, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert(strstr(line, "high") != NULL);
    ck_assert(strstr(line, "budget:") != NULL);
}

END_TEST

// Test: Google Gemini 2.5 with min level (should show budget)
START_TEST(test_model_google_thinking_none) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/model gemini-2.5-flash/min");
    ck_assert(is_ok(&res));
    ck_assert_str_eq(repl->current->provider, "google");

    const char *line;
    size_t length;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 2, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert(strstr(line, "min") != NULL);
    ck_assert(strstr(line, "budget:") != NULL);
}

END_TEST

// Test: Google Gemini 3.x level-based model
START_TEST(test_model_google_level_based) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/model gemini-3.0-flash/high");
    ck_assert(is_ok(&res));
    ck_assert_str_eq(repl->current->provider, "google");

    const char *line;
    size_t length;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 2, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert(strstr(line, "high") != NULL);
    ck_assert(strstr(line, "level:") != NULL);
}

END_TEST

// Test: Unknown Google model with typo
START_TEST(test_model_google_unknown_typo) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/model gemini-2.5-flash-light/low");
    ck_assert(is_err(&res));

    const char *line;
    size_t length;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 2, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert(strstr(line, "Error") != NULL);
    ck_assert(strstr(line, "Unknown") != NULL);
}

END_TEST

// Test: Unknown Google Gemini 2.5 model
START_TEST(test_model_google_unknown_2_5) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/model gemini-2.5-experimental/high");
    ck_assert(is_err(&res));

    const char *line;
    size_t length;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 2, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert(strstr(line, "Error") != NULL);
    ck_assert(strstr(line, "Unknown") != NULL);
}

END_TEST

// Test: Gemini 2.5 Pro with min level (cannot disable thinking)
START_TEST(test_model_google_pro_none_fails) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/model gemini-2.5-pro/min");
    ck_assert(is_err(&res));

    const char *line;
    size_t length;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 2, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert(strstr(line, "Error") != NULL);
    ck_assert(strstr(line, "cannot disable thinking") != NULL);
}

END_TEST

// Test: OpenAI GPT-5 with high thinking effort
START_TEST(test_model_openai_thinking) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/model gpt-5/high");
    ck_assert(is_ok(&res));
    ck_assert_str_eq(repl->current->provider, "openai");

    const char *line;
    size_t length;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 2, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert(strstr(line, "high") != NULL);
    ck_assert(strstr(line, "effort: high") != NULL);
}

END_TEST

// Test: OpenAI GPT-5 with low thinking effort
START_TEST(test_model_openai_thinking_low) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/model gpt-5/low");
    ck_assert(is_ok(&res));
    ck_assert_str_eq(repl->current->provider, "openai");

    const char *line;
    size_t length;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 2, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert(strstr(line, "low") != NULL);
    ck_assert(strstr(line, "effort: low") != NULL);
}

END_TEST

// Test: OpenAI GPT-5 with med thinking effort
START_TEST(test_model_openai_thinking_med) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/model gpt-5/med");
    ck_assert(is_ok(&res));
    ck_assert_str_eq(repl->current->provider, "openai");

    const char *line;
    size_t length;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 2, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert(strstr(line, "med") != NULL);
    ck_assert(strstr(line, "effort: medium") != NULL);
}

END_TEST

// Test: OpenAI GPT-5 with min thinking effort
START_TEST(test_model_openai_thinking_none) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/model gpt-5/min");
    ck_assert(is_ok(&res));
    ck_assert_str_eq(repl->current->provider, "openai");
    ck_assert_int_eq(repl->current->thinking_level, 0); // IK_THINKING_MIN

    const char *line;
    size_t length;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 2, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert(strstr(line, "min") != NULL);
    ck_assert(strstr(line, "effort: minimal") != NULL);
}

END_TEST

// Test: Anthropic adaptive model (claude-opus-4-6)
START_TEST(test_model_anthropic_adaptive) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/model claude-opus-4-6/high");
    ck_assert(is_ok(&res));
    ck_assert_str_eq(repl->current->provider, "anthropic");

    const char *line;
    size_t length;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 2, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert(strstr(line, "high") != NULL);
    ck_assert(strstr(line, "adaptive: high") != NULL);
}

END_TEST

// Test: Anthropic adaptive model with min (thinking omitted)
START_TEST(test_model_anthropic_adaptive_min) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/model claude-opus-4-6/min");
    ck_assert(is_ok(&res));
    ck_assert_str_eq(repl->current->provider, "anthropic");

    const char *line;
    size_t length;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 2, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert(strstr(line, "min") != NULL);
    ck_assert(strstr(line, "adaptive: none") != NULL);
}

END_TEST

// Test: Anthropic model not in budget table (uses default budget)
START_TEST(test_model_anthropic_no_budget) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/model claude-3-5-sonnet-20241022/high");
    ck_assert(is_ok(&res));
    ck_assert_str_eq(repl->current->provider, "anthropic");

    const char *line;
    size_t length;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 2, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert(strstr(line, "high") != NULL);
    ck_assert(strstr(line, "budget:") != NULL);
}

END_TEST

// Suite definition
static Suite *commands_model_thinking_providers_suite(void)
{
    Suite *s = suite_create("commands_model_thinking_providers");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_core, setup, teardown);

    tcase_add_test(tc_core, test_model_google_thinking);
    tcase_add_test(tc_core, test_model_google_thinking_none);
    tcase_add_test(tc_core, test_model_google_level_based);
    tcase_add_test(tc_core, test_model_google_unknown_typo);
    tcase_add_test(tc_core, test_model_google_unknown_2_5);
    tcase_add_test(tc_core, test_model_google_pro_none_fails);
    tcase_add_test(tc_core, test_model_openai_thinking);
    tcase_add_test(tc_core, test_model_openai_thinking_low);
    tcase_add_test(tc_core, test_model_openai_thinking_med);
    tcase_add_test(tc_core, test_model_openai_thinking_none);
    tcase_add_test(tc_core, test_model_anthropic_adaptive);
    tcase_add_test(tc_core, test_model_anthropic_adaptive_min);
    tcase_add_test(tc_core, test_model_anthropic_no_budget);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = commands_model_thinking_providers_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/commands/model_thinking_providers_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
