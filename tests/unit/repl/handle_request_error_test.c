/**
 * @file handle_request_error_test.c
 * @brief Unit tests for handle_request_error function path
 *
 * Tests the error handling path when HTTP requests fail during LLM communication.
 * This covers the handle_request_error function which displays errors in scrollback.
 */

#include "repl_streaming_test_common.h"
#include "../../../src/agent.h"

/* Test: Error handling without partial response */
START_TEST(test_error_handling_no_partial_response) {
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = create_test_repl_with_llm(ctx);

    // Reset mock state
    simulate_completion = false;

    // Set up state to simulate a failed request
    repl->state = IK_REPL_STATE_WAITING_FOR_LLM;
    repl->http_error_message = talloc_strdup(repl, "Connection timeout");
    repl->assistant_response = NULL;

    // Add a user message
    res_t msg_res = ik_openai_msg_create(ctx, "user", "Hello");
    ck_assert(is_ok(&msg_res));
    res_t add_res = ik_openai_conversation_add_msg(repl->current->conversation, msg_res.ok);
    ck_assert(is_ok(&add_res));

    // Initial conversation should have 1 message (user)
    ck_assert_uint_eq((unsigned int)repl->current->conversation->message_count, 1);

    // Count scrollback lines before
    size_t lines_before = ik_scrollback_get_line_count(repl->current->scrollback);

    // Simulate a request that is running
    repl->curl_still_running = 1;

    // Enable completion simulation
    simulate_completion = true;

    // Call handle_curl_events - should detect error and call handle_request_error
    res_t result = handle_curl_events(repl, 1);
    ck_assert(is_ok(&result));

    // Verify error was added to scrollback
    size_t lines_after = ik_scrollback_get_line_count(repl->current->scrollback);
    ck_assert_uint_eq(lines_after, lines_before + 1);

    // Verify the error message is in scrollback
    ck_assert_uint_gt(lines_after, 0);

    // Get the last line
    const char *last_line = NULL;
    size_t last_line_len = 0;
    res_t line_res = ik_scrollback_get_line_text(repl->current->scrollback, lines_after - 1, &last_line, &last_line_len);
    ck_assert(is_ok(&line_res));
    ck_assert_ptr_nonnull(last_line);
    ck_assert_ptr_nonnull(strstr(last_line, "Error:"));
    ck_assert_ptr_nonnull(strstr(last_line, "Connection timeout"));

    // Verify NO assistant message was added to conversation
    ck_assert_uint_eq((unsigned int)repl->current->conversation->message_count, 1);

    // Verify state transitioned back to IDLE
    ck_assert_int_eq(repl->state, IK_REPL_STATE_IDLE);

    // Verify error message was cleared
    ck_assert_ptr_null(repl->http_error_message);

    // Verify assistant_response remains NULL
    ck_assert_ptr_null(repl->assistant_response);

    // Clean up
    simulate_completion = false;
    talloc_free(ctx);
}
END_TEST
/* Test: Error handling with partial assistant response */
START_TEST(test_error_handling_with_partial_response)
{
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = create_test_repl_with_llm(ctx);

    // Reset mock state
    simulate_completion = false;

    // Set up state to simulate a failed request with partial response
    repl->state = IK_REPL_STATE_WAITING_FOR_LLM;
    repl->http_error_message = talloc_strdup(repl, "Stream interrupted");
    repl->assistant_response = talloc_strdup(repl, "Partial response text that was received before error");

    // Add a user message
    res_t msg_res = ik_openai_msg_create(ctx, "user", "Tell me a story");
    ck_assert(is_ok(&msg_res));
    res_t add_res = ik_openai_conversation_add_msg(repl->current->conversation, msg_res.ok);
    ck_assert(is_ok(&add_res));

    // Count scrollback lines before
    size_t lines_before = ik_scrollback_get_line_count(repl->current->scrollback);

    // Simulate a running request
    repl->curl_still_running = 1;
    simulate_completion = true;

    // Call handle_curl_events - should detect error and call handle_request_error
    res_t result = handle_curl_events(repl, 1);
    ck_assert(is_ok(&result));

    // Verify error was added to scrollback
    size_t lines_after = ik_scrollback_get_line_count(repl->current->scrollback);
    ck_assert_uint_eq(lines_after, lines_before + 1);

    // Verify the error message is in scrollback
    const char *last_line = NULL;
    size_t last_line_len = 0;
    res_t line_res = ik_scrollback_get_line_text(repl->current->scrollback, lines_after - 1, &last_line, &last_line_len);
    ck_assert(is_ok(&line_res));
    ck_assert_ptr_nonnull(strstr(last_line, "Error:"));
    ck_assert_ptr_nonnull(strstr(last_line, "Stream interrupted"));

    // Verify NO assistant message was added (partial response was discarded)
    ck_assert_uint_eq((unsigned int)repl->current->conversation->message_count, 1);

    // Verify error message was cleared
    ck_assert_ptr_null(repl->http_error_message);

    // Verify partial assistant_response was cleared
    ck_assert_ptr_null(repl->assistant_response);

    // Verify state transitioned back to IDLE
    ck_assert_int_eq(repl->state, IK_REPL_STATE_IDLE);

    // Clean up
    simulate_completion = false;
    talloc_free(ctx);
}

