/**
 * @file repl_event_handlers_current_not_in_array_test.c
 * @brief Tests for curl event handling when current agent is not in agents array
 *
 * This test covers the edge case where repl->current is not in the repl->agents array,
 * triggering the conditional processing at line 284 in repl_event_handlers.c.
 */

#include "repl_event_handlers.h"
#include "../../../src/agent.h"
#include "../../../src/repl.h"
#include "../../../src/shared.h"
#include "../../../src/scrollback.h"
#include "../../../src/config.h"
#include "../../../src/terminal.h"
#include "../../../src/input.h"
#include "../../../src/db/connection.h"
#include "../../../src/db/message.h"
#include "../../../src/providers/provider.h"
#include "../../../src/providers/common/http_multi.h"
#include "../../../src/scroll_detector.h"
#include "../../../src/input_buffer/core.h"
#include "../../../src/render.h"
#include "../../../src/tool.h"
#include "../../../src/message.h"
#include "../../../src/wrapper.h"
#include "../../../src/repl_tool_completion.h"
#include <check.h>
#include <talloc.h>
#include <curl/curl.h>
#include <unistd.h>
#include <sys/select.h>
#include <errno.h>
#include <pthread.h>

static void *ctx;
static ik_repl_ctx_t *repl;
static ik_shared_ctx_t *shared;
static ik_agent_ctx_t *agent;
static ik_db_ctx_t *fake_db;

/* Global context for mock error creation */
static TALLOC_CTX *error_ctx = NULL;

/* Mock database insert that succeeds */
res_t ik_db_message_insert_(void *db,
                            int64_t session_id,
                            const char *agent_uuid,
                            const char *kind,
                            const char *content,
                            const char *data_json)
{
    (void)db;
    (void)session_id;
    (void)agent_uuid;
    (void)kind;
    (void)content;
    (void)data_json;
    return OK(NULL);
}

/* Mock render frame to avoid needing full render infrastructure */
res_t ik_repl_render_frame_(void *repl_ctx)
{
    (void)repl_ctx;
    return OK(NULL);
}

/* Mock agent add message */
res_t ik_agent_add_message_(void *agent_ctx, void *msg)
{
    (void)agent_ctx;
    (void)msg;
    return OK(NULL);
}

/* Mock agent should continue tool loop - return false to avoid tool loop */
int ik_agent_should_continue_tool_loop_(const void *agent_ctx)
{
    (void)agent_ctx;
    return 0;
}

/* Mock repl submit tool loop continuation */
void ik_repl_submit_tool_loop_continuation_(void *repl_ctx, void *agent_ctx)
{
    (void)repl_ctx;
    (void)agent_ctx;
}

/* Mock agent transition to idle */
void ik_agent_transition_to_idle_(void *agent_ctx)
{
    ik_agent_ctx_t *agent_local = (ik_agent_ctx_t *)agent_ctx;
    agent_local->state = IK_AGENT_STATE_IDLE;
}

/* Mock agent start tool execution */
void ik_agent_start_tool_execution_(void *agent_ctx)
{
    ik_agent_ctx_t *agent_local = (ik_agent_ctx_t *)agent_ctx;
    agent_local->state = IK_AGENT_STATE_EXECUTING_TOOL;
}

/* Mock provider vtable */
static res_t mock_fdset(void *provider_ctx, fd_set *read_fds, fd_set *write_fds,
                        fd_set *exc_fds, int *max_fd)
{
    (void)provider_ctx;
    (void)read_fds;
    (void)write_fds;
    (void)exc_fds;
    *max_fd = 10;
    return OK(NULL);
}

static res_t mock_timeout(void *provider_ctx, long *timeout)
{
    (void)provider_ctx;
    *timeout = 500;
    return OK(NULL);
}

static res_t mock_perform(void *provider_ctx, int *still_running)
{
    (void)provider_ctx;
    *still_running = 0;
    return OK(NULL);
}

static res_t mock_perform_error(void *provider_ctx, int *still_running)
{
    (void)provider_ctx;
    (void)still_running;
    /* Return error to test error handling path */
    return ERR(error_ctx, PROVIDER, "mock perform error");
}

static void mock_info_read(void *provider_ctx, ik_logger_t *logger)
{
    (void)provider_ctx;
    (void)logger;
}

static ik_provider_vtable_t mock_vt = {
    .fdset = mock_fdset,
    .timeout = mock_timeout,
    .perform = mock_perform,
    .info_read = mock_info_read,
    .cleanup = NULL
};

static ik_provider_vtable_t mock_vt_error = {
    .fdset = mock_fdset,
    .timeout = mock_timeout,
    .perform = mock_perform_error,
    .info_read = mock_info_read,
    .cleanup = NULL
};

