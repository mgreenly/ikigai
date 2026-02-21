/**
 * @file model_thinking_test.c
 * @brief Unit tests for /model command - thinking levels and edge cases
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

// Test: Thinking level - min
START_TEST(test_model_thinking_none) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/model claude-sonnet-4-5/min");
    ck_assert(is_ok(&res));
    ck_assert_str_eq(repl->current->model, "claude-sonnet-4-5");
    ck_assert_str_eq(repl->current->provider, "anthropic");
    ck_assert_int_eq(repl->current->thinking_level, 0); // IK_THINKING_MIN

    // Verify feedback shows "min" with budget for Anthropic (line 2, after echo and blank)
    const char *line;
    size_t length;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 2, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert(strstr(line, "min") != NULL);
    ck_assert(strstr(line, "budget:") != NULL);
}

END_TEST
// Test: Thinking level - low (Anthropic extended thinking model)
START_TEST(test_model_thinking_low) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/model claude-sonnet-4-5/low");
    ck_assert(is_ok(&res));
    ck_assert_int_eq(repl->current->thinking_level, 1); // IK_THINKING_LOW

    // Verify feedback shows thinking budget for Anthropic (line 2, after echo and blank)
    const char *line;
    size_t length;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 2, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert(strstr(line, "low") != NULL);
    ck_assert(strstr(line, "budget:") != NULL);
}

END_TEST
// Test: Thinking level - med (Anthropic extended thinking model)
START_TEST(test_model_thinking_med) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/model claude-sonnet-4-5/med");
    ck_assert(is_ok(&res));
    ck_assert_int_eq(repl->current->thinking_level, 2); // IK_THINKING_MED

    // Verify feedback shows thinking budget for Anthropic (line 2, after echo and blank)
    const char *line;
    size_t length;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 2, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert(strstr(line, "med") != NULL);
    ck_assert(strstr(line, "budget:") != NULL);
}

END_TEST
// Test: Thinking level - high (Anthropic extended thinking model)
START_TEST(test_model_thinking_high) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/model claude-sonnet-4-5/high");
    ck_assert(is_ok(&res));
    ck_assert_int_eq(repl->current->thinking_level, 3); // IK_THINKING_HIGH

    // Verify feedback shows thinking budget for Anthropic (line 2, after echo and blank)
    const char *line;
    size_t length;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 2, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert(strstr(line, "high") != NULL);
    ck_assert(strstr(line, "budget:") != NULL);
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
// Test: Model switch during active LLM request
START_TEST(test_model_switch_during_request) {
    // Set agent state to waiting for LLM
    atomic_store(&repl->current->state, 1); // IK_AGENT_STATE_WAITING_FOR_LLM

    res_t res = ik_cmd_dispatch(ctx, repl, "/model gpt-4");
    ck_assert(is_err(&res));

    // Verify error message (line 2, after echo and blank)
    const char *line;
    size_t length;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 2, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert(strstr(line, "Cannot switch models during active request") != NULL);

    // Reset state for other tests
    atomic_store(&repl->current->state, 0);
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
    tcase_add_test(tc_core, test_model_switch_during_request);
    tcase_add_test(tc_core, test_model_parse_trailing_slash);
    tcase_add_test(tc_core, test_model_parse_empty_model);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = commands_model_thinking_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/commands/model_thinking_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
