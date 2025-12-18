/**
 * @file repl_completion_test.c
 * @brief Unit tests for REPL request completion flow (Phase 1.6)
 *
 * Tests the request completion code paths.
 */

#include "repl_streaming_test_common.h"

/* Test: Request completion adds assistant message to conversation */
START_TEST(test_request_completion_adds_to_conversation) {
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = create_test_repl_with_llm(ctx);

    // Reset mock state
    simulate_completion = false;

    // Manually set up state to simulate an active request
    repl->current->state = IK_AGENT_STATE_WAITING_FOR_LLM;
    repl->current->assistant_response = talloc_strdup(repl, "This is the assistant response");
    ck_assert_ptr_nonnull(repl->current->assistant_response);

    // Add a user message first (as would happen during normal flow)
    res_t msg_res = ik_openai_msg_create(ctx, "user", "Hello");
    ck_assert(is_ok(&msg_res));
    res_t add_res = ik_openai_conversation_add_msg(repl->current->conversation, msg_res.ok);
    ck_assert(is_ok(&add_res));

    // Initial conversation should have 1 message (user)
    ck_assert_uint_eq((unsigned int)repl->current->conversation->message_count, 1);

    // Simulate a request that is running
    repl->current->curl_still_running = 1;

    // Enable completion simulation - when multi_perform is called, it will set still_running to 0
    simulate_completion = true;

    // Call handle_curl_events_ with ready=1 to trigger the completion logic
    // The function will:
    // 1. Save prev_running = 1
    // 2. Call multi_perform which sets still_running = 0 (via our mock)
    // 3. Detect completion: prev_running=1, still_running=0, state=WAITING_FOR_LLM
    // 4. Add assistant message to conversation and transition to IDLE
    res_t result = ik_repl_handle_curl_events(repl, 1);
    ck_assert(is_ok(&result));

    // Verify assistant message was added to conversation
    ck_assert_uint_eq((unsigned int)repl->current->conversation->message_count, 2);

    // Verify state transitioned back to IDLE
    ck_assert_int_eq(repl->current->state, IK_AGENT_STATE_IDLE);

    // Verify assistant_response was cleared
    ck_assert_ptr_null(repl->current->assistant_response);

    // Clean up
    simulate_completion = false;
    talloc_free(ctx);
}

END_TEST
/* Test: Request completion with NULL assistant_response */
START_TEST(test_request_completion_with_null_response)
{
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = create_test_repl_with_llm(ctx);

    // Reset mock state
    simulate_completion = false;

    // Set up state to simulate a request that completed but has no response
    repl->current->state = IK_AGENT_STATE_WAITING_FOR_LLM;
    repl->current->assistant_response = NULL;  // No response

    // Add a user message
    res_t msg_res = ik_openai_msg_create(ctx, "user", "Hello");
    ck_assert(is_ok(&msg_res));
    res_t add_res = ik_openai_conversation_add_msg(repl->current->conversation, msg_res.ok);
    ck_assert(is_ok(&add_res));

    // Simulate a running request
    repl->current->curl_still_running = 1;
    simulate_completion = true;

    // Call handle_curl_events_ - should complete but not add assistant message
    res_t result = ik_repl_handle_curl_events(repl, 1);
    ck_assert(is_ok(&result));

    // Verify NO assistant message was added (still only 1 message)
    ck_assert_uint_eq((unsigned int)repl->current->conversation->message_count, 1);

    // Verify state transitioned back to IDLE anyway
    ck_assert_int_eq(repl->current->state, IK_AGENT_STATE_IDLE);

    // Clean up
    simulate_completion = false;
    talloc_free(ctx);
}

END_TEST
/* Test: Request completion with empty assistant_response */
START_TEST(test_request_completion_with_empty_response)
{
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = create_test_repl_with_llm(ctx);

    // Reset mock state
    simulate_completion = false;

    // Set up state with empty response
    repl->current->state = IK_AGENT_STATE_WAITING_FOR_LLM;
    repl->current->assistant_response = talloc_strdup(repl, "");  // Empty string
    ck_assert_ptr_nonnull(repl->current->assistant_response);

    // Add a user message
    res_t msg_res = ik_openai_msg_create(ctx, "user", "Hello");
    ck_assert(is_ok(&msg_res));
    res_t add_res = ik_openai_conversation_add_msg(repl->current->conversation, msg_res.ok);
    ck_assert(is_ok(&add_res));

    // Simulate a running request
    repl->current->curl_still_running = 1;
    simulate_completion = true;

    // Call handle_curl_events_
    res_t result = ik_repl_handle_curl_events(repl, 1);
    ck_assert(is_ok(&result));

    // Verify NO assistant message was added
    ck_assert_uint_eq((unsigned int)repl->current->conversation->message_count, 1);

    // Verify state transitioned back to IDLE
    ck_assert_int_eq(repl->current->state, IK_AGENT_STATE_IDLE);

    // Clean up
    simulate_completion = false;
    talloc_free(ctx);
}

