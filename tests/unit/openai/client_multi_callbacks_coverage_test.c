/* Unit tests for OpenAI multi-handle callback - coverage completion */

#include "client_multi_test_common.h"

/*
 * Test tool call handling in http_write_callback
 */

START_TEST(test_http_write_callback_tool_call_first_chunk) {
    /* Create multi-handle */
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    /* Create conversation */
    res_t conv_res = ik_openai_conversation_create(ctx);
    ck_assert(!conv_res.is_err);
    ik_openai_conversation_t *conv = conv_res.ok;

    res_t msg_res = ik_openai_msg_create(ctx, "user", "What's the weather?");
    ck_assert(!msg_res.is_err);
    ik_openai_conversation_add_msg(conv, msg_res.ok);

    /* Create config */
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    cfg->openai_api_key = talloc_strdup(cfg, "sk-test");
    cfg->openai_model = talloc_strdup(cfg, "gpt-4");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 1000;

    /* Add request */
    res_t add_res = ik_openai_multi_add_request(multi, cfg, conv, NULL, NULL, NULL, NULL, false, NULL);
    ck_assert(!add_res.is_err);

    /* Set up mock SSE response with tool call */
    const char *sse_data =
        "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"call_123\",\"type\":\"function\",\"function\":{\"name\":\"get_weather\",\"arguments\":\"{\\\"location\\\":\\\"Boston\\\"}\"}}]}}]}\n\n";
    mock_response_data = sse_data;
    mock_response_len = strlen(sse_data);
    invoke_write_callback = true;

    /* Perform - should handle tool call */
    int still_running = 0;
    res_t perform_res = ik_openai_multi_perform(multi, &still_running);
    ck_assert(!perform_res.is_err);

    /* Clean up */
    invoke_write_callback = false;
    talloc_free(multi);
}
END_TEST START_TEST(test_http_write_callback_tool_call_streaming_chunks)
{
    /* Create multi-handle */
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    /* Create conversation */
    res_t conv_res = ik_openai_conversation_create(ctx);
    ck_assert(!conv_res.is_err);
    ik_openai_conversation_t *conv = conv_res.ok;

    res_t msg_res = ik_openai_msg_create(ctx, "user", "What's the weather?");
    ck_assert(!msg_res.is_err);
    ik_openai_conversation_add_msg(conv, msg_res.ok);

    /* Create config */
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    cfg->openai_api_key = talloc_strdup(cfg, "sk-test");
    cfg->openai_model = talloc_strdup(cfg, "gpt-4");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 1000;

    /* Add request */
    res_t add_res = ik_openai_multi_add_request(multi, cfg, conv, NULL, NULL, NULL, NULL, false, NULL);
    ck_assert(!add_res.is_err);

    /* Send TWO SSE events with complete tool calls in ONE write callback invocation
     * This simulates receiving multiple tool call chunks in a single network packet.
     * The SSE parser will extract both events, and the second one should trigger
     * the argument accumulation path (lines 269-276).
     */
    const char *sse_data =
        "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"call_123\",\"type\":\"function\",\"function\":{\"name\":\"get_weather\",\"arguments\":\"{\\\"location\\\":\"}}]}}]}\n\n"
        "data: {\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"call_123\",\"type\":\"function\",\"function\":{\"name\":\"get_weather\",\"arguments\":\"\\\"Boston\\\"}\"}}]}}]}\n\n";

    mock_response_data = sse_data;
    mock_response_len = strlen(sse_data);
    invoke_write_callback = true;

    int still_running = 0;
    res_t perform_res = ik_openai_multi_perform(multi, &still_running);
    ck_assert(!perform_res.is_err);

    /* Clean up */
    invoke_write_callback = false;
    talloc_free(multi);
}

END_TEST START_TEST(test_http_write_callback_parse_tool_calls_returns_error)
{
    /* Create multi-handle */
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    /* Create conversation */
    res_t conv_res = ik_openai_conversation_create(ctx);
    ck_assert(!conv_res.is_err);
    ik_openai_conversation_t *conv = conv_res.ok;

    res_t msg_res = ik_openai_msg_create(ctx, "user", "test");
    ck_assert(!msg_res.is_err);
    ik_openai_conversation_add_msg(conv, msg_res.ok);

    /* Create config */
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    cfg->openai_api_key = talloc_strdup(cfg, "sk-test");
    cfg->openai_model = talloc_strdup(cfg, "gpt-4");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 1000;

    /* Add request */
    res_t add_res = ik_openai_multi_add_request(multi, cfg, conv, NULL, NULL, NULL, NULL, false, NULL);
    ck_assert(!add_res.is_err);

    /* Send SSE event with invalid JSON after "data: "
     * This causes ik_openai_parse_sse_event to return an error (invalid JSON)
     * But we need NO content and parse_tool_calls to return ERROR
     * First send an event with no content to enter the else branch,
     * then the invalid JSON will cause parse_tool_calls to return error
     * This covers the false branch of "is_ok(&tool_res)" at line 264
     */
    const char *sse_data = "data: {invalid json}\n\n";

    mock_response_data = sse_data;
    mock_response_len = strlen(sse_data);
    invoke_write_callback = true;

    int still_running = 0;
    res_t perform_res = ik_openai_multi_perform(multi, &still_running);
    ck_assert(!perform_res.is_err);

    /* Clean up */
    invoke_write_callback = false;
    talloc_free(multi);
}

END_TEST

/*
 * Test suite
 */

static Suite *client_multi_callbacks_coverage_suite(void)
{
    Suite *s = suite_create("openai_client_multi_callbacks_coverage");

    TCase *tc_tool_calls = tcase_create("tool_call_handling");
    tcase_add_checked_fixture(tc_tool_calls, setup, teardown);
    tcase_add_test(tc_tool_calls, test_http_write_callback_tool_call_first_chunk);
    tcase_add_test(tc_tool_calls, test_http_write_callback_tool_call_streaming_chunks);
    tcase_add_test(tc_tool_calls, test_http_write_callback_parse_tool_calls_returns_error);
    suite_add_tcase(s, tc_tool_calls);

    return s;
}

int main(void)
{
    Suite *s = client_multi_callbacks_coverage_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
