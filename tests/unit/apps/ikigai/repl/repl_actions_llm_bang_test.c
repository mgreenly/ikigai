#include "tests/test_constants.h"
#include <check.h>
#include <stdatomic.h>
#include "apps/ikigai/repl_actions.h"
#include "apps/ikigai/repl_actions_internal.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/agent.h"
#include "apps/ikigai/scrollback.h"
#include "apps/ikigai/input_buffer/core.h"
#include "apps/ikigai/providers/provider.h"
#include "apps/ikigai/providers/request.h"
#include "apps/ikigai/config.h"
#include "apps/ikigai/shared.h"
#include "apps/ikigai/bang_commands.h"
#include "apps/ikigai/token_cache.h"
#include "apps/ikigai/db/agent.h"
#include "shared/error.h"
#include "shared/wrapper.h"
#include "apps/ikigai/db/message.h"
#include "apps/ikigai/db/session.h"
#include <talloc.h>
#include <string.h>

/* ================================================================
 * Mock state
 * ================================================================ */

static bool mock_start_stream_should_fail = false;
static bool mock_get_provider_should_fail = false;
static bool mock_build_request_should_fail = false;
static bool mock_bang_dispatch_should_fail = false;
static bool mock_token_cache_add_turn_called = false;
static TALLOC_CTX *mock_err_ctx = NULL;

/* ================================================================
 * Required mock functions (strong symbols override weak)
 * ================================================================ */

res_t ik_db_message_insert(ik_db_ctx_t *db_ctx, int64_t session_id,
                            const char *agent_uuid, const char *kind,
                            const char *content, const char *data_json)
{
    (void)db_ctx; (void)session_id; (void)agent_uuid;
    (void)kind; (void)content; (void)data_json;
    return OK(NULL);
}

res_t ik_db_session_get_active(ik_db_ctx_t *db_ctx, int64_t *session_id_out)
{
    (void)db_ctx; (void)session_id_out;
    return OK(NULL);
}

res_t ik_db_session_create(ik_db_ctx_t *db_ctx, int64_t *session_id_out)
{
    (void)db_ctx; (void)session_id_out;
    return OK(NULL);
}

void ik_agent_invalidate_provider(ik_agent_ctx_t *agent)
{
    (void)agent;
}

res_t ik_agent_restore_from_row(ik_agent_ctx_t *agent, const void *row)
{
    (void)agent; (void)row;
    return OK(NULL);
}

res_t ik_db_agent_set_idle(ik_db_ctx_t *db_ctx, const char *uuid, bool idle)
{
    (void)db_ctx; (void)uuid; (void)idle;
    return OK(NULL);
}

void ik_token_cache_add_turn(ik_token_cache_t *cache)
{
    (void)cache;
    mock_token_cache_add_turn_called = true;
}

res_t ik_bang_dispatch(void *ctx, ik_repl_ctx_t *repl, const char *input)
{
    (void)ctx; (void)repl; (void)input;
    if (mock_bang_dispatch_should_fail) {
        if (mock_err_ctx == NULL) mock_err_ctx = talloc_new(NULL);
        return ERR(mock_err_ctx, PROVIDER, "mock bang error");
    }
    return OK(NULL);
}

static res_t mock_start_stream(void *ctx, const ik_request_t *req,
                               ik_stream_cb_t stream_cb, void *stream_ctx,
                               ik_provider_completion_cb_t completion_cb, void *completion_ctx)
{
    (void)ctx; (void)req; (void)stream_cb; (void)stream_ctx;
    (void)completion_cb; (void)completion_ctx;
    if (mock_start_stream_should_fail) {
        if (mock_err_ctx == NULL) mock_err_ctx = talloc_new(NULL);
        return ERR(mock_err_ctx, PROVIDER, "mock start_stream error");
    }
    return OK(NULL);
}

res_t ik_agent_get_provider(ik_agent_ctx_t *agent, ik_provider_t **provider_out)
{
    if (mock_get_provider_should_fail) {
        if (mock_err_ctx == NULL) mock_err_ctx = talloc_new(NULL);
        return ERR(mock_err_ctx, PROVIDER, "mock get_provider error");
    }
    *provider_out = agent->provider_instance;
    return OK(NULL);
}

