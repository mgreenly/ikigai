#include "../../../test_constants.h"
/**
 * @file response_main_coverage_test.c
 * @brief Coverage tests for Google response.c main parsing edge cases
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
 * ik_google_parse_response Edge Cases
 * ================================================================ */

START_TEST(test_parse_invalid_json) {
    const char *json = "{invalid json";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
    ck_assert_ptr_nonnull(strstr(result.err->msg, "Invalid JSON"));
    talloc_free(result.err);
}
END_TEST

START_TEST(test_parse_root_not_object) {
    const char *json = "[]";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
    ck_assert_ptr_nonnull(strstr(result.err->msg, "Root is not an object"));
    talloc_free(result.err);
}
END_TEST

START_TEST(test_parse_error_with_null_message) {
    const char *json = "{"
                       "\"error\":{"
                       "\"code\":500"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
    ck_assert_ptr_nonnull(strstr(result.err->msg, "Unknown error"));
    talloc_free(result.err);
}
END_TEST

START_TEST(test_parse_error_with_message) {
    const char *json = "{"
                       "\"error\":{"
                       "\"code\":400,"
                       "\"message\":\"Invalid request\""
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
    ck_assert_ptr_nonnull(strstr(result.err->msg, "Invalid request"));
    talloc_free(result.err);
}
END_TEST

START_TEST(test_parse_blocked_prompt_null_reason) {
    const char *json = "{"
                       "\"promptFeedback\":{"
                       "\"blockReason\":null"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
    ck_assert_ptr_nonnull(strstr(result.err->msg, "Unknown reason"));
    talloc_free(result.err);
}
END_TEST

START_TEST(test_parse_blocked_prompt_with_reason) {
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

START_TEST(test_parse_promptfeedback_without_blockreason) {
    const char *json = "{\"promptFeedback\":{\"other\":\"x\"},"
                       "\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Hi\"}]}}]}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_uint_eq((unsigned int)resp->content_count, 1);
}
END_TEST

START_TEST(test_parse_no_model_version) {
    const char *json = "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Hi\"}]}}]}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
}
END_TEST

START_TEST(test_parse_model_version_not_string) {
    const char *json = "{\"modelVersion\":123,\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Hi\"}]}}]}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
}
END_TEST

START_TEST(test_parse_no_usage_metadata) {
    const char *json = "{"
                       "\"modelVersion\":\"gemini-2.5-flash\","
                       "\"candidates\":[{"
                       "\"content\":{\"parts\":[{\"text\":\"Hello\"}]},"
                       "\"finishReason\":\"STOP\""
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
}
END_TEST

START_TEST(test_parse_usage_missing_some_fields) {
    const char *json = "{"
                       "\"modelVersion\":\"gemini-2.5-flash\","
                       "\"candidates\":[{"
                       "\"content\":{\"parts\":[{\"text\":\"Hello\"}]},"
                       "\"finishReason\":\"STOP\""
                       "}],"
                       "\"usageMetadata\":{"
                       "\"promptTokenCount\":10"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq(resp->usage.input_tokens, 10);
    ck_assert_int_eq(resp->usage.output_tokens, 0);
    ck_assert_int_eq(resp->usage.thinking_tokens, 0);
}
END_TEST

START_TEST(test_parse_usage_all_fields_present) {
    const char *json = "{"
                       "\"modelVersion\":\"gemini-3\","
                       "\"candidates\":[{"
                       "\"content\":{\"parts\":[{\"text\":\"Hello\"}]},"
                       "\"finishReason\":\"STOP\""
                       "}],"
                       "\"usageMetadata\":{"
                       "\"promptTokenCount\":100,"
                       "\"candidatesTokenCount\":50,"
                       "\"thoughtsTokenCount\":10,"
                       "\"totalTokenCount\":150"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq(resp->usage.input_tokens, 100);
    ck_assert_int_eq(resp->usage.thinking_tokens, 10);
    ck_assert_int_eq(resp->usage.output_tokens, 40);
    ck_assert_int_eq(resp->usage.total_tokens, 150);
}
END_TEST

START_TEST(test_parse_usage_all_fields_null) {
    const char *json = "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Hi\"}]}}],"
                       "\"usageMetadata\":{\"promptTokenCount\":null,\"candidatesTokenCount\":null,"
                       "\"thoughtsTokenCount\":null,\"totalTokenCount\":null}}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_int_eq(resp->usage.input_tokens, 0);
    ck_assert_int_eq(resp->usage.total_tokens, 0);
}
END_TEST

START_TEST(test_parse_no_candidates_field) {
    const char *json = "{\"modelVersion\":\"gemini-2.5-flash\"}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_uint_eq((unsigned int)resp->content_count, 0);
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_UNKNOWN);
}
END_TEST

START_TEST(test_parse_content_without_parts_field) {
    const char *json = "{\"candidates\":[{\"content\":{\"other\":\"x\"},\"finishReason\":\"STOP\"}]}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_uint_eq((unsigned int)resp->content_count, 0);
    ck_assert_ptr_null(resp->content_blocks);
}
END_TEST

START_TEST(test_parse_candidates_not_array) {
    const char *json = "{"
                       "\"modelVersion\":\"gemini-2.5-flash\","
                       "\"candidates\":\"not an array\""
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_uint_eq((unsigned int)resp->content_count, 0);
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_UNKNOWN);
}
END_TEST

