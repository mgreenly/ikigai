#include "../../../test_constants.h"
/**
 * @file response_responses_error_test.c
 * @brief Tests for OpenAI Responses API error cases
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
 * Error Cases
 * ================================================================ */

START_TEST(test_parse_response_invalid_json) {
    const char *json = "{invalid json}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
    ck_assert_ptr_null(resp);
}

END_TEST

START_TEST(test_parse_response_not_object) {
    const char *json = "[\"array\", \"not\", \"object\"]";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
    ck_assert_ptr_null(resp);
}

END_TEST

START_TEST(test_parse_response_error_response) {
    const char *json = "{"
                       "\"error\":{"
                       "\"message\":\"Invalid API key\","
                       "\"type\":\"invalid_request_error\","
                       "\"code\":\"invalid_api_key\""
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
    ck_assert_ptr_null(resp);
}

END_TEST

START_TEST(test_parse_response_error_no_message) {
    const char *json = "{"
                       "\"error\":{"
                       "\"type\":\"error_type\""
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
    ck_assert_ptr_null(resp);
}

END_TEST

START_TEST(test_parse_response_error_message_not_string) {
    const char *json = "{"
                       "\"error\":{"
                       "\"message\":123"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
    ck_assert_ptr_null(resp);
}

END_TEST

START_TEST(test_parse_response_function_call_no_id) {
    const char *json = "{"
                       "\"id\":\"resp-noid\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"function_call\","
                       "\"name\":\"get_weather\","
                       "\"arguments\":\"{}\""
                       "}],"
                       "\"usage\":{"
                       "\"prompt_tokens\":5,"
                       "\"completion_tokens\":2,"
                       "\"total_tokens\":7"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
}

END_TEST

START_TEST(test_parse_response_function_call_id_not_string) {
    const char *json = "{"
                       "\"id\":\"resp-idnotstr\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"function_call\","
                       "\"id\":999,"
                       "\"name\":\"get_weather\","
                       "\"arguments\":\"{}\""
                       "}],"
                       "\"usage\":{"
                       "\"prompt_tokens\":5,"
                       "\"completion_tokens\":2,"
                       "\"total_tokens\":7"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
}

END_TEST

START_TEST(test_parse_response_function_call_no_name) {
    const char *json = "{"
                       "\"id\":\"resp-noname\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"function_call\","
                       "\"id\":\"call_123\","
                       "\"arguments\":\"{}\""
                       "}],"
                       "\"usage\":{"
                       "\"prompt_tokens\":5,"
                       "\"completion_tokens\":2,"
                       "\"total_tokens\":7"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
}

END_TEST

START_TEST(test_parse_response_function_call_name_not_string) {
    const char *json = "{"
                       "\"id\":\"resp-namenotstr\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"function_call\","
                       "\"id\":\"call_123\","
                       "\"name\":456,"
                       "\"arguments\":\"{}\""
                       "}],"
                       "\"usage\":{"
                       "\"prompt_tokens\":5,"
                       "\"completion_tokens\":2,"
                       "\"total_tokens\":7"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
}

END_TEST

START_TEST(test_parse_response_function_call_no_arguments) {
    const char *json = "{"
                       "\"id\":\"resp-noargs\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"function_call\","
                       "\"id\":\"call_123\","
                       "\"name\":\"get_weather\""
                       "}],"
                       "\"usage\":{"
                       "\"prompt_tokens\":5,"
                       "\"completion_tokens\":2,"
                       "\"total_tokens\":7"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
}

END_TEST

START_TEST(test_parse_response_function_call_arguments_not_string) {
    const char *json = "{"
                       "\"id\":\"resp-argsnotstr\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"function_call\","
                       "\"id\":\"call_123\","
                       "\"name\":\"get_weather\","
                       "\"arguments\":123"
                       "}],"
                       "\"usage\":{"
                       "\"prompt_tokens\":5,"
                       "\"completion_tokens\":2,"
                       "\"total_tokens\":7"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
}

END_TEST

/* ================================================================
 * Test Suite
 * ================================================================ */

static Suite *response_responses_error_suite(void)
{
    Suite *s = suite_create("OpenAI Responses API Error Cases");

    TCase *tc_errors = tcase_create("Error Cases");
    tcase_set_timeout(tc_errors, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_errors, setup, teardown);
    tcase_add_test(tc_errors, test_parse_response_invalid_json);
    tcase_add_test(tc_errors, test_parse_response_not_object);
    tcase_add_test(tc_errors, test_parse_response_error_response);
    tcase_add_test(tc_errors, test_parse_response_error_no_message);
    tcase_add_test(tc_errors, test_parse_response_error_message_not_string);
    tcase_add_test(tc_errors, test_parse_response_function_call_no_id);
    tcase_add_test(tc_errors, test_parse_response_function_call_id_not_string);
    tcase_add_test(tc_errors, test_parse_response_function_call_no_name);
    tcase_add_test(tc_errors, test_parse_response_function_call_name_not_string);
    tcase_add_test(tc_errors, test_parse_response_function_call_no_arguments);
    tcase_add_test(tc_errors, test_parse_response_function_call_arguments_not_string);
    suite_add_tcase(s, tc_errors);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = response_responses_error_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
