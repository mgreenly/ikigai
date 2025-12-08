/* Unit tests for OpenAI multi-handle - tool_call coverage in client_multi.c */

#include "client_multi_test_common.h"
#include "client_multi_info_read_helpers.h"
#include "tool.h"

/*
 * Coverage targets:
 * - Line 149: talloc_steal for tool_call (successful HTTP with tool_call)
 * - Line 190: talloc_free for tool_call (callback error path with tool_call)
 * - Line 219: talloc_free for tool_call (success path with tool_call)
 */

/* Completion callback that captures tool_call fields for verification */
static char *captured_tool_call_id = NULL;
static char *captured_tool_call_name = NULL;

static res_t capture_tool_call_callback(const ik_http_completion_t *completion, void *callback_ctx)
{
    if (completion->tool_call != NULL) {
        /* Copy fields for verification - tool_call will be freed after callback returns */
        captured_tool_call_id = talloc_strdup(callback_ctx, completion->tool_call->id);
        captured_tool_call_name = talloc_strdup(callback_ctx, completion->tool_call->name);
    }
    return OK(NULL);
}

/* Completion callback that returns error (to test error path) */
static res_t error_with_tool_call_callback(const ik_http_completion_t *completion, void *callback_ctx)
{
    (void)completion;
    return ERR(callback_ctx, IO, "Callback error with tool_call");
}

/*
 * Test line 149: talloc_steal for tool_call in success path
 *
 * This test exercises the path where:
 * 1. HTTP response is successful (200)
 * 2. write_ctx has a tool_call
 * 3. talloc_steal transfers ownership to multi context
 */
START_TEST(test_info_read_success_with_tool_call_steal) {
    captured_tool_call_id = NULL;
    captured_tool_call_name = NULL;

    /* Create multi-handle */
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    /* Create conversation */
    ik_openai_conversation_t *conv = create_test_conversation("What's the weather?");
    ik_cfg_t *cfg = create_test_config();

    /* Add request with completion callback that captures tool_call fields */
    res_t add_res = ik_openai_multi_add_request(multi, cfg, conv, NULL, NULL,
                                                capture_tool_call_callback, ctx, false);
    ck_assert(!add_res.is_err);

    /* Set up mock SSE response with tool call */
    const char *sse_data =
        "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"call_123\",\"type\":\"function\",\"function\":{\"name\":\"get_weather\",\"arguments\":\"{\\\"location\\\":\\\"Boston\\\"}\"}}]}}]}\n\n";
    mock_response_data = sse_data;
    mock_response_len = strlen(sse_data);
    invoke_write_callback = true;

    /* Perform to process write callback and populate tool_call */
    int still_running = 0;
    res_t perform_res = ik_openai_multi_perform(multi, &still_running);
    ck_assert(!perform_res.is_err);

    /* Setup mock message for successful completion with HTTP 200 */
    CURLMsg msg;
    setup_mock_curl_msg(&msg, g_last_easy_handle, CURLE_OK, 200);

    /* Call info_read - should execute line 149: talloc_steal for tool_call */
    res_t info_res = ik_openai_multi_info_read(multi);
    ck_assert(!info_res.is_err);

    /* Verify tool_call fields were captured by callback */
    ck_assert(captured_tool_call_id != NULL);
    ck_assert(captured_tool_call_name != NULL);
    ck_assert_str_eq(captured_tool_call_id, "call_123");
    ck_assert_str_eq(captured_tool_call_name, "get_weather");

    /* Clean up */
    invoke_write_callback = false;
    talloc_free(captured_tool_call_id);
    talloc_free(captured_tool_call_name);
    captured_tool_call_id = NULL;
    captured_tool_call_name = NULL;
    talloc_free(multi);
}
END_TEST
/*
 * Test line 190: talloc_free for tool_call in error path
 *
 * This test exercises the path where:
 * 1. HTTP response is successful (200)
 * 2. write_ctx has a tool_call
 * 3. talloc_steal transfers ownership (line 149)
 * 4. Completion callback returns an error
 * 5. Error cleanup path frees tool_call (line 190)
 */
