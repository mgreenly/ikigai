/**
 * @file tool_format_test.c
 * @brief Tests for formatting tool calls and results for display
 */

#include <check.h>
#include <string.h>
#include <talloc.h>

#include "../../../src/format.h"
#include "../../../src/tool.h"
#include "../../test_utils.h"

// Test fixture
static TALLOC_CTX *ctx;

static void setup(void)
{
    ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);
}

static void teardown(void)
{
    talloc_free(ctx);
}

// Test: Format simple tool call with basic arguments
START_TEST(test_format_tool_call_glob_basic) {
    ik_tool_call_t *call = ik_tool_call_create(ctx, "call_123", "glob", "{\"pattern\": \"*.c\", \"path\": \"src/\"}");
    ck_assert_ptr_nonnull(call);

    const char *formatted = ik_format_tool_call(ctx, call);
    ck_assert_ptr_nonnull(formatted);

    // Should use arrow prefix and contain tool name
    ck_assert(strstr(formatted, "→ glob:") != NULL);
    // Should contain formatted arguments
    ck_assert(strstr(formatted, "pattern=\"*.c\"") != NULL);
    ck_assert(strstr(formatted, "path=\"src/\"") != NULL);
}
END_TEST
// Test: Format tool result with list of files
START_TEST(test_format_tool_result_glob_files)
{
    const char *result =
        "{\"success\": true, \"data\": {\"output\": \"src/main.c\\nsrc/config.c\\nsrc/repl.c\", \"count\": 3}}";

    const char *formatted = ik_format_tool_result(ctx, "glob", result);
    ck_assert_ptr_nonnull(formatted);

    // Should contain file names or count
    ck_assert(strstr(formatted, "main.c") != NULL || strstr(formatted, "3") != NULL);
    ck_assert(strlen(formatted) > 0);
}

END_TEST
// Test: Format tool call with only required parameters
START_TEST(test_format_tool_call_minimal)
{
    ik_tool_call_t *call = ik_tool_call_create(ctx, "call_456", "glob", "{\"pattern\": \"*.h\"}");
    ck_assert_ptr_nonnull(call);

    const char *formatted = ik_format_tool_call(ctx, call);
    ck_assert_ptr_nonnull(formatted);

    // Should use arrow format with single argument
    ck_assert_str_eq(formatted, "→ glob: pattern=\"*.h\"");
}

END_TEST
// Test: Format empty tool result
START_TEST(test_format_tool_result_empty)
{
    const char *result = "{\"success\": true, \"data\": {\"output\": \"\", \"count\": 0}}";

    const char *formatted = ik_format_tool_result(ctx, "glob", result);
    ck_assert_ptr_nonnull(formatted);

    // Should handle empty result gracefully
    ck_assert(strlen(formatted) > 0);
}

END_TEST
// Test: Format tool result with NULL result_json
START_TEST(test_format_tool_result_null_result)
{
    const char *formatted = ik_format_tool_result(ctx, "glob", NULL);
    ck_assert_ptr_nonnull(formatted);

    // Should use arrow format with (no output)
    ck_assert_str_eq(formatted, "← glob: (no output)");
}

END_TEST
// Test: Format tool call with special characters in arguments
START_TEST(test_format_tool_call_special_chars)
{
    ik_tool_call_t *call = ik_tool_call_create(ctx, "call_789", "grep",
                                               "{\"pattern\": \"test.*error\", \"path\": \"src/\"}");
    ck_assert_ptr_nonnull(call);

    const char *formatted = ik_format_tool_call(ctx, call);
    ck_assert_ptr_nonnull(formatted);

    // Should use arrow format with special chars preserved
    ck_assert(strstr(formatted, "→ grep:") != NULL);
    ck_assert(strstr(formatted, "pattern=\"test.*error\"") != NULL);
    ck_assert(strstr(formatted, "path=\"src/\"") != NULL);
}

END_TEST
// Test: Format tool result with large output (truncation handling)
START_TEST(test_format_tool_result_large_output)
{
    // Create a moderately large output string
    const char *large_result =
        "{\"success\": true, \"data\": {\"output\": \"file1\\nfile2\\nfile3\\nfile4\\nfile5\\nfile6\\nfile7\\nfile8\\nfile9\\nfile10\", \"count\": 10}}";

    const char *formatted = ik_format_tool_result(ctx, "glob", large_result);
    ck_assert_ptr_nonnull(formatted);

    // Should handle large output gracefully
    ck_assert(strlen(formatted) > 0);
}

