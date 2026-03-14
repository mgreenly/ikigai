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
                            const char *url, const char *org, const char *repo,
                            const char *agent)
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

        if (url != NULL) setenv("RALPH_REMEMBERS_URL", url, 1);
        else unsetenv("RALPH_REMEMBERS_URL");
        if (org != NULL) setenv("PROJECT_ORG", org, 1);
        else unsetenv("PROJECT_ORG");
        if (repo != NULL) setenv("PROJECT_REPO", repo, 1);
        else unsetenv("PROJECT_REPO");
        if (agent != NULL) setenv("IKIGAI_AGENT_ID", agent, 1);
        else unsetenv("IKIGAI_AGENT_ID");

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

// Helper: Run tool with default env set to a non-listening URL
static int32_t run_tool(const char *input, char **output, int32_t *exit_code)
{
    return run_tool_env(input, output, exit_code, "http://127.0.0.1:19999",
                        "test-org", "test-repo", "test-agent-id");
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
    ck_assert_msg(strstr(buffer, "\"update\"") != NULL, "Schema missing update enum value");
    ck_assert_msg(strstr(buffer, "\"revisions\"") != NULL, "Schema missing revisions enum value");
    ck_assert_msg(strstr(buffer, "\"revision_get\"") != NULL, "Schema missing revision_get enum value");
    ck_assert_msg(strstr(buffer, "\"path\"") != NULL, "Schema missing path property");
    ck_assert_msg(strstr(buffer, "\"scope\"") != NULL, "Schema missing scope property");
    ck_assert_msg(strstr(buffer, "\"global\"") != NULL, "Schema missing global scope value");
    ck_assert_msg(strstr(buffer, "\"title\"") != NULL, "Schema missing title property");
    ck_assert_msg(strstr(buffer, "\"search\"") != NULL, "Schema missing search property");
    ck_assert_msg(strstr(buffer, "\"limit\"") != NULL, "Schema missing limit property");
    ck_assert_msg(strstr(buffer, "\"offset\"") != NULL, "Schema missing offset property");
    ck_assert_msg(strstr(buffer, "\"revision_id\"") != NULL, "Schema missing revision_id property");
    ck_assert_msg(strstr(buffer, "\"id\"") == NULL, "Schema should not contain id property");
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

// Test: Missing path for get → JSON error
START_TEST(test_get_missing_path) {
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

// Test: Missing path for delete → JSON error
START_TEST(test_delete_missing_path) {
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

// Test: update action missing body → JSON error
START_TEST(test_update_missing_body) {
    char *output = NULL;
    int32_t exit_code = 0;

    int32_t result = run_tool("{\"action\":\"update\",\"path\":\"notes/session.md\"}", &output,
                              &exit_code);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);
    ck_assert_msg(strstr(output, "\"error\"") != NULL, "Missing error field");
    ck_assert_msg(strstr(output, "ERR_PARAMS") != NULL, "Wrong error code");
}
END_TEST

// Test: update action missing path → JSON error
START_TEST(test_update_missing_path) {
    char *output = NULL;
    int32_t exit_code = 0;

    int32_t result = run_tool("{\"action\":\"update\",\"body\":\"new content\"}", &output,
                              &exit_code);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);
    ck_assert_msg(strstr(output, "\"error\"") != NULL, "Missing error field");
    ck_assert_msg(strstr(output, "ERR_PARAMS") != NULL, "Wrong error code");
}
END_TEST

// Test: revisions action missing path → JSON error
START_TEST(test_revisions_missing_path) {
    char *output = NULL;
    int32_t exit_code = 0;

    int32_t result = run_tool("{\"action\":\"revisions\"}", &output, &exit_code);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);
    ck_assert_msg(strstr(output, "\"error\"") != NULL, "Missing error field");
    ck_assert_msg(strstr(output, "ERR_PARAMS") != NULL, "Wrong error code");
}
END_TEST

// Test: revision_get action missing path → JSON error
START_TEST(test_revision_get_missing_path) {
    char *output = NULL;
    int32_t exit_code = 0;

    int32_t result = run_tool("{\"action\":\"revision_get\",\"revision_id\":\"abc123\"}", &output,
                              &exit_code);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);
    ck_assert_msg(strstr(output, "\"error\"") != NULL, "Missing error field");
    ck_assert_msg(strstr(output, "ERR_PARAMS") != NULL, "Wrong error code");
}
END_TEST

// Test: revision_get action missing revision_id → JSON error
START_TEST(test_revision_get_missing_revision_id) {
    char *output = NULL;
    int32_t exit_code = 0;

    int32_t result = run_tool("{\"action\":\"revision_get\",\"path\":\"notes/session.md\"}",
                              &output, &exit_code);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);
    ck_assert_msg(strstr(output, "\"error\"") != NULL, "Missing error field");
    ck_assert_msg(strstr(output, "ERR_PARAMS") != NULL, "Wrong error code");
}
END_TEST

// Test: all valid-param actions return ERR_IO when server not running
START_TEST(test_connection_refused_actions) {
    static const char * const inputs[] = {
        "{\"action\":\"list\"}",
        "{\"action\":\"list\",\"scope\":\"global\"}",
        "{\"action\":\"list\",\"search\":\"session notes\"}",
        "{\"action\":\"create\",\"body\":\"hello world\"}",
        "{\"action\":\"create\",\"path\":\"my/doc.md\",\"body\":\"content\"}",
        "{\"action\":\"get\",\"path\":\"notes/session.md\"}",
        "{\"action\":\"delete\",\"path\":\"notes/session.md\"}",
        "{\"action\":\"update\",\"path\":\"notes/session.md\",\"body\":\"new content\"}",
        "{\"action\":\"revisions\",\"path\":\"notes/session.md\"}",
        "{\"action\":\"revision_get\",\"path\":\"notes/session.md\",\"revision_id\":\"abc123\"}",
        NULL
    };

    for (int i = 0; inputs[i] != NULL; i++) {
        char *output = NULL;
        int32_t exit_code = 0;
        int32_t result = run_tool(inputs[i], &output, &exit_code);
        ck_assert_int_eq(result, 0);
        ck_assert_int_eq(exit_code, 0);
        ck_assert_ptr_nonnull(output);
        ck_assert_msg(strstr(output, "\"error\"") != NULL, "Missing error field");
        ck_assert_msg(strstr(output, "ERR_IO") != NULL, "Wrong error code");
    }
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
    tcase_add_test(tc_core, test_get_missing_path);
    tcase_add_test(tc_core, test_delete_missing_path);
    tcase_add_test(tc_core, test_update_missing_body);
    tcase_add_test(tc_core, test_update_missing_path);
    tcase_add_test(tc_core, test_revisions_missing_path);
    tcase_add_test(tc_core, test_revision_get_missing_path);
    tcase_add_test(tc_core, test_revision_get_missing_revision_id);
    tcase_add_test(tc_core, test_connection_refused_actions);

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
