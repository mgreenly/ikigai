#include "../../../test_constants.h"
/**
 * @file response_chat_error_test.c
 * @brief Tests for OpenAI Chat Completions error parsing
 */

#include "../../../../src/providers/openai/response.h"
#include "../../../../src/providers/provider.h"
#include "../../../../src/error.h"

#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

/* Test fixture */
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
 * Error Response Tests
 * ================================================================ */

START_TEST(test_parse_error_response) {
    const char *json = "{"
                       "\"error\":{"
                       "\"message\":\"Incorrect API key provided\","
                       "\"type\":\"invalid_request_error\","
                       "\"code\":\"invalid_api_key\""
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_PROVIDER);
}

END_TEST

START_TEST(test_parse_error_response_no_message) {
    /* Error object with no message field - line 170 branch */
    const char *json = "{"
                       "\"error\":{"
                       "\"type\":\"api_error\","
                       "\"code\":\"test_code\""
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_PROVIDER);
    /* Should use default "Unknown error" message */
}

END_TEST

START_TEST(test_parse_error_response_non_string_message) {
    /* Error object with non-string message - line 172 branch */
    const char *json = "{"
                       "\"error\":{"
                       "\"message\":999,"
                       "\"type\":\"api_error\""
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_PROVIDER);
    /* Should use default "Unknown error" message when msg is NULL */
}

END_TEST

START_TEST(test_parse_malformed_json) {
    const char *json = "{invalid json";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_PARSE);
}

END_TEST

START_TEST(test_parse_not_object) {
    const char *json = "[\"array\", \"not\", \"object\"]";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_PARSE);
}

END_TEST
/* ================================================================
 * Error Parsing Tests
 * ================================================================ */

START_TEST(test_parse_error_auth) {
    const char *json = "{"
                       "\"error\":{"
                       "\"message\":\"Invalid API key\","
                       "\"type\":\"invalid_request_error\","
                       "\"code\":\"invalid_api_key\""
                       "}"
                       "}";

    ik_error_category_t category;
    char *message = NULL;
    res_t result = ik_openai_parse_error(test_ctx, 401, json, strlen(json),
                                         &category, &message);

    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_AUTH);
    ck_assert_ptr_nonnull(message);
    ck_assert_str_eq(message, "invalid_request_error (invalid_api_key): Invalid API key");
}

END_TEST

START_TEST(test_parse_error_rate_limit) {
    const char *json = "{"
                       "\"error\":{"
                       "\"message\":\"Rate limit exceeded\","
                       "\"type\":\"rate_limit_error\""
                       "}"
                       "}";

    ik_error_category_t category;
    char *message = NULL;
    res_t result = ik_openai_parse_error(test_ctx, 429, json, strlen(json),
                                         &category, &message);

    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_RATE_LIMIT);
    ck_assert_str_eq(message, "rate_limit_error: Rate limit exceeded");
}

END_TEST

START_TEST(test_parse_error_server) {
    const char *json = "{"
                       "\"error\":{"
                       "\"message\":\"Internal server error\""
                       "}"
                       "}";

    ik_error_category_t category;
    char *message = NULL;
    res_t result = ik_openai_parse_error(test_ctx, 500, json, strlen(json),
                                         &category, &message);

    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
    ck_assert_str_eq(message, "Internal server error");
}

END_TEST

START_TEST(test_parse_error_invalid_arg) {
    ik_error_category_t category;
    char *message = NULL;
    res_t result = ik_openai_parse_error(test_ctx, 400, NULL, 0,
                                         &category, &message);

    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_INVALID_ARG);
    ck_assert_str_eq(message, "HTTP 400");
}

END_TEST

START_TEST(test_parse_error_not_found) {
    ik_error_category_t category;
    char *message = NULL;
    res_t result = ik_openai_parse_error(test_ctx, 404, NULL, 0,
                                         &category, &message);

    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_NOT_FOUND);
    ck_assert_str_eq(message, "HTTP 404");
}

END_TEST

START_TEST(test_parse_error_unknown) {
    ik_error_category_t category;
    char *message = NULL;
    res_t result = ik_openai_parse_error(test_ctx, 418, NULL, 0,
                                         &category, &message);

    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_UNKNOWN);
    ck_assert_str_eq(message, "HTTP 418");
}

END_TEST

