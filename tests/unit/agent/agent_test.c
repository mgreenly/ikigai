#include "../../../src/agent.h"
#include "../../../src/shared.h"
#include "../../../src/error.h"
#include "../../test_utils.h"

#include <check.h>
#include <talloc.h>
#include <string.h>
#include <time.h>

// Test ik_agent_create() succeeds
START_TEST(test_agent_create_success) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create minimal shared context (we just need a pointer for this test)
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);

    talloc_free(ctx);
}
END_TEST
// Test agent->name is NULL initially
START_TEST(test_agent_name_null_initially)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert_ptr_null(agent->name);

    talloc_free(ctx);
}

END_TEST
// Test agent->shared matches input
START_TEST(test_agent_shared_matches_input)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert_ptr_eq(agent->shared, shared);

    talloc_free(ctx);
}

END_TEST
// Test agent->scrollback is initialized
START_TEST(test_agent_scrollback_initialized)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert_ptr_nonnull(agent->scrollback);

    talloc_free(ctx);
}

END_TEST
// Test agent->layer_cake is initialized
START_TEST(test_agent_layer_cake_initialized)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert_ptr_nonnull(agent->layer_cake);

    talloc_free(ctx);
}

END_TEST
// Test all layer pointers are non-NULL
START_TEST(test_agent_all_layers_initialized)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert_ptr_nonnull(agent->scrollback_layer);
    ck_assert_ptr_nonnull(agent->spinner_layer);
    ck_assert_ptr_nonnull(agent->separator_layer);
    ck_assert_ptr_nonnull(agent->input_layer);
    ck_assert_ptr_nonnull(agent->completion_layer);

    talloc_free(ctx);
}

END_TEST
// Test agent->viewport_offset is 0 initially
START_TEST(test_agent_viewport_offset_zero)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert_uint_eq(agent->viewport_offset, 0);

    talloc_free(ctx);
}

END_TEST
// Test agent->input_buffer is initialized
START_TEST(test_agent_input_buffer_initialized)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert_ptr_nonnull(agent->input_buffer);

    talloc_free(ctx);
}

END_TEST
// Test agent->separator_visible is true initially
START_TEST(test_agent_separator_visible_true)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert(agent->separator_visible == true);

    talloc_free(ctx);
}

END_TEST
// Test agent->input_buffer_visible is true initially
START_TEST(test_agent_input_buffer_visible_true)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert(agent->input_buffer_visible == true);

    talloc_free(ctx);
}

END_TEST
// Test agent->messages is initialized (empty array)
START_TEST(test_agent_messages_initialized)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    // Messages array starts empty (NULL) with count 0
    ck_assert_uint_eq(agent->message_count, 0);

    talloc_free(ctx);
}

END_TEST
// Test agent->marks is NULL and mark_count is 0 initially
START_TEST(test_agent_marks_and_count_initially)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert_ptr_null(agent->marks);
    ck_assert_uint_eq(agent->mark_count, 0);
    talloc_free(ctx);
}

END_TEST
// Test agent->state is IK_AGENT_STATE_IDLE initially
START_TEST(test_agent_state_idle_initially)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert_int_eq(agent->state, IK_AGENT_STATE_IDLE);

    talloc_free(ctx);
}

END_TEST
// Test agent->provider_instance is NULL initially (lazy loading)
START_TEST(test_agent_provider_instance_null_initially)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    // Provider instance is NULL initially (lazy-loaded on first use)
    ck_assert_ptr_null(agent->provider_instance);

    talloc_free(ctx);
}

END_TEST
// Test agent->curl_still_running is 0 initially
START_TEST(test_agent_curl_still_running_zero_initially)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert_int_eq(agent->curl_still_running, 0);

    talloc_free(ctx);
}

END_TEST
// Test agent response-related fields are NULL initially
START_TEST(test_agent_response_fields_null_initially)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert_ptr_null(agent->assistant_response);
    ck_assert_ptr_null(agent->streaming_line_buffer);
    ck_assert_ptr_null(agent->http_error_message);
    ck_assert_ptr_null(agent->response_model);
    ck_assert_ptr_null(agent->response_finish_reason);
    talloc_free(ctx);
}

