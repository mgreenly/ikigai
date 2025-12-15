/* Unit tests for OpenAI multi-handle manager - logging coverage gaps */

#include "client_multi_test_common.h"
#include "client_multi_info_read_helpers.h"
#include "vendor/yyjson/yyjson.h"

/*
 * Wrapper function declarations for testing
 */
extern yyjson_mut_val *yyjson_mut_doc_get_root_wrapper(yyjson_mut_doc *doc);
extern bool yyjson_mut_obj_add_str_wrapper(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, const char *val);
extern bool yyjson_mut_obj_add_int_wrapper(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, int64_t val);
extern yyjson_mut_val *yyjson_mut_obj_add_obj_wrapper(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key);

/*
 * Wrapper function tests - cover inline vendor function NULL branches
 */

START_TEST(test_yyjson_mut_doc_get_root_wrapper_null) {
    yyjson_mut_val *result = yyjson_mut_doc_get_root_wrapper(NULL);
    ck_assert_ptr_null(result);
}

END_TEST

START_TEST(test_yyjson_mut_doc_get_root_wrapper_valid) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    ck_assert_ptr_nonnull(doc);

    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);

    yyjson_mut_val *result = yyjson_mut_doc_get_root_wrapper(doc);
    ck_assert_ptr_nonnull(result);
    ck_assert_ptr_eq(result, root);

    yyjson_mut_doc_free(doc);
}

END_TEST

START_TEST(test_yyjson_mut_obj_add_str_wrapper_null_doc) {
    bool result = yyjson_mut_obj_add_str_wrapper(NULL, NULL, "key", "value");
    ck_assert(!result);
}

END_TEST

START_TEST(test_yyjson_mut_obj_add_str_wrapper_null_obj) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    bool result = yyjson_mut_obj_add_str_wrapper(doc, NULL, "key", "value");
    ck_assert(!result);
    yyjson_mut_doc_free(doc);
}

END_TEST

START_TEST(test_yyjson_mut_obj_add_str_wrapper_valid) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *obj = yyjson_mut_obj(doc);
    bool result = yyjson_mut_obj_add_str_wrapper(doc, obj, "key", "value");
    ck_assert(result);
    yyjson_mut_doc_free(doc);
}

END_TEST

START_TEST(test_yyjson_mut_obj_add_str_wrapper_null_val) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *obj = yyjson_mut_obj(doc);
    bool result = yyjson_mut_obj_add_str_wrapper(doc, obj, "key", NULL);
    ck_assert(!result);
    yyjson_mut_doc_free(doc);
}

END_TEST

START_TEST(test_yyjson_mut_obj_add_int_wrapper_null_doc) {
    bool result = yyjson_mut_obj_add_int_wrapper(NULL, NULL, "key", 42);
    ck_assert(!result);
}

END_TEST

START_TEST(test_yyjson_mut_obj_add_int_wrapper_null_obj) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    bool result = yyjson_mut_obj_add_int_wrapper(doc, NULL, "key", 42);
    ck_assert(!result);
    yyjson_mut_doc_free(doc);
}

END_TEST

START_TEST(test_yyjson_mut_obj_add_int_wrapper_valid) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *obj = yyjson_mut_obj(doc);
    bool result = yyjson_mut_obj_add_int_wrapper(doc, obj, "key", 42);
    ck_assert(result);
    yyjson_mut_doc_free(doc);
}

END_TEST

START_TEST(test_yyjson_mut_obj_add_obj_wrapper_null_doc) {
    yyjson_mut_val *result = yyjson_mut_obj_add_obj_wrapper(NULL, NULL, "key");
    ck_assert_ptr_null(result);
}

END_TEST

START_TEST(test_yyjson_mut_obj_add_obj_wrapper_null_obj) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *result = yyjson_mut_obj_add_obj_wrapper(doc, NULL, "key");
    ck_assert_ptr_null(result);
    yyjson_mut_doc_free(doc);
}

END_TEST

START_TEST(test_yyjson_mut_obj_add_obj_wrapper_valid) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *obj = yyjson_mut_obj(doc);
    yyjson_mut_val *result = yyjson_mut_obj_add_obj_wrapper(doc, obj, "key");
    ck_assert_ptr_nonnull(result);
    yyjson_mut_doc_free(doc);
}

END_TEST

/*
 * Test for empty response body (complete_response == NULL)
 * This covers the else branch when complete_response is NULL
 */

