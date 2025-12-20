/**
 * @file event_handlers_fd_sets_test.c
 * @brief Coverage tests for ik_repl_setup_fd_sets in repl_event_handlers.c
 *
 * Tests for Line 100: ik_repl_setup_fd_sets with agent_max_fd > max_fd
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
#include <sys/select.h>
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
    mock_fdset_max_fd = -1;
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
    mock_fdset_max_fd = 10;

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
 * Test suite
 */
static Suite *event_handlers_fd_sets_suite(void)
{
    Suite *s = suite_create("Event Handlers FD Sets");
    TCase *tc_core = tcase_create("Core");

    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_setup_fd_sets_agent_max_fd_greater);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = event_handlers_fd_sets_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
