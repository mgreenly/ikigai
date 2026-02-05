/**
 * @file event_render_format_test.c
 * @brief Unit tests for event render formatting helpers
 */

#include "apps/ikigai/event_render_format.h"
#include "apps/ikigai/output_style.h"

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

    // Should return formatted content with prefix and truncation applied
    ck_assert_ptr_ne(result, raw_content);
    const char *prefix = ik_output_prefix(IK_OUTPUT_TOOL_RESPONSE);
    ck_assert(strncmp(result, prefix, strlen(prefix)) == 0);
    ck_assert(strstr(result, "some raw content") != NULL);
}
END_TEST

// Test tool_result formatting with invalid JSON in data_json
START_TEST(test_format_tool_result_invalid_json)
{
    const char *raw_content = "raw";
    const char *bad_json = "not valid json{";
    const char *result = ik_event_render_format_tool_result(test_ctx, raw_content, bad_json);

    // Should return formatted content with prefix and truncation applied
    ck_assert_ptr_ne(result, raw_content);
    const char *prefix = ik_output_prefix(IK_OUTPUT_TOOL_RESPONSE);
    ck_assert(strncmp(result, prefix, strlen(prefix)) == 0);
    ck_assert(strstr(result, "raw") != NULL);
}
END_TEST

// Test tool_result formatting with missing tool name
START_TEST(test_format_tool_result_missing_name)
{
    const char *raw_content = "raw";
    // Missing name field
    const char *incomplete_json = "{\"output\":\"result data\"}";
    const char *result = ik_event_render_format_tool_result(test_ctx, raw_content, incomplete_json);

    // Should return formatted content with prefix and truncation applied
    ck_assert_ptr_ne(result, raw_content);
    const char *prefix = ik_output_prefix(IK_OUTPUT_TOOL_RESPONSE);
    ck_assert(strncmp(result, prefix, strlen(prefix)) == 0);
    ck_assert(strstr(result, "raw") != NULL);
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

// Test tool_call formatting with NULL content
START_TEST(test_format_tool_call_null_content)
{
    const char *data_json = "{\"tool_call_id\":\"id123\",\"tool_name\":\"glob\","
                           "\"tool_args\":\"{\\\"pattern\\\":\\\"*.c\\\"}\"}";

    const char *result = ik_event_render_format_tool_call(test_ctx, NULL, data_json);

    // Should return formatted content even with NULL content
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(result, "→ glob: pattern=\"*.c\"");
}
END_TEST

// Test tool_result formatting with NULL content
START_TEST(test_format_tool_result_null_content)
{
    const char *data_json = "{\"name\":\"read\",\"output\":\"result\"}";

    const char *result = ik_event_render_format_tool_result(test_ctx, NULL, data_json);

    // Should return formatted content even with NULL content
    ck_assert_ptr_nonnull(result);
    ck_assert(strstr(result, "← read:") != NULL);
    ck_assert(strstr(result, "result") != NULL);
}
END_TEST

// Test tool_call formatting with only tool_name missing
START_TEST(test_format_tool_call_name_null)
{
    const char *raw_content = "raw";
    const char *data_json = "{\"tool_call_id\":\"id123\",\"tool_args\":\"{}\"}";

    const char *result = ik_event_render_format_tool_call(test_ctx, raw_content, data_json);

    // Should return raw content when tool_name is missing
    ck_assert_ptr_eq(result, raw_content);
}
END_TEST

// Test tool_call formatting with only tool_args missing
START_TEST(test_format_tool_call_args_null)
{
    const char *raw_content = "raw";
    const char *data_json = "{\"tool_call_id\":\"id123\",\"tool_name\":\"foo\"}";

    const char *result = ik_event_render_format_tool_call(test_ctx, raw_content, data_json);

    // Should return raw content when tool_args is missing
    ck_assert_ptr_eq(result, raw_content);
}
END_TEST

// Test tool_call formatting with only tool_call_id missing
START_TEST(test_format_tool_call_id_null)
{
    const char *raw_content = "raw";
    const char *data_json = "{\"tool_name\":\"foo\",\"tool_args\":\"{}\"}";

    const char *result = ik_event_render_format_tool_call(test_ctx, raw_content, data_json);

    // Should return raw content when tool_call_id is missing
    ck_assert_ptr_eq(result, raw_content);
}
END_TEST

// Test tool_call formatting with wrong type for tool_name
START_TEST(test_format_tool_call_name_not_string)
{
    const char *raw_content = "raw";
    const char *data_json = "{\"tool_call_id\":\"id123\",\"tool_name\":123,\"tool_args\":\"{}\"}";

    const char *result = ik_event_render_format_tool_call(test_ctx, raw_content, data_json);

    // Should return raw content when tool_name is not a string
    ck_assert_ptr_eq(result, raw_content);
}
END_TEST

// Test tool_call formatting with wrong type for tool_args
START_TEST(test_format_tool_call_args_not_string)
{
    const char *raw_content = "raw";
    const char *data_json = "{\"tool_call_id\":\"id123\",\"tool_name\":\"foo\",\"tool_args\":123}";

    const char *result = ik_event_render_format_tool_call(test_ctx, raw_content, data_json);

    // Should return raw content when tool_args is not a string
    ck_assert_ptr_eq(result, raw_content);
}
END_TEST

// Test tool_call formatting with wrong type for tool_call_id
START_TEST(test_format_tool_call_id_not_string)
{
    const char *raw_content = "raw";
    const char *data_json = "{\"tool_call_id\":123,\"tool_name\":\"foo\",\"tool_args\":\"{}\"}";

    const char *result = ik_event_render_format_tool_call(test_ctx, raw_content, data_json);

    // Should return raw content when tool_call_id is not a string
    ck_assert_ptr_eq(result, raw_content);
}
END_TEST

// Test tool_result formatting with name not a string
START_TEST(test_format_tool_result_name_not_string)
{
    const char *raw_content = "raw";
    const char *data_json = "{\"name\":123,\"output\":\"result\"}";

    const char *result = ik_event_render_format_tool_result(test_ctx, raw_content, data_json);

    // Should return formatted content with prefix and truncation applied when name is not a string
    ck_assert_ptr_ne(result, raw_content);
    const char *prefix = ik_output_prefix(IK_OUTPUT_TOOL_RESPONSE);
    ck_assert(strncmp(result, prefix, strlen(prefix)) == 0);
    ck_assert(strstr(result, "raw") != NULL);
}
END_TEST

// Test tool_result formatting with output not a string
START_TEST(test_format_tool_result_output_not_string)
{
    const char *raw_content = "ignored";
    const char *data_json = "{\"name\":\"read\",\"output\":123}";

    const char *result = ik_event_render_format_tool_result(test_ctx, raw_content, data_json);

    // Should return formatted content with (no output) when output is not a string
    ck_assert_ptr_ne(result, raw_content);
    ck_assert(strstr(result, "← read:") != NULL);
    ck_assert(strstr(result, "(no output)") != NULL);
}
END_TEST

// Test tool_result raw formatting applies truncation to long content
START_TEST(test_format_tool_result_raw_truncates_long_content)
{
    // Create content longer than 400 chars
    char *long_content = talloc_array(test_ctx, char, 600);
    memset(long_content, 'x', 599);
    long_content[599] = '\0';

    const char *result = ik_event_render_format_tool_result_raw(test_ctx, long_content);

    // Should have prefix and be truncated with ...
    const char *prefix = ik_output_prefix(IK_OUTPUT_TOOL_RESPONSE);
    ck_assert(strncmp(result, prefix, strlen(prefix)) == 0);
    ck_assert(strstr(result, "...") != NULL);
    // Result should be shorter than original (prefix + truncated content + ...)
    ck_assert(strlen(result) < 500);
}
END_TEST

// Test tool_result raw formatting with NULL content
START_TEST(test_format_tool_result_raw_null_content)
{
    const char *result = ik_event_render_format_tool_result_raw(test_ctx, NULL);

    // Should return formatted content with (no output)
    const char *prefix = ik_output_prefix(IK_OUTPUT_TOOL_RESPONSE);
    ck_assert(strncmp(result, prefix, strlen(prefix)) == 0);
    ck_assert(strstr(result, "(no output)") != NULL);
}
END_TEST

// Test tool_result raw formatting truncates at 3 lines
START_TEST(test_format_tool_result_raw_truncates_at_lines)
{
    const char *content = "line1\nline2\nline3\nline4\nline5";
    const char *result = ik_event_render_format_tool_result_raw(test_ctx, content);

    // Should have prefix and be truncated with ...
    const char *prefix = ik_output_prefix(IK_OUTPUT_TOOL_RESPONSE);
    ck_assert(strncmp(result, prefix, strlen(prefix)) == 0);
    ck_assert(strstr(result, "...") != NULL);
    // Should contain first 3 lines but not line4
    ck_assert(strstr(result, "line1") != NULL);
    ck_assert(strstr(result, "line2") != NULL);
    ck_assert(strstr(result, "line3") != NULL);
    ck_assert(strstr(result, "line4") == NULL);
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
    tcase_add_test(tc_tool_call, test_format_tool_call_null_content);
    tcase_add_test(tc_tool_call, test_format_tool_call_name_null);
    tcase_add_test(tc_tool_call, test_format_tool_call_args_null);
    tcase_add_test(tc_tool_call, test_format_tool_call_id_null);
    tcase_add_test(tc_tool_call, test_format_tool_call_name_not_string);
    tcase_add_test(tc_tool_call, test_format_tool_call_args_not_string);
    tcase_add_test(tc_tool_call, test_format_tool_call_id_not_string);
    suite_add_tcase(s, tc_tool_call);

    TCase *tc_tool_result = tcase_create("Tool Result Formatting");
    tcase_add_checked_fixture(tc_tool_result, setup, teardown);
    tcase_add_test(tc_tool_result, test_format_tool_result_already_formatted);
    tcase_add_test(tc_tool_result, test_format_tool_result_null_data_json);
    tcase_add_test(tc_tool_result, test_format_tool_result_invalid_json);
    tcase_add_test(tc_tool_result, test_format_tool_result_missing_name);
    tcase_add_test(tc_tool_result, test_format_tool_result_valid_data);
    tcase_add_test(tc_tool_result, test_format_tool_result_null_output);
    tcase_add_test(tc_tool_result, test_format_tool_result_null_content);
    tcase_add_test(tc_tool_result, test_format_tool_result_name_not_string);
    tcase_add_test(tc_tool_result, test_format_tool_result_output_not_string);
    tcase_add_test(tc_tool_result, test_format_tool_result_raw_truncates_long_content);
    tcase_add_test(tc_tool_result, test_format_tool_result_raw_null_content);
    tcase_add_test(tc_tool_result, test_format_tool_result_raw_truncates_at_lines);
    suite_add_tcase(s, tc_tool_result);

    return s;
}

int main(void)
{
    Suite *s = event_render_format_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/event_render_format_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
