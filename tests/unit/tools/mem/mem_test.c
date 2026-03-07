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

    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        snprintf(tool_path, sizeof(tool_path), "%s/libexec/mem-tool", cwd);
    } else {
        snprintf(tool_path, sizeof(tool_path), "libexec/mem-tool");
    }
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

// Helper: Run tool with input, optional env overrides, and capture output
static int32_t run_tool_env(const char *input, char **output, int32_t *exit_code,
                            const char *scheme, const char *host, const char *port,
                            const char *project)
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

        if (scheme != NULL) setenv("RALPH_REMEMBERS_SCHEME", scheme, 1);
        if (host != NULL) setenv("RALPH_REMEMBERS_HOST", host, 1);
        if (port != NULL) setenv("RALPH_REMEMBERS_PORT", port, 1);
        if (project != NULL) setenv("RALPH_REMEMBERS_PROJECT", project, 1);

        execl(tool_path, tool_path, (char *)NULL);
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

    char buffer[65536];
    ssize_t total_read = 0;
    ssize_t bytes_read;

    while ((bytes_read = read(pipe_out[0], buffer + total_read,
                              sizeof(buffer) - (size_t)total_read - 1)) > 0) {
        total_read += bytes_read;
    }
    close(pipe_out[0]);

    buffer[total_read] = '\0';
    *output = talloc_strdup(test_ctx, buffer);

    int32_t status;
    waitpid(pid, &status, 0);
    *exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

    return 0;
}

// Helper: Run tool with default env set to a non-listening port
static int32_t run_tool(const char *input, char **output, int32_t *exit_code)
{
    return run_tool_env(input, output, exit_code, "http", "127.0.0.1", "19999", "test/project");
}

// Test: --schema outputs valid JSON
START_TEST(test_schema_output) {
    int32_t pipe_out[2];
    ck_assert_int_ne(pipe(pipe_out), -1);

    pid_t pid = fork();
    ck_assert_int_ne(pid, -1);

    if (pid == 0) {
        close(pipe_out[0]);
        dup2(pipe_out[1], STDOUT_FILENO);
        close(pipe_out[1]);
        execl(tool_path, tool_path, "--schema", (char *)NULL);
        exit(127);
    }

    close(pipe_out[1]);

    char buffer[8192];
    ssize_t total_read = 0;
    ssize_t bytes_read;
    while ((bytes_read = read(pipe_out[0], buffer + total_read,
                              sizeof(buffer) - (size_t)total_read - 1)) > 0) {
        total_read += bytes_read;
    }
    close(pipe_out[0]);
    buffer[total_read] = '\0';

    int32_t status;
    waitpid(pid, &status, 0);
    ck_assert_int_eq(WEXITSTATUS(status), 0);

    ck_assert_msg(strstr(buffer, "\"name\":\"mem\"") != NULL, "Schema missing name field");
    ck_assert_msg(strstr(buffer, "\"action\"") != NULL, "Schema missing action property");
    ck_assert_msg(strstr(buffer, "\"create\"") != NULL, "Schema missing create enum value");
    ck_assert_msg(strstr(buffer, "\"get\"") != NULL, "Schema missing get enum value");
    ck_assert_msg(strstr(buffer, "\"list\"") != NULL, "Schema missing list enum value");
    ck_assert_msg(strstr(buffer, "\"delete\"") != NULL, "Schema missing delete enum value");
}
END_TEST

// Test: Missing env vars → JSON error
START_TEST(test_missing_env_vars) {
    char *output = NULL;
    int32_t exit_code = 0;

    int32_t result = run_tool_env("{\"action\":\"list\"}", &output, &exit_code,
                                  NULL, NULL, NULL, NULL);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);
    ck_assert_msg(strstr(output, "\"error\"") != NULL, "Missing error field");
    ck_assert_msg(strstr(output, "ERR_CONFIG") != NULL, "Wrong error code");
}
END_TEST

// Test: Missing body for create → JSON error
START_TEST(test_create_missing_body) {
    char *output = NULL;
    int32_t exit_code = 0;

    int32_t result = run_tool("{\"action\":\"create\"}", &output, &exit_code);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);
    ck_assert_msg(strstr(output, "\"error\"") != NULL, "Missing error field");
    ck_assert_msg(strstr(output, "ERR_PARAMS") != NULL, "Wrong error code");
}
END_TEST

