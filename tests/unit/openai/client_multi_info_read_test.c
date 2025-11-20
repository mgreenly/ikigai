/* Unit tests for OpenAI multi-handle manager - info_read operations */

#include "client_multi_test_common.h"

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
    res_t add_res = ik_openai_multi_add_request(multi, cfg, conv, NULL, NULL, NULL, NULL, NULL);
    ck_assert(!add_res.is_err);

    /* Create a mock CURLMsg indicating completed transfer with actual handle */
    CURLMsg msg;
    msg.msg = CURLMSG_DONE;
    msg.easy_handle = g_last_easy_handle;  /* Use the actual easy handle */
    msg.data.result = CURLE_OK;
    mock_curl_msg = &msg;

    res_t info_res = ik_openai_multi_info_read(multi);
    ck_assert(!info_res.is_err);

    /* Clean up */
    talloc_free(multi);
}

END_TEST START_TEST(test_multi_info_read_non_done_message)
{
    /* Create multi-handle */
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    /* Create a mock CURLMsg with msg != CURLMSG_DONE */
    CURLMsg msg;
    msg.msg = CURLMSG_NONE;  /* Not DONE */
    msg.easy_handle = (CURL *)0x1;
    msg.data.result = CURLE_OK;
    mock_curl_msg = &msg;

    res_t info_res = ik_openai_multi_info_read(multi);
    ck_assert(!info_res.is_err);

    /* Clean up */
    talloc_free(multi);
}

END_TEST START_TEST(test_multi_info_read_multiple_requests)
{
    /* Create multi-handle */
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    /* Create two conversations and add two requests */
    res_t conv_res1 = ik_openai_conversation_create(ctx);
    ck_assert(!conv_res1.is_err);
    ik_openai_conversation_t *conv1 = conv_res1.ok;

    res_t msg_res1 = ik_openai_msg_create(ctx, "user", "Hello");
    ck_assert(!msg_res1.is_err);
    ik_openai_conversation_add_msg(conv1, msg_res1.ok);

    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    cfg->openai_api_key = talloc_strdup(cfg, "sk-test");
    cfg->openai_model = talloc_strdup(cfg, "gpt-4");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 1000;

    /* Add first request */
    res_t add_res1 = ik_openai_multi_add_request(multi, cfg, conv1, NULL, NULL, NULL, NULL, NULL);
    ck_assert(!add_res1.is_err);

    /* Add second request */
    res_t conv_res2 = ik_openai_conversation_create(ctx);
    ck_assert(!conv_res2.is_err);
    ik_openai_conversation_t *conv2 = conv_res2.ok;

    res_t msg_res2 = ik_openai_msg_create(ctx, "user", "World");
    ck_assert(!msg_res2.is_err);
    ik_openai_conversation_add_msg(conv2, msg_res2.ok);

    res_t add_res2 = ik_openai_multi_add_request(multi, cfg, conv2, NULL, NULL, NULL, NULL, NULL);
    ck_assert(!add_res2.is_err);
    CURL *second_handle = g_last_easy_handle;

    /* Complete the SECOND request - this will make the loop check first handle (FALSE),
     * then second handle (TRUE), and exercise the shift loop */
    CURLMsg msg;
    msg.msg = CURLMSG_DONE;
    msg.easy_handle = second_handle;  /* Complete the second request */
    msg.data.result = CURLE_OK;
    mock_curl_msg = &msg;

    res_t info_res = ik_openai_multi_info_read(multi);
    ck_assert(!info_res.is_err);

    /* Clean up */
    talloc_free(multi);
}

END_TEST START_TEST(test_multi_info_read_multiple_requests_shift)
{
    /* Create multi-handle */
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    cfg->openai_api_key = talloc_strdup(cfg, "sk-test");
    cfg->openai_model = talloc_strdup(cfg, "gpt-4");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 1000;

    CURL *first_handle = NULL;

    /* Add three requests */
    for (int i = 0; i < 3; i++) {
        res_t conv_res = ik_openai_conversation_create(ctx);
        ck_assert(!conv_res.is_err);
        ik_openai_conversation_t *conv = conv_res.ok;

        res_t msg_res = ik_openai_msg_create(ctx, "user", "Hello");
        ck_assert(!msg_res.is_err);
        ik_openai_conversation_add_msg(conv, msg_res.ok);

        res_t add_res = ik_openai_multi_add_request(multi, cfg, conv, NULL, NULL, NULL, NULL, NULL);
        ck_assert(!add_res.is_err);

        /* Capture the first handle */
        if (i == 0) {
            first_handle = g_last_easy_handle;
        }
    }

    /* Now complete the first request - this will exercise the shift loop
     * to move requests[1] and requests[2] down */
    CURLMsg msg;
    msg.msg = CURLMSG_DONE;
    msg.easy_handle = first_handle;
    msg.data.result = CURLE_OK;
    mock_curl_msg = &msg;

    res_t info_res = ik_openai_multi_info_read(multi);
    ck_assert(!info_res.is_err);

    /* Clean up */
    talloc_free(multi);
}

END_TEST START_TEST(test_multi_info_read_message_no_active_requests)
{
    /* Create multi-handle */
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    /* Create a mock CURLMsg with CURLMSG_DONE, but no active requests
     * This tests the edge case where we get a completion message
     * but active_count is 0 (loop at line 325 doesn't execute) */
    CURLMsg msg;
    msg.msg = CURLMSG_DONE;
    msg.easy_handle = (CURL *)0x12345;  /* Some handle that won't match */
    msg.data.result = CURLE_OK;
    mock_curl_msg = &msg;

    res_t info_res = ik_openai_multi_info_read(multi);
    ck_assert(!info_res.is_err);

    /* Clean up */
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