END_TEST
/* Test: ik_repl_handle_curl_events when not in WAITING_FOR_LLM state */
START_TEST(test_handle_curl_events_not_waiting_state)
{
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = create_test_repl_with_llm(ctx);

    // Reset mock state
    simulate_completion = false;

    // Set state to IDLE (not WAITING_FOR_LLM)
    repl->current->state = IK_AGENT_STATE_IDLE;
    repl->current->assistant_response = talloc_strdup(repl, "Some response");

    // Simulate a request completing
    repl->current->curl_still_running = 1;
    simulate_completion = true;

    // Call handle_curl_events_ - should NOT process completion since state is wrong
    res_t result = ik_repl_handle_curl_events(repl, 1);
    ck_assert(is_ok(&result));

    // State should remain IDLE
    ck_assert_int_eq(repl->current->state, IK_AGENT_STATE_IDLE);

    // assistant_response should still be there (not cleared)
    ck_assert_ptr_nonnull(repl->current->assistant_response);

    // Clean up
    simulate_completion = false;
    talloc_free(ctx);
}

END_TEST
/* Test: ik_repl_handle_curl_events with ready=0 (timeout case) */
START_TEST(test_handle_curl_events_with_ready_zero)
{
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = create_test_repl_with_llm(ctx);

    // Reset mock state
    simulate_completion = false;

    // Set up for a running request
    repl->current->state = IK_AGENT_STATE_WAITING_FOR_LLM;
    repl->current->curl_still_running = 1;

    // Call with ready=0 (select timeout)
    simulate_completion = true;
    res_t result = ik_repl_handle_curl_events(repl, 0);
    ck_assert(is_ok(&result));

    // Clean up
    simulate_completion = false;
    talloc_free(ctx);
}

END_TEST
/* Test: ik_repl_handle_curl_events when request is still running (doesn't complete) */
START_TEST(test_handle_curl_events_request_still_running)
{
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = create_test_repl_with_llm(ctx);

    // Reset mock state
    simulate_completion = false;

    // Set state to WAITING_FOR_LLM
    repl->current->state = IK_AGENT_STATE_WAITING_FOR_LLM;
    repl->current->assistant_response = talloc_strdup(repl, "Partial response");

    // Simulate a running request that does NOT complete
    repl->current->curl_still_running = 1;
    simulate_completion = false;  // Request stays running

    // Call handle_curl_events_ - request is still running
    res_t result = ik_repl_handle_curl_events(repl, 1);
    ck_assert(is_ok(&result));

    // State should remain WAITING_FOR_LLM (no completion)
    ck_assert_int_eq(repl->current->state, IK_AGENT_STATE_WAITING_FOR_LLM);

    // assistant_response should still be there (not cleared)
    ck_assert_ptr_nonnull(repl->current->assistant_response);

    // Clean up
    talloc_free(ctx);
}

END_TEST
/* Test: ik_repl_handle_curl_events render failure on completion */
START_TEST(test_handle_curl_events_render_failure_on_completion)
{
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = create_test_repl_with_llm(ctx);

    // Reset mock state
    simulate_completion = false;
    mock_write_should_fail = false;

    // Set up state to simulate a request completing
    repl->current->state = IK_AGENT_STATE_WAITING_FOR_LLM;
    repl->current->assistant_response = talloc_strdup(repl, "Test response");

    // Add a user message first
    res_t msg_res = ik_openai_msg_create(ctx, "user", "Hello");
    ck_assert(is_ok(&msg_res));
    res_t add_res = ik_openai_conversation_add_msg(repl->current->conversation, msg_res.ok);
    ck_assert(is_ok(&add_res));

    // Simulate a running request
    repl->current->curl_still_running = 1;
    simulate_completion = true;

    // Make render fail by making posix_write_ fail
    mock_write_should_fail = true;

    // Call handle_curl_events_ - completion will be detected but render will fail
    res_t result = ik_repl_handle_curl_events(repl, 1);
    ck_assert(is_err(&result));

    // Clean up
    mock_write_should_fail = false;
    simulate_completion = false;
    talloc_free(ctx);
}

END_TEST

/* Test Suite */
static Suite *repl_completion_suite(void)
{
    Suite *s = suite_create("REPL Completion");

    TCase *tc_completion = tcase_create("Completion");
    tcase_set_timeout(tc_completion, 30);
    tcase_add_test(tc_completion, test_request_completion_adds_to_conversation);
    tcase_add_test(tc_completion, test_request_completion_with_null_response);
    tcase_add_test(tc_completion, test_request_completion_with_empty_response);
    tcase_add_test(tc_completion, test_handle_curl_events_not_waiting_state);
    tcase_add_test(tc_completion, test_handle_curl_events_with_ready_zero);
    tcase_add_test(tc_completion, test_handle_curl_events_request_still_running);
    tcase_add_test(tc_completion, test_handle_curl_events_render_failure_on_completion);
    suite_add_tcase(s, tc_completion);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = repl_completion_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
