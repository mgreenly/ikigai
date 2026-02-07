#include "tests/test_constants.h"
#include "apps/ikigai/wrapper_pthread.h"
/**
 * @file agent_tool_execution_test.c
 * @brief Unit tests for agent-based tool execution context
 *
 * Tests that tool execution operates on a specific agent context
 * even when repl->current switches to a different agent.
 */

#include "apps/ikigai/agent.h"
#include "apps/ikigai/message.h"
#include "apps/ikigai/providers/provider.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/shared.h"
#include "apps/ikigai/scrollback.h"
#include "apps/ikigai/tool.h"
#include "apps/ikigai/tool_registry.h"
#include "shared/json_allocator.h"
#include "shared/wrapper.h"
#include "apps/ikigai/db/message.h"
#include "vendor/yyjson/yyjson.h"
#include <check.h>
#include <talloc.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Forward declarations for new functions we're testing */
void ik_agent_start_tool_execution(ik_agent_ctx_t *agent);
void ik_agent_complete_tool_execution(ik_agent_ctx_t *agent);

/* Captured data from mock for verification */
static char *captured_tool_call_data_json;
static char *captured_tool_result_data_json;
static int db_insert_call_count;

/* Mock for db message insert - captures data_json for verification */
res_t ik_db_message_insert_(void *db, int64_t session_id, const char *agent_uuid,
                            const char *kind, const char *content, const char *data_json)
{
    (void)db; (void)session_id; (void)agent_uuid; (void)content;

    if (strcmp(kind, "tool_call") == 0) {
        if (captured_tool_call_data_json != NULL) {
            free(captured_tool_call_data_json);
        }
        captured_tool_call_data_json = data_json ? strdup(data_json) : NULL;
    } else if (strcmp(kind, "tool_result") == 0) {
        if (captured_tool_result_data_json != NULL) {
            free(captured_tool_result_data_json);
        }
        captured_tool_result_data_json = data_json ? strdup(data_json) : NULL;
    }
    db_insert_call_count++;
    return OK(NULL);
}

static void *ctx;
static ik_repl_ctx_t *repl;
static ik_agent_ctx_t *agent_a;
static ik_agent_ctx_t *agent_b;

static void setup(void)
{
    ctx = talloc_new(NULL);

    /* Reset captured data */
    if (captured_tool_call_data_json != NULL) {
        free(captured_tool_call_data_json);
        captured_tool_call_data_json = NULL;
    }
    if (captured_tool_result_data_json != NULL) {
        free(captured_tool_result_data_json);
        captured_tool_result_data_json = NULL;
    }
    db_insert_call_count = 0;

    /* Create minimal shared context */
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    shared->db_ctx = NULL;
    shared->session_id = 0;

    /* Create minimal REPL context */
    repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->shared = shared;

    /* Create agent A */
    agent_a = talloc_zero(repl, ik_agent_ctx_t);
    agent_a->shared = shared;
    agent_a->repl = repl;
    agent_a->scrollback = ik_scrollback_create(agent_a, 80);
    atomic_store(&agent_a->state, IK_AGENT_STATE_WAITING_FOR_LLM);

    /* Messages array starts empty in new API */
    agent_a->messages = NULL;
    agent_a->message_count = 0;
    agent_a->message_capacity = 0;

    pthread_mutex_init_(&agent_a->tool_thread_mutex, NULL);
    agent_a->tool_thread_running = false;
    agent_a->tool_thread_complete = false;
    agent_a->tool_thread_result = NULL;
    agent_a->tool_thread_ctx = NULL;

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

    /* Messages array starts empty in new API */
    agent_b->messages = NULL;
    agent_b->message_count = 0;
    agent_b->message_capacity = 0;

    pthread_mutex_init_(&agent_b->tool_thread_mutex, NULL);
    agent_b->tool_thread_running = false;
    agent_b->tool_thread_complete = false;
    agent_b->tool_thread_result = NULL;
    agent_b->tool_thread_ctx = NULL;
    agent_b->pending_tool_call = NULL;

    /* Set repl->current to agent_a initially */
    repl->current = agent_a;
}

static void teardown(void)
{
    talloc_free(ctx);

    /* Clean up captured data */
    if (captured_tool_call_data_json != NULL) {
        free(captured_tool_call_data_json);
        captured_tool_call_data_json = NULL;
    }
    if (captured_tool_result_data_json != NULL) {
        free(captured_tool_result_data_json);
        captured_tool_result_data_json = NULL;
    }
}

