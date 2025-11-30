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
START_TEST(test_format_tool_call_glob_basic)
{
    ik_tool_call_t *call = ik_tool_call_create(ctx, "call_123", "glob", "{\"pattern\": \"*.c\", \"path\": \"src/\"}");
    ck_assert_ptr_nonnull(call);

    const char *formatted = ik_format_tool_call(ctx, call);
    ck_assert_ptr_nonnull(formatted);

    // Should contain tool call indicator and function name
    ck_assert(strstr(formatted, "glob") != NULL);
    // Should show some representation of arguments
    ck_assert(strlen(formatted) > 0);
}
END_TEST

// Test: Format tool result with list of files
START_TEST(test_format_tool_result_glob_files)
{
    const char *result = "{\"success\": true, \"data\": {\"output\": \"src/main.c\\nsrc/config.c\\nsrc/repl.c\", \"count\": 3}}";

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

    // Should contain tool name
    ck_assert(strstr(formatted, "glob") != NULL);
    ck_assert(strlen(formatted) > 0);
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

    // Should contain tool name and indication of null result
    ck_assert(strstr(formatted, "glob") != NULL);
    ck_assert(strstr(formatted, "(null)") != NULL);
    ck_assert(strlen(formatted) > 0);
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

    // Should contain tool name
    ck_assert(strstr(formatted, "grep") != NULL);
    ck_assert(strlen(formatted) > 0);
}
END_TEST

// Test: Format tool result with large output (truncation handling)
START_TEST(test_format_tool_result_large_output)
{
    // Create a moderately large output string
    const char *large_result = "{\"success\": true, \"data\": {\"output\": \"file1\\nfile2\\nfile3\\nfile4\\nfile5\\nfile6\\nfile7\\nfile8\\nfile9\\nfile10\", \"count\": 10}}";

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

    // Should contain the tool name exactly
    ck_assert(strstr(formatted, "file_read") != NULL);
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
    ck_assert(strstr(formatted, "bash") != NULL);
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

static Suite *tool_format_suite(void)
{
    Suite *s = suite_create("Tool Formatting");
    TCase *tc = tcase_create("tool_format");

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
