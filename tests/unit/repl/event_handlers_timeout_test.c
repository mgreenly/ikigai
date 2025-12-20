/**
 * @file event_handlers_timeout_test.c
 * @brief Coverage tests for ik_repl_calculate_curl_min_timeout in repl_event_handlers.c
 *
 * Tests for Lines 379-380: ik_repl_calculate_curl_min_timeout with positive agent timeout
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
#include "wrapper.h"

#include <check.h>
#include <curl/curl.h>
#include <pthread.h>
#include <talloc.h>

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

    /* Reset mocks */
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
    mock_timeout_values[0] = 50;
    mock_timeout_values[1] = 200;

    /* Call ik_repl_calculate_curl_min_timeout */
    long timeout = -1;
    res_t result = ik_repl_calculate_curl_min_timeout(repl, &timeout);

    /* Should succeed */
    ck_assert(!is_err(&result));

    /* Timeout should be 50 (the minimum) */
    ck_assert_int_eq(timeout, 50);
}
END_TEST

/*
 * Test suite
 */
static Suite *event_handlers_timeout_suite(void)
{
    Suite *s = suite_create("Event Handlers Timeout");
    TCase *tc_core = tcase_create("Core");

    tcase_add_checked_fixture(tc_core, setup, teardown);

    tcase_add_test(tc_core, test_calculate_curl_min_timeout_positive);
    tcase_add_test(tc_core, test_calculate_curl_min_timeout_keeps_minimum);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = event_handlers_timeout_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
