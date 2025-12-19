/* Unit tests for OpenAI multi-handle manager - callback error path with HTTP
 * errors */

#include "client_multi_test_common.h"
#include "openai/client_multi_internal.h"

/* Completion callback that returns error */
static res_t error_completion_callback(const ik_http_completion_t *completion,
                                       void *callback_ctx)
{
    (void)completion;
    return ERR(callback_ctx, IO, "Completion callback error");
}

START_TEST(test_multi_info_read_callback_error_with_client_error) {
    /* Test callback error path when error_message is NOT NULL (HTTP 4xx) */
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

    res_t msg_res = ik_openai_msg_create(ctx, "user", "Hello");
    ck_assert(!msg_res.is_err);
    ik_openai_conversation_add_msg(conv, msg_res.ok);

    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    cfg->openai_api_key = talloc_strdup(cfg, "sk-test");
    cfg->openai_model = talloc_strdup(cfg, "gpt-4");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 1000;

    res_t add_res = ik_openai_multi_add_request(
        multi, cfg, conv, NULL, NULL, error_completion_callback, ctx, false, NULL);
    ck_assert(!add_res.is_err);

    /* HTTP 404 will create error_message */
    CURLMsg msg;
    msg.msg = CURLMSG_DONE;
    msg.easy_handle = g_last_easy_handle;
    msg.data.result = CURLE_OK;
    mock_curl_msg = &msg;
    mock_http_response_code = 404;

    /* info_read should return the callback error and free error_message (line
     * 175) */
    ik_openai_multi_info_read(multi, NULL);

    talloc_free(multi);
}

END_TEST START_TEST(test_multi_info_read_callback_error_with_server_error)
{
    /* Test callback error path when error_message is NOT NULL (HTTP 5xx) */
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

    res_t msg_res = ik_openai_msg_create(ctx, "user", "Hello");
    ck_assert(!msg_res.is_err);
    ik_openai_conversation_add_msg(conv, msg_res.ok);

    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    cfg->openai_api_key = talloc_strdup(cfg, "sk-test");
    cfg->openai_model = talloc_strdup(cfg, "gpt-4");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 1000;

    res_t add_res = ik_openai_multi_add_request(
        multi, cfg, conv, NULL, NULL, error_completion_callback, ctx, false, NULL);
    ck_assert(!add_res.is_err);

    /* HTTP 500 will create error_message */
    CURLMsg msg;
    msg.msg = CURLMSG_DONE;
    msg.easy_handle = g_last_easy_handle;
    msg.data.result = CURLE_OK;
    mock_curl_msg = &msg;
    mock_http_response_code = 500;

    /* info_read should return the callback error and free error_message (line
     * 175) */
    ik_openai_multi_info_read(multi, NULL);

    talloc_free(multi);
}

END_TEST START_TEST(test_multi_info_read_callback_error_with_network_error)
{
    /* Test callback error path when error_message is NOT NULL (network error) */
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

    res_t msg_res = ik_openai_msg_create(ctx, "user", "Hello");
    ck_assert(!msg_res.is_err);
    ik_openai_conversation_add_msg(conv, msg_res.ok);

    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    cfg->openai_api_key = talloc_strdup(cfg, "sk-test");
    cfg->openai_model = talloc_strdup(cfg, "gpt-4");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 1000;

    res_t add_res = ik_openai_multi_add_request(
        multi, cfg, conv, NULL, NULL, error_completion_callback, ctx, false, NULL);
    ck_assert(!add_res.is_err);

    /* Network error will create error_message */
    CURLMsg msg;
    msg.msg = CURLMSG_DONE;
    msg.easy_handle = g_last_easy_handle;
    msg.data.result = CURLE_COULDNT_CONNECT;
    mock_curl_msg = &msg;

    /* info_read should return the callback error and free error_message (line
     * 175) */
    ik_openai_multi_info_read(multi, NULL);

    talloc_free(multi);
}