static void setup(void)
{
    ctx = talloc_new(NULL);
    error_ctx = ctx;

    /* Create fake database context */
    fake_db = talloc_zero(ctx, ik_db_ctx_t);

    /* Create shared context */
    shared = talloc_zero(ctx, ik_shared_ctx_t);
    shared->term = talloc_zero(shared, ik_term_ctx_t);
    shared->term->tty_fd = 1;
    shared->term->screen_rows = 24;
    shared->term->screen_cols = 80;
    shared->db_ctx = fake_db;
    shared->session_id = 123;
    shared->logger = NULL;

    /* Create render context */
    ik_render_ctx_t *render = NULL;
    res_t res = ik_render_create(ctx, 24, 80, 1, &render);
    (void)res;
    shared->render = render;

    /* Create REPL context */
    repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->shared = shared;
    repl->agent_count = 0;
    repl->agents = NULL;
    repl->input_parser = NULL;
    repl->scroll_det = NULL;

    /* Create agent context */
    agent = talloc_zero(repl, ik_agent_ctx_t);
    agent->shared = shared;
    agent->scrollback = ik_scrollback_create(agent, 80);
    agent->input_buffer = ik_input_buffer_create(agent);
    agent->curl_still_running = 0;
    agent->http_error_message = NULL;
    agent->assistant_response = NULL;
    agent->pending_tool_call = NULL;
    agent->provider_instance = NULL;
    agent->state = IK_AGENT_STATE_IDLE;
    agent->tool_iteration_count = 0;
    pthread_mutex_init(&agent->tool_thread_mutex, NULL);

    /* Agent metadata */
    agent->uuid = talloc_strdup(agent, "test-uuid");
    agent->provider = NULL;
    agent->response_model = NULL;
    agent->response_finish_reason = NULL;
    agent->response_input_tokens = 0;
    agent->response_output_tokens = 0;
    agent->response_thinking_tokens = 0;
    agent->thinking_level = 0;
    agent->messages = NULL;
    agent->message_count = 0;
    agent->message_capacity = 0;

    /* Spinner state */
    agent->spinner_state.visible = false;
    agent->spinner_state.frame_index = 0;

    repl->current = agent;
}

static void teardown(void)
{
    pthread_mutex_destroy(&agent->tool_thread_mutex);
    talloc_free(ctx);
}

/* ========== Tests ========== */

START_TEST(test_curl_events_current_not_in_array) {
    /* Create a separate current agent that is NOT in the agents array */
    ik_agent_ctx_t *current_agent = talloc_zero(repl, ik_agent_ctx_t);
    current_agent->shared = shared;
    current_agent->scrollback = ik_scrollback_create(current_agent, 80);
    current_agent->input_buffer = ik_input_buffer_create(current_agent);
    current_agent->curl_still_running = 1;
    current_agent->state = IK_AGENT_STATE_WAITING_FOR_LLM;
    current_agent->assistant_response = talloc_strdup(current_agent, "Current agent response");
    current_agent->spinner_state.visible = false;
    current_agent->spinner_state.frame_index = 0;
    pthread_mutex_init(&current_agent->tool_thread_mutex, NULL);

    /* Initialize message array */
    current_agent->uuid = talloc_strdup(current_agent, "current-test-uuid");
    current_agent->provider = NULL;
    current_agent->response_model = NULL;
    current_agent->response_finish_reason = NULL;
    current_agent->response_input_tokens = 0;
    current_agent->response_output_tokens = 0;
    current_agent->response_thinking_tokens = 0;
    current_agent->thinking_level = 0;
    current_agent->messages = NULL;
    current_agent->message_count = 0;
    current_agent->message_capacity = 0;
    current_agent->pending_tool_call = NULL;
    current_agent->tool_iteration_count = 0;

    /* Create mock provider for current agent */
    struct ik_provider *current_instance = talloc_zero(current_agent, struct ik_provider);
    current_instance->vt = &mock_vt;
    current_instance->ctx = NULL;
    current_agent->provider_instance = current_instance;

    /* Set up agents array with DIFFERENT agent */
    repl->agent_count = 1;
    repl->agents = talloc_array(repl, ik_agent_ctx_t *, 1);
    repl->agents[0] = agent;  /* agent is in array */
    repl->current = current_agent;  /* current_agent is NOT in array */

    /* No provider for agent in array, so only current_agent will be processed */
    agent->provider_instance = NULL;
    agent->curl_still_running = 0;

    /* This should process current_agent even though it's not in the agents array */
    res_t result = ik_repl_handle_curl_events(repl, 1);
    ck_assert(is_ok(&result));

    /* Response should be freed */
    ck_assert_ptr_null(current_agent->assistant_response);

    pthread_mutex_destroy(&current_agent->tool_thread_mutex);
}

