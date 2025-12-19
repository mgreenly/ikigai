/* Unit tests for OpenAI multi-handle write callback functionality */

#include "client_multi_test_common.h"

/*
 * Write callback tests
 */

START_TEST(test_http_write_callback_with_sse_data) {
    /* Create multi-handle */
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    /* Create conversation */
    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

    ik_msg_t *msg_tmp = ik_openai_msg_create(ctx, "user", "Hello");
    ik_openai_conversation_add_msg(conv, msg_tmp);

    /* Create config */
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    cfg->openai_api_key = talloc_strdup(cfg, "sk-test");
    cfg->openai_model = talloc_strdup(cfg, "gpt-4");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 1000;

    /* Add request */
    res_t add_res = ik_openai_multi_add_request(multi, cfg, conv, NULL, NULL, NULL, NULL, false, NULL);
    ck_assert(!add_res.is_err);

    /* Set up mock SSE response data */
    const char *sse_data = "data: {\"choices\":[{\"delta\":{\"content\":\"Hello\"}}]}\n\n";
    mock_response_data = sse_data;
    mock_response_len = strlen(sse_data);
    invoke_write_callback = true;

    /* Perform - this will invoke the write callback */
    int still_running = 0;
    res_t perform_res = ik_openai_multi_perform(multi, &still_running);
    ck_assert(!perform_res.is_err);

    /* Clean up */
    invoke_write_callback = false;
    talloc_free(multi);
}

END_TEST START_TEST(test_http_write_callback_user_callback_error)
{
    /* Create multi-handle */
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    /* Create conversation */
    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

    ik_msg_t *msg_tmp = ik_openai_msg_create(ctx, "user", "Hello");
    ik_openai_conversation_add_msg(conv, msg_tmp);

    /* Create config */
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    cfg->openai_api_key = talloc_strdup(cfg, "sk-test");
    cfg->openai_model = talloc_strdup(cfg, "gpt-4");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 1000;

    /* Add request with error callback - pass ctx as user context */
    res_t add_res = ik_openai_multi_add_request(multi, cfg, conv, error_stream_callback, ctx, NULL, NULL, false, NULL);
    ck_assert(!add_res.is_err);

    /* Set up mock SSE response data */
    const char *sse_data = "data: {\"choices\":[{\"delta\":{\"content\":\"Hello\"}}]}\n\n";
    mock_response_data = sse_data;
    mock_response_len = strlen(sse_data);
    invoke_write_callback = true;

    /* Perform - callback will return error, write callback should return 0 */
    int still_running = 0;
    res_t perform_res = ik_openai_multi_perform(multi, &still_running);
    /* Performance should still succeed, the error is signaled via return value 0 from callback */
    ck_assert(!perform_res.is_err);

    /* Clean up */
    invoke_write_callback = false;
    talloc_free(multi);
}

END_TEST START_TEST(test_http_write_callback_user_callback_success)
{
    /* Create multi-handle */
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    /* Create conversation */
    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

    ik_msg_t *msg_tmp = ik_openai_msg_create(ctx, "user", "Hello");
    ik_openai_conversation_add_msg(conv, msg_tmp);

    /* Create config */
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    cfg->openai_api_key = talloc_strdup(cfg, "sk-test");
    cfg->openai_model = talloc_strdup(cfg, "gpt-4");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 1000;

    /* Add request with success callback */
    res_t add_res = ik_openai_multi_add_request(multi, cfg, conv, success_stream_callback, NULL, NULL, NULL,
                                                false, NULL);
    ck_assert(!add_res.is_err);

    /* Set up mock SSE response data */
    const char *sse_data = "data: {\"choices\":[{\"delta\":{\"content\":\"Hello\"}}]}\n\n";
    mock_response_data = sse_data;
    mock_response_len = strlen(sse_data);
    invoke_write_callback = true;

    /* Perform - callback will succeed */
    int still_running = 0;
    res_t perform_res = ik_openai_multi_perform(multi, &still_running);
    ck_assert(!perform_res.is_err);

    /* Clean up */
    invoke_write_callback = false;
    talloc_free(multi);
}

