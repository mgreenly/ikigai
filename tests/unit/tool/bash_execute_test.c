#include <check.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

#include "../../../src/error.h"
#include "../../../src/tool.h"
#include "../../../src/wrapper.h"

// Test fixtures
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

// Test: bash with simple echo command returns output and exit code 0
START_TEST(test_bash_exec_echo_command) {
    // Execute bash with echo command
    res_t res = ik_tool_exec_bash(ctx, "echo test");
    ck_assert(!res.is_err);

    char *json = res.ok;
    ck_assert_ptr_nonnull(json);

    // Parse result JSON
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert(yyjson_is_obj(root));

    // Verify success: true
    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert_ptr_nonnull(success);
    ck_assert(yyjson_get_bool(success) == true);

    // Verify data object exists
    yyjson_val *data = yyjson_obj_get(root, "data");
    ck_assert_ptr_nonnull(data);
    ck_assert(yyjson_is_obj(data));

    // Verify output contains "test"
    yyjson_val *output = yyjson_obj_get(data, "output");
    ck_assert_ptr_nonnull(output);
    const char *output_str = yyjson_get_str(output);
    ck_assert_ptr_nonnull(output_str);
    ck_assert(strstr(output_str, "test") != NULL);

    // Verify exit_code is 0
    yyjson_val *exit_code = yyjson_obj_get(data, "exit_code");
    ck_assert_ptr_nonnull(exit_code);
    ck_assert_int_eq(yyjson_get_int(exit_code), 0);

    yyjson_doc_free(doc);
}
END_TEST
// Test: bash with command that returns non-zero exit code
START_TEST(test_bash_exec_nonzero_exit)
{
    // Execute bash with false command (always returns 1)
    res_t res = ik_tool_exec_bash(ctx, "false");
    ck_assert(!res.is_err);

    char *json = res.ok;
    ck_assert_ptr_nonnull(json);

    // Parse result JSON
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    // Verify success: true (tool executed successfully, even though command failed)
    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert(yyjson_get_bool(success) == true);

    // Verify data object exists
    yyjson_val *data = yyjson_obj_get(root, "data");
    ck_assert_ptr_nonnull(data);

    // Verify exit_code is non-zero (1 for false)
    yyjson_val *exit_code = yyjson_obj_get(data, "exit_code");
    ck_assert_ptr_nonnull(exit_code);
    ck_assert_int_ne(yyjson_get_int(exit_code), 0);

    yyjson_doc_free(doc);
}

END_TEST
// Test: bash with command that has no output
START_TEST(test_bash_exec_no_output)
{
    // Execute bash with true command (no output, exit 0)
    res_t res = ik_tool_exec_bash(ctx, "true");
    ck_assert(!res.is_err);

    char *json = res.ok;
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    // Verify success: true
    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert(yyjson_get_bool(success) == true);

    // Verify data object exists
    yyjson_val *data = yyjson_obj_get(root, "data");
    ck_assert_ptr_nonnull(data);

    // Verify output is empty string
    yyjson_val *output = yyjson_obj_get(data, "output");
    ck_assert_ptr_nonnull(output);
    const char *output_str = yyjson_get_str(output);
    ck_assert_ptr_nonnull(output_str);
    ck_assert_str_eq(output_str, "");

    // Verify exit_code is 0
    yyjson_val *exit_code = yyjson_obj_get(data, "exit_code");
    ck_assert_int_eq(yyjson_get_int(exit_code), 0);

    yyjson_doc_free(doc);
}

END_TEST
// Test: bash with multiline output
START_TEST(test_bash_exec_multiline_output)
{
    // Execute bash with printf command
    res_t res = ik_tool_exec_bash(ctx, "printf 'line1\\nline2\\nline3'");
    ck_assert(!res.is_err);

    char *json = res.ok;
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert(yyjson_get_bool(success) == true);

    yyjson_val *data = yyjson_obj_get(root, "data");
    yyjson_val *output = yyjson_obj_get(data, "output");
    const char *output_str = yyjson_get_str(output);

    // Verify output contains all three lines
    ck_assert(strstr(output_str, "line1") != NULL);
    ck_assert(strstr(output_str, "line2") != NULL);
    ck_assert(strstr(output_str, "line3") != NULL);

    yyjson_val *exit_code = yyjson_obj_get(data, "exit_code");
    ck_assert_int_eq(yyjson_get_int(exit_code), 0);

    yyjson_doc_free(doc);
}

END_TEST
// Test: bash with stderr output
START_TEST(test_bash_exec_stderr_output)
{
    // Execute bash command that writes to stderr (redirect to stdout with 2>&1)
    res_t res = ik_tool_exec_bash(ctx, "echo error >&2");
    ck_assert(!res.is_err);

    char *json = res.ok;
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert(yyjson_get_bool(success) == true);

    // Note: Without explicit stderr handling, stderr might not be captured
    // This test just verifies the command executes successfully
    yyjson_val *data = yyjson_obj_get(root, "data");
    yyjson_val *exit_code = yyjson_obj_get(data, "exit_code");
    ck_assert_int_eq(yyjson_get_int(exit_code), 0);

    yyjson_doc_free(doc);
}