END_TEST
// Test: Format tool call preserves tool name
START_TEST(test_format_tool_call_preserves_name)
{
    ik_tool_call_t *call = ik_tool_call_create(ctx, "call_999", "file_read", "{\"path\": \"config.txt\"}");
    ck_assert_ptr_nonnull(call);

    const char *formatted = ik_format_tool_call(ctx, call);
    ck_assert_ptr_nonnull(formatted);

    // Should use arrow format with exact tool name
    ck_assert_str_eq(formatted, "→ file_read: path=\"config.txt\"");
}

END_TEST
// Test: Format tool result preserves tool name
START_TEST(test_format_tool_result_preserves_name)
{
    const char *result = "{\"success\": true, \"data\": {\"content\": \"file data\"}}";

    const char *formatted = ik_format_tool_result(ctx, "file_read", result);
    ck_assert_ptr_nonnull(formatted);

    // Result should be formatted successfully
    ck_assert(strlen(formatted) > 0);
}

END_TEST
// Test: Format tool call with different tool names
START_TEST(test_format_tool_call_different_names)
{
    // Test bash tool
    ik_tool_call_t *call = ik_tool_call_create(ctx, "call_bash", "bash", "{\"command\": \"ls\"}");
    ck_assert_ptr_nonnull(call);

    const char *formatted = ik_format_tool_call(ctx, call);
    ck_assert_ptr_nonnull(formatted);
    ck_assert_str_eq(formatted, "→ bash: command=\"ls\"");
}

END_TEST
// Test: Format tool result for different tool
START_TEST(test_format_tool_result_bash_tool)
{
    const char *result = "{\"success\": true, \"data\": {\"output\": \"some output\"}}";

    const char *formatted = ik_format_tool_result(ctx, "bash", result);
    ck_assert_ptr_nonnull(formatted);
    ck_assert(strstr(formatted, "bash") != NULL);
}

END_TEST
// Test: Format tool call with multiple arguments (order may vary)
START_TEST(test_format_tool_call_multiple_args)
{
    ik_tool_call_t *call = ik_tool_call_create(ctx, "call_456", "file_read",
                                               "{\"path\": \"/src/main.c\", \"offset\": 0, \"limit\": 100}");
    ck_assert_ptr_nonnull(call);

    const char *formatted = ik_format_tool_call(ctx, call);
    ck_assert_ptr_nonnull(formatted);

    // JSON object order may vary, check for key parts
    ck_assert(strstr(formatted, "→ file_read:") != NULL);
    ck_assert(strstr(formatted, "path=\"/src/main.c\"") != NULL);
    ck_assert(strstr(formatted, "offset=0") != NULL);
    ck_assert(strstr(formatted, "limit=100") != NULL);
}

END_TEST
// Test: Format tool call with no arguments (empty object)
START_TEST(test_format_tool_call_no_args)
{
    ik_tool_call_t *call = ik_tool_call_create(ctx, "call_789", "some_tool", "{}");
    ck_assert_ptr_nonnull(call);

    const char *formatted = ik_format_tool_call(ctx, call);
    ck_assert_ptr_nonnull(formatted);

    // Should just show tool name without colon
    ck_assert_str_eq(formatted, "→ some_tool");
}

END_TEST
// Test: Format tool call with empty string arguments
START_TEST(test_format_tool_call_null_args)
{
    ik_tool_call_t *call = ik_tool_call_create(ctx, "call_000", "tool_x", "");
    ck_assert_ptr_nonnull(call);

    const char *formatted = ik_format_tool_call(ctx, call);
    ck_assert_ptr_nonnull(formatted);

    // Should just show tool name without colon
    ck_assert_str_eq(formatted, "→ tool_x");
}

END_TEST
// Test: Format tool call with invalid JSON (fallback)
START_TEST(test_format_tool_call_invalid_json)
{
    ik_tool_call_t *call = ik_tool_call_create(ctx, "call_bad", "broken", "not valid json");
    ck_assert_ptr_nonnull(call);

    const char *formatted = ik_format_tool_call(ctx, call);
    ck_assert_ptr_nonnull(formatted);

    // Fallback: show raw arguments
    ck_assert_str_eq(formatted, "→ broken: not valid json");
}

