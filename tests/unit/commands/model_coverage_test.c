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

// Suite definition
static Suite *commands_model_coverage_suite(void)
{
    Suite *s = suite_create("commands_model_coverage");

    TCase *tc_core = tcase_create("Core");
    tcase_add_checked_fixture(tc_core, setup, teardown);

    tcase_add_test(tc_core, test_model_switch_without_db);

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
