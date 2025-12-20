/**
 * @file event_handlers_tool_poll_test.c
 * @brief Coverage tests for ik_repl_poll_tool_completions in repl_event_handlers.c
 *
 * Tests for Lines 431, 434: ik_repl_poll_tool_completions in multi-agent and single-agent modes
 */

#include "agent.h"
#include "input_buffer/core.h"
#include "openai/client.h"
#include "openai/client_multi.h"
#include "render.h"
#include "repl.h"
#include "repl_event_handlers.h"
#include "scrollback.h"
#include "shared.h"
#include "terminal.h"
#include "tool.h"
#include "wrapper.h"

#include <check.h>
#include <curl/curl.h>
#include <pthread.h>
#include <talloc.h>
#include <unistd.h>

/* Forward declarations */
void ik_repl_handle_agent_tool_completion(ik_repl_ctx_t *repl, ik_agent_ctx_t *agent);

/* Mock for db message insert */
res_t ik_db_message_insert_(void *db, int64_t session_id, const char *agent_uuid,
                            const char *kind, const char *content, const char *data_json)
{
    (void)db;
    (void)session_id;
    (void)agent_uuid;
    (void)kind;
    (void)content;
    (void)data_json;
    return OK(NULL);
}

/* Mock write for rendering */
ssize_t posix_write_(int fd, const void *buf, size_t count)
{
    (void)fd;
    (void)buf;
    return (ssize_t)count;
}

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
    shared->term->tty_fd = 5;
    shared->term->screen_rows = 24;
    shared->term->screen_cols = 80;

    /* Create render context */
    res_t render_res = ik_render_create(repl, 24, 80, 5, &shared->render);
    ck_assert(!render_res.is_err);

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
    agent->shared = parent->shared;

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

    /* Create scrollback */
    agent->scrollback = ik_scrollback_create(agent, 80);

    /* Create input buffer for rendering */
    agent->input_buffer = ik_input_buffer_create(agent);

    return agent;
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

/*
 * Test: ik_repl_poll_tool_completions in multi-agent mode (line 431)
 * This covers the branch where we call ik_repl_handle_agent_tool_completion for a completed agent
 */
START_TEST(test_poll_tool_completions_multi_agent_mode)
{
    /* Create two agents */
    ik_agent_ctx_t *agent_a = create_test_agent(repl, "agent-a");
    ik_agent_ctx_t *agent_b = create_test_agent(repl, "agent-b");

    repl->agents[0] = agent_a;
    repl->agents[1] = agent_b;
    repl->agent_count = 2;
    repl->current = agent_b;

    /* Setup agent A with a completed tool */
    agent_a->state = IK_AGENT_STATE_EXECUTING_TOOL;
    agent_a->tool_thread_running = true;
    agent_a->tool_thread_complete = false;
    agent_a->tool_thread_ctx = talloc_new(agent_a);
    agent_a->tool_iteration_count = 0;

    /* Create pending tool call for agent A */
    agent_a->pending_tool_call = ik_tool_call_create(agent_a,
                                                      "call_a123",
                                                      "glob",
                                                      "{\"pattern\": \"*.c\"}");

    /* Spawn thread that will complete */
    pthread_create_(&agent_a->tool_thread, NULL, tool_completion_thread_func, agent_a);

    /* Wait for thread to complete */
    int32_t max_wait = 200;
    bool complete = false;
    for (int32_t i = 0; i < max_wait; i++) {
        pthread_mutex_lock_(&agent_a->tool_thread_mutex);
        complete = agent_a->tool_thread_complete;
        pthread_mutex_unlock_(&agent_a->tool_thread_mutex);
        if (complete) break;
        usleep(10000);
    }
    ck_assert(complete);

    /* Call ik_repl_poll_tool_completions */
    res_t result = ik_repl_poll_tool_completions(repl);

    /* Should succeed */
    ck_assert(!is_err(&result));

    /* Verify agent A was handled - state should transition to IDLE */
    ck_assert_int_eq(agent_a->state, IK_AGENT_STATE_IDLE);
    ck_assert_ptr_null(agent_a->pending_tool_call);
    ck_assert_uint_eq(agent_a->conversation->message_count, 2);
}
END_TEST

/*
 * Test: ik_repl_poll_tool_completions in single-agent mode (line 434)
 * This covers the branch where agent_count == 0 and we check current
 */
