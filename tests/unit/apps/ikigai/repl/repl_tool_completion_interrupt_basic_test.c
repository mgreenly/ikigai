#include "tests/test_constants.h"
#include "apps/ikigai/wrapper_pthread.h"

#include "apps/ikigai/agent.h"
#include "apps/ikigai/config.h"
#include "apps/ikigai/input_buffer/core.h"
#include "apps/ikigai/message.h"
#include "apps/ikigai/providers/provider.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/repl_event_handlers.h"
#include "apps/ikigai/repl_tool_completion.h"
#include "apps/ikigai/scrollback.h"
#include "apps/ikigai/shared.h"
#include "shared/terminal.h"
#include "apps/ikigai/tool.h"
#include "shared/wrapper.h"
#include "apps/ikigai/db/message.h"

#include <check.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdatomic.h>
#include <talloc.h>

res_t ik_db_message_insert_(void *db, int64_t session_id, const char *agent_uuid,
                            const char *kind, const char *content, const char *data_json);
res_t ik_repl_render_frame_(void *repl);

res_t ik_db_message_insert_(void *db, int64_t session_id, const char *agent_uuid,
                            const char *kind, const char *content, const char *data_json)
{
    (void)db; (void)session_id; (void)agent_uuid;
    (void)kind; (void)content; (void)data_json;
    return OK(NULL);
}

res_t ik_repl_render_frame_(void *repl)
{
    (void)repl;
    return OK(NULL);
}

static void *dummy_thread_func(void *arg)
{
    (void)arg;
    return NULL;
}

static void *ctx;
static ik_repl_ctx_t *repl;
static ik_agent_ctx_t *agent;