res_t ik_request_build_from_conversation(TALLOC_CTX *ctx,
                                         void *agent,
                                         ik_tool_registry_t *registry,
                                         ik_request_t **req_out)
{
    (void)agent; (void)registry;
    if (mock_build_request_should_fail) {
        if (mock_err_ctx == NULL) mock_err_ctx = talloc_new(NULL);
        return ERR(mock_err_ctx, PROVIDER, "mock build_request error");
    }
    ik_request_t *req = talloc_zero_(ctx, sizeof(ik_request_t));
    *req_out = req;
    return OK(NULL);
}

/* ================================================================
 * Test fixtures
 * ================================================================ */

static TALLOC_CTX *test_ctx;
static ik_repl_ctx_t *repl;
static ik_provider_vtable_t mock_vtable;
static ik_provider_t mock_provider;

static void setup(void)
{
    test_ctx = talloc_new(NULL);

    repl = talloc_zero_(test_ctx, sizeof(ik_repl_ctx_t));
    ik_shared_ctx_t *shared = talloc_zero_(test_ctx, sizeof(ik_shared_ctx_t));
    repl->shared = shared;

    shared->cfg = talloc_zero_(test_ctx, sizeof(ik_config_t));
    shared->cfg->openai_model = talloc_strdup_(shared->cfg, "gpt-4");
    shared->cfg->openai_temperature = 0.7;
    shared->cfg->openai_max_completion_tokens = 2048;

    ik_agent_ctx_t *agent = talloc_zero_(repl, sizeof(ik_agent_ctx_t));
    repl->current = agent;
    agent->shared = shared;
    agent->model = talloc_strdup_(agent, "test-model");

    repl->current->scrollback = ik_scrollback_create(repl, 80);
    repl->current->input_buffer = ik_input_buffer_create(repl);

    repl->shared->term = talloc_zero_(repl, sizeof(ik_term_ctx_t));
    repl->shared->term->screen_rows = 24;
    repl->shared->term->screen_cols = 80;

    atomic_store(&repl->current->state, IK_AGENT_STATE_IDLE);

    /* Set up mock provider */
    mock_vtable.start_stream = mock_start_stream;
    mock_provider.vt = &mock_vtable;
    mock_provider.ctx = NULL;
    agent->provider_instance = &mock_provider;

    /* Reset mocks */
    mock_start_stream_should_fail = false;
    mock_get_provider_should_fail = false;
    mock_build_request_should_fail = false;
    mock_bang_dispatch_should_fail = false;
    mock_token_cache_add_turn_called = false;
    if (mock_err_ctx != NULL) {
        talloc_free(mock_err_ctx);
        mock_err_ctx = NULL;
    }
}

static void teardown(void)
{
    if (mock_err_ctx != NULL) {
        talloc_free(mock_err_ctx);
        mock_err_ctx = NULL;
    }
    talloc_free(test_ctx);
}

/* ================================================================
 * Tests: send_to_llm_for_agent_bang
 * ================================================================ */

START_TEST(test_bang_happy_path) {
    send_to_llm_for_agent_bang(repl, repl->current, "Hello world", "!greet");

    ck_assert_int_eq(repl->current->curl_still_running, 1);
}
END_TEST

START_TEST(test_bang_no_model_noop) {
    talloc_free(repl->current->model);
    repl->current->model = talloc_strdup_(repl->current, "");

    send_to_llm_for_agent_bang(repl, repl->current, "Hi", "!hi");

    ck_assert_int_eq(repl->current->curl_still_running, 0);
}
END_TEST

START_TEST(test_bang_with_token_cache) {
    /* Set a non-null token_cache to trigger ik_token_cache_add_turn */
    repl->current->token_cache = (ik_token_cache_t *)1;

    send_to_llm_for_agent_bang(repl, repl->current, "Hello", "!hi");

    ck_assert(mock_token_cache_add_turn_called);

    /* Clear token_cache so teardown doesn't try to free it */
    repl->current->token_cache = NULL;
}
END_TEST

START_TEST(test_bang_get_provider_fails) {
    mock_get_provider_should_fail = true;

    send_to_llm_for_agent_bang(repl, repl->current, "Hi", "!hi");

    ck_assert_int_eq(repl->current->curl_still_running, 0);
}
END_TEST

