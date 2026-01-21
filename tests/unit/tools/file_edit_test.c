#include <check.h>
#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static void run_tool_with_args(const char *arg1, const char *stdin_data, char **stdout_out, char **stderr_out,
                               int32_t *exit_code)
{
    int32_t stdin_pipe[2];
    int32_t stdout_pipe[2];
    int32_t stderr_pipe[2];
    const char *tool_path = "libexec/ikigai/file-edit-tool";

    if (pipe(stdin_pipe) == -1 || pipe(stdout_pipe) == -1 || pipe(stderr_pipe) == -1) {
        perror("pipe");
        exit(1);
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(1);
    }

    if (pid == 0) {
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);

        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);

        close(stdin_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        if (arg1 != NULL) {
            execl(tool_path, tool_path, arg1, (char *)NULL);
        } else {
            execl(tool_path, tool_path, (char *)NULL);
        }
        perror("execl");
        exit(127);
    }

    close(stdin_pipe[0]);
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);

    if (stdin_data != NULL) {
        ssize_t written = write(stdin_pipe[1], stdin_data, strlen(stdin_data));
        (void)written;
    }
    close(stdin_pipe[1]);

    char stdout_buf[8192] = {0};
    char stderr_buf[8192] = {0};
    size_t stdout_len = 0;
    size_t stderr_len = 0;

    while (1) {
        ssize_t n = read(stdout_pipe[0], stdout_buf + stdout_len, sizeof(stdout_buf) - stdout_len - 1);
        if (n <= 0) break;
        stdout_len += (size_t)n;
    }

    while (1) {
        ssize_t n = read(stderr_pipe[0], stderr_buf + stderr_len, sizeof(stderr_buf) - stderr_len - 1);
        if (n <= 0) break;
        stderr_len += (size_t)n;
    }

    close(stdout_pipe[0]);
    close(stderr_pipe[0]);

    int32_t status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status)) {
        *exit_code = WEXITSTATUS(status);
    } else {
        *exit_code = -1;
    }

    *stdout_out = strdup(stdout_buf);
    *stderr_out = strdup(stderr_buf);
}

START_TEST(test_schema_output) {
    char *stdout_out = NULL;
    char *stderr_out = NULL;
    int32_t exit_code;

    run_tool_with_args("--schema", NULL, &stdout_out, &stderr_out, &exit_code);

    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(strstr(stdout_out, "\"name\": \"file_edit\""));
    ck_assert_ptr_nonnull(strstr(stdout_out, "\"file_path\""));
    ck_assert_ptr_nonnull(strstr(stdout_out, "\"old_string\""));
    ck_assert_ptr_nonnull(strstr(stdout_out, "\"new_string\""));
    ck_assert_ptr_nonnull(strstr(stdout_out, "\"replace_all\""));

    free(stdout_out);
    free(stderr_out);
}
END_TEST

START_TEST(test_empty_input) {
    char *stdout_out = NULL;
    char *stderr_out = NULL;
    int32_t exit_code;

    run_tool_with_args(NULL, "", &stdout_out, &stderr_out, &exit_code);

    ck_assert_int_eq(exit_code, 1);
    ck_assert_ptr_nonnull(strstr(stderr_out, "empty input"));

    free(stdout_out);
    free(stderr_out);
}
END_TEST

START_TEST(test_invalid_json) {
    char *stdout_out = NULL;
    char *stderr_out = NULL;
    int32_t exit_code;

    run_tool_with_args(NULL, "{invalid json", &stdout_out, &stderr_out, &exit_code);

    ck_assert_int_eq(exit_code, 1);
    ck_assert_ptr_nonnull(strstr(stderr_out, "invalid JSON"));

    free(stdout_out);
    free(stderr_out);
}
END_TEST

START_TEST(test_missing_fields) {
    char *stdout_out, *stderr_out;
    int32_t exit_code;
    const char *inputs[] = {"{\"old_string\":\"foo\",\"new_string\":\"bar\"}",
                            "{\"file_path\":\"test.txt\",\"new_string\":\"bar\"}",
                            "{\"file_path\":\"test.txt\",\"old_string\":\"foo\"}"};
    const char *errors[] = {"file_path", "old_string", "new_string"};
    for (int32_t i = 0; i < 3; i++) {
        run_tool_with_args(NULL, inputs[i], &stdout_out, &stderr_out, &exit_code);
        ck_assert_int_eq(exit_code, 1);
        ck_assert_ptr_nonnull(strstr(stderr_out, errors[i]));
        free(stdout_out);
        free(stderr_out);
    }
}
END_TEST

