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
#include "shared/error.h"
#include "shared/wrapper.h"
#include "apps/ikigai/db/message.h"
#include "apps/ikigai/db/session.h"
#include <talloc.h>
#include <string.h>

static TALLOC_CTX *mock_err_ctx = NULL;

res_t ik_db_message_insert(ik_db_ctx_t *db_ctx,
                           int64_t session_id,
                           const char *agent_uuid,
                           const char *kind,
                           const char *content,
                           const char *data_json)
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

static res_t mock_start_stream(void *ctx, const ik_request_t *req,
                               ik_stream_cb_t stream_cb, void *stream_ctx,
                               ik_provider_completion_cb_t completion_cb, void *completion_ctx)
{
    (void)ctx; (void)req; (void)stream_cb; (void)stream_ctx;
    (void)completion_cb; (void)completion_ctx;
    return OK(NULL);
}

res_t ik_agent_get_provider(ik_agent_ctx_t *agent, ik_provider_t **provider_out)
{
    *provider_out = agent->provider_instance;
    return OK(NULL);
}

res_t ik_request_build_from_conversation(TALLOC_CTX *ctx,
                                         void *agent,
                                         ik_tool_registry_t *registry,
                                         ik_request_t **req_out)
{
    (void)agent;
    (void)registry;

    ik_request_t *req = talloc_zero_(ctx, sizeof(ik_request_t));
    if (req == NULL) {
        if (mock_err_ctx == NULL) mock_err_ctx = talloc_new(NULL);
        return ERR(mock_err_ctx, OUT_OF_MEMORY, "Out of memory");
    }
    *req_out = req;
    return OK(NULL);
}

static TALLOC_CTX *test_ctx;
static ik_repl_ctx_t *repl;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(test_ctx);

    repl = talloc_zero_(test_ctx, sizeof(ik_repl_ctx_t));
    ck_assert_ptr_nonnull(repl);

    ik_shared_ctx_t *shared = talloc_zero_(test_ctx, sizeof(ik_shared_ctx_t));
    ck_assert_ptr_nonnull(shared);
    repl->shared = shared;

    shared->cfg = talloc_zero_(test_ctx, sizeof(ik_config_t));
    ck_assert_ptr_nonnull(shared->cfg);
    shared->cfg->openai_model = talloc_strdup_(shared->cfg, "gpt-4");
    shared->cfg->openai_temperature = 0.7;
    shared->cfg->openai_max_completion_tokens = 2048;

    ik_agent_ctx_t *agent = talloc_zero_(repl, sizeof(ik_agent_ctx_t));
    ck_assert_ptr_nonnull(agent);
    repl->current = agent;
    agent->shared = shared;

    repl->current->scrollback = ik_scrollback_create(repl, 80);
    ck_assert_ptr_nonnull(repl->current->scrollback);

    repl->current->input_buffer = ik_input_buffer_create(repl);
    ck_assert_ptr_nonnull(repl->current->input_buffer);

    repl->current->messages = NULL;
    repl->current->message_count = 0;
    repl->current->message_capacity = 0;

    repl->shared->term = talloc_zero_(repl, sizeof(ik_term_ctx_t));
    ck_assert_ptr_nonnull(repl->shared->term);
    repl->shared->term->screen_rows = 24;
    repl->shared->term->screen_cols = 80;

    repl->current->viewport_offset = 0;
    repl->current->curl_still_running = 0;

    atomic_store(&repl->current->state, IK_AGENT_STATE_IDLE);

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

// Test: Submit while agent is in WAITING_FOR_LLM state is silently ignored
START_TEST(test_submit_while_waiting_for_llm) {
    atomic_store(&repl->current->state, IK_AGENT_STATE_WAITING_FOR_LLM);

    const char *test_text = "hello world";
    for (const char *p = test_text; *p; p++) {
        res_t r = ik_byte_array_append(repl->current->input_buffer->text, (uint8_t)*p);
        ck_assert(is_ok(&r));
    }
    size_t initial_size = ik_byte_array_size(repl->current->input_buffer->text);

    res_t result = ik_repl_handle_newline_action(repl);
    ck_assert(is_ok(&result));

    // Input buffer must be unchanged
    ck_assert_uint_eq(ik_byte_array_size(repl->current->input_buffer->text), initial_size);

    // State must remain WAITING_FOR_LLM
    ck_assert_int_eq((int)atomic_load(&repl->current->state),
                     (int)IK_AGENT_STATE_WAITING_FOR_LLM);
}
END_TEST

