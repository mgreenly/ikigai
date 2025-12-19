/**
 * @file event_handlers_coverage_test.c
 * @brief Coverage tests for uncovered branches in repl_event_handlers.c
 *
 * This test file targets specific uncovered lines and branches:
 * - Line 100: ik_repl_setup_fd_sets with agent_max_fd > max_fd
 * - Line 325: ik_repl_handle_curl_events with current not in agents array
 * - Lines 379-380: ik_repl_calculate_curl_min_timeout with positive agent timeout
 * - Line 431: ik_repl_poll_tool_completions in multi-agent mode
 * - Line 434: ik_repl_poll_tool_completions in single-agent mode
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
#include <sys/select.h>
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

/* Mock curl_multi_fdset_ to return specific max_fd */
static int mock_fdset_max_fd = -1;
CURLMcode curl_multi_fdset_(CURLM *multi_handle, fd_set *read_fd_set,
                            fd_set *write_fd_set, fd_set *exc_fd_set,
                            int *max_fd)
{
    (void)multi_handle;
    (void)read_fd_set;
    (void)write_fd_set;
    (void)exc_fd_set;

    *max_fd = mock_fdset_max_fd;
    return CURLM_OK;
}

/* Mock curl_multi_timeout_ to return specific timeout */
static long mock_multi_timeout_ms = -1;
static long mock_timeout_call_count = 0;
static long mock_timeout_values[10] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
static bool use_stateful_timeout = false;

CURLMcode curl_multi_timeout_(CURLM *multi_handle, long *timeout_ms)
{
    (void)multi_handle;

    if (use_stateful_timeout) {
        *timeout_ms = mock_timeout_values[mock_timeout_call_count++];
    } else {
        *timeout_ms = mock_multi_timeout_ms;
    }
    return CURLM_OK;
}

/* Test fixtures */
static TALLOC_CTX *ctx = NULL;
static ik_repl_ctx_t *repl = NULL;
static ik_shared_ctx_t *shared = NULL;

/* Mock write for rendering */
ssize_t posix_write_(int fd, const void *buf, size_t count)
{
    (void)fd;
    (void)buf;
    return (ssize_t)count; /* Always succeed */
}