END_TEST

START_TEST(test_curl_events_current_not_in_array_perform_error) {
    /* Create a separate current agent that is NOT in the agents array */
    ik_agent_ctx_t *current_agent = talloc_zero(repl, ik_agent_ctx_t);
    current_agent->shared = shared;
    current_agent->scrollback = ik_scrollback_create(current_agent, 80);
    current_agent->input_buffer = ik_input_buffer_create(current_agent);
    current_agent->curl_still_running = 1;
    current_agent->state = IK_AGENT_STATE_WAITING_FOR_LLM;
    pthread_mutex_init(&current_agent->tool_thread_mutex, NULL);

    /* Create mock provider with error vtable for current agent */
    struct ik_provider *current_instance = talloc_zero(current_agent, struct ik_provider);
    current_instance->vt = &mock_vt_error;
    current_instance->ctx = NULL;
    current_agent->provider_instance = current_instance;

    /* Set up agents array with DIFFERENT agent */
    repl->agent_count = 1;
    repl->agents = talloc_array(repl, ik_agent_ctx_t *, 1);
    repl->agents[0] = agent;  /* agent is in array */
    repl->current = current_agent;  /* current_agent is NOT in array */

    /* No provider for agent in array, so only current_agent will be processed */
    agent->provider_instance = NULL;
    agent->curl_still_running = 0;

    /* This should propagate the error from process_agent_curl_events */
    res_t result = ik_repl_handle_curl_events(repl, 1);
    ck_assert(is_err(&result));

    pthread_mutex_destroy(&current_agent->tool_thread_mutex);
}

END_TEST

START_TEST(test_curl_events_background_agent_completes) {
    /* Create a background agent that IS in the array but NOT current */
    ik_agent_ctx_t *background_agent = talloc_zero(repl, ik_agent_ctx_t);
    background_agent->shared = shared;
    background_agent->scrollback = ik_scrollback_create(background_agent, 80);
    background_agent->input_buffer = ik_input_buffer_create(background_agent);
    background_agent->curl_still_running = 1;
    background_agent->state = IK_AGENT_STATE_WAITING_FOR_LLM;
    background_agent->assistant_response = talloc_strdup(background_agent, "Background agent response");
    background_agent->spinner_state.visible = false;
    background_agent->spinner_state.frame_index = 0;
    pthread_mutex_init(&background_agent->tool_thread_mutex, NULL);

    /* Initialize message array */
    background_agent->uuid = talloc_strdup(background_agent, "background-test-uuid");
    background_agent->provider = NULL;
    background_agent->response_model = NULL;
    background_agent->response_finish_reason = NULL;
    background_agent->response_input_tokens = 0;
    background_agent->response_output_tokens = 0;
    background_agent->response_thinking_tokens = 0;
    background_agent->thinking_level = 0;
    background_agent->messages = NULL;
    background_agent->message_count = 0;
    background_agent->message_capacity = 0;
    background_agent->pending_tool_call = NULL;
    background_agent->tool_iteration_count = 0;

    /* Create mock provider for background agent */
    struct ik_provider *background_instance = talloc_zero(background_agent, struct ik_provider);
    background_instance->vt = &mock_vt;
    background_instance->ctx = NULL;
    background_agent->provider_instance = background_instance;

    /* Set up agents array with background_agent */
    repl->agent_count = 1;
    repl->agents = talloc_array(repl, ik_agent_ctx_t *, 1);
    repl->agents[0] = background_agent;  /* background_agent is in array */
    repl->current = agent;  /* agent is current, background_agent is NOT current */

    /* No provider for current agent */
    agent->provider_instance = NULL;
    agent->curl_still_running = 0;

    /* This should process background_agent and NOT call render (because it's not current) */
    res_t result = ik_repl_handle_curl_events(repl, 1);
    ck_assert(is_ok(&result));

    /* Background agent response should be freed */
    ck_assert_ptr_null(background_agent->assistant_response);

    pthread_mutex_destroy(&background_agent->tool_thread_mutex);
}

END_TEST

/* ========== Test Suite Setup ========== */

static Suite *repl_event_handlers_current_not_in_array_suite(void)
{
    Suite *s = suite_create("repl_event_handlers_current_not_in_array");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);
    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_curl_events_current_not_in_array);
    tcase_add_test(tc_core, test_curl_events_current_not_in_array_perform_error);
    tcase_add_test(tc_core, test_curl_events_background_agent_completes);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = repl_event_handlers_current_not_in_array_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
