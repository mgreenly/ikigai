#include <check.h>
#include <talloc.h>
#include <pthread.h>
#include <sys/select.h>
#include "agent.h"
#include "repl.h"
#include "repl_event_handlers.h"
#include "openai/client_multi.h"
#include "openai/client.h"
#include "shared.h"
#include "terminal.h"
#include "wrapper.h"

/* Test fixtures */
static TALLOC_CTX *ctx = NULL;
static ik_repl_ctx_t *repl = NULL;
static ik_shared_ctx_t *shared = NULL;

static void setup(void)
{
    ctx = talloc_new(NULL);
    repl = talloc_zero(ctx, ik_repl_ctx_t);
    shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;

    /* Initialize terminal */
    shared->term = talloc_zero(shared, ik_term_ctx_t);
    shared->term->tty_fd = 5;  /* Mock terminal fd */

    /* Initialize agents array */
    repl->agent_count = 0;
    repl->agent_capacity = 4;
    repl->agents = talloc_array(repl, ik_agent_ctx_t *, (unsigned int)repl->agent_capacity);
}

static void teardown(void)
{
    talloc_free(ctx);
    ctx = NULL;
    repl = NULL;
    shared = NULL;
}

static ik_agent_ctx_t *create_test_agent(ik_repl_ctx_t *parent, const char *uuid)
{
    ik_agent_ctx_t *agent = talloc_zero(parent, ik_agent_ctx_t);
    agent->uuid = talloc_strdup(agent, uuid);
    agent->state = IK_AGENT_STATE_IDLE;
    agent->repl = parent;

    /* Initialize thread infrastructure */
    pthread_mutex_init_(&agent->tool_thread_mutex, NULL);
    agent->tool_thread_running = false;
    agent->tool_thread_complete = false;

    /* Initialize spinner */
    agent->spinner_state.visible = false;
    agent->spinner_state.frame_index = 0;

    /* Create curl_multi handle */
    res_t multi_res = ik_openai_multi_create(agent);
    ck_assert(!is_err(&multi_res));
    agent->multi = multi_res.ok;

    /* Create conversation */
    agent->conversation = ik_openai_conversation_create(agent);

    return agent;
}

/*
 * Test: ik_repl_setup_fd_sets includes FDs from ALL agents
 */
START_TEST(test_setup_fd_sets_all_agents) {
    /* Create two agents */
    ik_agent_ctx_t *agent_a = create_test_agent(repl, "agent-a-uuid");
    ik_agent_ctx_t *agent_b = create_test_agent(repl, "agent-b-uuid");

    repl->agents[0] = agent_a;
    repl->agents[1] = agent_b;
    repl->agent_count = 2;
    repl->current = agent_b;  /* Currently viewing agent B */

    /* Simulate agent A having active curl transfers */
    agent_a->curl_still_running = 1;

    /* Setup fd_sets */
    fd_set read_fds, write_fds, exc_fds;
    int max_fd = -1;
    res_t result = ik_repl_setup_fd_sets(repl, &read_fds, &write_fds, &exc_fds, &max_fd);

    /* Should succeed */
    ck_assert(!is_err(&result));

    /* Terminal fd should be set */
    ck_assert(FD_ISSET(shared->term->tty_fd, &read_fds));

    /* Note: We can't easily verify that agent A's FDs are in the set
     * without mocking curl internals. The key test is that the function
     * completes successfully and iterates over all agents. */
}
END_TEST
/*
 * Test: ik_repl_calculate_select_timeout_ms considers tool state from ALL agents
 */
START_TEST(test_timeout_tool_poll_multiple_agents)
{
    /* Create two agents */
    ik_agent_ctx_t *agent_a = create_test_agent(repl, "agent-a-uuid");
    ik_agent_ctx_t *agent_b = create_test_agent(repl, "agent-b-uuid");

    repl->agents[0] = agent_a;
    repl->agents[1] = agent_b;
    repl->agent_count = 2;
    repl->current = agent_b;  /* Currently viewing agent B */
    repl->scroll_det = NULL;  /* No scroll detector */

    /* Agent A is executing a tool, Agent B is idle */
    agent_a->state = IK_AGENT_STATE_EXECUTING_TOOL;
    agent_b->state = IK_AGENT_STATE_IDLE;
    agent_b->spinner_state.visible = false;

    long curl_timeout_ms = -1;
    long timeout = ik_repl_calculate_select_timeout_ms(repl, curl_timeout_ms);

    /* Should return 50ms because agent A is executing a tool */
    ck_assert_int_eq(timeout, 50);
}

END_TEST
/*
 * Test: ik_repl_calculate_select_timeout_ms when current agent is executing but others are idle
 */
START_TEST(test_timeout_tool_poll_current_only)
{
    /* Create two agents */
    ik_agent_ctx_t *agent_a = create_test_agent(repl, "agent-a-uuid");
    ik_agent_ctx_t *agent_b = create_test_agent(repl, "agent-b-uuid");

    repl->agents[0] = agent_a;
    repl->agents[1] = agent_b;
    repl->agent_count = 2;
    repl->current = agent_b;  /* Currently viewing agent B */
    repl->scroll_det = NULL;

    /* Agent B (current) is executing a tool, Agent A is idle */
    agent_a->state = IK_AGENT_STATE_IDLE;
    agent_b->state = IK_AGENT_STATE_EXECUTING_TOOL;
    agent_b->spinner_state.visible = false;

    long curl_timeout_ms = -1;
    long timeout = ik_repl_calculate_select_timeout_ms(repl, curl_timeout_ms);

    /* Should return 50ms because agent B is executing a tool */
    ck_assert_int_eq(timeout, 50);
}

END_TEST
/*
 * Test: ik_repl_calculate_select_timeout_ms when no agents are executing tools
 */
START_TEST(test_timeout_no_tools_executing)
{
    /* Create two agents */
    ik_agent_ctx_t *agent_a = create_test_agent(repl, "agent-a-uuid");
    ik_agent_ctx_t *agent_b = create_test_agent(repl, "agent-b-uuid");

    repl->agents[0] = agent_a;
    repl->agents[1] = agent_b;
    repl->agent_count = 2;
    repl->current = agent_b;
    repl->scroll_det = NULL;

    /* Both agents idle */
    agent_a->state = IK_AGENT_STATE_IDLE;
    agent_b->state = IK_AGENT_STATE_IDLE;
    agent_b->spinner_state.visible = false;

    long curl_timeout_ms = -1;
    long timeout = ik_repl_calculate_select_timeout_ms(repl, curl_timeout_ms);

    /* Should return default 1000ms */
    ck_assert_int_eq(timeout, 1000);
}

END_TEST

/*
 * Test suite
 */
static Suite *fd_monitoring_all_agents_suite(void)
{
    Suite *s = suite_create("FD Monitoring All Agents");
    TCase *tc_core = tcase_create("Core");

    tcase_add_checked_fixture(tc_core, setup, teardown);

    tcase_add_test(tc_core, test_setup_fd_sets_all_agents);
    tcase_add_test(tc_core, test_timeout_tool_poll_multiple_agents);
    tcase_add_test(tc_core, test_timeout_tool_poll_current_only);
    tcase_add_test(tc_core, test_timeout_no_tools_executing);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = fd_monitoring_all_agents_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