static void setup(void)
{
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
    agent->pending_tool_call = NULL;
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

START_TEST(test_poll_interrupt_basic) {
    repl->agent_count = 0;
    repl->current = agent;
    agent->interrupt_requested = true;
    agent->tool_thread_ctx = talloc_new(agent);
    agent->tool_thread_result = talloc_strdup(agent->tool_thread_ctx, "result");
    agent->pending_tool_call = ik_tool_call_create(agent, "call_1", "bash", "{}");
    pthread_create_(&agent->tool_thread, NULL, dummy_thread_func, NULL);
    agent->tool_thread_running = true;

    agent->message_capacity = 2;
    agent->message_count = 1;
    agent->messages = talloc_array(agent, ik_message_t *, 2);
    ik_message_t *msg = talloc_zero(agent, ik_message_t);
    msg->role = IK_ROLE_USER;
    msg->content_count = 1;
    msg->content_blocks = talloc_array(msg, ik_content_block_t, 1);
    msg->content_blocks[0].type = IK_CONTENT_TEXT;
    msg->content_blocks[0].data.text.text = talloc_strdup(msg, "test");
    msg->interrupted = false;
    agent->messages[0] = msg;

    pthread_mutex_lock_(&agent->tool_thread_mutex);
    atomic_store(&agent->state, IK_AGENT_STATE_EXECUTING_TOOL);
    agent->tool_thread_complete = true;
    pthread_mutex_unlock_(&agent->tool_thread_mutex);

    res_t result = ik_repl_poll_tool_completions(repl);
    ck_assert(is_ok(&result));
    ck_assert(!agent->interrupt_requested);
    ck_assert(agent->messages[0]->interrupted);
    ck_assert_int_eq(agent->state, IK_AGENT_STATE_IDLE);
    ck_assert(!agent->tool_thread_running);
}
END_TEST

START_TEST(test_poll_interrupt_multi_types) {
    repl->agent_count = 0;
    repl->current = agent;
    agent->interrupt_requested = true;
    agent->tool_thread_ctx = talloc_new(agent);
    agent->tool_thread_result = talloc_strdup(agent->tool_thread_ctx, "result");
    agent->pending_tool_call = ik_tool_call_create(agent, "call_1", "bash", "{}");
    pthread_create_(&agent->tool_thread, NULL, dummy_thread_func, NULL);
    agent->tool_thread_running = true;

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

    pthread_mutex_lock_(&agent->tool_thread_mutex);
    atomic_store(&agent->state, IK_AGENT_STATE_EXECUTING_TOOL);
    agent->tool_thread_complete = true;
    pthread_mutex_unlock_(&agent->tool_thread_mutex);

    res_t result = ik_repl_poll_tool_completions(repl);
    ck_assert(is_ok(&result));
    ck_assert(!agent->interrupt_requested);
    ck_assert_int_eq(agent->state, IK_AGENT_STATE_IDLE);
}
END_TEST

START_TEST(test_poll_interrupt_with_db) {
    repl->agent_count = 0;
    repl->current = agent;
    agent->interrupt_requested = true;
    repl->shared->db_ctx = talloc_zero(ctx, ik_db_ctx_t);
    repl->shared->session_id = 123;

    agent->tool_thread_ctx = talloc_new(agent);
    agent->tool_thread_result = talloc_strdup(agent->tool_thread_ctx, "result");
    agent->pending_tool_call = ik_tool_call_create(agent, "call_1", "bash", "{}");
    pthread_create_(&agent->tool_thread, NULL, dummy_thread_func, NULL);
    agent->tool_thread_running = true;

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

    pthread_mutex_lock_(&agent->tool_thread_mutex);
    atomic_store(&agent->state, IK_AGENT_STATE_EXECUTING_TOOL);
    agent->tool_thread_complete = true;
    pthread_mutex_unlock_(&agent->tool_thread_mutex);

    res_t result = ik_repl_poll_tool_completions(repl);
    ck_assert(is_ok(&result));
    ck_assert(!agent->interrupt_requested);
    ck_assert(agent->messages[0]->interrupted);
}
END_TEST

START_TEST(test_poll_interrupt_edge_cases) {
    repl->agent_count = 0;
    repl->current = agent;
    agent->interrupt_requested = true;
    agent->tool_thread_ctx = talloc_new(agent);
    agent->tool_thread_result = talloc_strdup(agent->tool_thread_ctx, "result");
    agent->pending_tool_call = ik_tool_call_create(agent, "call_1", "bash", "{}");
    pthread_create_(&agent->tool_thread, NULL, dummy_thread_func, NULL);
    agent->tool_thread_running = true;

    agent->message_capacity = 6;
    agent->message_count = 5;
    agent->messages = talloc_array(agent, ik_message_t *, 6);

    ik_message_t *user_msg = talloc_zero(agent, ik_message_t);
    user_msg->role = IK_ROLE_USER;
    user_msg->content_count = 1;
    user_msg->content_blocks = talloc_array(user_msg, ik_content_block_t, 1);
    user_msg->content_blocks[0].type = IK_CONTENT_TEXT;
    user_msg->content_blocks[0].data.text.text = talloc_strdup(user_msg, "user");
    user_msg->interrupted = false;
    agent->messages[0] = user_msg;

    agent->messages[1] = NULL;

    ik_message_t *empty_msg = talloc_zero(agent, ik_message_t);
    empty_msg->role = IK_ROLE_ASSISTANT;
    empty_msg->content_count = 0;
    empty_msg->content_blocks = NULL;
    empty_msg->interrupted = false;
    agent->messages[2] = empty_msg;

    ik_message_t *wrong_type_user = talloc_zero(agent, ik_message_t);
    wrong_type_user->role = IK_ROLE_USER;
    wrong_type_user->content_count = 1;
    wrong_type_user->content_blocks = talloc_array(wrong_type_user, ik_content_block_t, 1);
    wrong_type_user->content_blocks[0].type = IK_CONTENT_TOOL_RESULT;
    wrong_type_user->content_blocks[0].data.tool_result.content = talloc_strdup(wrong_type_user, "x");
    wrong_type_user->interrupted = false;
    agent->messages[3] = wrong_type_user;

    ik_message_t *wrong_type_asst = talloc_zero(agent, ik_message_t);
    wrong_type_asst->role = IK_ROLE_ASSISTANT;
    wrong_type_asst->content_count = 1;
    wrong_type_asst->content_blocks = talloc_array(wrong_type_asst, ik_content_block_t, 1);
    wrong_type_asst->content_blocks[0].type = IK_CONTENT_TOOL_RESULT;
    wrong_type_asst->content_blocks[0].data.tool_result.content = talloc_strdup(wrong_type_asst, "y");
    wrong_type_asst->interrupted = false;
    agent->messages[4] = wrong_type_asst;

    pthread_mutex_lock_(&agent->tool_thread_mutex);
    atomic_store(&agent->state, IK_AGENT_STATE_EXECUTING_TOOL);
    agent->tool_thread_complete = true;
    pthread_mutex_unlock_(&agent->tool_thread_mutex);

    res_t result = ik_repl_poll_tool_completions(repl);
    ck_assert(is_ok(&result));
    ck_assert(!agent->interrupt_requested);
    ck_assert_int_eq(agent->state, IK_AGENT_STATE_IDLE);
}
END_TEST

static Suite *repl_tool_completion_interrupt_suite(void)
{
    Suite *s = suite_create("repl_tool_completion_interrupt_basic");
    TCase *tc = tcase_create("interrupt_basic");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_poll_interrupt_basic);
    tcase_add_test(tc, test_poll_interrupt_multi_types);
    tcase_add_test(tc, test_poll_interrupt_with_db);
    tcase_add_test(tc, test_poll_interrupt_edge_cases);
    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    Suite *s = repl_tool_completion_interrupt_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/repl/repl_tool_completion_interrupt_basic_test.xml");
    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
