#include "../../../test_constants.h"
/**
 * @file response_parse_validation_test.c
 * @brief Unit tests for Google response parsing - validation and errors
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/google/response.h"
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
 * Error and Validation Tests
 * ================================================================ */

START_TEST(test_parse_error_response) {
    const char *json = "{"
                       "\"error\":{"
                       "\"code\":403,"
                       "\"message\":\"API key invalid\","
                       "\"status\":\"PERMISSION_DENIED\""
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
    ck_assert_ptr_nonnull(strstr(result.err->msg, "API key invalid"));
    talloc_free(result.err);
}

END_TEST

START_TEST(test_parse_blocked_prompt) {
    const char *json = "{"
                       "\"promptFeedback\":{"
                       "\"blockReason\":\"SAFETY\""
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
    ck_assert_ptr_nonnull(strstr(result.err->msg, "SAFETY"));
    talloc_free(result.err);
}

END_TEST

START_TEST(test_parse_invalid_json) {
    const char *json = "not valid json";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
    talloc_free(result.err);
}

END_TEST

START_TEST(test_parse_part_without_text_or_function) {
    const char *json = "{"
                       "\"modelVersion\":\"gemini-2.5-flash\","
                       "\"candidates\":[{"
                       "\"content\":{\"parts\":["
                       "{\"someOtherField\":\"value\"},"
                       "{\"text\":\"Hello world\"}"
                       "]},"
                       "\"finishReason\":\"STOP\""
                       "}],"
                       "\"usageMetadata\":{\"totalTokenCount\":10}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    // First part is skipped, only second part should be present
    ck_assert_uint_eq((unsigned int)resp->content_count, 2);
    ck_assert_int_eq(resp->content_blocks[1].type, IK_CONTENT_TEXT);
    ck_assert_str_eq(resp->content_blocks[1].data.text.text, "Hello world");
}

END_TEST

START_TEST(test_parse_function_call_missing_name) {
    const char *json = "{"
                       "\"modelVersion\":\"gemini-2.5-pro\","
                       "\"candidates\":[{"
                       "\"content\":{\"parts\":[{"
                       "\"functionCall\":{"
                       "\"args\":{\"key\":\"value\"}"
                       "}"
                       "}]},"
                       "\"finishReason\":\"STOP\""
                       "}],"
                       "\"usageMetadata\":{\"totalTokenCount\":10}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
    ck_assert_ptr_nonnull(strstr(result.err->msg, "missing 'name' field"));
    talloc_free(result.err);
}

END_TEST

START_TEST(test_parse_function_call_name_not_string) {
    const char *json = "{"
                       "\"modelVersion\":\"gemini-2.5-pro\","
                       "\"candidates\":[{"
                       "\"content\":{\"parts\":[{"
                       "\"functionCall\":{"
                       "\"name\":123,"
                       "\"args\":{\"key\":\"value\"}"
                       "}"
                       "}]},"
                       "\"finishReason\":\"STOP\""
                       "}],"
                       "\"usageMetadata\":{\"totalTokenCount\":10}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
    ck_assert_ptr_nonnull(strstr(result.err->msg, "not a string"));
    talloc_free(result.err);
}

END_TEST

START_TEST(test_parse_text_not_string) {
    const char *json = "{"
                       "\"modelVersion\":\"gemini-2.5-flash\","
                       "\"candidates\":[{"
                       "\"content\":{\"parts\":[{"
                       "\"text\":42"
                       "}]},"
                       "\"finishReason\":\"STOP\""
                       "}],"
                       "\"usageMetadata\":{\"totalTokenCount\":10}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
    ck_assert_ptr_nonnull(strstr(result.err->msg, "not a string"));
    talloc_free(result.err);
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *google_response_parse_validation_suite(void)
{
    Suite *s = suite_create("Google Response Parsing - Validation");

    TCase *tc_validation = tcase_create("Validation and Errors");
    tcase_set_timeout(tc_validation, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_validation, setup, teardown);
    tcase_add_test(tc_validation, test_parse_error_response);
    tcase_add_test(tc_validation, test_parse_blocked_prompt);
    tcase_add_test(tc_validation, test_parse_invalid_json);
    tcase_add_test(tc_validation, test_parse_part_without_text_or_function);
    tcase_add_test(tc_validation, test_parse_function_call_missing_name);
    tcase_add_test(tc_validation, test_parse_function_call_name_not_string);
    tcase_add_test(tc_validation, test_parse_text_not_string);
    suite_add_tcase(s, tc_validation);

    return s;
}

int main(void)
{
    Suite *s = google_response_parse_validation_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/providers/google/response_parse_validation_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