END_TEST START_TEST(test_http_write_callback_parse_error)
{
    /* Create multi-handle */
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    /* Create conversation */
    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

    ik_msg_t *msg_tmp = ik_openai_msg_create(ctx, "user", "Hello");
    ik_openai_conversation_add_msg(conv, msg_tmp);

    /* Create config */
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    cfg->openai_api_key = talloc_strdup(cfg, "sk-test");
    cfg->openai_model = talloc_strdup(cfg, "gpt-4");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 1000;

    /* Add request */
    res_t add_res = ik_openai_multi_add_request(multi, cfg, conv, NULL, NULL, NULL, NULL, false, NULL);
    ck_assert(!add_res.is_err);

    /* Set up mock SSE response with invalid format (missing "data: " prefix) */
    const char *sse_data = "invalid: no data prefix\n\n";
    mock_response_data = sse_data;
    mock_response_len = strlen(sse_data);
    invoke_write_callback = true;

    /* Perform - write callback should handle parse error gracefully */
    int still_running = 0;
    res_t perform_res = ik_openai_multi_perform(multi, &still_running);
    ck_assert(!perform_res.is_err);

    /* Clean up */
    invoke_write_callback = false;
    talloc_free(multi);
}

END_TEST START_TEST(test_http_write_callback_null_content)
{
    /* Create multi-handle */
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    /* Create conversation */
    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

    ik_msg_t *msg_tmp = ik_openai_msg_create(ctx, "user", "Hello");
    ik_openai_conversation_add_msg(conv, msg_tmp);

    /* Create config */
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    cfg->openai_api_key = talloc_strdup(cfg, "sk-test");
    cfg->openai_model = talloc_strdup(cfg, "gpt-4");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 1000;

    /* Add request */
    res_t add_res = ik_openai_multi_add_request(multi, cfg, conv, NULL, NULL, NULL, NULL, false, NULL);
    ck_assert(!add_res.is_err);

    /* Set up mock SSE response with [DONE] marker (returns NULL content) */
    const char *sse_data = "data: [DONE]\n\n";
    mock_response_data = sse_data;
    mock_response_len = strlen(sse_data);
    invoke_write_callback = true;

    /* Perform - write callback should handle NULL content gracefully */
    int still_running = 0;
    res_t perform_res = ik_openai_multi_perform(multi, &still_running);
    ck_assert(!perform_res.is_err);

    /* Clean up */
    invoke_write_callback = false;
    talloc_free(multi);
}

END_TEST START_TEST(test_http_write_callback_multiple_chunks)
{
    /* Create multi-handle */
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    /* Create conversation */
    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

    ik_msg_t *msg_tmp = ik_openai_msg_create(ctx, "user", "Hello");
    ik_openai_conversation_add_msg(conv, msg_tmp);

    /* Create config */
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    cfg->openai_api_key = talloc_strdup(cfg, "sk-test");
    cfg->openai_model = talloc_strdup(cfg, "gpt-4");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 1000;

    /* Add request */
    res_t add_res = ik_openai_multi_add_request(multi, cfg, conv, NULL, NULL, NULL, NULL, false, NULL);
    ck_assert(!add_res.is_err);

    /* Send first chunk */
    const char *sse_data1 = "data: {\"choices\":[{\"delta\":{\"content\":\"Hello\"}}]}\n\n";
    mock_response_data = sse_data1;
    mock_response_len = strlen(sse_data1);
    invoke_write_callback = true;

    int still_running = 0;
    res_t perform_res = ik_openai_multi_perform(multi, &still_running);
    ck_assert(!perform_res.is_err);

    /* Send second chunk - this will exercise talloc_strdup_append path */
    const char *sse_data2 = "data: {\"choices\":[{\"delta\":{\"content\":\" World\"}}]}\n\n";
    mock_response_data = sse_data2;
    mock_response_len = strlen(sse_data2);

    perform_res = ik_openai_multi_perform(multi, &still_running);
    ck_assert(!perform_res.is_err);

    /* Clean up */
    invoke_write_callback = false;
    talloc_free(multi);
}

END_TEST

/*
 * Test suite
 */

static Suite *client_multi_write_callback_suite(void)
{
    Suite *s = suite_create("openai_client_multi_write_callback");

    TCase *tc_callback = tcase_create("write_callback");
    tcase_add_checked_fixture(tc_callback, setup, teardown);
    tcase_add_test(tc_callback, test_http_write_callback_with_sse_data);
    tcase_add_test(tc_callback, test_http_write_callback_user_callback_error);
    tcase_add_test(tc_callback, test_http_write_callback_user_callback_success);
    tcase_add_test(tc_callback, test_http_write_callback_parse_error);
    tcase_add_test(tc_callback, test_http_write_callback_null_content);
    tcase_add_test(tc_callback, test_http_write_callback_multiple_chunks);
    suite_add_tcase(s, tc_callback);

    return s;
}

int main(void)
{
    Suite *s = client_multi_write_callback_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
