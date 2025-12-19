/* Unit tests for OpenAI multi-handle manager - callback error path coverage */

#include "client_multi_test_common.h"
#include "openai/client_multi_internal.h"

/* Completion callback that returns error */
static res_t error_completion_callback(const ik_http_completion_t *completion, void *callback_ctx)
{
    (void)completion;
    return ERR(callback_ctx, IO, "Completion callback error");
}

START_TEST(test_multi_info_read_callback_error_with_model) {
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

    ik_msg_t *msg_tmp = ik_openai_msg_create(ctx, "user", "Hello");
    ik_openai_conversation_add_msg(conv, msg_tmp);

    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    cfg->openai_api_key = talloc_strdup(cfg, "sk-test");
    cfg->openai_model = talloc_strdup(cfg, "gpt-4");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 1000;

    res_t add_res = ik_openai_multi_add_request(multi, cfg, conv, NULL, NULL,
                                                error_completion_callback, ctx, false, NULL);
    ck_assert(!add_res.is_err);

    active_request_t *active_req = multi->active_requests[0];
    active_req->write_ctx->model = talloc_strdup(active_req->write_ctx, "gpt-4");
    active_req->write_ctx->completion_tokens = 50;

    CURLMsg msg;
    msg.msg = CURLMSG_DONE;
    msg.easy_handle = g_last_easy_handle;
    msg.data.result = CURLE_OK;
    mock_curl_msg = &msg;
    mock_http_response_code = 200;

    ik_openai_multi_info_read(multi, NULL);

    talloc_free(multi);
}

END_TEST START_TEST(test_multi_info_read_callback_error_with_finish_reason)
{
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

    ik_msg_t *msg_tmp = ik_openai_msg_create(ctx, "user", "Hello");
    ik_openai_conversation_add_msg(conv, msg_tmp);

    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    cfg->openai_api_key = talloc_strdup(cfg, "sk-test");
    cfg->openai_model = talloc_strdup(cfg, "gpt-4");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 1000;

    res_t add_res = ik_openai_multi_add_request(multi, cfg, conv, NULL, NULL,
                                                error_completion_callback, ctx, false, NULL);
    ck_assert(!add_res.is_err);

    active_request_t *active_req = multi->active_requests[0];
    active_req->write_ctx->finish_reason = talloc_strdup(active_req->write_ctx, "stop");
    active_req->write_ctx->completion_tokens = 75;

    CURLMsg msg;
    msg.msg = CURLMSG_DONE;
    msg.easy_handle = g_last_easy_handle;
    msg.data.result = CURLE_OK;
    mock_curl_msg = &msg;
    mock_http_response_code = 200;

    ik_openai_multi_info_read(multi, NULL);

    talloc_free(multi);
}

END_TEST START_TEST(test_multi_info_read_callback_error_with_both_metadata)
{
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

    ik_msg_t *msg_tmp = ik_openai_msg_create(ctx, "user", "Hello");
    ik_openai_conversation_add_msg(conv, msg_tmp);

    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    cfg->openai_api_key = talloc_strdup(cfg, "sk-test");
    cfg->openai_model = talloc_strdup(cfg, "gpt-4");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 1000;

    res_t add_res = ik_openai_multi_add_request(multi, cfg, conv, NULL, NULL,
                                                error_completion_callback, ctx, false, NULL);
    ck_assert(!add_res.is_err);

    active_request_t *active_req = multi->active_requests[0];
    active_req->write_ctx->model = talloc_strdup(active_req->write_ctx, "gpt-4");
    active_req->write_ctx->finish_reason = talloc_strdup(active_req->write_ctx, "length");
    active_req->write_ctx->completion_tokens = 150;

    CURLMsg msg;
    msg.msg = CURLMSG_DONE;
    msg.easy_handle = g_last_easy_handle;
    msg.data.result = CURLE_OK;
    mock_curl_msg = &msg;
    mock_http_response_code = 200;

    ik_openai_multi_info_read(multi, NULL);

    talloc_free(multi);
}

END_TEST START_TEST(test_multi_info_read_callback_error_multiple_requests_shift)
{
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    cfg->openai_api_key = talloc_strdup(cfg, "sk-test");
    cfg->openai_model = talloc_strdup(cfg, "gpt-4");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 1000;

    CURL *first_handle = NULL;

    /* Add three requests - the first one will have the error callback */
    for (int i = 0; i < 3; i++) {
        ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

        ik_msg_t *msg_tmp = ik_openai_msg_create(ctx, "user", "Hello");
    ik_openai_conversation_add_msg(conv, msg_tmp);

        /* Only first request has error callback */
        if (i == 0) {
            res_t add_res = ik_openai_multi_add_request(multi, cfg, conv, NULL, NULL,
                                                        error_completion_callback, ctx, false, NULL);
            ck_assert(!add_res.is_err);
            first_handle = g_last_easy_handle;
        } else {
            res_t add_res = ik_openai_multi_add_request(multi, cfg, conv, NULL, NULL, NULL, NULL, false, NULL);
            ck_assert(!add_res.is_err);
        }
    }

    /* Populate write context for first request */
    active_request_t *active_req = multi->active_requests[0];
    active_req->write_ctx->model = talloc_strdup(active_req->write_ctx, "gpt-4");
    active_req->write_ctx->finish_reason = talloc_strdup(active_req->write_ctx, "stop");
    active_req->write_ctx->completion_tokens = 25;

    CURLMsg msg;
    msg.msg = CURLMSG_DONE;
    msg.easy_handle = first_handle;
    msg.data.result = CURLE_OK;
    mock_curl_msg = &msg;
    mock_http_response_code = 200;

    ik_openai_multi_info_read(multi, NULL);

    /* Verify that the array was properly shifted - should have 2 requests left */
    ck_assert_uint_eq(multi->active_count, 2);

    talloc_free(multi);
}

END_TEST

static Suite *client_multi_callback_error_suite(void)
{
    Suite *s = suite_create("openai_client_multi_callback_error");

    TCase *tc = tcase_create("callback_error_paths");
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_multi_info_read_callback_error_with_model);
    tcase_add_test(tc, test_multi_info_read_callback_error_with_finish_reason);
    tcase_add_test(tc, test_multi_info_read_callback_error_with_both_metadata);
    tcase_add_test(tc, test_multi_info_read_callback_error_multiple_requests_shift);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    Suite *s = client_multi_callback_error_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