START_TEST(test_empty_old_string) {
    char *stdout_out = NULL;
    char *stderr_out = NULL;
    int32_t exit_code;

    const char *input = "{\"file_path\":\"test.txt\",\"old_string\":\"\",\"new_string\":\"bar\"}";
    run_tool_with_args(NULL, input, &stdout_out, &stderr_out, &exit_code);

    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(strstr(stdout_out, "\"error_code\":\"INVALID_ARG\""));
    ck_assert_ptr_nonnull(strstr(stdout_out, "old_string cannot be empty"));

    free(stdout_out);
    free(stderr_out);
}
END_TEST

START_TEST(test_identical_strings) {
    char *stdout_out = NULL;
    char *stderr_out = NULL;
    int32_t exit_code;

    const char *input = "{\"file_path\":\"test.txt\",\"old_string\":\"foo\",\"new_string\":\"foo\"}";
    run_tool_with_args(NULL, input, &stdout_out, &stderr_out, &exit_code);

    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(strstr(stdout_out, "\"error_code\":\"INVALID_ARG\""));
    ck_assert_ptr_nonnull(strstr(stdout_out, "old_string and new_string are identical"));

    free(stdout_out);
    free(stderr_out);
}
END_TEST

START_TEST(test_file_not_found) {
    char *stdout_out = NULL;
    char *stderr_out = NULL;
    int32_t exit_code;

    const char *input =
        "{\"file_path\":\"/tmp/nonexistent_file_12345.txt\",\"old_string\":\"foo\",\"new_string\":\"bar\"}";
    run_tool_with_args(NULL, input, &stdout_out, &stderr_out, &exit_code);

    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(strstr(stdout_out, "\"error_code\":\"FILE_NOT_FOUND\""));

    free(stdout_out);
    free(stderr_out);
}
END_TEST

START_TEST(test_simple_replacement) {
    char *tempfile = strdup("/tmp/file_edit_test_XXXXXX");
    int32_t fd = mkstemp(tempfile);
    ck_assert_int_ge(fd, 0);

    const char *content = "Hello world";
    write(fd, content, strlen(content));
    close(fd);

    char input[256];
    snprintf(input,
             sizeof(input),
             "{\"file_path\":\"%s\",\"old_string\":\"world\",\"new_string\":\"universe\"}",
             tempfile);

    char *stdout_out = NULL;
    char *stderr_out = NULL;
    int32_t exit_code;

    run_tool_with_args(NULL, input, &stdout_out, &stderr_out, &exit_code);

    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(strstr(stdout_out, "\"output\":\"Replaced 1 occurrence"));
    ck_assert_ptr_nonnull(strstr(stdout_out, "\"replacements\":1"));

    FILE *fp = fopen(tempfile, "r");
    char result[256];
    fgets(result, sizeof(result), fp);
    fclose(fp);

    ck_assert_str_eq(result, "Hello universe");

    unlink(tempfile);
    free(tempfile);
    free(stdout_out);
    free(stderr_out);
}
END_TEST

START_TEST(test_string_not_found) {
    char *tempfile = strdup("/tmp/file_edit_test_XXXXXX");
    int32_t fd = mkstemp(tempfile);
    ck_assert_int_ge(fd, 0);

    const char *content = "Hello world";
    write(fd, content, strlen(content));
    close(fd);

    char input[256];
    snprintf(input, sizeof(input), "{\"file_path\":\"%s\",\"old_string\":\"missing\",\"new_string\":\"bar\"}",
             tempfile);

    char *stdout_out = NULL;
    char *stderr_out = NULL;
    int32_t exit_code;

    run_tool_with_args(NULL, input, &stdout_out, &stderr_out, &exit_code);

    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(strstr(stdout_out, "\"error_code\":\"NOT_FOUND\""));

    unlink(tempfile);
    free(tempfile);
    free(stdout_out);
    free(stderr_out);
}
END_TEST

START_TEST(test_not_unique_without_replace_all) {
    char *tempfile = strdup("/tmp/file_edit_test_XXXXXX");
    int32_t fd = mkstemp(tempfile);
    ck_assert_int_ge(fd, 0);

    const char *content = "foo foo foo";
    write(fd, content, strlen(content));
    close(fd);

    char input[256];
    snprintf(input, sizeof(input), "{\"file_path\":\"%s\",\"old_string\":\"foo\",\"new_string\":\"bar\"}", tempfile);

    char *stdout_out = NULL;
    char *stderr_out = NULL;
    int32_t exit_code;

    run_tool_with_args(NULL, input, &stdout_out, &stderr_out, &exit_code);

    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(strstr(stdout_out, "\"error_code\":\"NOT_UNIQUE\""));
    ck_assert_ptr_nonnull(strstr(stdout_out, "found 3 times"));

    unlink(tempfile);
    free(tempfile);
    free(stdout_out);
    free(stderr_out);
}
END_TEST

