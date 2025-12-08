/* Unit tests for OpenAI multi-handle callback metadata extraction edge cases */

#include "client_multi_test_common.h"

/*
 * Test metadata extraction with missing/invalid fields
 */

START_TEST(test_http_write_callback_done_marker_model) {
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

    /* Add request */
    res_t add_res = ik_openai_multi_add_request(multi, cfg, conv, NULL, NULL, NULL, NULL, false);
    ck_assert(!add_res.is_err);

    /* Set up mock SSE response with [DONE] marker */
    const char *sse_data = "data: [DONE]\n\n";
    mock_response_data = sse_data;
    mock_response_len = strlen(sse_data);
    invoke_write_callback = true;

    /* Perform - should handle [DONE] gracefully (extract_model returns NULL) */
    int still_running = 0;
    res_t perform_res = ik_openai_multi_perform(multi, &still_running);
    ck_assert(!perform_res.is_err);

    /* Clean up */
    invoke_write_callback = false;
    talloc_free(multi);
}
END_TEST START_TEST(test_http_write_callback_missing_model)
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

    /* Add request */
    res_t add_res = ik_openai_multi_add_request(multi, cfg, conv, NULL, NULL, NULL, NULL, false);
    ck_assert(!add_res.is_err);

    /* Set up mock SSE response without model field */
    const char *sse_data = "data: {\"choices\":[{\"delta\":{\"content\":\"Hello\"}}]}\n\n";
    mock_response_data = sse_data;
    mock_response_len = strlen(sse_data);
    invoke_write_callback = true;

    /* Perform - should handle missing model gracefully */
    int still_running = 0;
    res_t perform_res = ik_openai_multi_perform(multi, &still_running);
    ck_assert(!perform_res.is_err);

    /* Clean up */
    invoke_write_callback = false;
    talloc_free(multi);
}

END_TEST START_TEST(test_http_write_callback_model_not_string)
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

    /* Add request */
    res_t add_res = ik_openai_multi_add_request(multi, cfg, conv, NULL, NULL, NULL, NULL, false);
    ck_assert(!add_res.is_err);

    /* Set up mock SSE response with model as integer (not string) */
    const char *sse_data = "data: {\"model\":123,\"choices\":[{\"delta\":{\"content\":\"Hello\"}}]}\n\n";
    mock_response_data = sse_data;
    mock_response_len = strlen(sse_data);
    invoke_write_callback = true;

    /* Perform - should handle non-string model gracefully */
    int still_running = 0;
    res_t perform_res = ik_openai_multi_perform(multi, &still_running);
    ck_assert(!perform_res.is_err);

    /* Clean up */
    invoke_write_callback = false;
    talloc_free(multi);
}

END_TEST START_TEST(test_http_write_callback_missing_completion_tokens)
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

    /* Add request */
    res_t add_res = ik_openai_multi_add_request(multi, cfg, conv, NULL, NULL, NULL, NULL, false);
    ck_assert(!add_res.is_err);

    /* Set up mock SSE response with usage but no completion_tokens */
    const char *sse_data =
        "data: {\"usage\":{\"prompt_tokens\":10},\"choices\":[{\"delta\":{\"content\":\"Hello\"}}]}\n\n";
    mock_response_data = sse_data;
    mock_response_len = strlen(sse_data);
    invoke_write_callback = true;

    /* Perform - should handle missing completion_tokens gracefully */
    int still_running = 0;
    res_t perform_res = ik_openai_multi_perform(multi, &still_running);
    ck_assert(!perform_res.is_err);

    /* Clean up */
    invoke_write_callback = false;
    talloc_free(multi);
}

END_TEST START_TEST(test_http_write_callback_completion_tokens_valid_int)
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

    /* Add request */
    res_t add_res = ik_openai_multi_add_request(multi, cfg, conv, NULL, NULL, NULL, NULL, false);
    ck_assert(!add_res.is_err);

    /* Set up mock SSE response with valid integer completion_tokens */
    const char *sse_data =
        "data: {\"usage\":{\"completion_tokens\":42},\"choices\":[{\"delta\":{\"content\":\"Hello\"}}]}\n\n";
    mock_response_data = sse_data;
    mock_response_len = strlen(sse_data);
    invoke_write_callback = true;

    /* Perform - should extract valid integer */
    int still_running = 0;
    res_t perform_res = ik_openai_multi_perform(multi, &still_running);
    ck_assert(!perform_res.is_err);

    /* Clean up */
    invoke_write_callback = false;
    talloc_free(multi);
}

END_TEST START_TEST(test_http_write_callback_completion_tokens_not_int)
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

    /* Add request */
    res_t add_res = ik_openai_multi_add_request(multi, cfg, conv, NULL, NULL, NULL, NULL, false);
    ck_assert(!add_res.is_err);

    /* Set up mock SSE response with completion_tokens as string (not int) */
    const char *sse_data =
        "data: {\"usage\":{\"completion_tokens\":\"not_an_int\"},\"choices\":[{\"delta\":{\"content\":\"Hello\"}}]}\n\n";
    mock_response_data = sse_data;
    mock_response_len = strlen(sse_data);
    invoke_write_callback = true;

    /* Perform - should handle non-int completion_tokens gracefully */
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

static Suite *client_multi_callbacks_metadata_suite(void)
{
    Suite *s = suite_create("openai_client_multi_callbacks_metadata");

    TCase *tc_metadata = tcase_create("metadata_extraction");
    tcase_add_checked_fixture(tc_metadata, setup, teardown);
    tcase_add_test(tc_metadata, test_http_write_callback_done_marker_model);
    tcase_add_test(tc_metadata, test_http_write_callback_missing_model);
    tcase_add_test(tc_metadata, test_http_write_callback_model_not_string);
    tcase_add_test(tc_metadata, test_http_write_callback_missing_completion_tokens);
    tcase_add_test(tc_metadata, test_http_write_callback_completion_tokens_valid_int);
    tcase_add_test(tc_metadata, test_http_write_callback_completion_tokens_not_int);
    suite_add_tcase(s, tc_metadata);

    return s;
}

int main(void)
{
    Suite *s = client_multi_callbacks_metadata_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