START_TEST(test_bang_build_request_fails) {
    mock_build_request_should_fail = true;

    send_to_llm_for_agent_bang(repl, repl->current, "Hi", "!hi");

    ck_assert_int_eq(repl->current->curl_still_running, 0);
}
END_TEST

START_TEST(test_bang_start_stream_fails) {
    mock_start_stream_should_fail = true;

    send_to_llm_for_agent_bang(repl, repl->current, "Hi", "!hi");

    ck_assert_int_eq(repl->current->curl_still_running, 0);
}
END_TEST

START_TEST(test_bang_with_db_and_session) {
    /* Set db_ctx and session_id > 0 to trigger DB persist block */
    repl->shared->db_ctx = (ik_db_ctx_t *)1;
    repl->shared->session_id = 42;

    send_to_llm_for_agent_bang(repl, repl->current, "Hello", "!hi");

    ck_assert_int_eq(repl->current->curl_still_running, 1);
}
END_TEST

START_TEST(test_bang_with_existing_response_fields) {
    /* Set assistant_response and streaming_line_buffer non-null to cover free branches */
    repl->current->assistant_response = talloc_strdup_(repl->current, "old response");
    repl->current->streaming_line_buffer = talloc_strdup_(repl->current, "old buffer");

    send_to_llm_for_agent_bang(repl, repl->current, "Hello", "!hi");

    ck_assert_ptr_null(repl->current->assistant_response);
    ck_assert_ptr_null(repl->current->streaming_line_buffer);
}
END_TEST

START_TEST(test_send_to_llm_for_agent_with_token_cache) {
    /* Test non-bang send_to_llm_for_agent with token_cache set */
    repl->current->token_cache = (ik_token_cache_t *)1;

    send_to_llm_for_agent(repl, repl->current, "hello world");

    ck_assert(mock_token_cache_add_turn_called);

    repl->current->token_cache = NULL;
}
END_TEST

/* ================================================================
 * Tests: bang path in ik_repl_handle_newline_action
 * ================================================================ */

START_TEST(test_handle_newline_bang_command) {
    const char *cmd = "!hello";
    res_t sr = ik_input_buffer_set_text(repl->current->input_buffer, cmd, strlen(cmd));
    ck_assert(!is_err(&sr));

    res_t r = ik_repl_handle_newline_action(repl);
    ck_assert(!is_err(&r));
}
END_TEST

START_TEST(test_handle_newline_bang_dispatch_fails) {
    mock_bang_dispatch_should_fail = true;
    const char *cmd = "!fail";
    res_t sr = ik_input_buffer_set_text(repl->current->input_buffer, cmd, strlen(cmd));
    ck_assert(!is_err(&sr));

    res_t r = ik_repl_handle_newline_action(repl);
    ck_assert(!is_err(&r));
}
END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *repl_actions_llm_bang_suite(void)
{
    Suite *s = suite_create("REPL Actions LLM - Bang Commands");

    TCase *tc_bang = tcase_create("bang");
    tcase_set_timeout(tc_bang, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_bang, setup, teardown);
    tcase_add_test(tc_bang, test_bang_happy_path);
    tcase_add_test(tc_bang, test_bang_no_model_noop);
    tcase_add_test(tc_bang, test_bang_with_token_cache);
    tcase_add_test(tc_bang, test_bang_get_provider_fails);
    tcase_add_test(tc_bang, test_bang_build_request_fails);
    tcase_add_test(tc_bang, test_bang_start_stream_fails);
    tcase_add_test(tc_bang, test_bang_with_db_and_session);
    tcase_add_test(tc_bang, test_bang_with_existing_response_fields);
    tcase_add_test(tc_bang, test_send_to_llm_for_agent_with_token_cache);
    suite_add_tcase(s, tc_bang);

    TCase *tc_newline = tcase_create("newline_bang");
    tcase_set_timeout(tc_newline, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_newline, setup, teardown);
    tcase_add_test(tc_newline, test_handle_newline_bang_command);
    tcase_add_test(tc_newline, test_handle_newline_bang_dispatch_fails);
    suite_add_tcase(s, tc_newline);

    return s;
}

int main(void)
{
    Suite *s = repl_actions_llm_bang_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr,
        "reports/check/unit/apps/ikigai/repl/repl_actions_llm_bang_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