START_TEST(test_replace_all) {
    char *tempfile = strdup("/tmp/file_edit_test_XXXXXX");
    int32_t fd = mkstemp(tempfile);
    ck_assert_int_ge(fd, 0);

    const char *content = "foo foo foo";
    write(fd, content, strlen(content));
    close(fd);

    char input[256];
    snprintf(input, sizeof(input),
             "{\"file_path\":\"%s\",\"old_string\":\"foo\",\"new_string\":\"bar\",\"replace_all\":true}", tempfile);

    char *stdout_out = NULL;
    char *stderr_out = NULL;
    int32_t exit_code;

    run_tool_with_args(NULL, input, &stdout_out, &stderr_out, &exit_code);

    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(strstr(stdout_out, "\"replacements\":3"));

    FILE *fp = fopen(tempfile, "r");
    char result[256];
    fgets(result, sizeof(result), fp);
    fclose(fp);

    ck_assert_str_eq(result, "bar bar bar");

    unlink(tempfile);
    free(tempfile);
    free(stdout_out);
    free(stderr_out);
}
END_TEST

START_TEST(test_multiline_replacement) {
    char *tempfile = strdup("/tmp/file_edit_test_XXXXXX");
    int32_t fd = mkstemp(tempfile);
    ck_assert_int_ge(fd, 0);

    const char *content = "Line 1\nLine 2\nLine 3";
    write(fd, content, strlen(content));
    close(fd);

    char input[256];
    snprintf(input, sizeof(input), "{\"file_path\":\"%s\",\"old_string\":\"Line 2\",\"new_string\":\"Modified Line\"}",
             tempfile);

    char *stdout_out = NULL;
    char *stderr_out = NULL;
    int32_t exit_code;

    run_tool_with_args(NULL, input, &stdout_out, &stderr_out, &exit_code);

    ck_assert_int_eq(exit_code, 0);

    FILE *fp = fopen(tempfile, "r");
    char result[256];
    size_t len = fread(result, 1, sizeof(result) - 1, fp);
    result[len] = '\0';
    fclose(fp);

    ck_assert_str_eq(result, "Line 1\nModified Line\nLine 3");

    unlink(tempfile);
    free(tempfile);
    free(stdout_out);
    free(stderr_out);
}
END_TEST

START_TEST(test_special_characters) {
    char *tempfile = strdup("/tmp/file_edit_test_XXXXXX");
    int32_t fd = mkstemp(tempfile);
    ck_assert_int_ge(fd, 0);

    const char *content = "Line with \"quotes\" and \ttabs\t";
    write(fd, content, strlen(content));
    close(fd);

    char input[512];
    snprintf(input, sizeof(input),
             "{\"file_path\":\"%s\",\"old_string\":\"\\\"quotes\\\" and \\ttabs\\t\",\"new_string\":\"replaced\"}",
             tempfile);

    char *stdout_out = NULL;
    char *stderr_out = NULL;
    int32_t exit_code;

    run_tool_with_args(NULL, input, &stdout_out, &stderr_out, &exit_code);

    ck_assert_int_eq(exit_code, 0);

    FILE *fp = fopen(tempfile, "r");
    char result[256];
    fgets(result, sizeof(result), fp);
    fclose(fp);

    ck_assert_str_eq(result, "Line with replaced");

    unlink(tempfile);
    free(tempfile);
    free(stdout_out);
    free(stderr_out);
}
END_TEST

START_TEST(test_large_file) {
    char *tempfile = strdup("/tmp/file_edit_test_XXXXXX");
    int32_t fd = mkstemp(tempfile);
    ck_assert_int_ge(fd, 0);

    for (int32_t i = 0; i < 1000; i++) {
        write(fd, "Line of text\n", 13);
    }
    close(fd);

    char input[256];
    snprintf(input, sizeof(input),
             "{\"file_path\":\"%s\",\"old_string\":\"Line of text\",\"new_string\":\"Modified\",\"replace_all\":true}",
             tempfile);

    char *stdout_out = NULL;
    char *stderr_out = NULL;
    int32_t exit_code;

    run_tool_with_args(NULL, input, &stdout_out, &stderr_out, &exit_code);

    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(strstr(stdout_out, "\"replacements\":1000"));

    unlink(tempfile);
    free(tempfile);
    free(stdout_out);
    free(stderr_out);
}
END_TEST

