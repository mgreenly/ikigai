#include "tests/test_constants.h"
#include "apps/ikigai/wrapper_pthread.h"
/**
 * @file all_agents_tool_polling_test.c
 * @brief Unit tests for polling tool completion across all agents
 *
 * Tests that the event loop polls tool_thread_complete for ALL agents,
 * not just repl->current. This enables background agents to complete
 * tools autonomously even when user has switched to another agent.
 */

#include "apps/ikigai/agent.h"
#include "apps/ikigai/message.h"
#include "apps/ikigai/providers/provider.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/repl_event_handlers.h"
#include "apps/ikigai/shared.h"
#include "apps/ikigai/scrollback.h"
#include "apps/ikigai/tool.h"
#include "shared/wrapper.h"
#include "apps/ikigai/db/message.h"

#include <check.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdatomic.h>
#include <talloc.h>
#include <unistd.h>

/* Forward declarations */
void ik_repl_handle_agent_tool_completion(ik_repl_ctx_t *repl, ik_agent_ctx_t *agent);

/* Mock for db message insert */
res_t ik_db_message_insert_(void *db, int64_t session_id, const char *agent_uuid,
                            const char *kind, const char *content, const char *data_json)
{
    (void)db; (void)session_id; (void)agent_uuid;
    (void)kind; (void)content; (void)data_json;
    return OK(NULL);
}

/* Thread function for tool execution - sets result and marks complete */
static void *tool_completion_thread_func(void *arg)
{
    ik_agent_ctx_t *agent = (ik_agent_ctx_t *)arg;

    /* Set result */
    agent->tool_thread_result = talloc_strdup(agent->tool_thread_ctx, "test result");

    /* Mark as complete */
    pthread_mutex_lock_(&agent->tool_thread_mutex);
    agent->tool_thread_complete = true;
    pthread_mutex_unlock_(&agent->tool_thread_mutex);

    return NULL;
}

static void *ctx;
static ik_repl_ctx_t *repl;
static ik_agent_ctx_t *agent_a;
static ik_agent_ctx_t *agent_b;

static void setup(void)
{
    ctx = talloc_new(NULL);

    /* Create minimal shared context */
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    shared->db_ctx = NULL;
    shared->session_id = 0;

    /* Create minimal REPL context */
    repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->shared = shared;
    repl->agents = NULL;
    repl->agent_count = 0;

    /* Create agent A */
    agent_a = talloc_zero(repl, ik_agent_ctx_t);
    agent_a->shared = shared;
    agent_a->repl = repl;
    agent_a->scrollback = ik_scrollback_create(agent_a, 80);
    atomic_store(&agent_a->state, IK_AGENT_STATE_EXECUTING_TOOL);

    agent_a->messages = NULL; agent_a->message_count = 0; agent_a->message_capacity = 0;

    pthread_mutex_init_(&agent_a->tool_thread_mutex, NULL);
    agent_a->tool_thread_running = true;
    agent_a->tool_thread_complete = false;  // Thread will set this
    agent_a->tool_thread_ctx = talloc_new(agent_a);
    agent_a->tool_thread_result = NULL;  // Thread will set this
    agent_a->tool_iteration_count = 0;

    /* Spawn actual thread that will complete and set the result */
    pthread_create_(&agent_a->tool_thread, NULL, tool_completion_thread_func, agent_a);

    /* Create pending tool call for agent A */
    agent_a->pending_tool_call = ik_tool_call_create(agent_a,
                                                     "call_a123",
                                                     "glob",
                                                     "{\"pattern\": \"*.c\"}");

    /* Create agent B */
    agent_b = talloc_zero(repl, ik_agent_ctx_t);
    agent_b->shared = shared;
    agent_b->repl = repl;
    agent_b->scrollback = ik_scrollback_create(agent_b, 80);
    atomic_store(&agent_b->state, IK_AGENT_STATE_IDLE);

    agent_b->messages = NULL; agent_b->message_count = 0; agent_b->message_capacity = 0;

    pthread_mutex_init_(&agent_b->tool_thread_mutex, NULL);
    agent_b->tool_thread_running = false;
    agent_b->tool_thread_complete = false;
    agent_b->tool_thread_result = NULL;
    agent_b->tool_thread_ctx = NULL;
    agent_b->pending_tool_call = NULL;
    agent_b->tool_iteration_count = 0;

    /* Add agents to repl array */
    repl->agent_count = 2;
    repl->agents = talloc_array(repl, ik_agent_ctx_t *, 2);
    repl->agents[0] = agent_a;
    repl->agents[1] = agent_b;

    /* Set repl->current to agent B (user switched away from A) */
    repl->current = agent_b;
}

static void teardown(void)
{
    talloc_free(ctx);
}

