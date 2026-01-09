#include "../../../test_constants.h"
/**
 * @file response_responses_branch_test.c
 * @brief Tests for uncovered branches in OpenAI Responses API parsing
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
 * Branch Coverage Tests
 * ================================================================ */

START_TEST(test_model_not_string) {
    // model field present but not a string (should be treated as NULL)
    const char *json = "{"
                       "\"model\":123,"
                       "\"status\":\"completed\","
                       "\"output\":[]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_ptr_null(resp->model);
}
END_TEST

START_TEST(test_call_id_not_string) {
    // call_id field present but not a string (fallback to id)
    const char *json = "{"
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"function_call\","
                       "\"id\":\"fallback-123\","
                       "\"call_id\":456,"
                       "\"name\":\"test_func\","
                       "\"arguments\":\"{}\""
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 1);
    ck_assert_str_eq(resp->content_blocks[0].data.tool_call.id, "fallback-123");
}
END_TEST

START_TEST(test_text_not_string) {
    // text field present but not a string (should skip block)
    const char *json = "{"
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"message\","
                       "\"content\":[{"
                       "\"type\":\"output_text\","
                       "\"text\":789"
                       "}]"
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 0);
}
END_TEST

START_TEST(test_refusal_not_string) {
    // refusal field present but not a string (should skip block)
    const char *json = "{"
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"message\","
                       "\"content\":[{"
                       "\"type\":\"refusal\","
                       "\"refusal\":999"
                       "}]"
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 0);
}
END_TEST

START_TEST(test_incomplete_details_empty) {
    // incomplete_details present but no reason field
    const char *json = "{"
                       "\"status\":\"incomplete\","
                       "\"incomplete_details\":{},"
                       "\"output\":[]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_LENGTH);
}
END_TEST

START_TEST(test_incomplete_details_reason_not_string) {
    // incomplete_details.reason present but not a string
    const char *json = "{"
                       "\"status\":\"incomplete\","
                       "\"incomplete_details\":{"
                       "\"reason\":123"
                       "},"
                       "\"output\":[]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_LENGTH);
}
END_TEST

START_TEST(test_status_not_string) {
    // status field present but not a string
    const char *json = "{"
                       "\"model\":\"gpt-4o\","
                       "\"status\":999,"
                       "\"output\":[]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_UNKNOWN);
}
END_TEST

START_TEST(test_mixed_valid_invalid_output_items) {
    // Mix of valid and invalid items in output array to test loop branches
    const char *json = "{"
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"foo\":\"bar\""  // No type field
                       "},{"
                       "\"type\":null"  // Null type
                       "},{"
                       "\"type\":123"  // Non-string type
                       "},{"
                       "\"type\":\"message\","
                       "\"content\":[{"
                       "\"type\":\"output_text\","
                       "\"text\":\"Valid\""
                       "}]"
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 1);
    ck_assert_str_eq(resp->content_blocks[0].data.text.text, "Valid");
}
END_TEST

START_TEST(test_mixed_valid_invalid_content_items) {
    // Mix of valid and invalid items in content array to test inner loop branches
    const char *json = "{"
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"message\","
                       "\"content\":[{"
                       "\"foo\":\"bar\""  // No type field
                       "},{"
                       "\"type\":null"  // Null type
                       "},{"
                       "\"type\":456"  // Non-string type
                       "},{"
                       "\"type\":\"output_text\","
                       "\"text\":\"First\""
                       "},{"
                       "\"type\":\"unknown_type\","  // Unknown type
                       "\"data\":\"ignored\""
                       "},{"
                       "\"type\":\"output_text\","
                       "\"text\":\"Second\""
                       "}]"
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 2);
    ck_assert_str_eq(resp->content_blocks[0].data.text.text, "First");
    ck_assert_str_eq(resp->content_blocks[1].data.text.text, "Second");
}
END_TEST

/* ================================================================
 * Test Suite
 * ================================================================ */

static Suite *response_responses_branch_suite(void)
{
    Suite *s = suite_create("OpenAI Responses API Branch Coverage Tests");

    TCase *tc_branches = tcase_create("Branch Coverage");
    tcase_set_timeout(tc_branches, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_branches, setup, teardown);
    tcase_add_test(tc_branches, test_model_not_string);
    tcase_add_test(tc_branches, test_call_id_not_string);
    tcase_add_test(tc_branches, test_text_not_string);
    tcase_add_test(tc_branches, test_refusal_not_string);
    tcase_add_test(tc_branches, test_incomplete_details_empty);
    tcase_add_test(tc_branches, test_incomplete_details_reason_not_string);
    tcase_add_test(tc_branches, test_status_not_string);
    tcase_add_test(tc_branches, test_mixed_valid_invalid_output_items);
    tcase_add_test(tc_branches, test_mixed_valid_invalid_content_items);
    suite_add_tcase(s, tc_branches);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = response_responses_branch_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
