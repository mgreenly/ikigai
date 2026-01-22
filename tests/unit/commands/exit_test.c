/**
 * @file exit_test.c
 * @brief Unit tests for /exit command
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
static Suite *commands_exit_suite(void);

// Test fixture
static void *ctx;
static ik_repl_ctx_t *repl;

/**
 * Create a minimal REPL context for command testing.
 */
static ik_repl_ctx_t *create_test_repl_for_commands(void *parent)
{
    // Create scrollback buffer (80 columns is standard)
    ik_scrollback_t *scrollback = ik_scrollback_create(parent, 80);
    ck_assert_ptr_nonnull(scrollback);

    // Create minimal config
    ik_config_t *cfg = talloc_zero(parent, ik_config_t);
    ck_assert_ptr_nonnull(cfg);

    // Create shared context
    ik_shared_ctx_t *shared = talloc_zero(parent, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    shared->cfg = cfg;

    // Create minimal REPL context
    ik_repl_ctx_t *r = talloc_zero(parent, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(r);

    // Create agent context
    ik_agent_ctx_t *agent = talloc_zero(r, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);
    agent->scrollback = scrollback;

    r->current = agent;
    r->shared = shared;

    return r;
}

static void setup(void)
{
    ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    repl = create_test_repl_for_commands(ctx);
    ck_assert_ptr_nonnull(repl);
}

static void teardown(void)
{
    talloc_free(ctx);
}

// Test: Exit command is recognized
START_TEST(test_exit_command_recognized) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/exit");
    ck_assert(is_ok(&res));
}
END_TEST

// Test: Exit command sets quit flag
START_TEST(test_exit_sets_quit_flag) {
    repl->quit = false;
    res_t res = ik_cmd_dispatch(ctx, repl, "/exit");
    ck_assert(is_ok(&res));
    ck_assert(repl->quit == true);
}
END_TEST

// Test: Exit command aborts in-flight LLM calls by invalidating providers
START_TEST(test_exit_aborts_llm_calls) {
    // Set up agents array with multiple agents
    repl->agents = talloc_array(repl, ik_agent_ctx_t *, 2);
    ck_assert_ptr_nonnull(repl->agents);
    repl->agent_count = 2;
    repl->agent_capacity = 2;

    // Create first agent with a simulated provider instance
    // We use a dummy talloc object to simulate a provider being present
    ik_agent_ctx_t *agent1 = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent1);
    void *dummy_provider1 = talloc_zero(agent1, char);
    agent1->provider_instance = (struct ik_provider *)dummy_provider1;
    repl->agents[0] = agent1;

    // Create second agent with a simulated provider instance
    ik_agent_ctx_t *agent2 = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent2);
    void *dummy_provider2 = talloc_zero(agent2, char);
    agent2->provider_instance = (struct ik_provider *)dummy_provider2;
    repl->agents[1] = agent2;

    // Execute /exit command
    res_t res = ik_cmd_dispatch(ctx, repl, "/exit");
    ck_assert(is_ok(&res));

    // Verify quit flag is set
    ck_assert(repl->quit == true);

    // Verify all provider instances were invalidated (freed and set to NULL)
    ck_assert_ptr_null(agent1->provider_instance);
    ck_assert_ptr_null(agent2->provider_instance);
}
END_TEST

static Suite *commands_exit_suite(void)
{
    Suite *s = suite_create("Commands/Exit");
    TCase *tc = tcase_create("Core");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);

    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_exit_command_recognized);
    tcase_add_test(tc, test_exit_sets_quit_flag);
    tcase_add_test(tc, test_exit_aborts_llm_calls);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = commands_exit_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/commands/exit_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