/**
 * Test: ik_repl_handle_agent_tool_completion operates on passed agent
 *
 * Scenario:
 * 1. Agent A has completed tool (EXECUTING_TOOL, tool_thread_complete=true)
 * 2. repl->current points to Agent B (user switched)
 * 3. Call ik_repl_handle_agent_tool_completion(repl, agent_a)
 * 4. Verify Agent A's tool was harvested and messages added
 * 5. Verify Agent B is unaffected
 */
START_TEST(test_handle_agent_tool_completion_uses_agent_param) {
    /* Wait for thread to complete */
    int32_t max_wait = 200;
    bool complete = false;
    for (int32_t i = 0; i < max_wait; i++) {
        pthread_mutex_lock_(&agent_a->tool_thread_mutex);
        complete = agent_a->tool_thread_complete;
        pthread_mutex_unlock_(&agent_a->tool_thread_mutex);
        if (complete) break;
        usleep(10000);  // 10ms
    }
    ck_assert(complete);

    /* Verify initial state */
    ck_assert_ptr_eq(repl->current, agent_b);
    ck_assert_int_eq(agent_a->state, IK_AGENT_STATE_EXECUTING_TOOL);
    ck_assert(agent_a->tool_thread_complete);
    ck_assert_ptr_nonnull(agent_a->tool_thread_result);
    ck_assert_uint_eq(agent_a->message_count, 0);
    ck_assert_uint_eq(agent_b->message_count, 0);

    /* Call ik_repl_handle_agent_tool_completion on Agent A */
    ik_repl_handle_agent_tool_completion(repl, agent_a);

    /* Verify Agent A's tool was completed */
    ck_assert_uint_eq(agent_a->message_count, 2);
    ck_assert(agent_a->messages[0]->role == IK_ROLE_ASSISTANT);
    ck_assert(agent_a->messages[0]->content_blocks[0].type == IK_CONTENT_TOOL_CALL);
    ck_assert(agent_a->messages[1]->role == IK_ROLE_TOOL);
    ck_assert(agent_a->messages[1]->content_blocks[0].type == IK_CONTENT_TOOL_RESULT);

    /* Verify Agent A transitioned to IDLE (no more tools to run) */
    ck_assert_int_eq(agent_a->state, IK_AGENT_STATE_IDLE);
    ck_assert(!agent_a->tool_thread_running);
    ck_assert_ptr_null(agent_a->pending_tool_call);

    /* Verify Agent B is still unaffected */
    ck_assert_uint_eq(agent_b->message_count, 0);
    ck_assert_int_eq(agent_b->state, IK_AGENT_STATE_IDLE);
}
END_TEST
/**
 * Test: Event loop polls all agents for tool completion
 *
 * This test simulates what would happen in the event loop:
 * - Agent A is in background with completed tool
 * - Agent B is current (user switched)
 * - Event loop iterates all agents and finds Agent A needs handling
 */
START_TEST(test_event_loop_polls_all_agents) {
    /* Wait for agent_a's thread to complete */
    int32_t max_wait = 200;
    bool complete = false;
    for (int32_t i = 0; i < max_wait; i++) {
        pthread_mutex_lock_(&agent_a->tool_thread_mutex);
        complete = agent_a->tool_thread_complete;
        pthread_mutex_unlock_(&agent_a->tool_thread_mutex);
        if (complete) break;
        usleep(10000);  // 10ms
    }
    ck_assert(complete);

    /* Simulate event loop polling all agents */
    for (size_t i = 0; i < repl->agent_count; i++) {
        ik_agent_ctx_t *agent = repl->agents[i];

        pthread_mutex_lock_(&agent->tool_thread_mutex);
        ik_agent_state_t state = agent->state;
        bool complete_flag = agent->tool_thread_complete;
        pthread_mutex_unlock_(&agent->tool_thread_mutex);

        if (state == IK_AGENT_STATE_EXECUTING_TOOL && complete_flag) {
            ik_repl_handle_agent_tool_completion(repl, agent);
        }
    }

    /* Verify Agent A was handled (was EXECUTING_TOOL with complete=true) */
    ck_assert_uint_eq(agent_a->message_count, 2);
    ck_assert_int_eq(agent_a->state, IK_AGENT_STATE_IDLE);

    /* Verify Agent B was not affected (was IDLE with complete=false) */
    ck_assert_uint_eq(agent_b->message_count, 0);
    ck_assert_int_eq(agent_b->state, IK_AGENT_STATE_IDLE);
}

END_TEST

/**
 * Test suite
 */
static Suite *all_agents_tool_polling_suite(void)
{
    Suite *s = suite_create("all_agents_tool_polling");

    TCase *tc_core = tcase_create("polling");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_handle_agent_tool_completion_uses_agent_param);
    tcase_add_test(tc_core, test_event_loop_polls_all_agents);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = all_agents_tool_polling_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/repl/all_agents_tool_polling_test.xml");
    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
