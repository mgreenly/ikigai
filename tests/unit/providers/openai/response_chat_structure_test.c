/**
 * @file response_chat_structure_test.c
 * @brief Coverage tests for OpenAI Chat response structure and error parsing
 *
 * Tests edge cases in response structure and error parsing.
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
 * Response Structure Tests
 * ================================================================ */

START_TEST(test_parse_choices_not_array) {
    /* choices field exists but is not an array - line 199 branch */
    const char *json = "{"
                       "\"id\":\"chatcmpl-test\","
                       "\"model\":\"gpt-4\","
                       "\"choices\":\"not_an_array\","
                       "\"usage\":{"
                       "\"prompt_tokens\":5,"
                       "\"completion_tokens\":0,"
                       "\"total_tokens\":5"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 0);
    ck_assert_ptr_null(resp->content_blocks);
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_UNKNOWN);
}

END_TEST

START_TEST(test_parse_choice_null) {
    /* First choice is null - line 220 branch (choice == NULL) */
    /* Note: yyjson_arr_get_first returns NULL for empty array, not for null element */
    /* This test covers the defensive check even though it may be unreachable */
    /* We'll test with empty array which returns NULL from yyjson_arr_get_first */
    const char *json = "{"
                       "\"id\":\"chatcmpl-test\","
                       "\"model\":\"gpt-4\","
                       "\"choices\":[],"
                       "\"usage\":{"
                       "\"prompt_tokens\":5,"
                       "\"completion_tokens\":0,"
                       "\"total_tokens\":5"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 0);
    ck_assert_ptr_null(resp->content_blocks);
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_UNKNOWN);
}

END_TEST

START_TEST(test_parse_message_null) {
    /* Message field missing from choice - line 238 branch (message == NULL) */
    const char *json = "{"
                       "\"id\":\"chatcmpl-test\","
                       "\"model\":\"gpt-4\","
                       "\"choices\":[{"
                       "\"index\":0,"
                       "\"finish_reason\":\"stop\""
                       "}],"
                       "\"usage\":{"
                       "\"prompt_tokens\":5,"
                       "\"completion_tokens\":0,"
                       "\"total_tokens\":5"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 0);
    ck_assert_ptr_null(resp->content_blocks);
}

END_TEST

START_TEST(test_parse_model_non_string) {
    /* Model field exists but is not a string - line 187 branch */
    const char *json = "{"
                       "\"id\":\"chatcmpl-test\","
                       "\"model\":123,"
                       "\"choices\":[{"
                       "\"index\":0,"
                       "\"message\":{"
                       "\"role\":\"assistant\","
                       "\"content\":\"Test\""
                       "},"
                       "\"finish_reason\":\"stop\""
                       "}],"
                       "\"usage\":{"
                       "\"prompt_tokens\":5,"
                       "\"completion_tokens\":0,"
                       "\"total_tokens\":5"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    /* model should be NULL when not a string */
    ck_assert_ptr_null(resp->model);
}

END_TEST

START_TEST(test_parse_content_non_string) {
    /* Content field exists but is not a string - line 252 branch */
    const char *json = "{"
                       "\"id\":\"chatcmpl-test\","
                       "\"model\":\"gpt-4\","
                       "\"choices\":[{"
                       "\"index\":0,"
                       "\"message\":{"
                       "\"role\":\"assistant\","
                       "\"content\":123"
                       "},"
                       "\"finish_reason\":\"stop\""
                       "}],"
                       "\"usage\":{"
                       "\"prompt_tokens\":5,"
                       "\"completion_tokens\":0,"
                       "\"total_tokens\":5"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    /* content_str is NULL, so no content blocks */
    ck_assert_int_eq((int)resp->content_count, 0);
    ck_assert_ptr_null(resp->content_blocks);
}

END_TEST

START_TEST(test_parse_finish_reason_non_string) {
    /* finish_reason field exists but is not a string - line 232 branch */
    const char *json = "{"
                       "\"id\":\"chatcmpl-test\","
                       "\"model\":\"gpt-4\","
                       "\"choices\":[{"
                       "\"index\":0,"
                       "\"message\":{"
                       "\"role\":\"assistant\","
                       "\"content\":\"Test\""
                       "},"
                       "\"finish_reason\":999"
                       "}],"
                       "\"usage\":{"
                       "\"prompt_tokens\":5,"
                       "\"completion_tokens\":0,"
                       "\"total_tokens\":5"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    /* finish_reason should be UNKNOWN when not a string */
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_UNKNOWN);
}

END_TEST

