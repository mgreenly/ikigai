#include <check.h>
#include "../../../src/repl_actions.h"
#include "../../../src/repl_actions_internal.h"
#include "../../../src/repl.h"
#include "../../../src/agent.h"
#include "../../../src/scrollback.h"
#include "../../../src/input_buffer/core.h"
#include "../../../src/providers/provider.h"
#include "../../../src/providers/request.h"
#include "../../../src/config.h"
#include "../../../src/shared.h"
#include "../../../src/error.h"
#include "../../../src/wrapper.h"
#include "../../../src/db/message.h"
#include "../../../src/db/session.h"
#include <talloc.h>
#include <string.h>

// Mock state for provider operations
static bool mock_get_provider_should_fail = false;
static bool mock_build_request_should_fail = false;
static bool mock_start_stream_should_fail = false;
static TALLOC_CTX *mock_err_ctx = NULL;

// Mock ik_db_message_insert (not used in these tests, but needed for linking)
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

// Mock ik_db_session_get_active (not used in these tests, but needed for linking)
res_t ik_db_session_get_active(ik_db_ctx_t *db_ctx, int64_t *session_id_out)
{
    (void)db_ctx; (void)session_id_out;
    return OK(NULL);
}

// Mock ik_db_session_create (not used in these tests, but needed for linking)
res_t ik_db_session_create(ik_db_ctx_t *db_ctx, int64_t *session_id_out)
{
    (void)db_ctx; (void)session_id_out;
    return OK(NULL);
}

// Mock ik_agent_invalidate_provider (not used in these tests, but needed for linking)
void ik_agent_invalidate_provider(ik_agent_ctx_t *agent)
{
    (void)agent;
}

// Mock ik_agent_restore_from_row (not used in these tests, but needed for linking)
res_t ik_agent_restore_from_row(ik_agent_ctx_t *agent, const void *row)
{
    (void)agent; (void)row;
    return OK(NULL);
}

// Mock start_stream for provider
static res_t mock_start_stream(void *ctx, const ik_request_t *req,
                               ik_stream_cb_t stream_cb, void *stream_ctx,
                               ik_provider_completion_cb_t completion_cb, void *completion_ctx)
{
    (void)ctx; (void)req; (void)stream_cb; (void)stream_ctx;
    (void)completion_cb; (void)completion_ctx;

    if (mock_start_stream_should_fail) {
        if (mock_err_ctx == NULL) mock_err_ctx = talloc_new(NULL);
        return ERR(mock_err_ctx, PROVIDER, "Mock provider error: Failed to start stream");
    }

    return OK(NULL);
}

// Mock ik_agent_get_provider
res_t ik_agent_get_provider(ik_agent_ctx_t *agent, ik_provider_t **provider_out)
{
    if (mock_get_provider_should_fail) {
        if (mock_err_ctx == NULL) mock_err_ctx = talloc_new(NULL);
        return ERR(mock_err_ctx, PROVIDER, "Mock provider error: Failed to get provider");
    }

    *provider_out = agent->provider_instance;
    return OK(NULL);
}

