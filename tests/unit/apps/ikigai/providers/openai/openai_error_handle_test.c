#include "tests/test_constants.h"
/**
 * @file openai_error_handle_test.c
 * @brief Unit tests for ik_openai_handle_error function
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "apps/ikigai/providers/openai/error.h"

static TALLOC_CTX *test_ctx;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

/* ================================================================
 * ik_openai_handle_error Tests
 * ================================================================ */

START_TEST(test_handle_error_401_auth) {
    const char *json =
        "{\"error\": {\"message\": \"Invalid API key\", \"type\": \"auth_error\", \"code\": \"invalid_api_key\"}}";
    ik_error_category_t category;

    res_t r = ik_openai_handle_error(test_ctx, 401, json, &category);
    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_AUTH);
}
END_TEST

START_TEST(test_handle_error_403_auth) {
    const char *json = "{\"error\": {\"message\": \"Forbidden\", \"type\": \"auth_error\", \"code\": \"forbidden\"}}";
    ik_error_category_t category;

    res_t r = ik_openai_handle_error(test_ctx, 403, json, &category);
    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_AUTH);
}

END_TEST

START_TEST(test_handle_error_429_rate_limit) {
    const char *json =
        "{\"error\": {\"message\": \"Rate limit exceeded\", \"type\": \"rate_limit\", \"code\": \"rate_limit_exceeded\"}}";
    ik_error_category_t category;

    res_t r = ik_openai_handle_error(test_ctx, 429, json, &category);
    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_RATE_LIMIT);
}

END_TEST

START_TEST(test_handle_error_400_invalid_arg) {
    const char *json =
        "{\"error\": {\"message\": \"Bad request\", \"type\": \"invalid_request\", \"code\": \"bad_request\"}}";
    ik_error_category_t category;

    res_t r = ik_openai_handle_error(test_ctx, 400, json, &category);
    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_INVALID_ARG);
}

END_TEST

START_TEST(test_handle_error_404_not_found) {
    const char *json = "{\"error\": {\"message\": \"Not found\", \"type\": \"not_found\", \"code\": \"not_found\"}}";
    ik_error_category_t category;

    res_t r = ik_openai_handle_error(test_ctx, 404, json, &category);
    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_NOT_FOUND);
}

END_TEST

START_TEST(test_handle_error_500_server) {
    const char *json =
        "{\"error\": {\"message\": \"Server error\", \"type\": \"server_error\", \"code\": \"server_error\"}}";
    ik_error_category_t category;

    res_t r = ik_openai_handle_error(test_ctx, 500, json, &category);
    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
}

END_TEST

START_TEST(test_handle_error_502_server) {
    const char *json =
        "{\"error\": {\"message\": \"Bad gateway\", \"type\": \"server_error\", \"code\": \"bad_gateway\"}}";
    ik_error_category_t category;

    res_t r = ik_openai_handle_error(test_ctx, 502, json, &category);
    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
}

END_TEST

START_TEST(test_handle_error_503_server) {
    const char *json =
        "{\"error\": {\"message\": \"Service unavailable\", \"type\": \"server_error\", \"code\": \"service_unavailable\"}}";
    ik_error_category_t category;

    res_t r = ik_openai_handle_error(test_ctx, 503, json, &category);
    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
}

END_TEST

START_TEST(test_handle_error_unknown_status) {
    const char *json = "{\"error\": {\"message\": \"Unknown\", \"type\": \"unknown\", \"code\": \"unknown\"}}";
    ik_error_category_t category;

    res_t r = ik_openai_handle_error(test_ctx, 418, json, &category);
    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_UNKNOWN);
}

END_TEST

START_TEST(test_handle_error_content_filter_code) {
    const char *json =
        "{\"error\": {\"message\": \"Content filtered\", \"type\": \"invalid_request\", \"code\": \"content_filter\"}}";
    ik_error_category_t category;

    res_t r = ik_openai_handle_error(test_ctx, 400, json, &category);
    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_CONTENT_FILTER);
}

END_TEST

START_TEST(test_handle_error_content_filter_type) {
    const char *json =
        "{\"error\": {\"message\": \"Content filtered\", \"type\": \"content_filter\", \"code\": \"blocked\"}}";
    ik_error_category_t category;

    res_t r = ik_openai_handle_error(test_ctx, 400, json, &category);
    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_CONTENT_FILTER);
}

END_TEST

START_TEST(test_handle_error_invalid_api_key_code) {
    const char *json = "{\"error\": {\"message\": \"Invalid key\", \"type\": \"auth\", \"code\": \"invalid_api_key\"}}";
    ik_error_category_t category;

    res_t r = ik_openai_handle_error(test_ctx, 401, json, &category);
    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_AUTH);
}

END_TEST

START_TEST(test_handle_error_invalid_org_code) {
    const char *json = "{\"error\": {\"message\": \"Invalid org\", \"type\": \"auth\", \"code\": \"invalid_org\"}}";
    ik_error_category_t category;

    res_t r = ik_openai_handle_error(test_ctx, 401, json, &category);
    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_AUTH);
}

END_TEST

START_TEST(test_handle_error_quota_exceeded_code) {
    const char *json =
        "{\"error\": {\"message\": \"Quota exceeded\", \"type\": \"rate_limit\", \"code\": \"quota_exceeded\"}}";
    ik_error_category_t category;

    res_t r = ik_openai_handle_error(test_ctx, 429, json, &category);
    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_RATE_LIMIT);
}

END_TEST

START_TEST(test_handle_error_model_not_found_code) {
    const char *json =
        "{\"error\": {\"message\": \"Model not found\", \"type\": \"not_found\", \"code\": \"model_not_found\"}}";
    ik_error_category_t category;

    res_t r = ik_openai_handle_error(test_ctx, 404, json, &category);
    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_NOT_FOUND);
}

