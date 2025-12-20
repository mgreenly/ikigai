/**
 * @file agent_tool_execution_test.c
 * @brief Unit tests for agent-based tool execution
 *
 * Tests that tool execution operates on a specific agent context
 * even when repl->current switches to a different agent.
 */

#include "agent.h"
#include "repl.h"
#include "shared.h"
#include "scrollback.h"
#include "openai/client.h"
#include "tool.h"
#include "wrapper.h"
#include "db/message.h"
#include <check.h>
#include <talloc.h>
#include <pthread.h>
#include <unistd.h>

/* Forward declarations for new functions we're testing */
void ik_agent_start_tool_execution(ik_agent_ctx_t *agent);
void ik_agent_complete_tool_execution(ik_agent_ctx_t *agent);

/* Mock for db message insert */
res_t ik_db_message_insert_(void *db, int64_t session_id, const char *agent_uuid,
                            const char *kind, const char *content, const char *data_json)
{
    (void)db; (void)session_id; (void)agent_uuid;
    (void)kind; (void)content; (void)data_json;
    return OK(NULL);
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

    /* Create agent A */
    agent_a = talloc_zero(repl, ik_agent_ctx_t);
    agent_a->shared = shared;
    agent_a->repl = repl;
    agent_a->scrollback = ik_scrollback_create(agent_a, 80);
    agent_a->state = IK_AGENT_STATE_WAITING_FOR_LLM;

    agent_a->conversation = ik_openai_conversation_create(agent_a);

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
    agent_b->state = IK_AGENT_STATE_IDLE;

    agent_b->conversation = ik_openai_conversation_create(agent_b);

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

    /* Verify agent A's conversation has tool messages */
    ck_assert_uint_eq(agent_a->conversation->message_count, 2);
    ck_assert_str_eq(agent_a->conversation->messages[0]->kind, "tool_call");
    ck_assert_str_eq(agent_a->conversation->messages[1]->kind, "tool_result");

    /* Verify agent B's conversation is still empty */
    ck_assert_uint_eq(agent_b->conversation->message_count, 0);

    /* Verify agent A's state transitioned correctly */
    ck_assert_int_eq(agent_a->state, IK_AGENT_STATE_WAITING_FOR_LLM);
    ck_assert(!agent_a->tool_thread_running);
    ck_assert_ptr_null(agent_a->pending_tool_call);
}
END_TEST
/**
 * Test: Start tool execution directly on agent (not via repl)
 */
START_TEST(test_start_tool_execution_on_agent)
{
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
    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
