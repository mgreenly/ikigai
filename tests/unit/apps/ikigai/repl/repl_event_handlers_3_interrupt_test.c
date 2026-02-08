#include "tests/test_constants.h"
/**
 * @file repl_event_handlers_3_interrupt_test.c
 * @brief Unit tests for REPL interrupt handling in curl events
 */

#include "apps/ikigai/repl_event_handlers.h"
#include "apps/ikigai/agent.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/shared.h"
#include "apps/ikigai/scrollback.h"
#include "apps/ikigai/config.h"
#include "shared/terminal.h"
#include "apps/ikigai/db/connection.h"
#include "apps/ikigai/db/message.h"
#include "apps/ikigai/providers/provider.h"
#include "apps/ikigai/render.h"
#include "apps/ikigai/message.h"
#include <check.h>
#include <talloc.h>
#include <curl/curl.h>
#include <pthread.h>
#include <stdatomic.h>

/* Forward declarations for mocks */
res_t ik_db_message_insert_(void *db, int64_t session_id, const char *agent_uuid,
                            const char *kind, const char *content, const char *data_json);
res_t ik_db_agent_set_idle(ik_db_ctx_t *db, const char *uuid, bool idle);
res_t ik_db_notify(ik_db_ctx_t *db, const char *channel, const char *payload);
res_t ik_repl_render_frame_(void *repl_ctx);

static void *ctx;
static ik_repl_ctx_t *repl;
static ik_shared_ctx_t *shared;
static ik_agent_ctx_t *agent;
static ik_db_ctx_t *fake_db;

/* Mock database insert */
res_t ik_db_message_insert_(void *db, int64_t session_id, const char *agent_uuid,
                            const char *kind, const char *content, const char *data_json)
{
    (void)db; (void)session_id; (void)agent_uuid;
    (void)kind; (void)content; (void)data_json;
    return OK(NULL);
}

/* Mock agent set idle that succeeds */
res_t ik_db_agent_set_idle(ik_db_ctx_t *db, const char *uuid, bool idle)
{
    (void)db;
    (void)uuid;
    (void)idle;
    return OK(NULL);
}

/* Mock notify that succeeds */
res_t ik_db_notify(ik_db_ctx_t *db, const char *channel, const char *payload)
{
    (void)db;
    (void)channel;
    (void)payload;
    return OK(NULL);
}

/* Mock render frame */
res_t ik_repl_render_frame_(void *repl_ctx)
{
    (void)repl_ctx;
    return OK(NULL);
}

/* Mock provider vtable */
static res_t mock_perform(void *provider_ctx, int *still_running)
{
    (void)provider_ctx;
    *still_running = 0;
    return OK(NULL);
}

static void mock_info_read(void *provider_ctx, ik_logger_t *logger)
{
    (void)provider_ctx; (void)logger;
}

static ik_provider_vtable_t mock_vt = {
    .fdset = NULL,
    .timeout = NULL,
    .perform = mock_perform,
    .info_read = mock_info_read,
    .cleanup = NULL
};

static void setup(void)
{
    ctx = talloc_new(NULL);
    fake_db = talloc_zero(ctx, ik_db_ctx_t);
    shared = talloc_zero(ctx, ik_shared_ctx_t);
    shared->term = talloc_zero(shared, ik_term_ctx_t);
    shared->term->tty_fd = 1;
    shared->term->screen_rows = 24;
    shared->term->screen_cols = 80;
    shared->db_ctx = NULL;
    shared->session_id = 0;
    shared->logger = NULL;
    ik_render_ctx_t *render = NULL;
    res_t res = ik_render_create(ctx, 24, 80, 1, &render);
    (void)res;
    shared->render = render;

    repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->shared = shared;
    repl->agent_count = 0;
    repl->agents = NULL;

    agent = talloc_zero(repl, ik_agent_ctx_t);
    agent->shared = shared;
    agent->scrollback = ik_scrollback_create(agent, 80);
    agent->curl_still_running = 0;
    agent->http_error_message = NULL;
    agent->assistant_response = NULL;
    agent->provider_instance = NULL;
    atomic_store(&agent->state, IK_AGENT_STATE_IDLE);
    agent->uuid = talloc_strdup(agent, "test-uuid");
    agent->messages = NULL;
    agent->message_count = 0;
    agent->message_capacity = 0;
    pthread_mutex_init(&agent->tool_thread_mutex, NULL);
    repl->current = agent;
}

static void teardown(void)
{
    pthread_mutex_destroy(&agent->tool_thread_mutex);
    talloc_free(ctx);
}

START_TEST(test_curl_events_interrupt_requested) {
    struct ik_provider *instance = talloc_zero(agent, struct ik_provider);
    instance->vt = &mock_vt;
    instance->ctx = NULL;
    agent->provider_instance = instance;
    agent->curl_still_running = 1;
    atomic_store(&agent->state, IK_AGENT_STATE_WAITING_FOR_LLM);
    agent->interrupt_requested = true;
    agent->assistant_response = talloc_strdup(agent, "Partial response");

    agent->message_capacity = 2;
    agent->message_count = 1;
    agent->messages = talloc_array(agent, ik_message_t *, 2);
    ik_message_t *msg = talloc_zero(agent, ik_message_t);
    msg->role = IK_ROLE_USER;
    msg->content_count = 1;
    msg->content_blocks = talloc_array(msg, ik_content_block_t, 1);
    msg->content_blocks[0].type = IK_CONTENT_TEXT;
    msg->content_blocks[0].data.text.text = talloc_strdup(msg, "test message");
    msg->interrupted = false;
    agent->messages[0] = msg;

    repl->agent_count = 1;
    repl->agents = talloc_array(repl, ik_agent_ctx_t *, 1);
    repl->agents[0] = agent;
    repl->current = agent;

    res_t result = ik_repl_handle_curl_events(repl, 1);
    ck_assert(is_ok(&result));
    ck_assert(!agent->interrupt_requested);
    ck_assert_ptr_null(agent->assistant_response);
    ck_assert(agent->messages[0]->interrupted);
    ik_agent_state_t state = atomic_load(&agent->state);
    ck_assert_int_eq(state, IK_AGENT_STATE_IDLE);
}
END_TEST