START_TEST(test_parse_empty_candidates_array) {
    const char *json = "{"
                       "\"modelVersion\":\"gemini-2.5-flash\","
                       "\"candidates\":[]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_uint_eq((unsigned int)resp->content_count, 0);
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_UNKNOWN);
}
END_TEST

START_TEST(test_parse_no_finish_reason) {
    const char *json = "{"
                       "\"modelVersion\":\"gemini-2.5-flash\","
                       "\"candidates\":[{"
                       "\"content\":{\"parts\":[{\"text\":\"Hello\"}]}"
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_UNKNOWN);
}
END_TEST

START_TEST(test_parse_no_content) {
    const char *json = "{"
                       "\"modelVersion\":\"gemini-2.5-flash\","
                       "\"candidates\":[{"
                       "\"finishReason\":\"STOP\""
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_uint_eq((unsigned int)resp->content_count, 0);
    ck_assert_ptr_null(resp->content_blocks);
}
END_TEST

START_TEST(test_parse_parts_not_array) {
    const char *json = "{"
                       "\"modelVersion\":\"gemini-2.5-flash\","
                       "\"candidates\":[{"
                       "\"content\":{\"parts\":\"not an array\"},"
                       "\"finishReason\":\"STOP\""
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_uint_eq((unsigned int)resp->content_count, 0);
    ck_assert_ptr_null(resp->content_blocks);
}
END_TEST

START_TEST(test_parse_with_provider_data) {
    const char *json = "{"
                       "\"modelVersion\":\"gemini-3\","
                       "\"candidates\":[{"
                       "\"content\":{\"parts\":[{\"text\":\"Hello\"}]},"
                       "\"finishReason\":\"STOP\""
                       "}],"
                       "\"thoughtSignature\":\"sig123\""
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_ptr_nonnull(resp->provider_data);
}
END_TEST

/* ================================================================
 * Stub Function Tests
 * ================================================================ */

static res_t dummy_completion_cb(const ik_provider_completion_t *completion, void *ctx)
{
    (void)completion;
    (void)ctx;
    return OK(NULL);
}

static res_t dummy_stream_cb(const ik_stream_event_t *event, void *ctx)
{
    (void)event;
    (void)ctx;
    return OK(NULL);
}

START_TEST(test_start_request_stub) {
    ik_request_t req = {0};
    void *impl_ctx = test_ctx;

    res_t result = ik_google_start_request(impl_ctx, &req, dummy_completion_cb, NULL);

    ck_assert(!is_err(&result));
}
END_TEST

START_TEST(test_start_stream_stub) {
    ik_request_t req = {0};
    void *impl_ctx = test_ctx;

    res_t result = ik_google_start_stream(impl_ctx, &req, dummy_stream_cb, NULL,
                                          dummy_completion_cb, NULL);

    ck_assert(!is_err(&result));
}
END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *google_response_main_coverage_suite(void)
{
    Suite *s = suite_create("Google Response Main Coverage");

    TCase *tc_parse = tcase_create("ik_google_parse_response edge cases");
    tcase_set_timeout(tc_parse, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_parse, setup, teardown);
    tcase_add_test(tc_parse, test_parse_invalid_json);
    tcase_add_test(tc_parse, test_parse_root_not_object);
    tcase_add_test(tc_parse, test_parse_error_with_null_message);
    tcase_add_test(tc_parse, test_parse_error_with_message);
    tcase_add_test(tc_parse, test_parse_blocked_prompt_null_reason);
    tcase_add_test(tc_parse, test_parse_blocked_prompt_with_reason);
    tcase_add_test(tc_parse, test_parse_promptfeedback_without_blockreason);
    tcase_add_test(tc_parse, test_parse_no_model_version);
    tcase_add_test(tc_parse, test_parse_model_version_not_string);
    tcase_add_test(tc_parse, test_parse_no_usage_metadata);
    tcase_add_test(tc_parse, test_parse_usage_missing_some_fields);
    tcase_add_test(tc_parse, test_parse_usage_all_fields_present);
    tcase_add_test(tc_parse, test_parse_usage_all_fields_null);
    tcase_add_test(tc_parse, test_parse_no_candidates_field);
    tcase_add_test(tc_parse, test_parse_content_without_parts_field);
    tcase_add_test(tc_parse, test_parse_candidates_not_array);
    tcase_add_test(tc_parse, test_parse_empty_candidates_array);
    tcase_add_test(tc_parse, test_parse_no_finish_reason);
    tcase_add_test(tc_parse, test_parse_no_content);
    tcase_add_test(tc_parse, test_parse_parts_not_array);
    tcase_add_test(tc_parse, test_parse_with_provider_data);
    suite_add_tcase(s, tc_parse);

    TCase *tc_stubs = tcase_create("Stub functions");
    tcase_set_timeout(tc_stubs, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_stubs, setup, teardown);
    tcase_add_test(tc_stubs, test_start_request_stub);
    tcase_add_test(tc_stubs, test_start_stream_stub);
    suite_add_tcase(s, tc_stubs);

    return s;
}

int main(void)
{
    Suite *s = google_response_main_coverage_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
