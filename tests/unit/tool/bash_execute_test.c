#include <check.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

#include "../../../src/error.h"
#include "../../../src/tool.h"
#include "../../../src/wrapper.h"
#include "../../test_utils.h"

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

    yyjson_doc *doc = NULL;
    yyjson_val *data = ik_test_tool_parse_success(res.ok, &doc);
    const char *output = ik_test_tool_get_output(data);
    ck_assert(strstr(output, "test") != NULL);
    ck_assert_int_eq(ik_test_tool_get_exit_code(data), 0);

    yyjson_doc_free(doc);
}
END_TEST
// Test: bash with command that returns non-zero exit code
START_TEST(test_bash_exec_nonzero_exit) {
    // Execute bash with false command (always returns 1)
    res_t res = ik_tool_exec_bash(ctx, "false");
    ck_assert(!res.is_err);

    yyjson_doc *doc = NULL;
    yyjson_val *data = ik_test_tool_parse_success(res.ok, &doc);
    ck_assert_int_ne(ik_test_tool_get_exit_code(data), 0);

    yyjson_doc_free(doc);
}

END_TEST
// Test: bash with command that has no output
START_TEST(test_bash_exec_no_output) {
    // Execute bash with true command (no output, exit 0)
    res_t res = ik_tool_exec_bash(ctx, "true");
    ck_assert(!res.is_err);

    yyjson_doc *doc = NULL;
    yyjson_val *data = ik_test_tool_parse_success(res.ok, &doc);
    const char *output = ik_test_tool_get_output(data);
    ck_assert_str_eq(output, "");
    ck_assert_int_eq(ik_test_tool_get_exit_code(data), 0);

    yyjson_doc_free(doc);
}

END_TEST
// Test: bash with multiline output
START_TEST(test_bash_exec_multiline_output) {
    // Execute bash with printf command
    res_t res = ik_tool_exec_bash(ctx, "printf 'line1\\nline2\\nline3'");
    ck_assert(!res.is_err);

    yyjson_doc *doc = NULL;
    yyjson_val *data = ik_test_tool_parse_success(res.ok, &doc);
    const char *output = ik_test_tool_get_output(data);

    // Verify output contains all three lines
    ck_assert(strstr(output, "line1") != NULL);
    ck_assert(strstr(output, "line2") != NULL);
    ck_assert(strstr(output, "line3") != NULL);
    ck_assert_int_eq(ik_test_tool_get_exit_code(data), 0);

    yyjson_doc_free(doc);
}

END_TEST
// Test: bash with stderr output
START_TEST(test_bash_exec_stderr_output) {
    // Execute bash command that writes to stderr (redirect to stdout with 2>&1)
    res_t res = ik_tool_exec_bash(ctx, "echo error >&2");
    ck_assert(!res.is_err);

    yyjson_doc *doc = NULL;
    yyjson_val *data = ik_test_tool_parse_success(res.ok, &doc);
    ck_assert_int_eq(ik_test_tool_get_exit_code(data), 0);

    yyjson_doc_free(doc);
}

END_TEST
// Test: bash with special characters in output
START_TEST(test_bash_exec_special_characters) {
    // Execute bash with special characters
    res_t res = ik_tool_exec_bash(ctx, "echo 'Hello World with quotes'");
    ck_assert(!res.is_err);

    yyjson_doc *doc = NULL;
    yyjson_val *data = ik_test_tool_parse_success(res.ok, &doc);
    const char *output = ik_test_tool_get_output(data);
    ck_assert(strstr(output, "Hello") != NULL);

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

    yyjson_doc *doc = NULL;
    const char *error = ik_test_tool_parse_error(res.ok, &doc);
    ck_assert(strstr(error, "Failed to execute") != NULL || strstr(error, "popen") != NULL);

    yyjson_doc_free(doc);
    mock_popen_should_fail = 0;
}
END_TEST
// Test: bash with very long output (triggers buffer reallocation)
START_TEST(test_bash_exec_long_output) {
    // Execute bash that generates long output (more than 4096 bytes to trigger realloc)
    // seq 1 2000 produces about 7800 bytes, which will trigger reallocation
    res_t res = ik_tool_exec_bash(ctx, "seq 1 2000");
    ck_assert(!res.is_err);

    yyjson_doc *doc = NULL;
    yyjson_val *data = ik_test_tool_parse_success(res.ok, &doc);
    const char *output = ik_test_tool_get_output(data);

    // Verify output contains first and last numbers
    ck_assert(strstr(output, "1") != NULL);
    ck_assert(strstr(output, "2000") != NULL);

    // Verify the output is long enough to have triggered reallocation
    ck_assert(strlen(output) > 4096);
    ck_assert_int_eq(ik_test_tool_get_exit_code(data), 0);

    yyjson_doc_free(doc);
}

END_TEST
// Test: pclose failure returns exit code 127
START_TEST(test_bash_exec_pclose_failure) {
    mock_pclose_should_fail = 1;

    res_t res = ik_tool_exec_bash(ctx, "echo test");
    ck_assert(!res.is_err);

    yyjson_doc *doc = NULL;
    yyjson_val *data = ik_test_tool_parse_success(res.ok, &doc);
    ck_assert_int_eq(ik_test_tool_get_exit_code(data), 127);

    yyjson_doc_free(doc);
}

END_TEST

// Test suite
static Suite *bash_execute_suite(void)
{
    Suite *s = suite_create("Bash Execution");

    TCase *tc_bash_exec = tcase_create("Bash Execution");
    tcase_set_timeout(tc_bash_exec, 30);
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