START_TEST(test_multi_info_read_empty_response_body) {
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    ik_openai_conversation_t *conv = create_test_conversation("Hello");
    ik_cfg_t *cfg = create_test_config();

    /* Add request but don't invoke write callback - this leaves complete_response as NULL */
    res_t add_res = add_test_request(multi, cfg, conv);
    ck_assert(!add_res.is_err);

    /* Setup mock message with 200 OK but no response data written */
    CURLMsg msg;
    setup_mock_curl_msg(&msg, g_last_easy_handle, CURLE_OK, 200);

    /* This should log with empty body since complete_response is NULL */
    ik_openai_multi_info_read(multi, NULL);
    ck_assert(!info_res.is_err);

    talloc_free(multi);
}

END_TEST

/*
 * Test for non-empty response body (complete_response != NULL)
 * This covers lines 178-179 (the true branch when complete_response has content)
 */

START_TEST(test_multi_info_read_with_response_body) {
    res_t multi_res = ik_openai_multi_create(ctx);
    ck_assert(!multi_res.is_err);
    ik_openai_multi_t *multi = multi_res.ok;

    ik_openai_conversation_t *conv = create_test_conversation("Hello");
    ik_cfg_t *cfg = create_test_config();

    /* Add request */
    res_t add_res = add_test_request(multi, cfg, conv);
    ck_assert(!add_res.is_err);

    /* Set up mock response data to trigger write callback */
    const char *response_data = "data: {\"choices\":[{\"delta\":{\"content\":\"test\"}}]}\n\n";
    mock_response_data = response_data;
    mock_response_len = strlen(response_data);
    invoke_write_callback = true;

    /* Perform to invoke write callback and populate complete_response */
    int still_running = 0;
    res_t perform_res = ik_openai_multi_perform(multi, &still_running);
    ck_assert(!perform_res.is_err);

    /* Clean up callback state */
    invoke_write_callback = false;

    /* Setup mock message with 200 OK - now complete_response should be populated */
    CURLMsg msg;
    setup_mock_curl_msg(&msg, g_last_easy_handle, CURLE_OK, 200);

    /* This should log with body content since complete_response is populated */
    ik_openai_multi_info_read(multi, NULL);
    ck_assert(!info_res.is_err);

    talloc_free(multi);
}

END_TEST

/*
 * Test suite
 */

static Suite *client_multi_logging_coverage_suite(void)
{
    Suite *s = suite_create("openai_client_multi_logging_coverage");

    TCase *tc_wrappers = tcase_create("yyjson_wrappers");
    tcase_add_test(tc_wrappers, test_yyjson_mut_doc_get_root_wrapper_null);
    tcase_add_test(tc_wrappers, test_yyjson_mut_doc_get_root_wrapper_valid);
    tcase_add_test(tc_wrappers, test_yyjson_mut_obj_add_str_wrapper_null_doc);
    tcase_add_test(tc_wrappers, test_yyjson_mut_obj_add_str_wrapper_null_obj);
    tcase_add_test(tc_wrappers, test_yyjson_mut_obj_add_str_wrapper_valid);
    tcase_add_test(tc_wrappers, test_yyjson_mut_obj_add_str_wrapper_null_val);
    tcase_add_test(tc_wrappers, test_yyjson_mut_obj_add_int_wrapper_null_doc);
    tcase_add_test(tc_wrappers, test_yyjson_mut_obj_add_int_wrapper_null_obj);
    tcase_add_test(tc_wrappers, test_yyjson_mut_obj_add_int_wrapper_valid);
    tcase_add_test(tc_wrappers, test_yyjson_mut_obj_add_obj_wrapper_null_doc);
    tcase_add_test(tc_wrappers, test_yyjson_mut_obj_add_obj_wrapper_null_obj);
    tcase_add_test(tc_wrappers, test_yyjson_mut_obj_add_obj_wrapper_valid);
    suite_add_tcase(s, tc_wrappers);

    TCase *tc_logging = tcase_create("logging_coverage");
    tcase_add_checked_fixture(tc_logging, setup, teardown);
    tcase_add_test(tc_logging, test_multi_info_read_empty_response_body);
    tcase_add_test(tc_logging, test_multi_info_read_with_response_body);
    suite_add_tcase(s, tc_logging);

    return s;
}

int main(void)
{
    Suite *s = client_multi_logging_coverage_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
