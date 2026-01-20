#include "../../test_constants.h"

#include <check.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <talloc.h>
#include <unistd.h>

static TALLOC_CTX *test_ctx;
static const char *tool_path = "libexec/ikigai/web-search-brave-tool";

static void setup(void)
{
    test_ctx = talloc_new(NULL);
    unsetenv("BRAVE_API_KEY");
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

static int32_t run_tool(const char *input, char **output, const char *extra_arg)
{
    int32_t pipe_in[2];
    int32_t pipe_out[2];

    if (pipe(pipe_in) < 0 || pipe(pipe_out) < 0) {
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        return -1;
    }

    if (pid == 0) {
        close(pipe_in[1]);
        close(pipe_out[0]);

        dup2(pipe_in[0], STDIN_FILENO);
        dup2(pipe_out[1], STDOUT_FILENO);

        close(pipe_in[0]);
        close(pipe_out[1]);

        if (extra_arg != NULL) {
            execl(tool_path, tool_path, extra_arg, NULL);
        } else {
            execl(tool_path, tool_path, NULL);
        }
        exit(1);
    }

    close(pipe_in[0]);
    close(pipe_out[1]);

    if (input != NULL) {
        size_t input_len = strlen(input);
        write(pipe_in[1], input, input_len);
    }
    close(pipe_in[1]);

    char buffer[65536];
    ssize_t total = 0;
    ssize_t n;
    while ((n = read(pipe_out[0], buffer + total, sizeof(buffer) - (size_t)total - 1)) > 0) {
        total += n;
    }
    close(pipe_out[0]);
    buffer[total] = '\0';

    int32_t status;
    waitpid(pid, &status, 0);

    if (output != NULL) {
        *output = talloc_strdup(test_ctx, buffer);
    }

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }

    return -1;
}

START_TEST(test_schema_flag) {
    char *output = NULL;
    int32_t exit_code = run_tool(NULL, &output, "--schema");

    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);

    ck_assert(strstr(output, "\"name\": \"web_search_brave\"") != NULL);
    ck_assert(strstr(output, "\"description\"") != NULL);
    ck_assert(strstr(output, "\"parameters\"") != NULL);
    ck_assert(strstr(output, "\"query\"") != NULL);
    ck_assert(strstr(output, "\"count\"") != NULL);
    ck_assert(strstr(output, "\"offset\"") != NULL);
    ck_assert(strstr(output, "\"allowed_domains\"") != NULL);
    ck_assert(strstr(output, "\"blocked_domains\"") != NULL);
    ck_assert(strstr(output, "\"required\": [\"query\"]") != NULL);
}

END_TEST

START_TEST(test_empty_stdin) {
    char *output = NULL;
    int32_t exit_code = run_tool("", &output, NULL);

    ck_assert_int_eq(exit_code, 1);
}

END_TEST

START_TEST(test_invalid_json) {
    char *output = NULL;
    int32_t exit_code = run_tool("{invalid json", &output, NULL);

    ck_assert_int_eq(exit_code, 1);
}

END_TEST

START_TEST(test_missing_query) {
    char *output = NULL;
    int32_t exit_code = run_tool("{\"count\": 5}", &output, NULL);

    ck_assert_int_eq(exit_code, 1);
}

END_TEST

START_TEST(test_missing_credentials) {
    unsetenv("BRAVE_API_KEY");

    const char *home = getenv("HOME");
    if (home != NULL) {
        char *cred_path = talloc_asprintf(test_ctx, "%s/.config/ikigai/credentials.json", home);
        char *backup_path = talloc_asprintf(test_ctx, "%s.backup", cred_path);
        rename(cred_path, backup_path);

        char *output = NULL;
        int32_t exit_code = run_tool("{\"query\": \"test\"}", &output, NULL);

        rename(backup_path, cred_path);

        ck_assert_int_eq(exit_code, 0);
        ck_assert_ptr_nonnull(output);

        ck_assert(strstr(output, "\"success\": false") != NULL);
        ck_assert(strstr(output, "\"error_code\": \"AUTH_MISSING\"") != NULL);
        ck_assert(strstr(output, "\"_event\"") != NULL);
        ck_assert(strstr(output, "\"kind\": \"config_required\"") != NULL);
        ck_assert(strstr(output, "\"tool\": \"web_search_brave\"") != NULL);
        ck_assert(strstr(output, "\"credential\": \"api_key\"") != NULL);
        ck_assert(strstr(output, "\"signup_url\"") != NULL);
    }
}

END_TEST

