/* Unit tests for OpenAI multi-handle manager - info_read operations */

#include "client_multi_test_common.h"
#include "client_multi_info_read_helpers.h"

/*
 * Info read tests
 */

START_TEST(test_multi_info_read_no_messages) {
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    res_t info_res = ik_openai_multi_info_read(multi);
    ck_assert(!info_res.is_err);
}

END_TEST START_TEST(test_multi_info_read_with_completed_message)
{
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    ik_openai_conversation_t *conv = create_test_conversation("Hello");
    ik_cfg_t *cfg = create_test_config();
    res_t add_res = add_test_request(multi, cfg, conv);
    ck_assert(!add_res.is_err);

    CURLMsg msg;
    setup_mock_curl_msg(&msg, g_last_easy_handle, CURLE_OK, 0);

    res_t info_res = ik_openai_multi_info_read(multi);
    ck_assert(!info_res.is_err);

    talloc_free(multi);
}

END_TEST START_TEST(test_multi_info_read_non_done_message)
{
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    CURLMsg msg;
    msg.msg = CURLMSG_NONE;
    msg.easy_handle = (CURL *)0x1;
    msg.data.result = CURLE_OK;
    mock_curl_msg = &msg;

    res_t info_res = ik_openai_multi_info_read(multi);
    ck_assert(!info_res.is_err);

    talloc_free(multi);
}

END_TEST START_TEST(test_multi_info_read_multiple_requests)
{
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    ik_cfg_t *cfg = create_test_config();
    ik_openai_conversation_t *conv1 = create_test_conversation("Hello");
    res_t add_res1 = add_test_request(multi, cfg, conv1);
    ck_assert(!add_res1.is_err);

    ik_openai_conversation_t *conv2 = create_test_conversation("World");
    res_t add_res2 = add_test_request(multi, cfg, conv2);
    ck_assert(!add_res2.is_err);
    CURL *second_handle = g_last_easy_handle;

    /* Complete the SECOND request - exercises shift loop */
    CURLMsg msg;
    setup_mock_curl_msg(&msg, second_handle, CURLE_OK, 0);

    res_t info_res = ik_openai_multi_info_read(multi);
    ck_assert(!info_res.is_err);

    talloc_free(multi);
}

END_TEST START_TEST(test_multi_info_read_multiple_requests_shift)
{
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    ik_cfg_t *cfg = create_test_config();
    CURL *first_handle = NULL;

    for (int i = 0; i < 3; i++) {
        ik_openai_conversation_t *conv = create_test_conversation("Hello");
        res_t add_res = add_test_request(multi, cfg, conv);
        ck_assert(!add_res.is_err);
        if (i == 0) first_handle = g_last_easy_handle;
    }

    /* Complete first request - exercises shift loop */
    CURLMsg msg;
    setup_mock_curl_msg(&msg, first_handle, CURLE_OK, 0);

    res_t info_res = ik_openai_multi_info_read(multi);
    ck_assert(!info_res.is_err);

    talloc_free(multi);
}

END_TEST START_TEST(test_multi_info_read_message_no_active_requests)
{
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    /* Mock message with no active requests - edge case */
    CURLMsg msg;
    setup_mock_curl_msg(&msg, (CURL *)0x12345, CURLE_OK, 0);

    res_t info_res = ik_openai_multi_info_read(multi);
    ck_assert(!info_res.is_err);

    talloc_free(multi);
}

END_TEST START_TEST(test_multi_info_read_network_error)
{
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    ik_openai_conversation_t *conv = create_test_conversation("Hello");
    ik_cfg_t *cfg = create_test_config();
    res_t add_res = add_test_request(multi, cfg, conv);
    ck_assert(!add_res.is_err);

    CURLMsg msg;
    setup_mock_curl_msg(&msg, g_last_easy_handle, CURLE_COULDNT_CONNECT, 0);

    res_t info_res = ik_openai_multi_info_read(multi);
    ck_assert(!info_res.is_err);

    talloc_free(multi);
}