END_TEST
// Test agent token fields are 0 initially
START_TEST(test_agent_response_tokens_zero_initially)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert_int_eq(agent->response_input_tokens, 0);
    ck_assert_int_eq(agent->response_output_tokens, 0);
    ck_assert_int_eq(agent->response_thinking_tokens, 0);

    talloc_free(ctx);
}

END_TEST
// Test agent tool thread fields initialized correctly
START_TEST(test_agent_tool_fields_initialized)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert_ptr_null(agent->pending_tool_call);
    ck_assert(agent->tool_thread_running == false);
    ck_assert(agent->tool_thread_complete == false);
    ck_assert_int_eq(agent->tool_iteration_count, 0);
    talloc_free(ctx);
}

END_TEST
// Test agent->spinner_state is properly initialized
START_TEST(test_agent_spinner_state_initialized)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert_uint_eq(agent->spinner_state.frame_index, 0);
    ck_assert(agent->spinner_state.visible == false);

    talloc_free(ctx);
}

END_TEST
// Test agent->completion is NULL initially
START_TEST(test_agent_completion_null_initially)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert_ptr_null(agent->completion);

    talloc_free(ctx);
}

END_TEST
// Test mutex is initialized and can be locked/unlocked
START_TEST(test_agent_tool_thread_mutex_initialized)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);

    // Test that we can lock and unlock the mutex
    int lock_result = pthread_mutex_lock(&agent->tool_thread_mutex);
    ck_assert_int_eq(lock_result, 0);

    int unlock_result = pthread_mutex_unlock(&agent->tool_thread_mutex);
    ck_assert_int_eq(unlock_result, 0);

    talloc_free(ctx);
}

END_TEST
// Test agent->created_at is set to current time
START_TEST(test_agent_create_sets_created_at)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    int64_t before = time(NULL);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);

    int64_t after = time(NULL);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert_int_ge(agent->created_at, before);
    ck_assert_int_le(agent->created_at, after);

    talloc_free(ctx);
}

END_TEST
// Test agent->repl backpointer is NULL initially (no repl context yet)
START_TEST(test_agent_repl_backpointer_null_initially)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);

    ik_agent_ctx_t *agent = NULL;
    res_t res = ik_agent_create(ctx, shared, NULL, &agent);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(agent);
    ck_assert_ptr_null(agent->repl);

    talloc_free(ctx);
}

END_TEST

static Suite *agent_suite(void)
{
    Suite *s = suite_create("Agent Context");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);
    tcase_add_test(tc_core, test_agent_create_success);
    tcase_add_test(tc_core, test_agent_name_null_initially);
    tcase_add_test(tc_core, test_agent_shared_matches_input);
    tcase_add_test(tc_core, test_agent_scrollback_initialized);
    tcase_add_test(tc_core, test_agent_layer_cake_initialized);
    tcase_add_test(tc_core, test_agent_all_layers_initialized);
    tcase_add_test(tc_core, test_agent_viewport_offset_zero);
    tcase_add_test(tc_core, test_agent_input_buffer_initialized);
    tcase_add_test(tc_core, test_agent_separator_visible_true);
    tcase_add_test(tc_core, test_agent_input_buffer_visible_true);
    tcase_add_test(tc_core, test_agent_messages_initialized);
    tcase_add_test(tc_core, test_agent_marks_and_count_initially);
    tcase_add_test(tc_core, test_agent_state_idle_initially);
    tcase_add_test(tc_core, test_agent_provider_instance_null_initially);
    tcase_add_test(tc_core, test_agent_curl_still_running_zero_initially);
    tcase_add_test(tc_core, test_agent_response_fields_null_initially);
    tcase_add_test(tc_core, test_agent_response_tokens_zero_initially);
    tcase_add_test(tc_core, test_agent_tool_fields_initialized);
    tcase_add_test(tc_core, test_agent_spinner_state_initialized);
    tcase_add_test(tc_core, test_agent_completion_null_initially);
    tcase_add_test(tc_core, test_agent_tool_thread_mutex_initialized);
    tcase_add_test(tc_core, test_agent_create_sets_created_at);
    tcase_add_test(tc_core, test_agent_repl_backpointer_null_initially);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = agent_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
