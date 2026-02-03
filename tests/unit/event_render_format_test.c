/**
 * @file event_render_format_test.c
 * @brief Unit tests for event render formatting helpers
 */

#include "../../src/event_render_format.h"
#include "../../src/output_style.h"

#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

static Suite *event_render_format_suite(void);

static TALLOC_CTX *test_ctx = NULL;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(test_ctx);
}

static void teardown(void)
{
    if (test_ctx != NULL) {
        talloc_free(test_ctx);
        test_ctx = NULL;
    }
}

// Test tool_call formatting when content is already formatted
START_TEST(test_format_tool_call_already_formatted)
{
    const char *tool_req_prefix = ik_output_prefix(IK_OUTPUT_TOOL_REQUEST);
    char *already_formatted = talloc_asprintf(test_ctx, "%s foo: bar=\"baz\"", tool_req_prefix);

    const char *result = ik_event_render_format_tool_call(test_ctx, already_formatted, "{}");

    // Should return the original content unchanged
    ck_assert_ptr_eq(result, already_formatted);
}
END_TEST

// Test tool_call formatting with NULL data_json
START_TEST(test_format_tool_call_null_data_json)
{
    const char *raw_content = "some raw content";
    const char *result = ik_event_render_format_tool_call(test_ctx, raw_content, NULL);

    // Should return the original content when no data_json
    ck_assert_ptr_eq(result, raw_content);
}
END_TEST

// Test tool_call formatting with invalid JSON in data_json
START_TEST(test_format_tool_call_invalid_json)
{
    const char *raw_content = "raw";
    const char *bad_json = "not valid json{";
    const char *result = ik_event_render_format_tool_call(test_ctx, raw_content, bad_json);

    // Should return the original content when JSON is invalid
    ck_assert_ptr_eq(result, raw_content);
}
END_TEST

// Test tool_call formatting with missing required fields
START_TEST(test_format_tool_call_missing_fields)
{
    const char *raw_content = "raw";
    // Missing tool_args field
    const char *incomplete_json = "{\"tool_call_id\":\"id123\",\"tool_name\":\"foo\"}";
    const char *result = ik_event_render_format_tool_call(test_ctx, raw_content, incomplete_json);

    // Should return the original content when required fields are missing
    ck_assert_ptr_eq(result, raw_content);
}
END_TEST

// Test tool_call formatting with valid data_json
START_TEST(test_format_tool_call_valid_data)
{
    const char *raw_content = "ignored";
    const char *data_json = "{\"tool_call_id\":\"id123\",\"tool_name\":\"glob\","
                           "\"tool_args\":\"{\\\"pattern\\\":\\\"*.c\\\"}\"}";

    const char *result = ik_event_render_format_tool_call(test_ctx, raw_content, data_json);

    // Should return formatted content
    ck_assert_ptr_ne(result, raw_content);
    ck_assert_str_eq(result, "→ glob: pattern=\"*.c\"");
}
END_TEST

// Test tool_result formatting when content is already formatted
START_TEST(test_format_tool_result_already_formatted)
{
    const char *tool_resp_prefix = ik_output_prefix(IK_OUTPUT_TOOL_RESPONSE);
    char *already_formatted = talloc_asprintf(test_ctx, "%s grep: found it", tool_resp_prefix);

    const char *result = ik_event_render_format_tool_result(test_ctx, already_formatted, "{}");

    // Should return the original content unchanged
    ck_assert_ptr_eq(result, already_formatted);
}
END_TEST

// Test tool_result formatting with NULL data_json
START_TEST(test_format_tool_result_null_data_json)
{
    const char *raw_content = "some raw content";
    const char *result = ik_event_render_format_tool_result(test_ctx, raw_content, NULL);

    // Should return the original content when no data_json
    ck_assert_ptr_eq(result, raw_content);
}
END_TEST

// Test tool_result formatting with invalid JSON in data_json
START_TEST(test_format_tool_result_invalid_json)
{
    const char *raw_content = "raw";
    const char *bad_json = "not valid json{";
    const char *result = ik_event_render_format_tool_result(test_ctx, raw_content, bad_json);

    // Should return the original content when JSON is invalid
    ck_assert_ptr_eq(result, raw_content);
}
END_TEST

// Test tool_result formatting with missing tool name
START_TEST(test_format_tool_result_missing_name)
{
    const char *raw_content = "raw";
    // Missing name field
    const char *incomplete_json = "{\"output\":\"result data\"}";
    const char *result = ik_event_render_format_tool_result(test_ctx, raw_content, incomplete_json);

    // Should return the original content when tool name is missing
    ck_assert_ptr_eq(result, raw_content);
}
END_TEST

// Test tool_result formatting with valid data_json
START_TEST(test_format_tool_result_valid_data)
{
    const char *raw_content = "ignored";
    const char *data_json = "{\"name\":\"read\",\"output\":\"file contents here\"}";

    const char *result = ik_event_render_format_tool_result(test_ctx, raw_content, data_json);

    // Should return formatted content
    ck_assert_ptr_ne(result, raw_content);
    ck_assert(strstr(result, "← read:") != NULL);
    ck_assert(strstr(result, "file contents here") != NULL);
}
END_TEST

// Test tool_result formatting with NULL output
START_TEST(test_format_tool_result_null_output)
{
    const char *raw_content = "ignored";
    const char *data_json = "{\"name\":\"read\"}";

    const char *result = ik_event_render_format_tool_result(test_ctx, raw_content, data_json);

    // Should return formatted content with (no output)
    ck_assert_ptr_ne(result, raw_content);
    ck_assert(strstr(result, "← read:") != NULL);
    ck_assert(strstr(result, "(no output)") != NULL);
}
END_TEST

Suite *event_render_format_suite(void)
{
    Suite *s = suite_create("Event Render Format");

    TCase *tc_tool_call = tcase_create("Tool Call Formatting");
    tcase_add_checked_fixture(tc_tool_call, setup, teardown);
    tcase_add_test(tc_tool_call, test_format_tool_call_already_formatted);
    tcase_add_test(tc_tool_call, test_format_tool_call_null_data_json);
    tcase_add_test(tc_tool_call, test_format_tool_call_invalid_json);
    tcase_add_test(tc_tool_call, test_format_tool_call_missing_fields);
    tcase_add_test(tc_tool_call, test_format_tool_call_valid_data);
    suite_add_tcase(s, tc_tool_call);

    TCase *tc_tool_result = tcase_create("Tool Result Formatting");
    tcase_add_checked_fixture(tc_tool_result, setup, teardown);
    tcase_add_test(tc_tool_result, test_format_tool_result_already_formatted);
    tcase_add_test(tc_tool_result, test_format_tool_result_null_data_json);
    tcase_add_test(tc_tool_result, test_format_tool_result_invalid_json);
    tcase_add_test(tc_tool_result, test_format_tool_result_missing_name);
    tcase_add_test(tc_tool_result, test_format_tool_result_valid_data);
    tcase_add_test(tc_tool_result, test_format_tool_result_null_output);
    suite_add_tcase(s, tc_tool_result);

    return s;
}

int main(void)
{
    Suite *s = event_render_format_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