END_TEST START_TEST(test_multi_info_read_http_success_with_metadata)
{
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    ik_openai_conversation_t *conv = create_test_conversation("Hello");
    ik_cfg_t *cfg = create_test_config();
    res_t add_res = add_test_request(multi, cfg, conv);
    ck_assert(!add_res.is_err);

    CURLMsg msg;
    setup_mock_curl_msg(&msg, g_last_easy_handle, CURLE_OK, 200);

    res_t info_res = ik_openai_multi_info_read(multi);
    ck_assert(!info_res.is_err);

    talloc_free(multi);
}

END_TEST START_TEST(test_multi_info_read_http_client_error)
{
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    ik_openai_conversation_t *conv = create_test_conversation("Hello");
    ik_cfg_t *cfg = create_test_config();
    res_t add_res = add_test_request(multi, cfg, conv);
    ck_assert(!add_res.is_err);

    CURLMsg msg;
    setup_mock_curl_msg(&msg, g_last_easy_handle, CURLE_OK, 429);

    res_t info_res = ik_openai_multi_info_read(multi);
    ck_assert(!info_res.is_err);

    talloc_free(multi);
}

END_TEST START_TEST(test_multi_info_read_http_server_error)
{
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    ik_openai_conversation_t *conv = create_test_conversation("Hello");
    ik_cfg_t *cfg = create_test_config();
    res_t add_res = add_test_request(multi, cfg, conv);
    ck_assert(!add_res.is_err);

    CURLMsg msg;
    setup_mock_curl_msg(&msg, g_last_easy_handle, CURLE_OK, 503);

    res_t info_res = ik_openai_multi_info_read(multi);
    ck_assert(!info_res.is_err);

    talloc_free(multi);
}

END_TEST START_TEST(test_multi_info_read_http_unexpected_code)
{
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    ik_openai_conversation_t *conv = create_test_conversation("Hello");
    ik_cfg_t *cfg = create_test_config();
    res_t add_res = add_test_request(multi, cfg, conv);
    ck_assert(!add_res.is_err);

    CURLMsg msg;
    setup_mock_curl_msg(&msg, g_last_easy_handle, CURLE_OK, 100);

    res_t info_res = ik_openai_multi_info_read(multi);
    ck_assert(!info_res.is_err);

    talloc_free(multi);
}

END_TEST START_TEST(test_multi_info_read_completion_callback_error)
{
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    ik_openai_conversation_t *conv = create_test_conversation("Hello");
    ik_cfg_t *cfg = create_test_config();
    res_t add_res = ik_openai_multi_add_request(multi, cfg, conv, NULL, NULL,
                                                error_completion_callback, ctx, NULL);
    ck_assert(!add_res.is_err);

    CURLMsg msg;
    setup_mock_curl_msg(&msg, g_last_easy_handle, CURLE_OK, 200);

    res_t info_res = ik_openai_multi_info_read(multi);
    ck_assert(info_res.is_err);
    ck_assert_int_eq(info_res.err->code, ERR_IO);

    talloc_free(multi);
}

END_TEST

/*
 * Test suite
 */

static Suite *client_multi_info_read_suite(void)
{
    Suite *s = suite_create("openai_client_multi_info_read");

    TCase *tc_info = tcase_create("info_read");
    tcase_add_checked_fixture(tc_info, setup, teardown);
    tcase_add_test(tc_info, test_multi_info_read_no_messages);
    tcase_add_test(tc_info, test_multi_info_read_with_completed_message);
    tcase_add_test(tc_info, test_multi_info_read_non_done_message);
    tcase_add_test(tc_info, test_multi_info_read_multiple_requests);
    tcase_add_test(tc_info, test_multi_info_read_multiple_requests_shift);
    tcase_add_test(tc_info, test_multi_info_read_message_no_active_requests);
    tcase_add_test(tc_info, test_multi_info_read_network_error);
    tcase_add_test(tc_info, test_multi_info_read_http_success_with_metadata);
    tcase_add_test(tc_info, test_multi_info_read_http_client_error);
    tcase_add_test(tc_info, test_multi_info_read_http_server_error);
    tcase_add_test(tc_info, test_multi_info_read_http_unexpected_code);
    tcase_add_test(tc_info, test_multi_info_read_completion_callback_error);
    suite_add_tcase(s, tc_info);

    return s;
}

int main(void)
{
    Suite *s = client_multi_info_read_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
