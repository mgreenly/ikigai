/* Unit tests for OpenAI multi-handle debug output in add_request */

#include "client_multi_test_common.h"

/*
 * Debug output tests
 */

START_TEST(test_multi_add_request_with_debug_output_file) {
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    /* Create conversation */
    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

    ik_msg_t *msg_tmp = ik_openai_msg_create(ctx, "user", "Hello");
    ik_openai_conversation_add_msg(conv, msg_tmp);

    /* Create config */
    ik_config_t *cfg = talloc_zero(ctx, ik_config_t);
    cfg->openai_model = talloc_strdup(cfg, "gpt-4");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 1000;

    /* Add request - should succeed without crashing */
    res_t add_res = ik_openai_multi_add_request(multi, cfg, conv, NULL, NULL, NULL, NULL, false, NULL);
    ck_assert(!add_res.is_err);
}

END_TEST START_TEST(test_multi_add_request_no_debug_output)
{
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    /* Create conversation */
    ik_openai_conversation_t *conv = ik_openai_conversation_create(ctx);

    ik_msg_t *msg_tmp = ik_openai_msg_create(ctx, "user", "Hello");
    ik_openai_conversation_add_msg(conv, msg_tmp);

    /* Create config */
    ik_config_t *cfg = talloc_zero(ctx, ik_config_t);
    cfg->openai_model = talloc_strdup(cfg, "gpt-4");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 1000;

    /* Add request - should not crash */
    res_t add_res = ik_openai_multi_add_request(multi, cfg, conv, NULL, NULL, NULL, NULL, false, NULL);
    ck_assert(!add_res.is_err);
}

END_TEST

/*
 * Test suite
 */

static Suite *client_multi_add_request_debug_output_suite(void)
{
    Suite *s = suite_create("openai_client_multi_add_request_debug_output");

    TCase *tc_debug = tcase_create("debug_output");
    tcase_add_checked_fixture(tc_debug, setup, teardown);
    tcase_add_test(tc_debug, test_multi_add_request_with_debug_output_file);
    tcase_add_test(tc_debug, test_multi_add_request_no_debug_output);
    suite_add_tcase(s, tc_debug);

    return s;
}

int main(void)
{
    Suite *s = client_multi_add_request_debug_output_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