START_TEST(test_credentials_from_file) {
    unsetenv("BRAVE_API_KEY");

    const char *home = getenv("HOME");
    if (home != NULL) {
        char *config_dir = talloc_asprintf(test_ctx, "%s/.config/ikigai", home);
        char *cred_path = talloc_asprintf(test_ctx, "%s/credentials.json", config_dir);
        char *backup_path = talloc_asprintf(test_ctx, "%s.backup.test", cred_path);

        rename(cred_path, backup_path);
        mkdir(config_dir, 0755);

        FILE *f = fopen(cred_path, "w");
        if (f != NULL) {
            fprintf(f, "{\"web_search\":{\"brave\":{\"api_key\":\"test_from_file\"}}}");
            fclose(f);

            char *output = NULL;
            int32_t exit_code = run_tool("{\"query\": \"test\"}", &output, NULL);

            ck_assert_int_eq(exit_code, 0);
            ck_assert_ptr_nonnull(output);

            unlink(cred_path);
            rename(backup_path, cred_path);
        }
    }
}

END_TEST

START_TEST(test_query_with_count) {
    setenv("BRAVE_API_KEY", "test_key", 1);

    char *output = NULL;
    int32_t exit_code = run_tool("{\"query\": \"test\", \"count\": 5}", &output, NULL);

    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);
}

END_TEST

START_TEST(test_query_with_offset) {
    setenv("BRAVE_API_KEY", "test_key", 1);

    char *output = NULL;
    int32_t exit_code = run_tool("{\"query\": \"test\", \"offset\": 10}", &output, NULL);

    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);
}

END_TEST

START_TEST(test_query_with_allowed_domains) {
    setenv("BRAVE_API_KEY", "test_key", 1);

    char *output = NULL;
    int32_t exit_code = run_tool("{\"query\": \"test\", \"allowed_domains\": [\"example.com\"]}", &output, NULL);

    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);
}

END_TEST

START_TEST(test_query_with_blocked_domains) {
    setenv("BRAVE_API_KEY", "test_key", 1);

    char *output = NULL;
    int32_t exit_code = run_tool("{\"query\": \"test\", \"blocked_domains\": [\"spam.com\"]}", &output, NULL);

    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);
}

END_TEST

START_TEST(test_query_with_all_params) {
    setenv("BRAVE_API_KEY", "test_key", 1);

    const char *input = "{"
                        "\"query\": \"test\", "
                        "\"count\": 5, "
                        "\"offset\": 10, "
                        "\"allowed_domains\": [\"example.com\"], "
                        "\"blocked_domains\": [\"spam.com\"]"
                        "}";

    char *output = NULL;
    int32_t exit_code = run_tool(input, &output, NULL);

    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);
}

END_TEST

START_TEST(test_invalid_query_type) {
    char *output = NULL;
    int32_t exit_code = run_tool("{\"query\": 123}", &output, NULL);

    ck_assert_int_eq(exit_code, 1);
}

END_TEST

START_TEST(test_large_input) {
    char *large_input = malloc(20000);
    strcpy(large_input, "{\"query\": \"test");
    size_t len = strlen(large_input);
    for (int32_t i = 0; i < 500; i++) {
        strcpy(large_input + len, " word");
        len += 5;
    }
    strcpy(large_input + len, "\"}");

    setenv("BRAVE_API_KEY", "test_key", 1);

    char *output = NULL;
    int32_t exit_code = run_tool(large_input, &output, NULL);

    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);

    free(large_input);
}
END_TEST

START_TEST(test_special_characters_in_query) {
    setenv("BRAVE_API_KEY", "test_key", 1);

    char *output = NULL;
    int32_t exit_code = run_tool("{\"query\": \"test & special < > \\\" ' chars\"}", &output, NULL);

    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(output);
}

END_TEST

static Suite *web_search_brave_suite(void)
{
    Suite *s = suite_create("WebSearchBrave");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_core, setup, teardown);

    tcase_add_test(tc_core, test_schema_flag);
    tcase_add_test(tc_core, test_empty_stdin);
    tcase_add_test(tc_core, test_invalid_json);
    tcase_add_test(tc_core, test_missing_query);
    tcase_add_test(tc_core, test_missing_credentials);
    tcase_add_test(tc_core, test_credentials_from_file);
    tcase_add_test(tc_core, test_query_with_count);
    tcase_add_test(tc_core, test_query_with_offset);
    tcase_add_test(tc_core, test_query_with_allowed_domains);
    tcase_add_test(tc_core, test_query_with_blocked_domains);
    tcase_add_test(tc_core, test_query_with_all_params);
    tcase_add_test(tc_core, test_invalid_query_type);
    tcase_add_test(tc_core, test_large_input);
    tcase_add_test(tc_core, test_special_characters_in_query);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = web_search_brave_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