static void setup(void)
{
    ctx = talloc_new(NULL);
    repl = talloc_zero(ctx, ik_repl_ctx_t);
    shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;

    /* Initialize terminal */
    shared->term = talloc_zero(shared, ik_term_ctx_t);
    shared->term->tty_fd = 5;  /* Mock terminal fd */
    shared->term->screen_rows = 24;
    shared->term->screen_cols = 80;

    /* Create render context */
    res_t render_res = ik_render_create(repl, 24, 80, 5, &shared->render);
    ck_assert(!render_res.is_err);

    /* Initialize agents array */
    repl->agent_count = 0;
    repl->agent_capacity = 4;
    repl->agents = talloc_array(repl, ik_agent_ctx_t *, (unsigned int)repl->agent_capacity);

    /* Reset mocks */
    mock_fdset_max_fd = -1;
    mock_multi_timeout_ms = -1;
    mock_timeout_call_count = 0;
    use_stateful_timeout = false;
    for (int i = 0; i < 10; i++) {
        mock_timeout_values[i] = -1;
    }
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

/*
 * Test: ik_repl_setup_fd_sets with agent_max_fd > terminal_fd (line 100)
 * This covers the branch where we update max_fd
 */
START_TEST(test_setup_fd_sets_agent_max_fd_greater)
{
    /* Create one agent */
    ik_agent_ctx_t *agent = create_test_agent(repl, "agent-uuid");
    repl->agents[0] = agent;
    repl->agent_count = 1;
    repl->current = agent;

    /* Mock curl_multi_fdset to return fd > terminal_fd */
    mock_fdset_max_fd = 10;  /* Greater than terminal_fd (5) */

    /* Setup fd_sets */
    fd_set read_fds, write_fds, exc_fds;
    int max_fd = -1;
    res_t result = ik_repl_setup_fd_sets(repl, &read_fds, &write_fds, &exc_fds, &max_fd);

    /* Should succeed */
    ck_assert(!is_err(&result));

    /* max_fd should be updated to agent's max_fd */
    ck_assert_int_eq(max_fd, 10);

    /* Terminal fd should still be set */
    ck_assert(FD_ISSET(shared->term->tty_fd, &read_fds));
}
END_TEST

/*
 * Test: ik_repl_handle_curl_events with current not in agents array (line 325 branch)
 * This covers the path where current agent is not in the array (single-agent/test mode)
 */
START_TEST(test_handle_curl_events_current_not_in_array)
{
    /* Create two agents for the array */
    ik_agent_ctx_t *agent_a = create_test_agent(repl, "agent-a");
    ik_agent_ctx_t *agent_b = create_test_agent(repl, "agent-b");

    repl->agents[0] = agent_a;
    repl->agents[1] = agent_b;
    repl->agent_count = 2;

    /* Create a separate current agent NOT in the array */
    ik_agent_ctx_t *current = create_test_agent(repl, "current-agent");
    repl->current = current;
    current->curl_still_running = 0;  /* Not running, so won't trigger HTTP completion logic */

    /* Call ik_repl_handle_curl_events */
    res_t result = ik_repl_handle_curl_events(repl, 0);

    /* Should succeed */
    ck_assert(!is_err(&result));
}
END_TEST

/*
 * Test: ik_repl_handle_curl_events with current IN agents array (line 325 false branch)
 * This covers the path where current is in the array (normal multi-agent mode)
 */
START_TEST(test_handle_curl_events_current_in_array)
{
    /* Create two agents for the array */
    ik_agent_ctx_t *agent_a = create_test_agent(repl, "agent-a");
    ik_agent_ctx_t *agent_b = create_test_agent(repl, "agent-b");

    repl->agents[0] = agent_a;
    repl->agents[1] = agent_b;
    repl->agent_count = 2;

    /* Set current to one of the agents in the array */
    repl->current = agent_b;
    agent_b->curl_still_running = 0;

    /* Call ik_repl_handle_curl_events */
    res_t result = ik_repl_handle_curl_events(repl, 0);

    /* Should succeed */
    ck_assert(!is_err(&result));
}
END_TEST

/*
 * Test: ik_repl_handle_curl_events with current NULL (line 325 branch 1)
 * This covers the case where repl->current is NULL
 */
START_TEST(test_handle_curl_events_current_null)
{
    /* Create two agents for the array */
    ik_agent_ctx_t *agent_a = create_test_agent(repl, "agent-a");
    ik_agent_ctx_t *agent_b = create_test_agent(repl, "agent-b");

    repl->agents[0] = agent_a;
    repl->agents[1] = agent_b;
    repl->agent_count = 2;

    /* Set current to NULL */
    repl->current = NULL;

    /* Call ik_repl_handle_curl_events */
    res_t result = ik_repl_handle_curl_events(repl, 0);

    /* Should succeed */
    ck_assert(!is_err(&result));
}
END_TEST

/*
 * Test: ik_repl_calculate_curl_min_timeout with positive agent timeout (lines 379-380)
 * This covers the branch where we update curl_timeout_ms
 */
START_TEST(test_calculate_curl_min_timeout_positive)
{
    /* Create two agents */
    ik_agent_ctx_t *agent_a = create_test_agent(repl, "agent-a");
    ik_agent_ctx_t *agent_b = create_test_agent(repl, "agent-b");

    repl->agents[0] = agent_a;
    repl->agents[1] = agent_b;
    repl->agent_count = 2;
    repl->current = agent_a;

    /* Mock curl_multi_timeout to return a positive timeout (e.g., 100ms) */
    mock_multi_timeout_ms = 100;

    /* Call ik_repl_calculate_curl_min_timeout */
    long timeout = -1;
    res_t result = ik_repl_calculate_curl_min_timeout(repl, &timeout);

    /* Should succeed */
    ck_assert(!is_err(&result));

    /* Timeout should be updated to 100 */
    ck_assert_int_eq(timeout, 100);
}
END_TEST

/*
 * Test: ik_repl_calculate_curl_min_timeout with multiple agents, one with larger timeout (line 379 branch 2)
 * This covers the case where agent_timeout >= curl_timeout_ms (don't update)
 */
START_TEST(test_calculate_curl_min_timeout_keeps_minimum)
{
    /* Create two agents */
    ik_agent_ctx_t *agent_a = create_test_agent(repl, "agent-a");
    ik_agent_ctx_t *agent_b = create_test_agent(repl, "agent-b");

    repl->agents[0] = agent_a;
    repl->agents[1] = agent_b;
    repl->agent_count = 2;
    repl->current = agent_a;

    /* Use stateful mock: agent A returns 50ms, agent B returns 200ms
     * This tests: first iteration sets curl_timeout_ms to 50,
     * second iteration has agent_timeout (200) >= curl_timeout_ms (50), so doesn't update */
    use_stateful_timeout = true;
    mock_timeout_values[0] = 50;   /* Agent A */
    mock_timeout_values[1] = 200;  /* Agent B */

    /* Call ik_repl_calculate_curl_min_timeout */
    long timeout = -1;
    res_t result = ik_repl_calculate_curl_min_timeout(repl, &timeout);

    /* Should succeed */
    ck_assert(!is_err(&result));

    /* Timeout should be 50 (the minimum) */
    ck_assert_int_eq(timeout, 50);
}
END_TEST

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
    repl->current = agent_b;  /* Current is B, but A has completed tool */

    /* Setup agent A with a completed tool */
    agent_a->state = IK_AGENT_STATE_EXECUTING_TOOL;
    agent_a->tool_thread_running = true;
    agent_a->tool_thread_complete = false;  /* Thread will set this */
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
        usleep(10000);  /* 10ms */
    }
    ck_assert(complete);

    /* Call ik_repl_poll_tool_completions */
    res_t result = ik_repl_poll_tool_completions(repl);

    /* Should succeed */
    ck_assert(!is_err(&result));

    /* Verify agent A was handled - state should transition to IDLE */
    ck_assert_int_eq(agent_a->state, IK_AGENT_STATE_IDLE);
    ck_assert_ptr_null(agent_a->pending_tool_call);
    ck_assert_uint_eq(agent_a->conversation->message_count, 2);  /* tool_call + tool_result */
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
    current->tool_thread_complete = false;  /* Thread will set this */
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
        usleep(10000);  /* 10ms */
    }
    ck_assert(complete);

    /* Call ik_repl_poll_tool_completions */
    res_t result = ik_repl_poll_tool_completions(repl);

    /* Should succeed */
    ck_assert(!is_err(&result));

    /* Verify current was handled */
    ck_assert_int_eq(current->state, IK_AGENT_STATE_IDLE);
    ck_assert_ptr_null(current->pending_tool_call);
    ck_assert_uint_eq(current->conversation->message_count, 2);  /* tool_call + tool_result */
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
static Suite *event_handlers_coverage_suite(void)
{
    Suite *s = suite_create("Event Handlers Coverage");
    TCase *tc_core = tcase_create("Core");

    tcase_add_checked_fixture(tc_core, setup, teardown);

    tcase_add_test(tc_core, test_setup_fd_sets_agent_max_fd_greater);
    tcase_add_test(tc_core, test_handle_curl_events_current_not_in_array);
    tcase_add_test(tc_core, test_handle_curl_events_current_in_array);
    tcase_add_test(tc_core, test_handle_curl_events_current_null);
    tcase_add_test(tc_core, test_calculate_curl_min_timeout_positive);
    tcase_add_test(tc_core, test_calculate_curl_min_timeout_keeps_minimum);
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
    Suite *s = event_handlers_coverage_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