// Test: Submit while agent is in EXECUTING_TOOL state is silently ignored
START_TEST(test_submit_while_executing_tool) {
    atomic_store(&repl->current->state, IK_AGENT_STATE_EXECUTING_TOOL);

    const char *test_text = "some input";
    for (const char *p = test_text; *p; p++) {
        res_t r = ik_byte_array_append(repl->current->input_buffer->text, (uint8_t)*p);
        ck_assert(is_ok(&r));
    }
    size_t initial_size = ik_byte_array_size(repl->current->input_buffer->text);

    res_t result = ik_repl_handle_newline_action(repl);
    ck_assert(is_ok(&result));

    // Input buffer must be unchanged
    ck_assert_uint_eq(ik_byte_array_size(repl->current->input_buffer->text), initial_size);

    // State must remain EXECUTING_TOOL
    ck_assert_int_eq((int)atomic_load(&repl->current->state),
                     (int)IK_AGENT_STATE_EXECUTING_TOOL);
}
END_TEST

// Test: Rapid double submit — first succeeds, second is silently ignored
START_TEST(test_rapid_double_submit) {
    // Set up model and provider for the first submit to succeed
    repl->current->model = talloc_strdup(repl->current, "gpt-4");

    static const ik_provider_vtable_t mock_vt = {
        .fdset = NULL, .perform = NULL, .timeout = NULL, .info_read = NULL,
        .start_stream = mock_start_stream, .cleanup = NULL, .cancel = NULL,
    };
    ik_provider_t *mock_provider = talloc_zero(repl->current, ik_provider_t);
    mock_provider->name = "mock";
    mock_provider->vt = &mock_vt;
    mock_provider->ctx = talloc_zero_(repl->current, 1);
    repl->current->provider_instance = mock_provider;

    // First submit while IDLE — should succeed
    const char *first_text = "first message";
    for (const char *p = first_text; *p; p++) {
        res_t r = ik_byte_array_append(repl->current->input_buffer->text, (uint8_t)*p);
        ck_assert(is_ok(&r));
    }

    res_t result = ik_repl_handle_newline_action(repl);
    ck_assert(is_ok(&result));

    // After first submit, buffer should be cleared and state WAITING_FOR_LLM
    ck_assert_uint_eq(ik_byte_array_size(repl->current->input_buffer->text), 0);
    ck_assert_int_eq((int)atomic_load(&repl->current->state),
                     (int)IK_AGENT_STATE_WAITING_FOR_LLM);

    // Second submit (paste scenario) while WAITING_FOR_LLM — should be ignored
    const char *second_text = "second message";
    for (const char *p = second_text; *p; p++) {
        res_t r = ik_byte_array_append(repl->current->input_buffer->text, (uint8_t)*p);
        ck_assert(is_ok(&r));
    }
    size_t second_size = ik_byte_array_size(repl->current->input_buffer->text);

    result = ik_repl_handle_newline_action(repl);
    ck_assert(is_ok(&result));

    // Buffer must be preserved (second submit was ignored)
    ck_assert_uint_eq(ik_byte_array_size(repl->current->input_buffer->text), second_size);

    // State must still be WAITING_FOR_LLM
    ck_assert_int_eq((int)atomic_load(&repl->current->state),
                     (int)IK_AGENT_STATE_WAITING_FOR_LLM);
}
END_TEST

// Test: Submit while IDLE still works (regression)
START_TEST(test_submit_while_idle_regression) {
    repl->current->model = talloc_strdup(repl->current, "gpt-4");

    static const ik_provider_vtable_t mock_vt = {
        .fdset = NULL, .perform = NULL, .timeout = NULL, .info_read = NULL,
        .start_stream = mock_start_stream, .cleanup = NULL, .cancel = NULL,
    };
    ik_provider_t *mock_provider = talloc_zero(repl->current, ik_provider_t);
    mock_provider->name = "mock";
    mock_provider->vt = &mock_vt;
    mock_provider->ctx = talloc_zero_(repl->current, 1);
    repl->current->provider_instance = mock_provider;

    const char *test_text = "hello from idle";
    for (const char *p = test_text; *p; p++) {
        res_t r = ik_byte_array_append(repl->current->input_buffer->text, (uint8_t)*p);
        ck_assert(is_ok(&r));
    }

    res_t result = ik_repl_handle_newline_action(repl);
    ck_assert(is_ok(&result));

    // Buffer must be cleared after successful submit
    ck_assert_uint_eq(ik_byte_array_size(repl->current->input_buffer->text), 0);

    // Agent must have transitioned to WAITING_FOR_LLM
    ck_assert_int_eq((int)atomic_load(&repl->current->state),
                     (int)IK_AGENT_STATE_WAITING_FOR_LLM);
}
END_TEST

static Suite *repl_actions_llm_state_guard_suite(void)
{
    Suite *s = suite_create("REPL Actions LLM State Guard");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_submit_while_waiting_for_llm);
    tcase_add_test(tc_core, test_submit_while_executing_tool);
    tcase_add_test(tc_core, test_rapid_double_submit);
    tcase_add_test(tc_core, test_submit_while_idle_regression);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = repl_actions_llm_state_guard_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/repl/repl_actions_llm_state_guard_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
