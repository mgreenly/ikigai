#include "../../../test_constants.h"
/**
 * @file test_openai_errors.c
 * @brief Unit tests for OpenAI error handling and HTTP status mapping
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/openai/error.h"
#include "providers/openai/response.h"
#include "providers/provider.h"

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
 * Error Handling Tests
 * ================================================================ */

START_TEST(test_parse_authentication_error_401) {
    const char *error_json =
        "{"
        "  \"error\": {"
        "    \"message\": \"Incorrect API key provided: sk-****. You can find your API key at https://platform.openai.com/account/api-keys.\","
        "    \"type\": \"invalid_request_error\","
        "    \"param\": null,"
        "    \"code\": \"invalid_api_key\""
        "  }"
        "}";

    ik_error_category_t category;
    char *message = NULL;

    res_t r = ik_openai_parse_error(test_ctx, 401, error_json, strlen(error_json),
                                    &category, &message);

    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_AUTH);
    ck_assert_ptr_nonnull(message);
    ck_assert(strstr(message, "API key") != NULL || strstr(message, "authentication") != NULL);
}
END_TEST

START_TEST(test_parse_rate_limit_error_429) {
    const char *error_json =
        "{"
        "  \"error\": {"
        "    \"message\": \"Rate limit reached for requests\","
        "    \"type\": \"requests\","
        "    \"param\": null,"
        "    \"code\": \"rate_limit_exceeded\""
        "  }"
        "}";

    ik_error_category_t category;
    char *message = NULL;

    res_t r = ik_openai_parse_error(test_ctx, 429, error_json, strlen(error_json),
                                    &category, &message);

    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_RATE_LIMIT);
    ck_assert_ptr_nonnull(message);
}

END_TEST

START_TEST(test_parse_context_length_error_400) {
    const char *error_json =
        "{"
        "  \"error\": {"
        "    \"message\": \"This model's maximum context length is 8192 tokens\","
        "    \"type\": \"invalid_request_error\","
        "    \"param\": \"messages\","
        "    \"code\": \"context_length_exceeded\""
        "  }"
        "}";

    ik_error_category_t category;
    char *message = NULL;

    res_t r = ik_openai_parse_error(test_ctx, 400, error_json, strlen(error_json),
                                    &category, &message);

    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_INVALID_ARG);
    ck_assert_ptr_nonnull(message);
}

END_TEST

START_TEST(test_parse_model_not_found_error_404) {
    const char *error_json =
        "{"
        "  \"error\": {"
        "    \"message\": \"The model 'gpt-99' does not exist\","
        "    \"type\": \"invalid_request_error\","
        "    \"param\": null,"
        "    \"code\": \"model_not_found\""
        "  }"
        "}";

    ik_error_category_t category;
    char *message = NULL;

    res_t r = ik_openai_parse_error(test_ctx, 404, error_json, strlen(error_json),
                                    &category, &message);

    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_NOT_FOUND);
    ck_assert_ptr_nonnull(message);
}

END_TEST

START_TEST(test_map_errors_to_correct_categories) {
    // Test various HTTP status codes map correctly
    ik_error_category_t category;
    char *message = NULL;
    res_t r;

    // 401 -> AUTH
    r = ik_openai_parse_error(test_ctx, 401, NULL, 0, &category, &message);
    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_AUTH);

    // 403 -> AUTH
    r = ik_openai_parse_error(test_ctx, 403, NULL, 0, &category, &message);
    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_AUTH);

    // 404 -> NOT_FOUND
    r = ik_openai_parse_error(test_ctx, 404, NULL, 0, &category, &message);
    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_NOT_FOUND);

    // 500 -> SERVER
    r = ik_openai_parse_error(test_ctx, 500, NULL, 0, &category, &message);
    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);

    // 503 -> SERVER
    r = ik_openai_parse_error(test_ctx, 503, NULL, 0, &category, &message);
    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);

    // 504 -> UNKNOWN (not mapped to TIMEOUT in current implementation)
    r = ik_openai_parse_error(test_ctx, 504, NULL, 0, &category, &message);
    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_UNKNOWN);
}

END_TEST

START_TEST(test_parse_server_error_500) {
    const char *error_json =
        "{"
        "  \"error\": {"
        "    \"message\": \"The server had an error while processing your request. Sorry about that!\","
        "    \"type\": \"server_error\","
        "    \"param\": null,"
        "    \"code\": null"
        "  }"
        "}";

    ik_error_category_t category;
    char *message = NULL;

    res_t r = ik_openai_parse_error(test_ctx, 500, error_json, strlen(error_json),
                                    &category, &message);

    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
    ck_assert_ptr_nonnull(message);
}

END_TEST

START_TEST(test_parse_service_unavailable_503) {
    const char *error_json =
        "{"
        "  \"error\": {"
        "    \"message\": \"The server is currently overloaded with other requests. Sorry about that!\","
        "    \"type\": \"server_error\","
        "    \"param\": null,"
        "    \"code\": \"service_unavailable\""
        "  }"
        "}";

    ik_error_category_t category;
    char *message = NULL;

    res_t r = ik_openai_parse_error(test_ctx, 503, error_json, strlen(error_json),
                                    &category, &message);

    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
    ck_assert_ptr_nonnull(message);
}