/**
 * Test: Tool execution targets specific agent, not repl->current
 *
 * Scenario:
 * 1. Start tool execution on agent A
 * 2. Switch repl->current to agent B (simulates user switching agents)
 * 3. Complete tool execution for agent A
 * 4. Verify agent A has the tool result, agent B is unaffected
 */
START_TEST(test_tool_execution_uses_agent_context) {
    /* Start tool execution on agent A */
    ik_agent_start_tool_execution(agent_a);

    /* Verify agent A's thread started */
    pthread_mutex_lock_(&agent_a->tool_thread_mutex);
    bool a_running = agent_a->tool_thread_running;
    pthread_mutex_unlock_(&agent_a->tool_thread_mutex);
    ck_assert(a_running);
    ck_assert_int_eq(agent_a->state, IK_AGENT_STATE_EXECUTING_TOOL);

    /* Switch repl->current to agent B (simulate user switch) */
    repl->current = agent_b;

    /* Wait for agent A's tool to complete */
    int max_wait = 12000;
    bool complete = false;
    for (int i = 0; i < max_wait; i++) {
        pthread_mutex_lock_(&agent_a->tool_thread_mutex);
        complete = agent_a->tool_thread_complete;
        pthread_mutex_unlock_(&agent_a->tool_thread_mutex);
        if (complete) break;
        usleep(10000);
    }
    ck_assert(complete);

    /* Verify agent A has result, agent B does not */
    ck_assert_ptr_nonnull(agent_a->tool_thread_result);
    ck_assert_ptr_null(agent_b->tool_thread_result);

    /* Complete agent A's tool execution */
    ik_agent_complete_tool_execution(agent_a);

    /* Verify agent A's messages has tool messages */
    ck_assert_uint_eq(agent_a->message_count, 2);
    ck_assert(agent_a->messages[0]->role == IK_ROLE_ASSISTANT);
    ck_assert(agent_a->messages[0]->content_blocks[0].type == IK_CONTENT_TOOL_CALL);
    ck_assert(agent_a->messages[1]->role == IK_ROLE_TOOL);
    ck_assert(agent_a->messages[1]->content_blocks[0].type == IK_CONTENT_TOOL_RESULT);

    /* Verify agent B's messages is still empty */
    ck_assert_uint_eq(agent_b->message_count, 0);

    /* Verify agent A's state transitioned correctly */
    ck_assert_int_eq(agent_a->state, IK_AGENT_STATE_WAITING_FOR_LLM);
    ck_assert(!agent_a->tool_thread_running);
    ck_assert_ptr_null(agent_a->pending_tool_call);
}
END_TEST
/**
 * Test: Start tool execution directly on agent (not via repl)
 */
START_TEST(test_start_tool_execution_on_agent) {
    /* Call start on agent A directly */
    ik_agent_start_tool_execution(agent_a);

    /* Verify thread started */
    pthread_mutex_lock_(&agent_a->tool_thread_mutex);
    bool running = agent_a->tool_thread_running;
    pthread_mutex_unlock_(&agent_a->tool_thread_mutex);

    ck_assert(running);
    ck_assert_ptr_nonnull(agent_a->tool_thread_ctx);
    ck_assert_int_eq(agent_a->state, IK_AGENT_STATE_EXECUTING_TOOL);

    /* Wait for completion and clean up */
    int max_wait = 12000;
    bool complete = false;
    for (int i = 0; i < max_wait; i++) {
        pthread_mutex_lock_(&agent_a->tool_thread_mutex);
        complete = agent_a->tool_thread_complete;
        pthread_mutex_unlock_(&agent_a->tool_thread_mutex);
        if (complete) break;
        usleep(10000);
    }

    ik_agent_complete_tool_execution(agent_a);
}

END_TEST

/**
 * Test suite
 */
static Suite *agent_tool_execution_suite(void)
{
    Suite *s = suite_create("agent_tool_execution");

    TCase *tc_core = tcase_create("agent_context");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_tool_execution_uses_agent_context);
    tcase_add_test(tc_core, test_start_tool_execution_on_agent);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = agent_tool_execution_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/repl/agent_tool_execution_test.xml");
    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
