/* Unit tests for OpenAI multi-handle add_request functionality */

#include "client_multi_test_common.h"

/*
 * Add request tests
 */

START_TEST(test_multi_add_request_empty_conversation) {
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    /* Create empty conversation */
    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

    /* Create config */
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    cfg->openai_api_key = talloc_strdup(cfg, "test-key");
    cfg->openai_model = talloc_strdup(cfg, "gpt-4");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 1000;

    /* Try to add request with empty conversation */
    res_t add_res = ik_openai_multi_add_request(multi, cfg, conv, NULL, NULL, NULL, NULL, false, NULL);
    ck_assert(add_res.is_err);
    ck_assert_int_eq(add_res.err->code, ERR_INVALID_ARG);
}

END_TEST START_TEST(test_multi_add_request_no_api_key)
{
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    /* Create conversation with one message */
    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

    res_t msg_res = ik_openai_msg_create(ctx, "user", "Hello");
    ck_assert(!msg_res.is_err);
    ik_msg_t *msg = msg_res.ok;

    ik_openai_conversation_add_msg(conv, msg);

    /* Create config without API key */
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    cfg->openai_api_key = NULL;
    cfg->openai_model = talloc_strdup(cfg, "gpt-4");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 1000;

    /* Try to add request without API key */
    res_t add_res = ik_openai_multi_add_request(multi, cfg, conv, NULL, NULL, NULL, NULL, false, NULL);
    ck_assert(add_res.is_err);
    ck_assert_int_eq(add_res.err->code, ERR_INVALID_ARG);
}

END_TEST START_TEST(test_multi_add_request_empty_api_key)
{
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    /* Create conversation with one message */
    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

    res_t msg_res = ik_openai_msg_create(ctx, "user", "Hello");
    ck_assert(!msg_res.is_err);
    ik_msg_t *msg = msg_res.ok;

    ik_openai_conversation_add_msg(conv, msg);

    /* Create config with empty API key */
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    cfg->openai_api_key = talloc_strdup(cfg, "");  /* Empty string */
    cfg->openai_model = talloc_strdup(cfg, "gpt-4");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 1000;

    /* Try to add request with empty API key */
    res_t add_res = ik_openai_multi_add_request(multi, cfg, conv, NULL, NULL, NULL, NULL, false, NULL);
    ck_assert(add_res.is_err);
    ck_assert_int_eq(add_res.err->code, ERR_INVALID_ARG);
}

END_TEST START_TEST(test_multi_add_request_curl_easy_init_failure)
{
    /* Create multi-handle */
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    /* Create conversation */
    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

    res_t msg_res = ik_openai_msg_create(ctx, "user", "Hello");
    ck_assert(!msg_res.is_err);
    ik_openai_conversation_add_msg(conv, msg_res.ok);

    /* Create config */
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    cfg->openai_api_key = talloc_strdup(cfg, "test-key");
    cfg->openai_model = talloc_strdup(cfg, "gpt-4");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 1000;

    /* Mock curl_easy_init to fail */
    fail_curl_easy_init = true;

    res_t add_res = ik_openai_multi_add_request(multi, cfg, conv, NULL, NULL, NULL, NULL, false, NULL);
    ck_assert(add_res.is_err);
    ck_assert_int_eq(add_res.err->code, ERR_IO);

    fail_curl_easy_init = false;
}

END_TEST START_TEST(test_multi_add_request_api_key_too_long)
{
    /* Create multi-handle */
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    /* Create conversation */
    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

    res_t msg_res = ik_openai_msg_create(ctx, "user", "Hello");
    ck_assert(!msg_res.is_err);
    ik_openai_conversation_add_msg(conv, msg_res.ok);

    /* Create config with very long API key (> 512 - "Authorization: Bearer " length) */
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    char *long_key = talloc_array(cfg, char, 500);
    memset(long_key, 'A', 499);
    long_key[499] = '\0';
    cfg->openai_api_key = long_key;
    cfg->openai_model = talloc_strdup(cfg, "gpt-4");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 1000;

    res_t add_res = ik_openai_multi_add_request(multi, cfg, conv, NULL, NULL, NULL, NULL, false, NULL);
    ck_assert(add_res.is_err);
    ck_assert_int_eq(add_res.err->code, ERR_INVALID_ARG);
}