START_TEST(test_info_read_callback_error_with_tool_call_free)
{
    /* Create multi-handle */
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    /* Create conversation */
    ik_openai_conversation_t *conv = create_test_conversation("What's the weather?");
    ik_cfg_t *cfg = create_test_config();

    /* Add request with completion callback that returns error */
    res_t add_res = ik_openai_multi_add_request(multi, cfg, conv, NULL, NULL,
                                                error_with_tool_call_callback, ctx, false);
    ck_assert(!add_res.is_err);

    /* Set up mock SSE response with tool call */
    const char *sse_data =
        "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"call_456\",\"type\":\"function\",\"function\":{\"name\":\"get_weather\",\"arguments\":\"{\\\"location\\\":\\\"NYC\\\"}\"}}]}}]}\n\n";
    mock_response_data = sse_data;
    mock_response_len = strlen(sse_data);
    invoke_write_callback = true;

    /* Perform to process write callback and populate tool_call */
    int still_running = 0;
    res_t perform_res = ik_openai_multi_perform(multi, &still_running);
    ck_assert(!perform_res.is_err);

    /* Setup mock message for successful completion with HTTP 200 */
    CURLMsg msg;
    setup_mock_curl_msg(&msg, g_last_easy_handle, CURLE_OK, 200);

    /* Call info_read - should:
     * - Execute line 149: talloc_steal for tool_call
     * - Call completion callback which returns error
     * - Execute line 190: talloc_free for tool_call in error cleanup path
     * - Return the callback error
     */
    res_t info_res = ik_openai_multi_info_read(multi);
    ck_assert(info_res.is_err);
    ck_assert_int_eq(info_res.err->code, ERR_IO);

    /* Clean up */
    invoke_write_callback = false;
    talloc_free(multi);
}

END_TEST
/*
 * Test line 219: talloc_free for tool_call in success path
 *
 * This test exercises the path where:
 * 1. HTTP response is successful (200)
 * 2. write_ctx has a tool_call
 * 3. talloc_steal transfers ownership (line 149)
 * 4. Completion callback succeeds (no error)
 * 5. Success cleanup path frees tool_call (line 219)
 */
START_TEST(test_info_read_success_with_tool_call_free)
{
    captured_tool_call_id = NULL;
    captured_tool_call_name = NULL;

    /* Create multi-handle */
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    /* Create conversation */
    ik_openai_conversation_t *conv = create_test_conversation("What's the weather?");
    ik_cfg_t *cfg = create_test_config();

    /* Add request with completion callback that succeeds */
    res_t add_res = ik_openai_multi_add_request(multi, cfg, conv, NULL, NULL,
                                                capture_tool_call_callback, ctx, false);
    ck_assert(!add_res.is_err);

    /* Set up mock SSE response with tool call */
    const char *sse_data =
        "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"call_789\",\"type\":\"function\",\"function\":{\"name\":\"grep\",\"arguments\":\"{\\\"pattern\\\":\\\"TODO\\\"}\"}}]}}]}\n\n";
    mock_response_data = sse_data;
    mock_response_len = strlen(sse_data);
    invoke_write_callback = true;

    /* Perform to process write callback and populate tool_call */
    int still_running = 0;
    res_t perform_res = ik_openai_multi_perform(multi, &still_running);
    ck_assert(!perform_res.is_err);

    /* Setup mock message for successful completion with HTTP 200 */
    CURLMsg msg;
    setup_mock_curl_msg(&msg, g_last_easy_handle, CURLE_OK, 200);

    /* Call info_read - should:
     * - Execute line 149: talloc_steal for tool_call
     * - Call completion callback which succeeds
     * - Execute line 219: talloc_free for tool_call in success cleanup path
     * - Return OK
     */
    res_t info_res = ik_openai_multi_info_read(multi);
    ck_assert(!info_res.is_err);

    /* Verify tool_call fields were captured by callback */
    ck_assert(captured_tool_call_id != NULL);
    ck_assert(captured_tool_call_name != NULL);

    /* Clean up */
    invoke_write_callback = false;
    talloc_free(captured_tool_call_id);
    talloc_free(captured_tool_call_name);
    captured_tool_call_id = NULL;
    captured_tool_call_name = NULL;
    talloc_free(multi);
}

END_TEST

/*
 * Test suite
 */

static Suite *client_multi_tool_call_coverage_suite(void)
{
    Suite *s = suite_create("openai_client_multi_tool_call_coverage");

    TCase *tc_tool_call = tcase_create("tool_call_coverage");
    tcase_add_checked_fixture(tc_tool_call, setup, teardown);
    tcase_add_test(tc_tool_call, test_info_read_success_with_tool_call_steal);
    tcase_add_test(tc_tool_call, test_info_read_callback_error_with_tool_call_free);
    tcase_add_test(tc_tool_call, test_info_read_success_with_tool_call_free);
    suite_add_tcase(s, tc_tool_call);

    return s;
}

int main(void)
{
    Suite *s = client_multi_tool_call_coverage_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