START_TEST(test_parse_error_malformed_json) {
    const char *json = "{invalid";

    ik_error_category_t category;
    char *message = NULL;
    res_t result = ik_openai_parse_error(test_ctx, 500, json, strlen(json),
                                         &category, &message);

    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
    ck_assert_str_eq(message, "HTTP 500");
}

END_TEST

START_TEST(test_parse_error_non_string_type) {
    /* Error with non-string type field - tests yyjson_get_str returning NULL for type */
    const char *json = "{"
                       "\"error\":{"
                       "\"type\":123,"
                       "\"message\":\"Test error\""
                       "}"
                       "}";

    ik_error_category_t category;
    char *message = NULL;
    res_t result = ik_openai_parse_error(test_ctx, 500, json, strlen(json),
                                         &category, &message);

    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
    ck_assert_str_eq(message, "Test error");
}

END_TEST

START_TEST(test_parse_error_non_string_code) {
    /* Error with non-string code field - tests yyjson_get_str returning NULL for code */
    const char *json = "{"
                       "\"error\":{"
                       "\"type\":\"api_error\","
                       "\"code\":404,"
                       "\"message\":\"Not found\""
                       "}"
                       "}";

    ik_error_category_t category;
    char *message = NULL;
    res_t result = ik_openai_parse_error(test_ctx, 404, json, strlen(json),
                                         &category, &message);

    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_NOT_FOUND);
    /* Falls back to type: message format since code_str is NULL */
    ck_assert_str_eq(message, "api_error: Not found");
}

END_TEST

START_TEST(test_parse_error_non_string_message) {
    /* Error with non-string message field - tests yyjson_get_str returning NULL for message */
    const char *json = "{"
                       "\"error\":{"
                       "\"type\":\"server_error\","
                       "\"code\":\"internal\","
                       "\"message\":999"
                       "}"
                       "}";

    ik_error_category_t category;
    char *message = NULL;
    res_t result = ik_openai_parse_error(test_ctx, 500, json, strlen(json),
                                         &category, &message);

    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
    /* Falls back to type only since msg_str is NULL */
    ck_assert_str_eq(message, "server_error");
}

END_TEST

START_TEST(test_parse_error_all_non_string) {
    /* Error with all non-string fields - tests all NULL from yyjson_get_str */
    const char *json = "{"
                       "\"error\":{"
                       "\"type\":[],"
                       "\"code\":{},"
                       "\"message\":null"
                       "}"
                       "}";

    ik_error_category_t category;
    char *message = NULL;
    res_t result = ik_openai_parse_error(test_ctx, 500, json, strlen(json),
                                         &category, &message);

    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
    /* Falls back to HTTP status */
    ck_assert_str_eq(message, "HTTP 500");
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *response_chat_error_suite(void)
{
    Suite *s = suite_create("OpenAI Chat Error Parsing");

    TCase *tc_errors = tcase_create("Error Responses");
    tcase_set_timeout(tc_errors, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_errors, setup, teardown);
    tcase_add_test(tc_errors, test_parse_error_response);
    tcase_add_test(tc_errors, test_parse_error_response_no_message);
    tcase_add_test(tc_errors, test_parse_error_response_non_string_message);
    tcase_add_test(tc_errors, test_parse_malformed_json);
    tcase_add_test(tc_errors, test_parse_not_object);
    suite_add_tcase(s, tc_errors);

    TCase *tc_error_parse = tcase_create("Error Parsing");
    tcase_set_timeout(tc_error_parse, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_error_parse, setup, teardown);
    tcase_add_test(tc_error_parse, test_parse_error_auth);
    tcase_add_test(tc_error_parse, test_parse_error_rate_limit);
    tcase_add_test(tc_error_parse, test_parse_error_server);
    tcase_add_test(tc_error_parse, test_parse_error_invalid_arg);
    tcase_add_test(tc_error_parse, test_parse_error_not_found);
    tcase_add_test(tc_error_parse, test_parse_error_unknown);
    tcase_add_test(tc_error_parse, test_parse_error_malformed_json);
    tcase_add_test(tc_error_parse, test_parse_error_non_string_type);
    tcase_add_test(tc_error_parse, test_parse_error_non_string_code);
    tcase_add_test(tc_error_parse, test_parse_error_non_string_message);
    tcase_add_test(tc_error_parse, test_parse_error_all_non_string);
    suite_add_tcase(s, tc_error_parse);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = response_chat_error_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/providers/openai/response_chat_error_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
