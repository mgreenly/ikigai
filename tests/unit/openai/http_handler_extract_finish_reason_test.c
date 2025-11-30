#include <check.h>
#include <talloc.h>
#include <string.h>
#include "openai/http_handler_internal.h"

/*
 * HTTP handler extract_finish_reason tests
 *
 * Tests the JSON parsing and validation error handling in
 * ik_openai_http_extract_finish_reason().
 *
 * This function is tested directly to cover all error paths including:
 * - Missing "data: " prefix
 * - Malformed JSON
 * - JSON root not an object
 * - Missing or invalid choices array
 * - Invalid choice[0] structure
 * - Missing or invalid finish_reason field
 */

/* Test fixtures */
static TALLOC_CTX *ctx = NULL;

static void setup(void)
{
    ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(ctx);
    ctx = NULL;
}

/*
 * Test: Event without "data: " prefix
 *
 * Tests line 48: if (strncmp(event, data_prefix, strlen(data_prefix)) != 0)
 */
START_TEST(test_extract_finish_reason_missing_prefix) {
    const char *event = "{\"choices\":[{\"delta\":{},\"finish_reason\":\"stop\"}]}";
    char *result = ik_openai_http_extract_finish_reason(ctx, event);
    ck_assert_ptr_null(result);
}
END_TEST
/*
 * Test: Event with [DONE] marker
 *
 * Should return NULL (not an error, just end of stream)
 */
START_TEST(test_extract_finish_reason_done_marker)
{
    const char *event = "data: [DONE]";
    char *result = ik_openai_http_extract_finish_reason(ctx, event);
    ck_assert_ptr_null(result);
}

END_TEST
/*
 * Test: Malformed JSON
 *
 * Tests line 62: if (!doc)
 */
START_TEST(test_extract_finish_reason_malformed_json)
{
    const char *event = "data: {\"malformed\": invalid json}";
    char *result = ik_openai_http_extract_finish_reason(ctx, event);
    ck_assert_ptr_null(result);
}

END_TEST
/*
 * Test: JSON root is an array, not an object
 *
 * Tests line 68: if (!root || !yyjson_is_obj(root))
 */
START_TEST(test_extract_finish_reason_root_not_object)
{
    const char *event = "data: [\"array\", \"not\", \"object\"]";
    char *result = ik_openai_http_extract_finish_reason(ctx, event);
    ck_assert_ptr_null(result);
}

END_TEST
/*
 * Test: JSON root is a string
 *
 * Tests line 68: !yyjson_is_obj(root) branch
 */
START_TEST(test_extract_finish_reason_root_string)
{
    const char *event = "data: \"just a string\"";
    char *result = ik_openai_http_extract_finish_reason(ctx, event);
    ck_assert_ptr_null(result);
}

END_TEST
/*
 * Test: JSON root is a number
 *
 * Tests line 68: !yyjson_is_obj(root) branch
 */
START_TEST(test_extract_finish_reason_root_number)
{
    const char *event = "data: 12345";
    char *result = ik_openai_http_extract_finish_reason(ctx, event);
    ck_assert_ptr_null(result);
}

END_TEST
/*
 * Test: Missing choices field
 *
 * Tests line 75: if (!choices ...)
 */
START_TEST(test_extract_finish_reason_missing_choices)
{
    const char *event = "data: {\"other_field\":\"value\"}";
    char *result = ik_openai_http_extract_finish_reason(ctx, event);
    ck_assert_ptr_null(result);
}

END_TEST
/*
 * Test: choices is not an array
 *
 * Tests line 75: !yyjson_is_arr(choices)
 */
START_TEST(test_extract_finish_reason_choices_not_array)
{
    const char *event = "data: {\"choices\":\"not_an_array\"}";
    char *result = ik_openai_http_extract_finish_reason(ctx, event);
    ck_assert_ptr_null(result);
}

END_TEST
/*
 * Test: choices is an empty array
 *
 * Tests line 75: yyjson_arr_size(choices) == 0
 */
START_TEST(test_extract_finish_reason_choices_empty)
{
    const char *event = "data: {\"choices\":[]}";
    char *result = ik_openai_http_extract_finish_reason(ctx, event);
    ck_assert_ptr_null(result);
}

END_TEST
/*
 * Test: choice[0] is null
 *
 * Tests line 81: if (!choice0 ...)
 */
START_TEST(test_extract_finish_reason_choice0_null)
{
    const char *event = "data: {\"choices\":[null]}";
    char *result = ik_openai_http_extract_finish_reason(ctx, event);
    ck_assert_ptr_null(result);
}

END_TEST
/*
 * Test: choice[0] is not an object (string)
 *
 * Tests line 81: !yyjson_is_obj(choice0)
 */
START_TEST(test_extract_finish_reason_choice0_not_object_string)
{
    const char *event = "data: {\"choices\":[\"not_an_object\"]}";
    char *result = ik_openai_http_extract_finish_reason(ctx, event);
    ck_assert_ptr_null(result);
}

END_TEST
/*
 * Test: choice[0] is not an object (number)
 *
 * Tests line 81: !yyjson_is_obj(choice0)
 */
START_TEST(test_extract_finish_reason_choice0_not_object_number)
{
    const char *event = "data: {\"choices\":[123]}";
    char *result = ik_openai_http_extract_finish_reason(ctx, event);
    ck_assert_ptr_null(result);
}

END_TEST
/*
 * Test: choice[0] is an object but missing finish_reason
 *
 * Tests line 87: if (!finish_reason_val ...)
 */