END_TEST

START_TEST(test_handle_error_no_error_object) {
    const char *json = "{\"message\": \"Error without error object\"}";
    ik_error_category_t category;

    res_t r = ik_openai_handle_error(test_ctx, 500, json, &category);
    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
}

END_TEST

START_TEST(test_handle_error_invalid_json) {
    const char *json = "not valid json";
    ik_error_category_t category;

    res_t r = ik_openai_handle_error(test_ctx, 500, json, &category);
    ck_assert(is_err(&r));
}

END_TEST

START_TEST(test_handle_error_empty_json) {
    const char *json = "{}";
    ik_error_category_t category;

    res_t r = ik_openai_handle_error(test_ctx, 500, json, &category);
    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
}

END_TEST

START_TEST(test_handle_error_null_root) {
    const char *json = "null";
    ik_error_category_t category;

    res_t r = ik_openai_handle_error(test_ctx, 500, json, &category);
    // JSON "null" is valid and yyjson_doc_get_root returns non-NULL
    // It just doesn't have an "error" object, so uses default status mapping
    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
}

END_TEST

START_TEST(test_handle_error_error_object_not_object) {
    const char *json = "{\"error\": \"string not object\"}";
    ik_error_category_t category;

    res_t r = ik_openai_handle_error(test_ctx, 500, json, &category);
    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
}

END_TEST

START_TEST(test_handle_error_code_not_string) {
    const char *json = "{\"error\": {\"message\": \"Test\", \"type\": \"error\", \"code\": 123}}";
    ik_error_category_t category;

    res_t r = ik_openai_handle_error(test_ctx, 500, json, &category);
    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
}

END_TEST

START_TEST(test_handle_error_type_not_string) {
    const char *json = "{\"error\": {\"message\": \"Test\", \"type\": 123, \"code\": \"test\"}}";
    ik_error_category_t category;

    res_t r = ik_openai_handle_error(test_ctx, 500, json, &category);
    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
}

END_TEST

START_TEST(test_handle_error_null_code) {
    const char *json = "{\"error\": {\"message\": \"Test\", \"type\": \"error\", \"code\": null}}";
    ik_error_category_t category;

    res_t r = ik_openai_handle_error(test_ctx, 500, json, &category);
    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
}

END_TEST

START_TEST(test_handle_error_null_type) {
    const char *json = "{\"error\": {\"message\": \"Test\", \"type\": null, \"code\": \"test\"}}";
    ik_error_category_t category;

    res_t r = ik_openai_handle_error(test_ctx, 500, json, &category);
    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
}

END_TEST

START_TEST(test_handle_error_content_filter_in_code_substring) {
    const char *json =
        "{\"error\": {\"message\": \"Filtered\", \"type\": \"error\", \"code\": \"test_content_filter_test\"}}";
    ik_error_category_t category;

    res_t r = ik_openai_handle_error(test_ctx, 400, json, &category);
    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_CONTENT_FILTER);
}

END_TEST

START_TEST(test_handle_error_content_filter_in_type_substring) {
    const char *json =
        "{\"error\": {\"message\": \"Filtered\", \"type\": \"prefix_content_filter_suffix\", \"code\": \"test\"}}";
    ik_error_category_t category;

    res_t r = ik_openai_handle_error(test_ctx, 400, json, &category);
    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_CONTENT_FILTER);
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *openai_error_handle_suite(void)
{
    Suite *s = suite_create("OpenAI Error Handle");

    TCase *tc_handle = tcase_create("Handle Error");
    tcase_set_timeout(tc_handle, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_handle, setup, teardown);
    tcase_add_test(tc_handle, test_handle_error_401_auth);
    tcase_add_test(tc_handle, test_handle_error_403_auth);
    tcase_add_test(tc_handle, test_handle_error_429_rate_limit);
    tcase_add_test(tc_handle, test_handle_error_400_invalid_arg);
    tcase_add_test(tc_handle, test_handle_error_404_not_found);
    tcase_add_test(tc_handle, test_handle_error_500_server);
    tcase_add_test(tc_handle, test_handle_error_502_server);
    tcase_add_test(tc_handle, test_handle_error_503_server);
    tcase_add_test(tc_handle, test_handle_error_unknown_status);
    tcase_add_test(tc_handle, test_handle_error_content_filter_code);
    tcase_add_test(tc_handle, test_handle_error_content_filter_type);
    tcase_add_test(tc_handle, test_handle_error_invalid_api_key_code);
    tcase_add_test(tc_handle, test_handle_error_invalid_org_code);
    tcase_add_test(tc_handle, test_handle_error_quota_exceeded_code);
    tcase_add_test(tc_handle, test_handle_error_model_not_found_code);
    tcase_add_test(tc_handle, test_handle_error_no_error_object);
    tcase_add_test(tc_handle, test_handle_error_invalid_json);
    tcase_add_test(tc_handle, test_handle_error_empty_json);
    tcase_add_test(tc_handle, test_handle_error_null_root);
    tcase_add_test(tc_handle, test_handle_error_error_object_not_object);
    tcase_add_test(tc_handle, test_handle_error_code_not_string);
    tcase_add_test(tc_handle, test_handle_error_type_not_string);
    tcase_add_test(tc_handle, test_handle_error_null_code);
    tcase_add_test(tc_handle, test_handle_error_null_type);
    tcase_add_test(tc_handle, test_handle_error_content_filter_in_code_substring);
    tcase_add_test(tc_handle, test_handle_error_content_filter_in_type_substring);
    suite_add_tcase(s, tc_handle);

    return s;
}

int main(void)
{
    Suite *s = openai_error_handle_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/openai/openai_error_handle_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