END_TEST
/* Test: Various error message formats */
START_TEST(test_various_error_messages)
{
    const char *test_errors[] = {
        "HTTP 404 Not Found",
        "API key invalid",
        "Rate limit exceeded",
        "Network unreachable",
        "Timeout after 30 seconds",
        "SSL certificate verification failed"
    };

    for (size_t i = 0; i < sizeof(test_errors) / sizeof(test_errors[0]); i++) {
        void *ctx = talloc_new(NULL);
        ik_repl_ctx_t *repl = create_test_repl_with_llm(ctx);

        simulate_completion = false;
        repl->state = IK_REPL_STATE_WAITING_FOR_LLM;
        repl->http_error_message = talloc_strdup(repl, test_errors[i]);
        repl->assistant_response = NULL;

        // Simulate request completion
        repl->curl_still_running = 1;
        simulate_completion = true;

        res_t result = handle_curl_events(repl, 1);
        ck_assert(is_ok(&result));

        // Verify error was added to scrollback
        size_t line_count = ik_scrollback_get_line_count(repl->current->scrollback);
        ck_assert_uint_gt(line_count, 0);

        const char *last_line = NULL;
        size_t last_line_len = 0;
        res_t line_res = ik_scrollback_get_line_text(repl->current->scrollback, line_count - 1, &last_line, &last_line_len);
        ck_assert(is_ok(&line_res));
        ck_assert_ptr_nonnull(strstr(last_line, "Error:"));
        ck_assert_ptr_nonnull(strstr(last_line, test_errors[i]));

        // Verify error was cleared
        ck_assert_ptr_null(repl->http_error_message);

        simulate_completion = false;
        talloc_free(ctx);
    }
}

END_TEST
/* Test: Long error message handling */
START_TEST(test_long_error_message)
{
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = create_test_repl_with_llm(ctx);

    simulate_completion = false;
    repl->state = IK_REPL_STATE_WAITING_FOR_LLM;

    // Create a very long error message
    char long_error[512];
    memset(long_error, 'X', sizeof(long_error) - 1);
    long_error[sizeof(long_error) - 1] = '\0';

    repl->http_error_message = talloc_strdup(repl, long_error);
    repl->assistant_response = NULL;

    // Simulate request completion
    repl->curl_still_running = 1;
    simulate_completion = true;

    res_t result = handle_curl_events(repl, 1);
    ck_assert(is_ok(&result));

    // Verify error was added to scrollback
    size_t line_count = ik_scrollback_get_line_count(repl->current->scrollback);
    ck_assert_uint_gt(line_count, 0);

    const char *last_line = NULL;
    size_t last_line_len = 0;
    res_t line_res = ik_scrollback_get_line_text(repl->current->scrollback, line_count - 1, &last_line, &last_line_len);
    ck_assert(is_ok(&line_res));
    ck_assert_ptr_nonnull(strstr(last_line, "Error:"));
    ck_assert_ptr_nonnull(strstr(last_line, long_error));

    // Verify error was cleared
    ck_assert_ptr_null(repl->http_error_message);

    simulate_completion = false;
    talloc_free(ctx);
}

END_TEST
/* Test: Error with long partial response */
START_TEST(test_error_with_long_partial_response)
{
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = create_test_repl_with_llm(ctx);

    simulate_completion = false;
    repl->state = IK_REPL_STATE_WAITING_FOR_LLM;
    repl->http_error_message = talloc_strdup(repl, "Connection lost");

    // Create a long partial response
    char long_response[2048];
    memset(long_response, 'A', sizeof(long_response) - 1);
    long_response[sizeof(long_response) - 1] = '\0';

    repl->assistant_response = talloc_strdup(repl, long_response);

    // Simulate request completion
    repl->curl_still_running = 1;
    simulate_completion = true;

    res_t result = handle_curl_events(repl, 1);
    ck_assert(is_ok(&result));

    // Verify error was added to scrollback
    size_t line_count = ik_scrollback_get_line_count(repl->current->scrollback);
    ck_assert_uint_gt(line_count, 0);

    const char *last_line = NULL;
    size_t last_line_len = 0;
    res_t line_res = ik_scrollback_get_line_text(repl->current->scrollback, line_count - 1, &last_line, &last_line_len);
    ck_assert(is_ok(&line_res));
    ck_assert_ptr_nonnull(strstr(last_line, "Error:"));
    ck_assert_ptr_nonnull(strstr(last_line, "Connection lost"));

    // Verify partial response was cleared
    ck_assert_ptr_null(repl->assistant_response);

    // Verify error was cleared
    ck_assert_ptr_null(repl->http_error_message);

    simulate_completion = false;
    talloc_free(ctx);
}

END_TEST

static Suite *handle_request_error_suite(void)
{
    Suite *s = suite_create("handle_request_error");

    TCase *tc_core = tcase_create("Core");
    tcase_add_test(tc_core, test_error_handling_no_partial_response);
    tcase_add_test(tc_core, test_error_handling_with_partial_response);
    tcase_add_test(tc_core, test_various_error_messages);
    tcase_add_test(tc_core, test_long_error_message);
    tcase_add_test(tc_core, test_error_with_long_partial_response);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = handle_request_error_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