END_TEST

START_TEST(test_parse_error_message_only) {
    // Test error object with only message field (line 376)
    const char *error_json =
        "{"
        "  \"error\": {"
        "    \"message\": \"Something went wrong\""
        "  }"
        "}";

    ik_error_category_t category;
    char *message = NULL;

    res_t r = ik_openai_parse_error(test_ctx, 500, error_json, strlen(error_json),
                                    &category, &message);

    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
    ck_assert_ptr_nonnull(message);
    ck_assert_str_eq(message, "Something went wrong");
}

END_TEST

START_TEST(test_parse_error_type_only) {
    // Test error object with only type field (line 378)
    const char *error_json =
        "{"
        "  \"error\": {"
        "    \"type\": \"server_error\""
        "  }"
        "}";

    ik_error_category_t category;
    char *message = NULL;

    res_t r = ik_openai_parse_error(test_ctx, 500, error_json, strlen(error_json),
                                    &category, &message);

    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
    ck_assert_ptr_nonnull(message);
    ck_assert_str_eq(message, "server_error");
}

END_TEST

START_TEST(test_parse_error_no_fields) {
    // Test error object with no type, code, or message fields (line 381)
    const char *error_json =
        "{"
        "  \"error\": {"
        "    \"param\": null"
        "  }"
        "}";

    ik_error_category_t category;
    char *message = NULL;

    res_t r = ik_openai_parse_error(test_ctx, 500, error_json, strlen(error_json),
                                    &category, &message);

    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
    ck_assert_ptr_nonnull(message);
    ck_assert_str_eq(message, "HTTP 500");
}

END_TEST

START_TEST(test_parse_error_http_502) {
    // Test HTTP 502 status code (line 334)
    ik_error_category_t category;
    char *message = NULL;

    res_t r = ik_openai_parse_error(test_ctx, 502, NULL, 0, &category, &message);

    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
    ck_assert_ptr_nonnull(message);
    ck_assert_str_eq(message, "HTTP 502");
}

END_TEST
/* ================================================================
 * Retry-After Header Tests
 * ================================================================ */

START_TEST(test_extract_retry_after_from_reset_headers) {
    const char *headers[] = {
        "content-type: application/json",
        "x-ratelimit-reset-requests: 30s",
        "x-ratelimit-limit-requests: 10000",
        NULL
    };

    int32_t retry_after = ik_openai_get_retry_after(headers);
    ck_assert_int_eq(retry_after, 30);
}

END_TEST

START_TEST(test_retry_after_missing) {
    const char *headers[] = {
        "content-type: application/json",
        "x-ratelimit-limit-requests: 10000",
        NULL
    };

    int32_t retry_after = ik_openai_get_retry_after(headers);
    ck_assert_int_eq(retry_after, -1);
}

END_TEST

START_TEST(test_retry_after_tokens_reset) {
    const char *headers[] = {
        "content-type: application/json",
        "x-ratelimit-reset-tokens: 6m0s",
        "x-ratelimit-limit-tokens: 200000",
        NULL
    };

    int32_t retry_after = ik_openai_get_retry_after(headers);
    ck_assert_int_eq(retry_after, 360);  // 6 minutes = 360 seconds
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *openai_errors_suite(void)
{
    Suite *s = suite_create("OpenAI Errors");

    TCase *tc_errors = tcase_create("Error Handling");
    tcase_set_timeout(tc_errors, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_errors, setup, teardown);
    tcase_add_test(tc_errors, test_parse_authentication_error_401);
    tcase_add_test(tc_errors, test_parse_rate_limit_error_429);
    tcase_add_test(tc_errors, test_parse_context_length_error_400);
    tcase_add_test(tc_errors, test_parse_model_not_found_error_404);
    tcase_add_test(tc_errors, test_map_errors_to_correct_categories);
    tcase_add_test(tc_errors, test_parse_server_error_500);
    tcase_add_test(tc_errors, test_parse_service_unavailable_503);
    tcase_add_test(tc_errors, test_parse_error_message_only);
    tcase_add_test(tc_errors, test_parse_error_type_only);
    tcase_add_test(tc_errors, test_parse_error_no_fields);
    tcase_add_test(tc_errors, test_parse_error_http_502);
    suite_add_tcase(s, tc_errors);

    TCase *tc_retry = tcase_create("Retry-After");
    tcase_set_timeout(tc_retry, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_retry, setup, teardown);
    tcase_add_test(tc_retry, test_extract_retry_after_from_reset_headers);
    tcase_add_test(tc_retry, test_retry_after_missing);
    tcase_add_test(tc_retry, test_retry_after_tokens_reset);
    suite_add_tcase(s, tc_retry);

    return s;
}

int main(void)
{
    Suite *s = openai_errors_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/providers/openai/openai_errors_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