START_TEST(test_large_input_json) {
    char *tempfile = strdup("/tmp/file_edit_test_XXXXXX");
    int32_t fd = mkstemp(tempfile);
    ck_assert_int_ge(fd, 0);

    const char *content = "foo";
    write(fd, content, strlen(content));
    close(fd);

    char *input = malloc(20000);
    snprintf(input, 20000, "{\"file_path\":\"%s\",\"old_string\":\"foo\",\"new_string\":\"", tempfile);
    size_t len = strlen(input);
    for (size_t i = 0; i < 10000; i++) {
        input[len++] = 'x';
    }
    strcpy(input + len, "\"}");

    char *stdout_out = NULL;
    char *stderr_out = NULL;
    int32_t exit_code;

    run_tool_with_args(NULL, input, &stdout_out, &stderr_out, &exit_code);

    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(strstr(stdout_out, "\"replacements\":1"));

    unlink(tempfile);
    free(tempfile);
    free(input);
    free(stdout_out);
    free(stderr_out);
}
END_TEST

START_TEST(test_permission_denied_read) {
    char *tempfile = strdup("/tmp/file_edit_test_XXXXXX");
    int32_t fd = mkstemp(tempfile);
    ck_assert_int_ge(fd, 0);

    const char *content = "foo bar";
    write(fd, content, strlen(content));
    close(fd);

    chmod(tempfile, 0000);

    char input[256];
    snprintf(input, sizeof(input), "{\"file_path\":\"%s\",\"old_string\":\"foo\",\"new_string\":\"baz\"}", tempfile);

    char *stdout_out = NULL;
    char *stderr_out = NULL;
    int32_t exit_code;

    run_tool_with_args(NULL, input, &stdout_out, &stderr_out, &exit_code);

    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(strstr(stdout_out, "\"error_code\":\"PERMISSION_DENIED\""));

    chmod(tempfile, 0644);
    unlink(tempfile);
    free(tempfile);
    free(stdout_out);
    free(stderr_out);
}
END_TEST

START_TEST(test_permission_denied_write) {
    char *tempfile = strdup("/tmp/file_edit_test_XXXXXX");
    int32_t fd = mkstemp(tempfile);
    ck_assert_int_ge(fd, 0);

    const char *content = "foo bar";
    write(fd, content, strlen(content));

    chmod(tempfile, 0444);
    close(fd);

    char input[256];
    snprintf(input, sizeof(input), "{\"file_path\":\"%s\",\"old_string\":\"foo\",\"new_string\":\"baz\"}", tempfile);

    char *stdout_out = NULL;
    char *stderr_out = NULL;
    int32_t exit_code;

    run_tool_with_args(NULL, input, &stdout_out, &stderr_out, &exit_code);

    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(strstr(stdout_out, "\"error_code\":\"PERMISSION_DENIED\""));

    chmod(tempfile, 0644);
    unlink(tempfile);
    free(tempfile);
    free(stdout_out);
    free(stderr_out);
}
END_TEST

static Suite *file_edit_suite(void)
{
    Suite *s = suite_create("file_edit");

    TCase *tc_core = tcase_create("Core");
    tcase_add_test(tc_core, test_schema_output);
    tcase_add_test(tc_core, test_empty_input);
    tcase_add_test(tc_core, test_invalid_json);
    tcase_add_test(tc_core, test_missing_fields);
    tcase_add_test(tc_core, test_empty_old_string);
    tcase_add_test(tc_core, test_identical_strings);
    tcase_add_test(tc_core, test_file_not_found);
    tcase_add_test(tc_core, test_simple_replacement);
    tcase_add_test(tc_core, test_string_not_found);
    tcase_add_test(tc_core, test_not_unique_without_replace_all);
    tcase_add_test(tc_core, test_replace_all);
    tcase_add_test(tc_core, test_multiline_replacement);
    tcase_add_test(tc_core, test_special_characters);
    tcase_add_test(tc_core, test_large_file);
    tcase_add_test(tc_core, test_large_input_json);
    tcase_add_test(tc_core, test_permission_denied_read);
    tcase_add_test(tc_core, test_permission_denied_write);
    suite_add_tcase(s, tc_core);

    return s;
}

int32_t main(void)
{
    Suite *s = file_edit_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/tools/file_edit_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int32_t number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