START_TEST(test_poll_tool_completions_single_agent_mode)
{
    /* Set agent_count to 0 (single-agent/test mode) */
    repl->agent_count = 0;

    /* Create current agent with completed tool */
    ik_agent_ctx_t *current = create_test_agent(repl, "current-agent");
    repl->current = current;

    current->state = IK_AGENT_STATE_EXECUTING_TOOL;
    current->tool_thread_running = true;
    current->tool_thread_complete = false;
    current->tool_thread_ctx = talloc_new(current);
    current->tool_iteration_count = 0;

    /* Create pending tool call */
    current->pending_tool_call = ik_tool_call_create(current,
                                                      "call_c123",
                                                      "glob",
                                                      "{\"pattern\": \"*.h\"}");

    /* Spawn thread that will complete */
    pthread_create_(&current->tool_thread, NULL, tool_completion_thread_func, current);

    /* Wait for thread to complete */
    int32_t max_wait = 200;
    bool complete = false;
    for (int32_t i = 0; i < max_wait; i++) {
        pthread_mutex_lock_(&current->tool_thread_mutex);
        complete = current->tool_thread_complete;
        pthread_mutex_unlock_(&current->tool_thread_mutex);
        if (complete) break;
        usleep(10000);
    }
    ck_assert(complete);

    /* Call ik_repl_poll_tool_completions */
    res_t result = ik_repl_poll_tool_completions(repl);

    /* Should succeed */
    ck_assert(!is_err(&result));

    /* Verify current was handled */
    ck_assert_int_eq(current->state, IK_AGENT_STATE_IDLE);
    ck_assert_ptr_null(current->pending_tool_call);
    ck_assert_uint_eq(current->conversation->message_count, 2);
}
END_TEST

/*
 * Test: ik_repl_poll_tool_completions with agent NOT executing tool (line 430 false branch)
 * This covers the case where we don't call ik_repl_handle_agent_tool_completion
 */
START_TEST(test_poll_tool_completions_agent_not_executing)
{
    /* Create two agents, both IDLE */
    ik_agent_ctx_t *agent_a = create_test_agent(repl, "agent-a");
    ik_agent_ctx_t *agent_b = create_test_agent(repl, "agent-b");

    repl->agents[0] = agent_a;
    repl->agents[1] = agent_b;
    repl->agent_count = 2;
    repl->current = agent_a;

    /* Set states to IDLE */
    agent_a->state = IK_AGENT_STATE_IDLE;
    agent_b->state = IK_AGENT_STATE_IDLE;

    /* Call ik_repl_poll_tool_completions */
    res_t result = ik_repl_poll_tool_completions(repl);

    /* Should succeed */
    ck_assert(!is_err(&result));

    /* Verify no state changes occurred */
    ck_assert_int_eq(agent_a->state, IK_AGENT_STATE_IDLE);
    ck_assert_int_eq(agent_b->state, IK_AGENT_STATE_IDLE);
}
END_TEST

/*
 * Test: ik_repl_poll_tool_completions with agent EXECUTING but not complete (line 430 branch 3)
 * This covers the case where state is EXECUTING_TOOL but complete is false
 */
START_TEST(test_poll_tool_completions_executing_not_complete)
{
    /* Create one agent */
    ik_agent_ctx_t *agent_a = create_test_agent(repl, "agent-a");

    repl->agents[0] = agent_a;
    repl->agent_count = 1;
    repl->current = agent_a;

    /* Set state to EXECUTING_TOOL but complete to false */
    agent_a->state = IK_AGENT_STATE_EXECUTING_TOOL;
    agent_a->tool_thread_complete = false;

    /* Call ik_repl_poll_tool_completions */
    res_t result = ik_repl_poll_tool_completions(repl);

    /* Should succeed */
    ck_assert(!is_err(&result));

    /* Verify state unchanged (tool still executing, not complete) */
    ck_assert_int_eq(agent_a->state, IK_AGENT_STATE_EXECUTING_TOOL);
}
END_TEST

/*
 * Test: ik_repl_poll_tool_completions with current NULL (line 434 false branch)
 * This covers the case where repl->current is NULL in single-agent mode
 */
START_TEST(test_poll_tool_completions_current_null)
{
    /* Set agent_count to 0 (single-agent mode) */
    repl->agent_count = 0;

    /* Set current to NULL */
    repl->current = NULL;

    /* Call ik_repl_poll_tool_completions */
    res_t result = ik_repl_poll_tool_completions(repl);

    /* Should succeed */
    ck_assert(!is_err(&result));
}
END_TEST

/*
 * Test suite
 */
static Suite *event_handlers_tool_poll_suite(void)
{
    Suite *s = suite_create("Event Handlers Tool Poll");
    TCase *tc_core = tcase_create("Core");

    tcase_add_checked_fixture(tc_core, setup, teardown);

    tcase_add_test(tc_core, test_poll_tool_completions_multi_agent_mode);
    tcase_add_test(tc_core, test_poll_tool_completions_single_agent_mode);
    tcase_add_test(tc_core, test_poll_tool_completions_agent_not_executing);
    tcase_add_test(tc_core, test_poll_tool_completions_executing_not_complete);
    tcase_add_test(tc_core, test_poll_tool_completions_current_null);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = event_handlers_tool_poll_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