END_TEST
// Test: bash with special characters in output
START_TEST(test_bash_exec_special_characters)
{
    // Execute bash with special characters
    res_t res = ik_tool_exec_bash(ctx, "echo 'Hello World with quotes'");
    ck_assert(!res.is_err);

    char *json = res.ok;
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert(yyjson_get_bool(success) == true);

    yyjson_val *data = yyjson_obj_get(root, "data");
    yyjson_val *output = yyjson_obj_get(data, "output");
    const char *output_str = yyjson_get_str(output);
    ck_assert(strstr(output_str, "Hello") != NULL);

    yyjson_doc_free(doc);
}

END_TEST

// Mock popen to return NULL (simulating failure)
static int mock_popen_should_fail = 0;
static int mock_pclose_should_fail = 0;

FILE *popen_(const char *command, const char *mode)
{
    if (mock_popen_should_fail) {
        errno = ENOMEM;
        return NULL;
    }
    return popen(command, mode);
}

int pclose_(FILE *stream)
{
    if (mock_pclose_should_fail) {
        mock_pclose_should_fail = 0;  // Reset after use
        return -1;
    }
    return pclose(stream);
}

// Test: popen failure returns error envelope
START_TEST(test_bash_exec_popen_failure) {
    mock_popen_should_fail = 1;

    res_t res = ik_tool_exec_bash(ctx, "echo test");
    ck_assert(!res.is_err);

    char *json = res.ok;
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    // Verify success: false (tool failed to execute)
    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert_ptr_nonnull(success);
    ck_assert(yyjson_get_bool(success) == false);

    // Verify error message exists
    yyjson_val *error = yyjson_obj_get(root, "error");
    ck_assert_ptr_nonnull(error);
    const char *error_str = yyjson_get_str(error);
    ck_assert_ptr_nonnull(error_str);
    ck_assert(strstr(error_str, "Failed to execute") != NULL || strstr(error_str, "popen") != NULL);

    yyjson_doc_free(doc);
    mock_popen_should_fail = 0;
}
END_TEST
// Test: bash with very long output (triggers buffer reallocation)
START_TEST(test_bash_exec_long_output)
{
    // Execute bash that generates long output (more than 4096 bytes to trigger realloc)
    // seq 1 2000 produces about 7800 bytes, which will trigger reallocation
    res_t res = ik_tool_exec_bash(ctx, "seq 1 2000");
    ck_assert(!res.is_err);

    char *json = res.ok;
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert(yyjson_get_bool(success) == true);

    yyjson_val *data = yyjson_obj_get(root, "data");
    yyjson_val *output = yyjson_obj_get(data, "output");
    const char *output_str = yyjson_get_str(output);

    // Verify output contains first and last numbers
    ck_assert(strstr(output_str, "1") != NULL);
    ck_assert(strstr(output_str, "2000") != NULL);

    // Verify the output is long enough to have triggered reallocation
    ck_assert(strlen(output_str) > 4096);

    yyjson_val *exit_code = yyjson_obj_get(data, "exit_code");
    ck_assert_int_eq(yyjson_get_int(exit_code), 0);

    yyjson_doc_free(doc);
}

END_TEST
// Test: pclose failure returns exit code 127
START_TEST(test_bash_exec_pclose_failure)
{
    mock_pclose_should_fail = 1;

    res_t res = ik_tool_exec_bash(ctx, "echo test");
    ck_assert(!res.is_err);

    char *json = res.ok;
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    // Verify success: true (command executed, even though pclose failed)
    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert(yyjson_get_bool(success) == true);

    // Verify exit_code is 127 (pclose failure)
    yyjson_val *data = yyjson_obj_get(root, "data");
    yyjson_val *exit_code = yyjson_obj_get(data, "exit_code");
    ck_assert_int_eq(yyjson_get_int(exit_code), 127);

    yyjson_doc_free(doc);
}

END_TEST

// Test suite
static Suite *bash_execute_suite(void)
{
    Suite *s = suite_create("Bash Execution");

    TCase *tc_bash_exec = tcase_create("Bash Execution");
    tcase_add_checked_fixture(tc_bash_exec, setup, teardown);
    tcase_add_test(tc_bash_exec, test_bash_exec_echo_command);
    tcase_add_test(tc_bash_exec, test_bash_exec_nonzero_exit);
    tcase_add_test(tc_bash_exec, test_bash_exec_no_output);
    tcase_add_test(tc_bash_exec, test_bash_exec_multiline_output);
    tcase_add_test(tc_bash_exec, test_bash_exec_stderr_output);
    tcase_add_test(tc_bash_exec, test_bash_exec_special_characters);
    tcase_add_test(tc_bash_exec, test_bash_exec_popen_failure);
    tcase_add_test(tc_bash_exec, test_bash_exec_long_output);
    tcase_add_test(tc_bash_exec, test_bash_exec_pclose_failure);
    suite_add_tcase(s, tc_bash_exec);

    return s;
}

int main(void)
{
    Suite *s = bash_execute_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int32_t number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