// Mock ik_request_build_from_conversation
res_t ik_request_build_from_conversation(TALLOC_CTX *ctx, void *agent, ik_request_t **req_out)
{
    (void)agent;

    if (mock_build_request_should_fail) {
        if (mock_err_ctx == NULL) mock_err_ctx = talloc_new(NULL);
        return ERR(mock_err_ctx, INVALID_ARG, "Mock request error: Failed to build request");
    }

    // Create minimal request
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

    // Create minimal REPL context
    repl = talloc_zero_(test_ctx, sizeof(ik_repl_ctx_t));
    ck_assert_ptr_nonnull(repl);

    // Create shared context
    ik_shared_ctx_t *shared = talloc_zero_(test_ctx, sizeof(ik_shared_ctx_t));
    ck_assert_ptr_nonnull(shared);
    repl->shared = shared;

    // Create config
    shared->cfg = talloc_zero_(test_ctx, sizeof(ik_config_t));
    ck_assert_ptr_nonnull(shared->cfg);
    shared->cfg->openai_model = talloc_strdup_(shared->cfg, "gpt-4");
    shared->cfg->openai_temperature = 0.7;
    shared->cfg->openai_max_completion_tokens = 2048;

    // Create agent context
    ik_agent_ctx_t *agent = talloc_zero_(repl, sizeof(ik_agent_ctx_t));
    ck_assert_ptr_nonnull(agent);
    repl->current = agent;

    // Create scrollback
    repl->current->scrollback = ik_scrollback_create(repl, 80);
    ck_assert_ptr_nonnull(repl->current->scrollback);

    // Create input buffer
    repl->current->input_buffer = ik_input_buffer_create(repl);
    ck_assert_ptr_nonnull(repl->current->input_buffer);

    // Initialize conversation
    repl->current->messages = NULL;
    repl->current->message_count = 0;
    repl->current->message_capacity = 0;

    // Create terminal context
    repl->shared->term = talloc_zero_(repl, sizeof(ik_term_ctx_t));
    ck_assert_ptr_nonnull(repl->shared->term);
    repl->shared->term->screen_rows = 24;
    repl->shared->term->screen_cols = 80;

    // Set viewport offset
    repl->current->viewport_offset = 0;

    // Initialize curl_still_running
    repl->current->curl_still_running = 0;

    // Initialize state (idle)
    repl->current->state = IK_AGENT_STATE_IDLE;

    // Reset mock state
    mock_get_provider_should_fail = false;
    mock_build_request_should_fail = false;
    mock_start_stream_should_fail = false;
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

// Test: ik_agent_get_provider fails (lines 146-150)
START_TEST(test_get_provider_fails) {
    // Set up model
    repl->current->model = talloc_strdup(repl->current, "gpt-4");

    // Enable get_provider failure
    mock_get_provider_should_fail = true;

    // Insert text into input buffer
    const char *test_text = "Hello";
    for (const char *p = test_text; *p; p++) {
        res_t r = ik_byte_array_append(repl->current->input_buffer->text, (uint8_t)*p);
        ck_assert(is_ok(&r));
    }

    // Process newline action
    res_t result = ik_repl_handle_newline_action(repl);
    ck_assert(is_ok(&result));

    // Verify error message was added to scrollback
    ck_assert(repl->current->scrollback->count > 0);
    const char *last_line_text = NULL;
    size_t last_line_len = 0;
    res_t r = ik_scrollback_get_line_text(repl->current->scrollback,
                                          repl->current->scrollback->count - 1,
                                          &last_line_text, &last_line_len);
    ck_assert(is_ok(&r));
    ck_assert(strstr(last_line_text, "Failed to get provider") != NULL);

    // Verify agent returned to idle state
    ck_assert_int_eq(repl->current->state, IK_AGENT_STATE_IDLE);
}

END_TEST
// Test: ik_request_build_from_conversation fails (lines 157-161)
START_TEST(test_build_request_fails)
{
    // Set up model
    repl->current->model = talloc_strdup(repl->current, "gpt-4");

    // Create mock provider
    static const ik_provider_vtable_t mock_vt = {
        .fdset = NULL, .perform = NULL, .timeout = NULL, .info_read = NULL,
        .start_request = NULL, .start_stream = mock_start_stream, .cleanup = NULL, .cancel = NULL,
    };
    ik_provider_t *mock_provider = talloc_zero(repl->current, ik_provider_t);
    mock_provider->name = "mock";
    mock_provider->vt = &mock_vt;
    mock_provider->ctx = talloc_zero_(repl->current, 1);
    repl->current->provider_instance = mock_provider;

    // Enable build_request failure
    mock_build_request_should_fail = true;

    // Insert text into input buffer
    const char *test_text = "Hello";
    for (const char *p = test_text; *p; p++) {
        res_t r = ik_byte_array_append(repl->current->input_buffer->text, (uint8_t)*p);
        ck_assert(is_ok(&r));
    }

    // Process newline action
    res_t result = ik_repl_handle_newline_action(repl);
    ck_assert(is_ok(&result));

    // Verify error message was added to scrollback
    ck_assert(repl->current->scrollback->count > 0);
    const char *last_line_text = NULL;
    size_t last_line_len = 0;
    res_t r = ik_scrollback_get_line_text(repl->current->scrollback,
                                          repl->current->scrollback->count - 1,
                                          &last_line_text, &last_line_len);
    ck_assert(is_ok(&r));
    ck_assert(strstr(last_line_text, "Failed to build request") != NULL);

    // Verify agent returned to idle state
    ck_assert_int_eq(repl->current->state, IK_AGENT_STATE_IDLE);
}

END_TEST
// Test: provider start_stream fails (lines 169-172)
START_TEST(test_start_stream_fails)
{
    // Set up model
    repl->current->model = talloc_strdup(repl->current, "gpt-4");

    // Create mock provider
    static const ik_provider_vtable_t mock_vt = {
        .fdset = NULL, .perform = NULL, .timeout = NULL, .info_read = NULL,
        .start_request = NULL, .start_stream = mock_start_stream, .cleanup = NULL, .cancel = NULL,
    };
    ik_provider_t *mock_provider = talloc_zero(repl->current, ik_provider_t);
    mock_provider->name = "mock";
    mock_provider->vt = &mock_vt;
    mock_provider->ctx = talloc_zero_(repl->current, 1);
    repl->current->provider_instance = mock_provider;

    // Enable start_stream failure
    mock_start_stream_should_fail = true;

    // Insert text into input buffer
    const char *test_text = "Hello";
    for (const char *p = test_text; *p; p++) {
        res_t r = ik_byte_array_append(repl->current->input_buffer->text, (uint8_t)*p);
        ck_assert(is_ok(&r));
    }

    // Process newline action
    res_t result = ik_repl_handle_newline_action(repl);
    ck_assert(is_ok(&result));

    // Verify error message was added to scrollback
    ck_assert(repl->current->scrollback->count > 0);
    const char *last_line_text = NULL;
    size_t last_line_len = 0;
    res_t r = ik_scrollback_get_line_text(repl->current->scrollback,
                                          repl->current->scrollback->count - 1,
                                          &last_line_text, &last_line_len);
    ck_assert(is_ok(&r));
    ck_assert(strstr(last_line_text, "Failed to start stream") != NULL);

    // Verify agent returned to idle state
    ck_assert_int_eq(repl->current->state, IK_AGENT_STATE_IDLE);

    // Verify curl_still_running was not set
    ck_assert_int_eq(repl->current->curl_still_running, 0);
}

END_TEST

static Suite *repl_actions_llm_errors_suite(void)
{
    Suite *s = suite_create("REPL Actions LLM Errors");
    TCase *tc_core = tcase_create("Core");

    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_get_provider_fails);
    tcase_add_test(tc_core, test_build_request_fails);
    tcase_add_test(tc_core, test_start_stream_fails);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = repl_actions_llm_errors_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