START_TEST(test_extract_finish_reason_missing_finish_reason_field)
{
    const char *event = "data: {\"choices\":[{\"delta\":{}}]}";
    char *result = ik_openai_http_extract_finish_reason(ctx, event);
    ck_assert_ptr_null(result);
}

END_TEST
/*
 * Test: finish_reason is not a string (number)
 *
 * Tests line 87: !yyjson_is_str(finish_reason_val)
 */
START_TEST(test_extract_finish_reason_not_string_number)
{
    const char *event = "data: {\"choices\":[{\"delta\":{},\"finish_reason\":456}]}";
    char *result = ik_openai_http_extract_finish_reason(ctx, event);
    ck_assert_ptr_null(result);
}

END_TEST
/*
 * Test: finish_reason is not a string (object)
 *
 * Tests line 87: !yyjson_is_str(finish_reason_val)
 */
START_TEST(test_extract_finish_reason_not_string_object)
{
    const char *event = "data: {\"choices\":[{\"delta\":{},\"finish_reason\":{\"nested\":\"object\"}}]}";
    char *result = ik_openai_http_extract_finish_reason(ctx, event);
    ck_assert_ptr_null(result);
}

END_TEST
/*
 * Test: finish_reason is not a string (array)
 *
 * Tests line 87: !yyjson_is_str(finish_reason_val)
 */
START_TEST(test_extract_finish_reason_not_string_array)
{
    const char *event = "data: {\"choices\":[{\"delta\":{},\"finish_reason\":[\"array\"]}]}";
    char *result = ik_openai_http_extract_finish_reason(ctx, event);
    ck_assert_ptr_null(result);
}

END_TEST
/*
 * Test: Valid finish_reason "stop"
 */
START_TEST(test_extract_finish_reason_valid_stop)
{
    const char *event = "data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"stop\"}]}";
    char *result = ik_openai_http_extract_finish_reason(ctx, event);
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(result, "stop");
}

END_TEST
/*
 * Test: Valid finish_reason "length"
 */
START_TEST(test_extract_finish_reason_valid_length)
{
    const char *event = "data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"length\"}]}";
    char *result = ik_openai_http_extract_finish_reason(ctx, event);
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(result, "length");
}

END_TEST
/*
 * Test: Valid finish_reason with content present
 */
START_TEST(test_extract_finish_reason_with_content)
{
    const char *event = "data: {\"choices\":[{\"delta\":{\"content\":\"text\"},\"finish_reason\":\"stop\"}]}";
    char *result = ik_openai_http_extract_finish_reason(ctx, event);
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(result, "stop");
}

END_TEST
/*
 * Test: Valid finish_reason empty string
 */
START_TEST(test_extract_finish_reason_empty_string)
{
    const char *event = "data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"\"}]}";
    char *result = ik_openai_http_extract_finish_reason(ctx, event);
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(result, "");
}

END_TEST
/*
 * Test: Valid finish_reason "tool_calls"
 */
START_TEST(test_extract_finish_reason_valid_tool_calls)
{
    const char *event = "data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"tool_calls\"}]}";
    char *result = ik_openai_http_extract_finish_reason(ctx, event);
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(result, "tool_calls");
}

END_TEST

/*
 * Test suite
 */
static Suite *http_handler_extract_finish_reason_suite(void)
{
    Suite *s = suite_create("HTTP Handler Extract Finish Reason");

    TCase *tc_validation = tcase_create("Validation");
    tcase_add_checked_fixture(tc_validation, setup, teardown);
    tcase_add_test(tc_validation, test_extract_finish_reason_missing_prefix);
    tcase_add_test(tc_validation, test_extract_finish_reason_done_marker);
    tcase_add_test(tc_validation, test_extract_finish_reason_malformed_json);
    tcase_add_test(tc_validation, test_extract_finish_reason_root_not_object);
    tcase_add_test(tc_validation, test_extract_finish_reason_root_string);
    tcase_add_test(tc_validation, test_extract_finish_reason_root_number);
    tcase_add_test(tc_validation, test_extract_finish_reason_missing_choices);
    tcase_add_test(tc_validation, test_extract_finish_reason_choices_not_array);
    tcase_add_test(tc_validation, test_extract_finish_reason_choices_empty);
    tcase_add_test(tc_validation, test_extract_finish_reason_choice0_null);
    tcase_add_test(tc_validation, test_extract_finish_reason_choice0_not_object_string);
    tcase_add_test(tc_validation, test_extract_finish_reason_choice0_not_object_number);
    tcase_add_test(tc_validation, test_extract_finish_reason_missing_finish_reason_field);
    tcase_add_test(tc_validation, test_extract_finish_reason_not_string_number);
    tcase_add_test(tc_validation, test_extract_finish_reason_not_string_object);
    tcase_add_test(tc_validation, test_extract_finish_reason_not_string_array);
    suite_add_tcase(s, tc_validation);

    TCase *tc_valid = tcase_create("Valid Cases");
    tcase_add_checked_fixture(tc_valid, setup, teardown);
    tcase_add_test(tc_valid, test_extract_finish_reason_valid_stop);
    tcase_add_test(tc_valid, test_extract_finish_reason_valid_length);
    tcase_add_test(tc_valid, test_extract_finish_reason_with_content);
    tcase_add_test(tc_valid, test_extract_finish_reason_empty_string);
    tcase_add_test(tc_valid, test_extract_finish_reason_valid_tool_calls);
    suite_add_tcase(s, tc_valid);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = http_handler_extract_finish_reason_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
