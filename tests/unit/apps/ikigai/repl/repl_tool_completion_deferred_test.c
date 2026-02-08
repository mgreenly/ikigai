#include "tests/test_constants.h"
#include "apps/ikigai/wrapper_pthread.h"
/**
 * @file repl_tool_completion_deferred_test.c
 * @brief Unit tests for deferred command completion (pending_tool_call == NULL)
 */

#include "apps/ikigai/agent.h"
#include "apps/ikigai/config.h"
#include "apps/ikigai/db/message.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/repl_tool_completion.h"
#include "apps/ikigai/scrollback.h"
#include "apps/ikigai/shared.h"
#include "shared/terminal.h"

#include <check.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <talloc.h>

/* Forward declarations for mocks */
res_t ik_db_message_insert_(void *db, int64_t session_id, const char *agent_uuid,
                            const char *kind, const char *content, const char *data_json);
res_t ik_repl_render_frame_(void *repl);

/* Mock db message insert */
res_t ik_db_message_insert_(void *db, int64_t session_id, const char *agent_uuid,
                            const char *kind, const char *content, const char *data_json)
{
    (void)db; (void)session_id; (void)agent_uuid;
    (void)kind; (void)content; (void)data_json;
    return OK(NULL);
}

/* Mock render frame */
res_t ik_repl_render_frame_(void *repl)
{
    (void)repl;
    return OK(NULL);
}

/* Dummy thread function */
static void *dummy_thread_func(void *arg)
{
    (void)arg;
    return NULL;
}

/* Mock on_complete hook tracking */
static bool on_complete_called = false;

static void mock_on_complete(ik_repl_ctx_t *r, ik_agent_ctx_t *a)
{
    (void)r;
    (void)a;
    on_complete_called = true;
}

static void *ctx;
static ik_repl_ctx_t *repl;
static ik_agent_ctx_t *agent;

static void setup(void)
{
    on_complete_called = false;
    ctx = talloc_new(NULL);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    shared->db_ctx = NULL;
    shared->session_id = 0;
    shared->cfg = talloc_zero(ctx, ik_config_t);
    shared->cfg->max_tool_turns = 10;
    shared->term = talloc_zero(ctx, ik_term_ctx_t);
    shared->term->screen_rows = 24;
    shared->term->screen_cols = 80;
    shared->render = NULL;

    repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->shared = shared;
    repl->agents = NULL;
    repl->agent_count = 0;

    agent = talloc_zero(repl, ik_agent_ctx_t);
    agent->shared = shared;
    agent->repl = repl;
    agent->scrollback = ik_scrollback_create(agent, 80);
    atomic_store(&agent->state, IK_AGENT_STATE_EXECUTING_TOOL);
    agent->messages = NULL;
    agent->message_count = 0;
    agent->message_capacity = 0;
    agent->tool_iteration_count = 0;
    agent->response_finish_reason = NULL;
    agent->curl_still_running = 0;
    agent->pending_tool_call = NULL;  /* Deferred command has no pending_tool_call */
    agent->input_buffer = NULL;
    agent->provider = talloc_strdup(agent, "openai");
    agent->model = talloc_strdup(agent, "gpt-4");
    agent->uuid = talloc_strdup(agent, "test-uuid");

    pthread_mutex_init_(&agent->tool_thread_mutex, NULL);
    agent->tool_thread_running = false;
    agent->tool_thread_complete = false;
    agent->tool_thread_ctx = NULL;
    agent->tool_thread_result = NULL;

    repl->current = agent;
}

static void teardown(void)
{
    talloc_free(ctx);
}

/* Test: deferred command completion without on_complete */
START_TEST(test_deferred_command_no_on_complete) {
    agent->tool_thread_ctx = talloc_new(agent);
    agent->tool_thread_result = talloc_strdup(agent->tool_thread_ctx, "result");
    pthread_create_(&agent->tool_thread, NULL, dummy_thread_func, NULL);
    agent->tool_thread_running = true;
    agent->tool_thread_complete = true;
    agent->pending_on_complete = NULL;

    ik_repl_handle_agent_tool_completion(repl, agent);

    ck_assert(!agent->tool_thread_running);
    ck_assert(!agent->tool_thread_complete);
    ck_assert_ptr_null(agent->tool_thread_result);
    ck_assert_int_eq(agent->tool_child_pid, 0);
    ck_assert_int_eq(agent->state, IK_AGENT_STATE_IDLE);
}
END_TEST

/* Test: deferred command completion with on_complete */
START_TEST(test_deferred_command_with_on_complete) {
    agent->tool_thread_ctx = talloc_new(agent);
    agent->tool_thread_result = talloc_strdup(agent->tool_thread_ctx, "result");
    pthread_create_(&agent->tool_thread, NULL, dummy_thread_func, NULL);
    agent->tool_thread_running = true;
    agent->tool_thread_complete = true;
    agent->pending_on_complete = mock_on_complete;
    agent->tool_deferred_data = (void *)0xDEADBEEF;

    ik_repl_handle_agent_tool_completion(repl, agent);

    ck_assert(on_complete_called);
    ck_assert_ptr_null(agent->pending_on_complete);
    ck_assert_ptr_null(agent->tool_deferred_data);
    ck_assert_ptr_null(agent->tool_thread_ctx);
    ck_assert(!agent->tool_thread_running);
    ck_assert_int_eq(agent->state, IK_AGENT_STATE_IDLE);
}
END_TEST

