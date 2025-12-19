/* Unit tests for OpenAI multi-handle callback metadata extraction - choices edge cases */

#include "client_multi_test_common.h"

/*
 * Test choices array edge cases
 */

START_TEST(test_http_write_callback_missing_choices) {
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

    /* Set up mock SSE response without choices array */
    const char *sse_data = "data: {\"model\":\"gpt-4\"}\n\n";
    mock_response_data = sse_data;
    mock_response_len = strlen(sse_data);
    invoke_write_callback = true;

    /* Perform - should handle missing choices gracefully */
    int still_running = 0;
    res_t perform_res = ik_openai_multi_perform(multi, &still_running);
    ck_assert(!perform_res.is_err);

    /* Clean up */
    invoke_write_callback = false;
    talloc_free(multi);
}
END_TEST START_TEST(test_http_write_callback_empty_choices)
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

    /* Set up mock SSE response with empty choices array */
    const char *sse_data = "data: {\"choices\":[],\"model\":\"gpt-4\"}\n\n";
    mock_response_data = sse_data;
    mock_response_len = strlen(sse_data);
    invoke_write_callback = true;

    /* Perform - should handle empty choices gracefully */
    int still_running = 0;
    res_t perform_res = ik_openai_multi_perform(multi, &still_running);
    ck_assert(!perform_res.is_err);

    /* Clean up */
    invoke_write_callback = false;
    talloc_free(multi);
}

END_TEST START_TEST(test_http_write_callback_choice_not_object)
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

    /* Set up mock SSE response with choice as string (not object) */
    const char *sse_data = "data: {\"choices\":[\"not_an_object\"],\"model\":\"gpt-4\"}\n\n";
    mock_response_data = sse_data;
    mock_response_len = strlen(sse_data);
    invoke_write_callback = true;

    /* Perform - should handle non-object choice gracefully */
    int still_running = 0;
    res_t perform_res = ik_openai_multi_perform(multi, &still_running);
    ck_assert(!perform_res.is_err);

    /* Clean up */
    invoke_write_callback = false;
    talloc_free(multi);
}

END_TEST START_TEST(test_http_write_callback_finish_reason_not_string)
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

    /* Set up mock SSE response with finish_reason as int (not string) */
    const char *sse_data = "data: {\"choices\":[{\"finish_reason\":42,\"delta\":{\"content\":\"Hello\"}}]}\n\n";
    mock_response_data = sse_data;
    mock_response_len = strlen(sse_data);
    invoke_write_callback = true;

    /* Perform - should handle non-string finish_reason gracefully */
    int still_running = 0;
    res_t perform_res = ik_openai_multi_perform(multi, &still_running);
    ck_assert(!perform_res.is_err);

    /* Clean up */
    invoke_write_callback = false;
    talloc_free(multi);
}

END_TEST START_TEST(test_http_write_callback_metadata_already_captured)
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

    /* First chunk with model, finish_reason, and completion_tokens */
    const char *sse_data1 =
        "data: {\"model\":\"gpt-4\",\"usage\":{\"completion_tokens\":5},\"choices\":[{\"finish_reason\":\"stop\",\"delta\":{\"content\":\"Hello\"}}]}\n\n";
    mock_response_data = sse_data1;
    mock_response_len = strlen(sse_data1);
    invoke_write_callback = true;

    int still_running = 0;
    res_t perform_res = ik_openai_multi_perform(multi, &still_running);
    ck_assert(!perform_res.is_err);

    /* Second chunk with same metadata - should skip extraction since already captured */
    const char *sse_data2 =
        "data: {\"model\":\"gpt-3.5\",\"usage\":{\"completion_tokens\":10},\"choices\":[{\"finish_reason\":\"length\",\"delta\":{\"content\":\" World\"}}]}\n\n";
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

static Suite *client_multi_callbacks_metadata_choices_suite(void)
{
    Suite *s = suite_create("openai_client_multi_callbacks_metadata_choices");

    TCase *tc_choices = tcase_create("choices_extraction");
    tcase_add_checked_fixture(tc_choices, setup, teardown);
    tcase_add_test(tc_choices, test_http_write_callback_missing_choices);
    tcase_add_test(tc_choices, test_http_write_callback_empty_choices);
    tcase_add_test(tc_choices, test_http_write_callback_choice_not_object);
    tcase_add_test(tc_choices, test_http_write_callback_finish_reason_not_string);
    tcase_add_test(tc_choices, test_http_write_callback_metadata_already_captured);
    suite_add_tcase(s, tc_choices);

    return s;
}

int main(void)
{
    Suite *s = client_multi_callbacks_metadata_choices_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