// Test: Missing id for get → JSON error
START_TEST(test_get_missing_id) {
    char *output = NULL;
    int32_t exit_code = 0;

    int32_t result = run_tool("{\"action\":\"get\"}", &output, &exit_code);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);
    ck_assert_msg(strstr(output, "\"error\"") != NULL, "Missing error field");
    ck_assert_msg(strstr(output, "ERR_PARAMS") != NULL, "Wrong error code");
}
END_TEST

// Test: Missing id for delete → JSON error
START_TEST(test_delete_missing_id) {
    char *output = NULL;
    int32_t exit_code = 0;

    int32_t result = run_tool("{\"action\":\"delete\"}", &output, &exit_code);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);
    ck_assert_msg(strstr(output, "\"error\"") != NULL, "Missing error field");
    ck_assert_msg(strstr(output, "ERR_PARAMS") != NULL, "Wrong error code");
}
END_TEST

// Test: Connection refused (server not running) → JSON error
START_TEST(test_connection_refused) {
    char *output = NULL;
    int32_t exit_code = 0;

    // Port 19999 should not have anything listening in test environment
    int32_t result = run_tool("{\"action\":\"list\"}", &output, &exit_code);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);
    ck_assert_msg(strstr(output, "\"error\"") != NULL, "Missing error field");
    ck_assert_msg(strstr(output, "ERR_IO") != NULL, "Wrong error code");
}
END_TEST

// Test: list action connection refused
START_TEST(test_list_connection_refused) {
    char *output = NULL;
    int32_t exit_code = 0;

    int32_t result = run_tool("{\"action\":\"list\"}", &output, &exit_code);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);
    ck_assert_msg(strstr(output, "\"error\"") != NULL, "Missing error field");
}
END_TEST

// Test: create action connection refused (after params validation passes)
START_TEST(test_create_connection_refused) {
    char *output = NULL;
    int32_t exit_code = 0;

    int32_t result = run_tool("{\"action\":\"create\",\"body\":\"hello world\"}", &output,
                              &exit_code);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);
    ck_assert_msg(strstr(output, "\"error\"") != NULL, "Missing error field");
    ck_assert_msg(strstr(output, "ERR_IO") != NULL, "Wrong error code");
}
END_TEST

// Test: get action connection refused (after params validation passes)
START_TEST(test_get_connection_refused) {
    char *output = NULL;
    int32_t exit_code = 0;

    int32_t result = run_tool("{\"action\":\"get\",\"id\":\"some-uuid\"}", &output, &exit_code);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);
    ck_assert_msg(strstr(output, "\"error\"") != NULL, "Missing error field");
    ck_assert_msg(strstr(output, "ERR_IO") != NULL, "Wrong error code");
}
END_TEST

// Test: delete action connection refused (after params validation passes)
START_TEST(test_delete_connection_refused) {
    char *output = NULL;
    int32_t exit_code = 0;

    int32_t result = run_tool("{\"action\":\"delete\",\"id\":\"some-uuid\"}", &output, &exit_code);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);
    ck_assert_msg(strstr(output, "\"error\"") != NULL, "Missing error field");
    ck_assert_msg(strstr(output, "ERR_IO") != NULL, "Wrong error code");
}
END_TEST

static Suite *mem_suite(void)
{
    Suite *s = suite_create("Mem");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_core, setup, teardown);

    tcase_add_test(tc_core, test_schema_output);
    tcase_add_test(tc_core, test_missing_env_vars);
    tcase_add_test(tc_core, test_create_missing_body);
    tcase_add_test(tc_core, test_get_missing_id);
    tcase_add_test(tc_core, test_delete_missing_id);
    tcase_add_test(tc_core, test_connection_refused);
    tcase_add_test(tc_core, test_list_connection_refused);
    tcase_add_test(tc_core, test_create_connection_refused);
    tcase_add_test(tc_core, test_get_connection_refused);
    tcase_add_test(tc_core, test_delete_connection_refused);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = mem_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/tools/mem/mem_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
