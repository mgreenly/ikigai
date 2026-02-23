#include "tests/test_constants.h"

#include <check.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <talloc.h>
#include <unistd.h>

// Test fixture
static TALLOC_CTX *test_ctx;
static char tool_path[PATH_MAX + 256];

static void setup(void)
{
    test_ctx = talloc_new(NULL);

    // Construct absolute path to tool binary
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        snprintf(tool_path, sizeof(tool_path), "%s/libexec/web-fetch-tool", cwd);
    } else {
        snprintf(tool_path, sizeof(tool_path), "libexec/web-fetch-tool");
    }
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

// Helper: Run tool with input and capture output
static int32_t run_tool(const char *input, char **output, int32_t *exit_code)
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
        // Child process
        close(pipe_in[1]);
        close(pipe_out[0]);

        dup2(pipe_in[0], STDIN_FILENO);
        dup2(pipe_out[1], STDOUT_FILENO);

        close(pipe_in[0]);
        close(pipe_out[1]);

        execl(tool_path, tool_path, (char *)NULL);
        exit(127);
    }

    // Parent process
    close(pipe_in[0]);
    close(pipe_out[1]);

    // Write input
    if (input != NULL) {
        size_t len = strlen(input);
        ssize_t written = write(pipe_in[1], input, len);
        (void)written;
    }
    close(pipe_in[1]);

    // Read output
    char buffer[65536];
    ssize_t total_read = 0;
    ssize_t bytes_read;

    while ((bytes_read = read(pipe_out[0], buffer + total_read, sizeof(buffer) - (size_t)total_read - 1)) > 0) {
        total_read += bytes_read;
    }
    close(pipe_out[0]);

    buffer[total_read] = '\0';
    *output = talloc_strdup(test_ctx, buffer);

    // Wait for child
    int32_t status;
    waitpid(pid, &status, 0);
    *exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

    return 0;
}

// Helper: Run tool with args and capture output
static int32_t run_tool_with_args(const char *arg, char **output, int32_t *exit_code)
{
    int32_t pipe_out[2];

    if (pipe(pipe_out) == -1) {
        return -1;
    }

    pid_t pid = fork();
    if (pid == -1) {
        close(pipe_out[0]);
        close(pipe_out[1]);
        return -1;
    }

    if (pid == 0) {
        // Child process
        close(pipe_out[0]);

        dup2(pipe_out[1], STDOUT_FILENO);
        close(pipe_out[1]);

        execl(tool_path, tool_path, arg, (char *)NULL);
        exit(127);
    }

    // Parent process
    close(pipe_out[1]);

    // Read output
    char buffer[65536];
    ssize_t total_read = 0;
    ssize_t bytes_read;

    while ((bytes_read = read(pipe_out[0], buffer + total_read, sizeof(buffer) - (size_t)total_read - 1)) > 0) {
        total_read += bytes_read;
    }
    close(pipe_out[0]);

    buffer[total_read] = '\0';
    *output = talloc_strdup(test_ctx, buffer);

    // Wait for child
    int32_t status;
    waitpid(pid, &status, 0);
    *exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

    return 0;
}

// Test: --schema flag returns valid JSON
START_TEST(test_schema_flag) {
    char *output = NULL;
    int32_t exit_code = 0;

    int32_t result = run_tool_with_args("--schema", &output, &exit_code);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);

    // Check for required schema fields
    ck_assert_msg(strstr(output, "\"name\"") != NULL, "Schema missing name field");
    ck_assert_msg(strstr(output, "web_fetch") != NULL, "Schema has wrong name");
    ck_assert_msg(strstr(output, "\"description\"") != NULL, "Schema missing description");
    ck_assert_msg(strstr(output, "\"parameters\"") != NULL, "Schema missing parameters");
    ck_assert_msg(strstr(output, "\"url\"") != NULL, "Schema missing url parameter");
    ck_assert_msg(strstr(output, "\"required\"") != NULL, "Schema missing required field");
    ck_assert_msg(strstr(output, "\"offset\"") != NULL, "Schema missing offset parameter");
    ck_assert_msg(strstr(output, "\"limit\"") != NULL, "Schema missing limit parameter");
}
END_TEST

// Test: Empty stdin returns exit 1
START_TEST(test_empty_stdin) {
    char *output = NULL;
    int32_t exit_code = 0;

    int32_t result = run_tool("", &output, &exit_code);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 1);
}
END_TEST

// Test: Invalid JSON returns exit 1
START_TEST(test_invalid_json) {
    char *output = NULL;
    int32_t exit_code = 0;

    int32_t result = run_tool("{invalid json", &output, &exit_code);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 1);
}
END_TEST

// Test: Missing URL field returns exit 1
START_TEST(test_missing_url_field) {
    char *output = NULL;
    int32_t exit_code = 0;

    int32_t result = run_tool("{\"foo\":\"bar\"}", &output, &exit_code);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 1);
}
END_TEST

// Test: URL field is not a string returns exit 1
START_TEST(test_url_not_string) {
    char *output = NULL;
    int32_t exit_code = 0;

    int32_t result = run_tool("{\"url\":123}", &output, &exit_code);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 1);
}
END_TEST

// Test: Non-integer offset parameter (should be ignored)
START_TEST(test_non_integer_offset) {
    char *output = NULL;
    int32_t exit_code = 0;
    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        ck_abort_msg("Failed to get current directory");
    }

    char input[8192];
    snprintf(input,
             sizeof(input),
             "{\"url\":\"file://%s/tests/fixtures/html/simple.html\",\"offset\":\"not_a_number\"}",
             cwd);

    int32_t result = run_tool(input, &output, &exit_code);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);

    // Should succeed and ignore the invalid offset
    ck_assert_msg(strstr(output, "\"url\"") != NULL, "Output should contain url field");
}
END_TEST

// Test: Non-integer limit parameter (should be ignored)
START_TEST(test_non_integer_limit) {
    char *output = NULL;
    int32_t exit_code = 0;
    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        ck_abort_msg("Failed to get current directory");
    }

    char input[8192];
    snprintf(input, sizeof(input), "{\"url\":\"file://%s/tests/fixtures/html/simple.html\",\"limit\":true}", cwd);

    int32_t result = run_tool(input, &output, &exit_code);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);

    // Should succeed and ignore the invalid limit
    ck_assert_msg(strstr(output, "\"url\"") != NULL, "Output should contain url field");
}
END_TEST

static Suite *web_fetch_input_suite(void)
{
    Suite *s = suite_create("WebFetchInput");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_core, setup, teardown);

    tcase_add_test(tc_core, test_schema_flag);
    tcase_add_test(tc_core, test_empty_stdin);
    tcase_add_test(tc_core, test_invalid_json);
    tcase_add_test(tc_core, test_missing_url_field);
    tcase_add_test(tc_core, test_url_not_string);
    tcase_add_test(tc_core, test_non_integer_offset);
    tcase_add_test(tc_core, test_non_integer_limit);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = web_fetch_input_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/tools/web_fetch/web_fetch_input_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
