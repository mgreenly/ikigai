/**
 * @file bash_test.c
 * @brief Unit tests for bash tool
 */
#include "../../test_constants.h"

#include <check.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <talloc.h>
#include <unistd.h>

static TALLOC_CTX *test_ctx;
static const char *tool_path = "libexec/ikigai/bash-tool";

static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

// Helper to run tool with optional --schema flag and get exit code + output
static int32_t run_tool_with_args(const char *arg1, const char *input, char *output, size_t output_size)
{
    int32_t pipe_in[2];
    int32_t pipe_out[2];

    if (pipe(pipe_in) == -1 || pipe(pipe_out) == -1) {
        return -1;
    }

    pid_t pid = fork();
    if (pid == -1) {
        close(pipe_in[0]);
        close(pipe_in[1]);
        close(pipe_out[0]);
        close(pipe_out[1]);
        return -1;
    }

    if (pid == 0) {
        close(pipe_in[1]);
        close(pipe_out[0]);
        dup2(pipe_in[0], STDIN_FILENO);
        dup2(pipe_out[1], STDOUT_FILENO);
        close(pipe_in[0]);
        close(pipe_out[1]);
        if (arg1 != NULL) {
            execl(tool_path, tool_path, arg1, (char *)NULL);
        } else {
            execl(tool_path, tool_path, (char *)NULL);
        }
        exit(127);
    }

    close(pipe_in[0]);
    close(pipe_out[1]);

    if (input != NULL) {
        size_t len = strlen(input);
        ssize_t written = write(pipe_in[1], input, len);
        (void)written;
    }
    close(pipe_in[1]);

    if (output != NULL && output_size > 0) {
        size_t total = 0;
        ssize_t n;
        while ((n = read(pipe_out[0], output + total, output_size - total - 1)) > 0) {
            total += (size_t)n;
            if (total >= output_size - 1) {
                break;
            }
        }
        output[total] = '\0';
    }
    close(pipe_out[0]);

    int32_t status;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

// Helper to run tool and get exit code + output
static int32_t run_tool(const char *input, char *output, size_t output_size)
{
    return run_tool_with_args(NULL, input, output, output_size);
}

// Helper to run tool and get exit code only
static int32_t run_tool_basic(const char *input)
{
    return run_tool(input, NULL, 0);
}

START_TEST(test_schema_output) {
    char output[4096];
    int32_t exit_code = run_tool_with_args("--schema", NULL, output, sizeof(output));
    ck_assert_int_eq(exit_code, 0);
    ck_assert_str_ne(output, "");
    ck_assert(strstr(output, "\"name\": \"bash\"") != NULL);
    ck_assert(strstr(output, "\"command\"") != NULL);
}
END_TEST

START_TEST(test_empty_input) {
    int32_t exit_code = run_tool_basic("");
    ck_assert_int_eq(exit_code, 1);
}
END_TEST

START_TEST(test_invalid_json) {
    int32_t exit_code = run_tool_basic("not json");
    ck_assert_int_eq(exit_code, 1);
}
END_TEST

START_TEST(test_missing_command_field) {
    int32_t exit_code = run_tool_basic("{\"foo\": \"bar\"}");
    ck_assert_int_eq(exit_code, 1);
}
END_TEST

START_TEST(test_command_not_string) {
    int32_t exit_code = run_tool_basic("{\"command\": 123}");
    ck_assert_int_eq(exit_code, 1);
}
END_TEST

START_TEST(test_simple_command) {
    char output[4096];
    int32_t exit_code = run_tool("{\"command\": \"echo hello\"}", output, sizeof(output));
    ck_assert_int_eq(exit_code, 0);
    ck_assert(strstr(output, "\"output\":\"hello\"") != NULL);
    ck_assert(strstr(output, "\"exit_code\":0") != NULL);
}
END_TEST

START_TEST(test_command_with_exit_code) {
    char output[4096];
    int32_t exit_code = run_tool("{\"command\": \"exit 42\"}", output, sizeof(output));
    ck_assert_int_eq(exit_code, 0);
    ck_assert(strstr(output, "\"exit_code\":42") != NULL);
}
END_TEST

START_TEST(test_command_with_multiline_output) {
    char output[4096];
    int32_t exit_code = run_tool("{\"command\": \"echo -e 'line1\\nline2\\nline3'\"}", output, sizeof(output));
    ck_assert_int_eq(exit_code, 0);
    ck_assert(strstr(output, "line1") != NULL);
    ck_assert(strstr(output, "line2") != NULL);
    ck_assert(strstr(output, "line3") != NULL);
}
END_TEST

START_TEST(test_command_with_special_chars) {
    char output[4096];
    int32_t exit_code = run_tool("{\"command\": \"echo \\\"hello world\\\"\"}", output, sizeof(output));
    ck_assert_int_eq(exit_code, 0);
    ck_assert(strstr(output, "hello world") != NULL);
}
END_TEST

START_TEST(test_popen_failure) {
    // Create a command that will cause popen to fail (invalid shell syntax that causes exec failure)
    // Using a path that doesn't exist should cause popen to fail
    char output[4096];
    int32_t exit_code = run_tool("{\"command\": \"/nonexistent/command/path\"}", output, sizeof(output));
    ck_assert_int_eq(exit_code, 0);
    ck_assert(strstr(output, "\"exit_code\":127") != NULL);
}
END_TEST

START_TEST(test_large_output) {
    // Generate large output (> 4KB to trigger buffer reallocation during read)
    char output[16384];
    int32_t exit_code = run_tool("{\"command\": \"seq 1 1000\"}", output, sizeof(output));
    ck_assert_int_eq(exit_code, 0);
    ck_assert(strstr(output, "\"exit_code\":0") != NULL);
}
END_TEST

START_TEST(test_output_exactly_buffer_size) {
    // Generate output that's exactly 4096 bytes to trigger the else branch of null-termination
    // seq 1 800 produces about 2288 bytes, need more
    // Use yes command with head to generate exactly 4096 bytes
    char output[16384];
    int32_t exit_code = run_tool("{\"command\": \"yes a | head -c 4096\"}", output, sizeof(output));
    ck_assert_int_eq(exit_code, 0);
    ck_assert(strstr(output, "\"exit_code\":0") != NULL);
}
END_TEST

START_TEST(test_large_input) {
    // Create a large JSON input (> 8KB to definitely trigger input buffer reallocation)
    // Build a string with over 8192 characters to trigger multiple reallocations
    char *pattern = talloc_strdup(test_ctx, "");
    for (int32_t i = 0; i < 200; i++) {
        pattern = talloc_asprintf_append_buffer(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    }
    char *large_cmd = talloc_asprintf(test_ctx, "{\"command\": \"echo '%s'\"}", pattern);
    char output[16384];
    int32_t exit_code = run_tool(large_cmd, output, sizeof(output));
    ck_assert_int_eq(exit_code, 0);
    ck_assert(strstr(output, "\"exit_code\":0") != NULL);
}
END_TEST

START_TEST(test_input_exactly_buffer_size) {
    // Create input that fills buffer to trigger reallocation for null termination
    // The buffer starts at 4096 bytes. We need to make fread return exactly 4096 on first read.
    // Build JSON that's exactly 4096 bytes: {"command": "echo 'X'"} where X fills to 4096
    // Overhead: {"command": "echo ''"} = 22 chars, so payload = 4096 - 22 = 4074
    char *pattern = talloc_strdup(test_ctx, "");
    for (int32_t i = 0; i < 4074; i++) {
        pattern = talloc_asprintf_append_buffer(pattern, "a");
    }
    char *cmd = talloc_asprintf(test_ctx, "{\"command\": \"echo '%s'\"}", pattern);
    // Verify we built exactly 4096 bytes
    size_t len = strlen(cmd);
    ck_assert_uint_eq(len, 4096);

    char output[8192];
    int32_t exit_code = run_tool(cmd, output, sizeof(output));
    ck_assert_int_eq(exit_code, 0);
    ck_assert(strstr(output, "\"exit_code\":0") != NULL);
}
END_TEST

static Suite *bash_tool_suite(void)
{
    Suite *s = suite_create("BashTool");
    TCase *tc_core = tcase_create("Core");

    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_schema_output);
    tcase_add_test(tc_core, test_empty_input);
    tcase_add_test(tc_core, test_invalid_json);
    tcase_add_test(tc_core, test_missing_command_field);
    tcase_add_test(tc_core, test_command_not_string);
    tcase_add_test(tc_core, test_simple_command);
    tcase_add_test(tc_core, test_command_with_exit_code);
    tcase_add_test(tc_core, test_command_with_multiline_output);
    tcase_add_test(tc_core, test_command_with_special_chars);
    tcase_add_test(tc_core, test_popen_failure);
    tcase_add_test(tc_core, test_large_output);
    tcase_add_test(tc_core, test_output_exactly_buffer_size);
    tcase_add_test(tc_core, test_large_input);
    tcase_add_test(tc_core, test_input_exactly_buffer_size);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = bash_tool_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/tools/bash_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