/* Test: interrupted deferred command without on_complete */
START_TEST(test_interrupted_deferred_no_on_complete) {
    agent->interrupt_requested = true;
    agent->tool_thread_ctx = talloc_new(agent);
    agent->tool_thread_result = talloc_strdup(agent->tool_thread_ctx, "result");
    pthread_create_(&agent->tool_thread, NULL, dummy_thread_func, NULL);
    agent->tool_thread_running = true;
    agent->tool_thread_complete = true;
    agent->pending_on_complete = NULL;

    ik_repl_handle_interrupted_tool_completion(repl, agent);

    ck_assert(!agent->interrupt_requested);
    ck_assert(!agent->tool_thread_running);
    ck_assert(!agent->tool_thread_complete);
    ck_assert_ptr_null(agent->tool_thread_result);
    ck_assert_int_eq(agent->tool_child_pid, 0);
    ck_assert_int_eq(agent->state, IK_AGENT_STATE_IDLE);
}
END_TEST

/* Test: interrupted deferred command with on_complete */
START_TEST(test_interrupted_deferred_with_on_complete) {
    agent->interrupt_requested = true;
    agent->tool_thread_ctx = talloc_new(agent);
    agent->tool_thread_result = talloc_strdup(agent->tool_thread_ctx, "result");
    pthread_create_(&agent->tool_thread, NULL, dummy_thread_func, NULL);
    agent->tool_thread_running = true;
    agent->tool_thread_complete = true;
    agent->pending_on_complete = mock_on_complete;
    agent->tool_deferred_data = (void *)0xDEADBEEF;

    ik_repl_handle_interrupted_tool_completion(repl, agent);

    ck_assert(on_complete_called);
    ck_assert(!agent->interrupt_requested);
    ck_assert_ptr_null(agent->pending_on_complete);
    ck_assert_ptr_null(agent->tool_deferred_data);
    ck_assert_ptr_null(agent->tool_thread_ctx);
    ck_assert(!agent->tool_thread_running);
    ck_assert_int_eq(agent->state, IK_AGENT_STATE_IDLE);
}
END_TEST

/* Test: interrupted deferred when not current */
START_TEST(test_interrupted_deferred_not_current) {
    agent->interrupt_requested = true;
    agent->tool_thread_ctx = talloc_new(agent);
    pthread_create_(&agent->tool_thread, NULL, dummy_thread_func, NULL);
    agent->tool_thread_running = true;
    agent->tool_thread_complete = true;
    repl->current = NULL;  /* Agent is not current */

    ik_repl_handle_interrupted_tool_completion(repl, agent);

    ck_assert(!agent->interrupt_requested);
    ck_assert_int_eq(agent->state, IK_AGENT_STATE_IDLE);
}
END_TEST

/* Test: deferred with on_complete but NULL tool_thread_ctx */
START_TEST(test_deferred_on_complete_null_ctx) {
    pthread_create_(&agent->tool_thread, NULL, dummy_thread_func, NULL);
    agent->tool_thread_running = true;
    agent->tool_thread_complete = true;
    agent->pending_on_complete = mock_on_complete;
    agent->tool_deferred_data = (void *)0xDEADBEEF;
    agent->tool_thread_ctx = NULL;  /* NULL context */

    ik_repl_handle_agent_tool_completion(repl, agent);

    ck_assert(on_complete_called);
    ck_assert_ptr_null(agent->pending_on_complete);
    ck_assert_ptr_null(agent->tool_deferred_data);
    ck_assert(!agent->tool_thread_running);
    ck_assert_int_eq(agent->state, IK_AGENT_STATE_IDLE);
}
END_TEST

/* Test: interrupted deferred with on_complete but NULL tool_thread_ctx */
START_TEST(test_interrupted_deferred_on_complete_null_ctx) {
    agent->interrupt_requested = true;
    pthread_create_(&agent->tool_thread, NULL, dummy_thread_func, NULL);
    agent->tool_thread_running = true;
    agent->tool_thread_complete = true;
    agent->pending_on_complete = mock_on_complete;
    agent->tool_deferred_data = (void *)0xDEADBEEF;
    agent->tool_thread_ctx = NULL;  /* NULL context */

    ik_repl_handle_interrupted_tool_completion(repl, agent);

    ck_assert(on_complete_called);
    ck_assert(!agent->interrupt_requested);
    ck_assert_ptr_null(agent->pending_on_complete);
    ck_assert_ptr_null(agent->tool_deferred_data);
    ck_assert_ptr_null(agent->tool_thread_ctx);
    ck_assert(!agent->tool_thread_running);
    ck_assert_int_eq(agent->state, IK_AGENT_STATE_IDLE);
}
END_TEST

static Suite *repl_tool_completion_deferred_suite(void)
{
    Suite *s = suite_create("repl_tool_completion_deferred");
    TCase *tc = tcase_create("deferred");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_deferred_command_no_on_complete);
    tcase_add_test(tc, test_deferred_command_with_on_complete);
    tcase_add_test(tc, test_interrupted_deferred_no_on_complete);
    tcase_add_test(tc, test_interrupted_deferred_with_on_complete);
    tcase_add_test(tc, test_interrupted_deferred_not_current);
    tcase_add_test(tc, test_deferred_on_complete_null_ctx);
    tcase_add_test(tc, test_interrupted_deferred_on_complete_null_ctx);
    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    Suite *s = repl_tool_completion_deferred_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/repl/repl_tool_completion_deferred_test.xml");
    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