END_TEST START_TEST(test_multi_info_read_http_599_edge_case)
{
    /* Test HTTP 599 (edge of 5xx range) */
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

    res_t msg_res = ik_openai_msg_create(ctx, "user", "Hello");
    ck_assert(!msg_res.is_err);
    ik_openai_conversation_add_msg(conv, msg_res.ok);

    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    cfg->openai_api_key = talloc_strdup(cfg, "sk-test");
    cfg->openai_model = talloc_strdup(cfg, "gpt-4");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 1000;

    res_t add_res = ik_openai_multi_add_request(multi, cfg, conv, NULL, NULL,
                                                NULL, NULL, false, NULL);
    ck_assert(!add_res.is_err);

    /* HTTP 599 - edge of server error range */
    CURLMsg msg;
    msg.msg = CURLMSG_DONE;
    msg.easy_handle = g_last_easy_handle;
    msg.data.result = CURLE_OK;
    mock_curl_msg = &msg;
    mock_http_response_code = 599;

    ik_openai_multi_info_read(multi, NULL);

    talloc_free(multi);
}

END_TEST START_TEST(test_multi_info_read_http_600_unexpected)
{
    /* Test HTTP 600 (beyond server error range) to hit the else branch */
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

    res_t msg_res = ik_openai_msg_create(ctx, "user", "Hello");
    ck_assert(!msg_res.is_err);
    ik_openai_conversation_add_msg(conv, msg_res.ok);

    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    cfg->openai_api_key = talloc_strdup(cfg, "sk-test");
    cfg->openai_model = talloc_strdup(cfg, "gpt-4");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 1000;

    res_t add_res = ik_openai_multi_add_request(multi, cfg, conv, NULL, NULL,
                                                NULL, NULL, false, NULL);
    ck_assert(!add_res.is_err);

    /* HTTP 600 - beyond server error range, should hit "unexpected" branch */
    CURLMsg msg;
    msg.msg = CURLMSG_DONE;
    msg.easy_handle = g_last_easy_handle;
    msg.data.result = CURLE_OK;
    mock_curl_msg = &msg;
    mock_http_response_code = 600;

    ik_openai_multi_info_read(multi, NULL);

    talloc_free(multi);
}

END_TEST

/* Completion callback that succeeds */
static res_t success_completion_callback(const ik_http_completion_t *completion,
                                         void *callback_ctx)
{
    (void)completion;
    (void)callback_ctx;
    return OK(NULL);
}

START_TEST(test_multi_info_read_callback_success_with_error_message) {
    /* Test callback success path when error_message is present (HTTP 4xx)
     * This ensures the false branch of is_err(&cb_result) at line 172 is covered
     */
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

    res_t msg_res = ik_openai_msg_create(ctx, "user", "Hello");
    ck_assert(!msg_res.is_err);
    ik_openai_conversation_add_msg(conv, msg_res.ok);

    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    cfg->openai_api_key = talloc_strdup(cfg, "sk-test");
    cfg->openai_model = talloc_strdup(cfg, "gpt-4");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 1000;

    /* Use success callback instead of error callback */
    res_t add_res = ik_openai_multi_add_request(
        multi, cfg, conv, NULL, NULL, success_completion_callback, ctx, false, NULL);
    ck_assert(!add_res.is_err);

    /* HTTP 404 will create error_message, but callback succeeds */
    CURLMsg msg;
    msg.msg = CURLMSG_DONE;
    msg.easy_handle = g_last_easy_handle;
    msg.data.result = CURLE_OK;
    mock_curl_msg = &msg;
    mock_http_response_code = 404;

    /* info_read should succeed since callback returns OK */
    ik_openai_multi_info_read(multi, NULL);

    talloc_free(multi);
}

END_TEST

static Suite *client_multi_callback_error_http_suite(void)
{
    Suite *s = suite_create("openai_client_multi_callback_error_http");

    TCase *tc = tcase_create("callback_error_with_http_errors");
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_multi_info_read_callback_error_with_client_error);
    tcase_add_test(tc, test_multi_info_read_callback_error_with_server_error);
    tcase_add_test(tc, test_multi_info_read_callback_error_with_network_error);
    tcase_add_test(tc, test_multi_info_read_http_599_edge_case);
    tcase_add_test(tc, test_multi_info_read_http_600_unexpected);
    tcase_add_test(tc, test_multi_info_read_callback_success_with_error_message);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    Suite *s = client_multi_callback_error_http_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
