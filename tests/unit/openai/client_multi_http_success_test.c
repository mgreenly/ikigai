/* Unit tests for OpenAI multi-handle manager - HTTP success path coverage */

#include "client_multi_test_common.h"
#include "openai/client_multi_internal.h"

/*
 * Tests for HTTP success path with metadata transfer
 */

START_TEST(test_multi_info_read_http_success_with_model) {
    /* Create multi-handle */
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    /* Create conversation */
    res_t conv_res = ik_openai_conversation_create(ctx);
    ck_assert(!conv_res.is_err);
    ik_openai_conversation_t *conv = conv_res.ok;

    res_t msg_res = ik_openai_msg_create(ctx, "user", "Hello");
    ck_assert(!msg_res.is_err);
    ik_openai_conversation_add_msg(conv, msg_res.ok);

    /* Create config */
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    cfg->openai_api_key = talloc_strdup(cfg, "sk-test");
    cfg->openai_model = talloc_strdup(cfg, "gpt-4");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 1000;

    /* Add a request */
    res_t add_res = ik_openai_multi_add_request(multi, cfg, conv, NULL, NULL, NULL, NULL, false, NULL);
    ck_assert(!add_res.is_err);

    /* Manually populate write context with metadata to test the success path */
    active_request_t *active_req = multi->active_requests[0];
    active_req->write_ctx->model = talloc_strdup(active_req->write_ctx, "gpt-4");
    active_req->write_ctx->finish_reason = talloc_strdup(active_req->write_ctx, "stop");
    active_req->write_ctx->completion_tokens = 42;

    /* Create a mock CURLMsg with HTTP 200 success */
    CURLMsg msg;
    msg.msg = CURLMSG_DONE;
    msg.easy_handle = g_last_easy_handle;
    msg.data.result = CURLE_OK;
    mock_curl_msg = &msg;
    mock_http_response_code = 200;  /* HTTP 200 OK */

    ik_openai_multi_info_read(multi, NULL);

    /* Clean up */
    talloc_free(multi);
}

END_TEST START_TEST(test_multi_info_read_http_success_with_model_only)
{
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    res_t conv_res = ik_openai_conversation_create(ctx);
    ck_assert(!conv_res.is_err);
    ik_openai_conversation_t *conv = conv_res.ok;

    res_t msg_res = ik_openai_msg_create(ctx, "user", "Hello");
    ck_assert(!msg_res.is_err);
    ik_openai_conversation_add_msg(conv, msg_res.ok);

    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    cfg->openai_api_key = talloc_strdup(cfg, "sk-test");
    cfg->openai_model = talloc_strdup(cfg, "gpt-4");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 1000;

    res_t add_res = ik_openai_multi_add_request(multi, cfg, conv, NULL, NULL, NULL, NULL, false, NULL);
    ck_assert(!add_res.is_err);

    /* Populate write context with only model (no finish_reason) */
    active_request_t *active_req = multi->active_requests[0];
    active_req->write_ctx->model = talloc_strdup(active_req->write_ctx, "gpt-4");
    active_req->write_ctx->completion_tokens = 100;

    CURLMsg msg;
    msg.msg = CURLMSG_DONE;
    msg.easy_handle = g_last_easy_handle;
    msg.data.result = CURLE_OK;
    mock_curl_msg = &msg;
    mock_http_response_code = 200;

    ik_openai_multi_info_read(multi, NULL);

    talloc_free(multi);
}

END_TEST START_TEST(test_multi_info_read_http_success_with_finish_reason_only)
{
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    res_t conv_res = ik_openai_conversation_create(ctx);
    ck_assert(!conv_res.is_err);
    ik_openai_conversation_t *conv = conv_res.ok;

    res_t msg_res = ik_openai_msg_create(ctx, "user", "Hello");
    ck_assert(!msg_res.is_err);
    ik_openai_conversation_add_msg(conv, msg_res.ok);

    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    cfg->openai_api_key = talloc_strdup(cfg, "sk-test");
    cfg->openai_model = talloc_strdup(cfg, "gpt-4");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 1000;

    res_t add_res = ik_openai_multi_add_request(multi, cfg, conv, NULL, NULL, NULL, NULL, false, NULL);
    ck_assert(!add_res.is_err);

    /* Populate write context with only finish_reason (no model) */
    active_request_t *active_req = multi->active_requests[0];
    active_req->write_ctx->finish_reason = talloc_strdup(active_req->write_ctx, "length");
    active_req->write_ctx->completion_tokens = 200;

    CURLMsg msg;
    msg.msg = CURLMSG_DONE;
    msg.easy_handle = g_last_easy_handle;
    msg.data.result = CURLE_OK;
    mock_curl_msg = &msg;
    mock_http_response_code = 200;

    ik_openai_multi_info_read(multi, NULL);

    talloc_free(multi);
}

END_TEST START_TEST(test_multi_info_read_http_success_no_metadata)
{
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    res_t conv_res = ik_openai_conversation_create(ctx);
    ck_assert(!conv_res.is_err);
    ik_openai_conversation_t *conv = conv_res.ok;

    res_t msg_res = ik_openai_msg_create(ctx, "user", "Hello");
    ck_assert(!msg_res.is_err);
    ik_openai_conversation_add_msg(conv, msg_res.ok);

    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    cfg->openai_api_key = talloc_strdup(cfg, "sk-test");
    cfg->openai_model = talloc_strdup(cfg, "gpt-4");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 1000;

    res_t add_res = ik_openai_multi_add_request(multi, cfg, conv, NULL, NULL, NULL, NULL, false, NULL);
    ck_assert(!add_res.is_err);

    /* Write context has no metadata (all NULL/0) - default state */
    CURLMsg msg;
    msg.msg = CURLMSG_DONE;
    msg.easy_handle = g_last_easy_handle;
    msg.data.result = CURLE_OK;
    mock_curl_msg = &msg;
    mock_http_response_code = 200;

    ik_openai_multi_info_read(multi, NULL);

    talloc_free(multi);
}

END_TEST

static Suite *client_multi_http_success_suite(void)
{
    Suite *s = suite_create("openai_client_multi_http_success");

    TCase *tc = tcase_create("http_success_paths");
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_multi_info_read_http_success_with_model);
    tcase_add_test(tc, test_multi_info_read_http_success_with_model_only);
    tcase_add_test(tc, test_multi_info_read_http_success_with_finish_reason_only);
    tcase_add_test(tc, test_multi_info_read_http_success_no_metadata);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    Suite *s = client_multi_http_success_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