END_TEST
// Test: Format tool call with boolean value
START_TEST(test_format_tool_call_bool_value)
{
    ik_tool_call_t *call = ik_tool_call_create(ctx, "call_bool", "file_write",
                                               "{\"path\": \"test.txt\", \"create\": true}");
    ck_assert_ptr_nonnull(call);

    const char *formatted = ik_format_tool_call(ctx, call);
    ck_assert_ptr_nonnull(formatted);

    ck_assert(strstr(formatted, "→ file_write:") != NULL);
    ck_assert(strstr(formatted, "path=\"test.txt\"") != NULL);
    ck_assert(strstr(formatted, "create=true") != NULL);
}

END_TEST
// Test: Format tool call with integer value
START_TEST(test_format_tool_call_int_value)
{
    ik_tool_call_t *call = ik_tool_call_create(ctx, "call_int", "tool", "{\"count\": 42}");
    ck_assert_ptr_nonnull(call);

    const char *formatted = ik_format_tool_call(ctx, call);
    ck_assert_ptr_nonnull(formatted);

    ck_assert_str_eq(formatted, "→ tool: count=42");
}

END_TEST
// Test: Format tool call with real/float value
START_TEST(test_format_tool_call_real_value)
{
    ik_tool_call_t *call = ik_tool_call_create(ctx, "call_real", "tool", "{\"ratio\": 3.14}");
    ck_assert_ptr_nonnull(call);

    const char *formatted = ik_format_tool_call(ctx, call);
    ck_assert_ptr_nonnull(formatted);

    ck_assert(strstr(formatted, "→ tool:") != NULL);
    ck_assert(strstr(formatted, "ratio=3.14") != NULL);
}

END_TEST
// Test: Format tool call with null value
START_TEST(test_format_tool_call_null_value)
{
    ik_tool_call_t *call = ik_tool_call_create(ctx, "call_null", "tool", "{\"value\": null}");
    ck_assert_ptr_nonnull(call);

    const char *formatted = ik_format_tool_call(ctx, call);
    ck_assert_ptr_nonnull(formatted);

    ck_assert_str_eq(formatted, "→ tool: value=null");
}

END_TEST
// Test: Format tool result - short string array
START_TEST(test_format_tool_result_short)
{
    const char *formatted = ik_format_tool_result(ctx, "glob", "[\"a.c\", \"b.c\"]");
    ck_assert_ptr_nonnull(formatted);
    ck_assert_str_eq(formatted, "← glob: a.c, b.c");
}

END_TEST
// Test: Format tool result - empty string
START_TEST(test_format_tool_result_empty_string)
{
    const char *formatted = ik_format_tool_result(ctx, "bash", "\"\"");
    ck_assert_ptr_nonnull(formatted);
    ck_assert_str_eq(formatted, "← bash: (no output)");
}

END_TEST
// Test: Format tool result - truncate by characters (>400 chars)
START_TEST(test_format_tool_result_truncate_chars)
{
    // Create a string > 400 chars
    char long_content[500];
    memset(long_content, 'x', 450);
    long_content[450] = '\0';
    char json[600];
    snprintf(json, sizeof(json), "\"%s\"", long_content);

    const char *formatted = ik_format_tool_result(ctx, "file_read", json);
    ck_assert_ptr_nonnull(formatted);

    // Should be truncated with ...
    // "← file_read: " is 13 chars, + 400 + "..." = 416 max
    ck_assert(strlen(formatted) <= 420);
    ck_assert_ptr_nonnull(strstr(formatted, "..."));
}

END_TEST
// Test: Format tool result - truncate by lines (>3 lines)
START_TEST(test_format_tool_result_truncate_lines)
{
    const char *formatted = ik_format_tool_result(ctx, "grep", "\"line1\\nline2\\nline3\\nline4\\nline5\"");
    ck_assert_ptr_nonnull(formatted);

    // Should show only 3 lines + ...
    ck_assert_ptr_nonnull(strstr(formatted, "← grep:"));
    ck_assert_ptr_nonnull(strstr(formatted, "line1"));
    ck_assert_ptr_nonnull(strstr(formatted, "line2"));
    ck_assert_ptr_nonnull(strstr(formatted, "line3"));
    ck_assert_ptr_null(strstr(formatted, "line4"));
    ck_assert_ptr_nonnull(strstr(formatted, "..."));
}