START_TEST(test_curl_events_interrupt_multiple_types) {
    struct ik_provider *instance = talloc_zero(agent, struct ik_provider);
    instance->vt = &mock_vt;
    instance->ctx = NULL;
    agent->provider_instance = instance;
    agent->curl_still_running = 1;
    atomic_store(&agent->state, IK_AGENT_STATE_WAITING_FOR_LLM);
    agent->interrupt_requested = true;
    agent->assistant_response = NULL;
    agent->http_error_message = NULL;
    shared->db_ctx = fake_db;
    shared->session_id = 123;

    agent->message_capacity = 4;
    agent->message_count = 3;
    agent->messages = talloc_array(agent, ik_message_t *, 4);

    ik_message_t *user_msg = talloc_zero(agent, ik_message_t);
    user_msg->role = IK_ROLE_USER;
    user_msg->content_count = 1;
    user_msg->content_blocks = talloc_array(user_msg, ik_content_block_t, 1);
    user_msg->content_blocks[0].type = IK_CONTENT_TEXT;
    user_msg->content_blocks[0].data.text.text = talloc_strdup(user_msg, "user message");
    user_msg->interrupted = false;
    agent->messages[0] = user_msg;

    ik_message_t *asst_msg = talloc_zero(agent, ik_message_t);
    asst_msg->role = IK_ROLE_ASSISTANT;
    asst_msg->content_count = 1;
    asst_msg->content_blocks = talloc_array(asst_msg, ik_content_block_t, 1);
    asst_msg->content_blocks[0].type = IK_CONTENT_TEXT;
    asst_msg->content_blocks[0].data.text.text = talloc_strdup(asst_msg, "assistant message");
    asst_msg->interrupted = false;
    agent->messages[1] = asst_msg;

    ik_message_t *tool_msg = talloc_zero(agent, ik_message_t);
    tool_msg->role = IK_ROLE_TOOL;
    tool_msg->content_count = 1;
    tool_msg->content_blocks = talloc_array(tool_msg, ik_content_block_t, 1);
    tool_msg->content_blocks[0].type = IK_CONTENT_TOOL_RESULT;
    tool_msg->content_blocks[0].data.tool_result.content = talloc_strdup(tool_msg, "tool result");
    tool_msg->interrupted = false;
    agent->messages[2] = tool_msg;

    repl->agent_count = 1;
    repl->agents = talloc_array(repl, ik_agent_ctx_t *, 1);
    repl->agents[0] = agent;
    repl->current = agent;

    res_t result = ik_repl_handle_curl_events(repl, 1);
    ck_assert(is_ok(&result));
    ck_assert(!agent->interrupt_requested);
    ck_assert_int_eq(atomic_load(&agent->state), IK_AGENT_STATE_IDLE);
}
END_TEST

START_TEST(test_curl_events_interrupt_not_current) {
    struct ik_provider *instance = talloc_zero(agent, struct ik_provider);
    instance->vt = &mock_vt;
    instance->ctx = NULL;
    agent->provider_instance = instance;
    agent->curl_still_running = 1;
    atomic_store(&agent->state, IK_AGENT_STATE_WAITING_FOR_LLM);
    agent->interrupt_requested = true;

    agent->message_capacity = 2;
    agent->message_count = 1;
    agent->messages = talloc_array(agent, ik_message_t *, 2);
    ik_message_t *msg = talloc_zero(agent, ik_message_t);
    msg->role = IK_ROLE_USER;
    msg->content_count = 1;
    msg->content_blocks = talloc_array(msg, ik_content_block_t, 1);
    msg->content_blocks[0].type = IK_CONTENT_TEXT;
    msg->content_blocks[0].data.text.text = talloc_strdup(msg, "message");
    msg->interrupted = false;
    agent->messages[0] = msg;

    repl->agent_count = 1;
    repl->agents = talloc_array(repl, ik_agent_ctx_t *, 1);
    repl->agents[0] = agent;

    ik_agent_ctx_t *other_agent = talloc_zero(repl, ik_agent_ctx_t);
    other_agent->shared = shared;
    other_agent->scrollback = ik_scrollback_create(other_agent, 80);
    other_agent->curl_still_running = 0;
    other_agent->provider_instance = NULL;
    atomic_store(&other_agent->state, IK_AGENT_STATE_IDLE);
    pthread_mutex_init(&other_agent->tool_thread_mutex, NULL);
    repl->current = other_agent;

    res_t result = ik_repl_handle_curl_events(repl, 1);
    ck_assert(is_ok(&result));
    ck_assert(!agent->interrupt_requested);
    ck_assert(agent->messages[0]->interrupted);

    pthread_mutex_destroy(&other_agent->tool_thread_mutex);
}
END_TEST

static Suite *repl_event_handlers_interrupt_suite(void)
{
    Suite *s = suite_create("repl_event_handlers_interrupt");
    TCase *tc = tcase_create("interrupt");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_curl_events_interrupt_requested);
    tcase_add_test(tc, test_curl_events_interrupt_multiple_types);
    tcase_add_test(tc, test_curl_events_interrupt_not_current);
    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    Suite *s = repl_event_handlers_interrupt_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/repl/repl_event_handlers_3_interrupt_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