END_TEST START_TEST(test_multi_add_request_snprintf_error)
{
    /* Create multi-handle */
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    /* Create conversation */
    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

    res_t msg_res = ik_openai_msg_create(ctx, "user", "Hello");
    ck_assert(!msg_res.is_err);
    ik_openai_conversation_add_msg(conv, msg_res.ok);

    /* Create config */
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    cfg->openai_api_key = talloc_strdup(cfg, "sk-test");
    cfg->openai_model = talloc_strdup(cfg, "gpt-4");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 1000;

    /* Mock snprintf_ to fail */
    fail_snprintf = true;

    res_t add_res = ik_openai_multi_add_request(multi, cfg, conv, NULL, NULL, NULL, NULL, false, NULL);
    ck_assert(add_res.is_err);
    ck_assert_int_eq(add_res.err->code, ERR_INVALID_ARG);

    fail_snprintf = false;
}

END_TEST START_TEST(test_multi_add_request_success)
{
    /* Create multi-handle */
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    /* Create conversation */
    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

    res_t msg_res = ik_openai_msg_create(ctx, "user", "Hello");
    ck_assert(!msg_res.is_err);
    ik_openai_conversation_add_msg(conv, msg_res.ok);

    /* Create config with normal API key */
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    cfg->openai_api_key = talloc_strdup(cfg, "sk-test123");
    cfg->openai_model = talloc_strdup(cfg, "gpt-4");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 1000;

    /* Add request should succeed */
    res_t add_res = ik_openai_multi_add_request(multi, cfg, conv, NULL, NULL, NULL, NULL, false, NULL);
    ck_assert(!add_res.is_err);
}

END_TEST START_TEST(test_multi_add_request_curl_multi_add_handle_failure)
{
    /* Create multi-handle */
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    /* Create conversation */
    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

    res_t msg_res = ik_openai_msg_create(ctx, "user", "Hello");
    ck_assert(!msg_res.is_err);
    ik_openai_conversation_add_msg(conv, msg_res.ok);

    /* Create config */
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    cfg->openai_api_key = talloc_strdup(cfg, "sk-test");
    cfg->openai_model = talloc_strdup(cfg, "gpt-4");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 1000;

    /* Mock curl_multi_add_handle to fail */
    fail_curl_multi_add_handle = true;

    res_t add_res = ik_openai_multi_add_request(multi, cfg, conv, NULL, NULL, NULL, NULL, false, NULL);
    ck_assert(add_res.is_err);
    ck_assert_int_eq(add_res.err->code, ERR_IO);

    fail_curl_multi_add_handle = false;
}

END_TEST START_TEST(test_multi_destructor_with_active_requests)
{
    /* Create multi-handle */
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    /* Create conversation */
    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

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

    /* Free multi - should trigger destructor with active requests */
    talloc_free(multi);
}

END_TEST START_TEST(test_multi_add_request_limit_reached)
{
    /* Create multi-handle */
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    /* Create conversation */
    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

    res_t msg_res = ik_openai_msg_create(ctx, "user", "Hello");
    ck_assert(!msg_res.is_err);
    ik_openai_conversation_add_msg(conv, msg_res.ok);

    /* Create config with normal API key */
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    cfg->openai_api_key = talloc_strdup(cfg, "sk-test123");
    cfg->openai_model = talloc_strdup(cfg, "gpt-4");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 1000;

    /* Add request with limit_reached=true */
    res_t add_res = ik_openai_multi_add_request(multi, cfg, conv, NULL, NULL, NULL, NULL, true, NULL);
    ck_assert(!add_res.is_err);
}

END_TEST

/*
 * Test suite
 */

static Suite *client_multi_add_request_suite(void)
{
    Suite *s = suite_create("openai_client_multi_add_request");

    TCase *tc_add = tcase_create("add_request");
    tcase_add_checked_fixture(tc_add, setup, teardown);
    tcase_add_test(tc_add, test_multi_add_request_empty_conversation);
    tcase_add_test(tc_add, test_multi_add_request_no_api_key);
    tcase_add_test(tc_add, test_multi_add_request_empty_api_key);
    tcase_add_test(tc_add, test_multi_add_request_curl_easy_init_failure);
    tcase_add_test(tc_add, test_multi_add_request_api_key_too_long);
    tcase_add_test(tc_add, test_multi_add_request_snprintf_error);
    tcase_add_test(tc_add, test_multi_add_request_success);
    tcase_add_test(tc_add, test_multi_add_request_curl_multi_add_handle_failure);
    tcase_add_test(tc_add, test_multi_destructor_with_active_requests);
    tcase_add_test(tc_add, test_multi_add_request_limit_reached);
    suite_add_tcase(s, tc_add);

    return s;
}

int main(void)
{
    Suite *s = client_multi_add_request_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