START_TEST(test_parse_tool_calls_not_array) {
    /* tool_calls field exists but is not an array - line 262 branch */
    const char *json = "{"
                       "\"id\":\"chatcmpl-test\","
                       "\"model\":\"gpt-4\","
                       "\"choices\":[{"
                       "\"index\":0,"
                       "\"message\":{"
                       "\"role\":\"assistant\","
                       "\"content\":\"Test\","
                       "\"tool_calls\":\"not_an_array\""
                       "},"
                       "\"finish_reason\":\"stop\""
                       "}],"
                       "\"usage\":{"
                       "\"prompt_tokens\":5,"
                       "\"completion_tokens\":0,"
                       "\"total_tokens\":5"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    /* Should have text content but no tool calls */
    ck_assert_int_eq((int)resp->content_count, 1);
    ck_assert_int_eq(resp->content_blocks[0].type, IK_CONTENT_TEXT);
}

END_TEST

/* ================================================================
 * Error Parsing Coverage Tests
 * ================================================================ */

START_TEST(test_parse_error_403_forbidden) {
    /* Test 403 Forbidden - maps to AUTH category */
    ik_error_category_t category;
    char *message = NULL;
    res_t result = ik_openai_parse_error(test_ctx, 403, NULL, 0,
                                         &category, &message);

    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_AUTH);
    ck_assert_str_eq(message, "HTTP 403");
}

END_TEST

START_TEST(test_parse_error_502_bad_gateway) {
    /* Test 502 Bad Gateway - maps to SERVER category */
    ik_error_category_t category;
    char *message = NULL;
    res_t result = ik_openai_parse_error(test_ctx, 502, NULL, 0,
                                         &category, &message);

    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
    ck_assert_str_eq(message, "HTTP 502");
}

END_TEST

START_TEST(test_parse_error_503_service_unavailable) {
    /* Test 503 Service Unavailable - maps to SERVER category */
    ik_error_category_t category;
    char *message = NULL;
    res_t result = ik_openai_parse_error(test_ctx, 503, NULL, 0,
                                         &category, &message);

    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
    ck_assert_str_eq(message, "HTTP 503");
}

END_TEST

START_TEST(test_parse_error_only_type) {
    /* Error with only type field - line 378 branch */
    const char *json = "{"
                       "\"error\":{"
                       "\"type\":\"api_error\""
                       "}"
                       "}";

    ik_error_category_t category;
    char *message = NULL;
    res_t result = ik_openai_parse_error(test_ctx, 500, json, strlen(json),
                                         &category, &message);

    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
    ck_assert_str_eq(message, "api_error");
}

END_TEST

START_TEST(test_parse_error_root_not_object) {
    /* JSON root is not an object - line 350 branch */
    const char *json = "[\"not\", \"an\", \"object\"]";

    ik_error_category_t category;
    char *message = NULL;
    res_t result = ik_openai_parse_error(test_ctx, 500, json, strlen(json),
                                         &category, &message);

    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
    ck_assert_str_eq(message, "HTTP 500");
}

END_TEST

START_TEST(test_parse_error_no_error_object) {
    /* Valid JSON but no error object - line 352 branch */
    const char *json = "{"
                       "\"id\":\"chatcmpl-test\","
                       "\"model\":\"gpt-4\""
                       "}";

    ik_error_category_t category;
    char *message = NULL;
    res_t result = ik_openai_parse_error(test_ctx, 500, json, strlen(json),
                                         &category, &message);

    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
    ck_assert_str_eq(message, "HTTP 500");
}

END_TEST

/* ================================================================
 * Test Suite
 * ================================================================ */

static Suite *response_chat_structure_suite(void)
{
    Suite *s = suite_create("OpenAI Chat Response Parsing - Structure");

    TCase *tc_structure = tcase_create("Response Structure");
    tcase_set_timeout(tc_structure, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_structure, setup, teardown);
    tcase_add_test(tc_structure, test_parse_choices_not_array);
    tcase_add_test(tc_structure, test_parse_choice_null);
    tcase_add_test(tc_structure, test_parse_message_null);
    tcase_add_test(tc_structure, test_parse_model_non_string);
    tcase_add_test(tc_structure, test_parse_content_non_string);
    tcase_add_test(tc_structure, test_parse_finish_reason_non_string);
    tcase_add_test(tc_structure, test_parse_tool_calls_not_array);
    suite_add_tcase(s, tc_structure);

    TCase *tc_error_coverage = tcase_create("Error Parsing Coverage");
    tcase_set_timeout(tc_error_coverage, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_error_coverage, setup, teardown);
    tcase_add_test(tc_error_coverage, test_parse_error_403_forbidden);
    tcase_add_test(tc_error_coverage, test_parse_error_502_bad_gateway);
    tcase_add_test(tc_error_coverage, test_parse_error_503_service_unavailable);
    tcase_add_test(tc_error_coverage, test_parse_error_only_type);
    tcase_add_test(tc_error_coverage, test_parse_error_root_not_object);
    tcase_add_test(tc_error_coverage, test_parse_error_no_error_object);
    suite_add_tcase(s, tc_error_coverage);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = response_chat_structure_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