END_TEST
// Test: Format tool result - error object
START_TEST(test_format_tool_result_error_object)
{
    const char *formatted = ik_format_tool_result(ctx, "bash", "{\"error\": \"Command failed\", \"exit_code\": 1}");
    ck_assert_ptr_nonnull(formatted);

    // Should show JSON for objects
    ck_assert_ptr_nonnull(strstr(formatted, "← bash:"));
    ck_assert_ptr_nonnull(strstr(formatted, "error"));
}

END_TEST
// Test: Format tool result - array of strings joined with comma
START_TEST(test_format_tool_result_array_of_strings)
{
    const char *formatted = ik_format_tool_result(ctx, "glob", "[\"file1.c\", \"file2.c\", \"file3.c\"]");
    ck_assert_ptr_nonnull(formatted);
    ck_assert_str_eq(formatted, "← glob: file1.c, file2.c, file3.c");
}

END_TEST
// Test: Format tool result - exactly three lines (no truncation)
START_TEST(test_format_tool_result_exactly_three_lines)
{
    const char *formatted = ik_format_tool_result(ctx, "grep", "\"line1\\nline2\\nline3\"");
    ck_assert_ptr_nonnull(formatted);

    // Exactly 3 lines - no truncation needed
    ck_assert_ptr_nonnull(strstr(formatted, "line1"));
    ck_assert_ptr_nonnull(strstr(formatted, "line2"));
    ck_assert_ptr_nonnull(strstr(formatted, "line3"));
    ck_assert_ptr_null(strstr(formatted, "..."));
}

END_TEST
// Test: Format tool result - invalid JSON (fallback to raw)
START_TEST(test_format_tool_result_invalid_json)
{
    const char *formatted = ik_format_tool_result(ctx, "broken", "not json");
    ck_assert_ptr_nonnull(formatted);

    // Fallback to raw content
    ck_assert_str_eq(formatted, "← broken: not json");
}

END_TEST
// Test: Format tool result - simple string content
START_TEST(test_format_tool_result_simple_string)
{
    const char *formatted = ik_format_tool_result(ctx, "bash", "\"hello world\"");
    ck_assert_ptr_nonnull(formatted);
    ck_assert_str_eq(formatted, "← bash: hello world");
}

END_TEST

static Suite *tool_format_suite(void)
{
    Suite *s = suite_create("Tool Formatting");
    TCase *tc = tcase_create("tool_format");
    tcase_set_timeout(tc, 30);

    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_format_tool_call_glob_basic);
    tcase_add_test(tc, test_format_tool_result_glob_files);
    tcase_add_test(tc, test_format_tool_call_minimal);
    tcase_add_test(tc, test_format_tool_result_empty);
    tcase_add_test(tc, test_format_tool_result_null_result);
    tcase_add_test(tc, test_format_tool_call_special_chars);
    tcase_add_test(tc, test_format_tool_result_large_output);
    tcase_add_test(tc, test_format_tool_call_preserves_name);
    tcase_add_test(tc, test_format_tool_result_preserves_name);
    tcase_add_test(tc, test_format_tool_call_different_names);
    tcase_add_test(tc, test_format_tool_result_bash_tool);
    tcase_add_test(tc, test_format_tool_call_multiple_args);
    tcase_add_test(tc, test_format_tool_call_no_args);
    tcase_add_test(tc, test_format_tool_call_null_args);
    tcase_add_test(tc, test_format_tool_call_invalid_json);
    tcase_add_test(tc, test_format_tool_call_bool_value);
    tcase_add_test(tc, test_format_tool_call_int_value);
    tcase_add_test(tc, test_format_tool_call_real_value);
    tcase_add_test(tc, test_format_tool_call_null_value);
    tcase_add_test(tc, test_format_tool_result_short);
    tcase_add_test(tc, test_format_tool_result_empty_string);
    tcase_add_test(tc, test_format_tool_result_truncate_chars);
    tcase_add_test(tc, test_format_tool_result_truncate_lines);
    tcase_add_test(tc, test_format_tool_result_error_object);
    tcase_add_test(tc, test_format_tool_result_array_of_strings);
    tcase_add_test(tc, test_format_tool_result_exactly_three_lines);
    tcase_add_test(tc, test_format_tool_result_invalid_json);
    tcase_add_test(tc, test_format_tool_result_simple_string);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = tool_format_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
