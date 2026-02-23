#include <check.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <talloc.h>
#include <unistd.h>

static TALLOC_CTX *test_ctx;
static const char *tool_path = "libexec/grep-tool";
static char test_dir[PATH_MAX];

static void setup(void)
{
    test_ctx = talloc_new(NULL);
    snprintf(test_dir, sizeof(test_dir), "/tmp/grep_test_%d", getpid());
    mkdir(test_dir, 0755);
}

static void teardown(void)
{
    talloc_free(test_ctx);
    char cmd[PATH_MAX + 32];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", test_dir);
    (void)system(cmd);
}

static int32_t run_tool(const char *input, char *output, size_t output_size)
{
    int32_t pipe_in[2], pipe_out[2];
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

START_TEST(test_schema_output) {
    int32_t pipe_out[2];
    ck_assert(pipe(pipe_out) == 0);

    pid_t pid = fork();
    ck_assert(pid != -1);

    if (pid == 0) {
        close(pipe_out[0]);
        dup2(pipe_out[1], STDOUT_FILENO);
        close(pipe_out[1]);
        execl(tool_path, tool_path, "--schema", (char *)NULL);
        exit(127);
    }

    close(pipe_out[1]);
    char output[2048];
    ssize_t nread = read(pipe_out[0], output, sizeof(output) - 1);
    ck_assert(nread > 0);
    output[nread] = '\0';
    close(pipe_out[0]);

    int32_t status;
    waitpid(pid, &status, 0);
    ck_assert_int_eq(WEXITSTATUS(status), 0);

    ck_assert(strstr(output, "\"name\":") != NULL);
    ck_assert(strstr(output, "\"grep\"") != NULL);
    ck_assert(strstr(output, "\"pattern\"") != NULL);
}
END_TEST

START_TEST(test_empty_input) {
    char output[1024];
    int32_t exit_code = run_tool("", output, sizeof(output));
    ck_assert_int_eq(exit_code, 1);
}
END_TEST

START_TEST(test_invalid_json) {
    char output[1024];
    int32_t exit_code = run_tool("{invalid json", output, sizeof(output));
    ck_assert_int_eq(exit_code, 1);
}
END_TEST

START_TEST(test_missing_pattern) {
    char output[1024];
    int32_t exit_code = run_tool("{\"glob\": \"*.c\"}", output, sizeof(output));
    ck_assert_int_eq(exit_code, 1);
}
END_TEST

START_TEST(test_invalid_pattern_type) {
    char output[1024];
    int32_t exit_code = run_tool("{\"pattern\": 123}", output, sizeof(output));
    ck_assert_int_eq(exit_code, 1);
}
END_TEST

START_TEST(test_invalid_regex) {
    char output[1024];
    int32_t exit_code = run_tool("{\"pattern\": \"[invalid\"}", output, sizeof(output));
    ck_assert_int_eq(exit_code, 0);
    ck_assert(strstr(output, "\"error\"") != NULL);
    ck_assert(strstr(output, "INVALID_PATTERN") != NULL);
}
END_TEST

START_TEST(test_simple_match) {
    char file_path[PATH_MAX + 32];
    snprintf(file_path, sizeof(file_path), "%s/test.txt", test_dir);
    FILE *f = fopen(file_path, "w");
    ck_assert(f != NULL);
    fprintf(f, "hello world\n");
    fprintf(f, "foo bar\n");
    fprintf(f, "hello again\n");
    fclose(f);

    char input[PATH_MAX + 256];
    snprintf(input, sizeof(input), "{\"pattern\": \"hello\", \"path\": \"%s\"}", test_dir);
    char output[2048];
    int32_t exit_code = run_tool(input, output, sizeof(output));
    ck_assert_int_eq(exit_code, 0);
    ck_assert(strstr(output, "\"count\":2") != NULL);
    ck_assert(strstr(output, "hello world") != NULL);
    ck_assert(strstr(output, "hello again") != NULL);
}
END_TEST

START_TEST(test_no_matches) {
    char file_path[PATH_MAX + 32];
    snprintf(file_path, sizeof(file_path), "%s/test.txt", test_dir);
    FILE *f = fopen(file_path, "w");
    ck_assert(f != NULL);
    fprintf(f, "foo bar\n");
    fclose(f);

    char input[PATH_MAX + 256];
    snprintf(input, sizeof(input), "{\"pattern\": \"nonexistent\", \"path\": \"%s\"}", test_dir);
    char output[2048];
    int32_t exit_code = run_tool(input, output, sizeof(output));
    ck_assert_int_eq(exit_code, 0);
    ck_assert(strstr(output, "\"count\":0") != NULL);
}
END_TEST

START_TEST(test_with_glob_filter) {
    char file1[PATH_MAX + 32], file2[PATH_MAX + 32];
    snprintf(file1, sizeof(file1), "%s/test.c", test_dir);
    snprintf(file2, sizeof(file2), "%s/test.txt", test_dir);

    FILE *f1 = fopen(file1, "w");
    ck_assert(f1 != NULL);
    fprintf(f1, "int main(void) {}\n");
    fclose(f1);

    FILE *f2 = fopen(file2, "w");
    ck_assert(f2 != NULL);
    fprintf(f2, "some text main here\n");
    fclose(f2);

    char input[PATH_MAX + 256];
    snprintf(input, sizeof(input), "{\"pattern\": \"main\", \"glob\": \"*.c\", \"path\": \"%s\"}",
             test_dir);
    char output[2048];
    int32_t exit_code = run_tool(input, output, sizeof(output));
    ck_assert_int_eq(exit_code, 0);
    ck_assert(strstr(output, "\"count\":1") != NULL);
    ck_assert(strstr(output, "test.c") != NULL);
    ck_assert(strstr(output, "test.txt") == NULL);
}
END_TEST

START_TEST(test_extended_regex) {
    char file_path[PATH_MAX + 32];
    snprintf(file_path, sizeof(file_path), "%s/test.txt", test_dir);
    FILE *f = fopen(file_path, "w");
    ck_assert(f != NULL);
    fprintf(f, "error: something\n");
    fprintf(f, "warning: something\n");
    fprintf(f, "info: something\n");
    fclose(f);

    char input[PATH_MAX + 256];
    snprintf(input, sizeof(input), "{\"pattern\": \"(error|warning)\", \"path\": \"%s\"}",
             test_dir);
    char output[2048];
    int32_t exit_code = run_tool(input, output, sizeof(output));
    ck_assert_int_eq(exit_code, 0);
    ck_assert(strstr(output, "\"count\":2") != NULL);
    ck_assert(strstr(output, "error:") != NULL);
    ck_assert(strstr(output, "warning:") != NULL);
}
END_TEST

START_TEST(test_multiple_files) {
    char file1[PATH_MAX + 32], file2[PATH_MAX + 32];
    snprintf(file1, sizeof(file1), "%s/file1.txt", test_dir);
    snprintf(file2, sizeof(file2), "%s/file2.txt", test_dir);

    FILE *f1 = fopen(file1, "w");
    ck_assert(f1 != NULL);
    fprintf(f1, "match here\n");
    fclose(f1);

    FILE *f2 = fopen(file2, "w");
    ck_assert(f2 != NULL);
    fprintf(f2, "another match\n");
    fclose(f2);

    char input[PATH_MAX + 256];
    snprintf(input, sizeof(input), "{\"pattern\": \"match\", \"path\": \"%s\"}", test_dir);
    char output[2048];
    int32_t exit_code = run_tool(input, output, sizeof(output));
    ck_assert_int_eq(exit_code, 0);
    ck_assert(strstr(output, "\"count\":2") != NULL);
}
END_TEST

START_TEST(test_line_numbers) {
    char file_path[PATH_MAX + 32];
    snprintf(file_path, sizeof(file_path), "%s/test.txt", test_dir);
    FILE *f = fopen(file_path, "w");
    ck_assert(f != NULL);
    fprintf(f, "line one\n");
    fprintf(f, "line two match\n");
    fprintf(f, "line three\n");
    fprintf(f, "line four match\n");
    fclose(f);

    char input[PATH_MAX + 256];
    snprintf(input, sizeof(input), "{\"pattern\": \"match\", \"path\": \"%s\"}", test_dir);
    char output[2048];
    int32_t exit_code = run_tool(input, output, sizeof(output));
    ck_assert_int_eq(exit_code, 0);
    ck_assert(strstr(output, ":2:") != NULL);
    ck_assert(strstr(output, ":4:") != NULL);
}
END_TEST

START_TEST(test_special_chars_in_pattern) {
    char file_path[PATH_MAX + 32];
    snprintf(file_path, sizeof(file_path), "%s/test.txt", test_dir);
    FILE *f = fopen(file_path, "w");
    ck_assert(f != NULL);
    fprintf(f, "foo.bar\n");
    fprintf(f, "fooXbar\n");
    fclose(f);

    char input[PATH_MAX + 256];
    snprintf(input, sizeof(input), "{\"pattern\": \"foo\\\\.bar\", \"path\": \"%s\"}", test_dir);
    char output[2048];
    int32_t exit_code = run_tool(input, output, sizeof(output));
    ck_assert_int_eq(exit_code, 0);
    ck_assert(strstr(output, "\"count\":1") != NULL);
    ck_assert(strstr(output, "foo.bar") != NULL);
}
END_TEST

START_TEST(test_skip_directory) {
    char subdir[PATH_MAX + 32];
    snprintf(subdir, sizeof(subdir), "%s/subdir", test_dir);
    mkdir(subdir, 0755);

    char input[PATH_MAX + 256];
    snprintf(input, sizeof(input), "{\"pattern\": \"test\", \"path\": \"%s\"}", test_dir);
    char output[2048];
    int32_t exit_code = run_tool(input, output, sizeof(output));
    ck_assert_int_eq(exit_code, 0);
    ck_assert(strstr(output, "\"count\":0") != NULL);
}
END_TEST

START_TEST(test_skip_unreadable_file) {
    char file_path[PATH_MAX + 32];
    snprintf(file_path, sizeof(file_path), "%s/unreadable.txt", test_dir);
    FILE *f = fopen(file_path, "w");
    ck_assert(f != NULL);
    fprintf(f, "should not match\n");
    fclose(f);
    chmod(file_path, 0000);

    char input[PATH_MAX + 256];
    snprintf(input, sizeof(input), "{\"pattern\": \"match\", \"path\": \"%s\"}", test_dir);
    char output[2048];
    int32_t exit_code = run_tool(input, output, sizeof(output));
    ck_assert_int_eq(exit_code, 0);
    ck_assert(strstr(output, "\"count\":0") != NULL);

    chmod(file_path, 0644);
}
END_TEST

START_TEST(test_many_matches) {
    char file_path[PATH_MAX + 32];
    snprintf(file_path, sizeof(file_path), "%s/many.txt", test_dir);
    FILE *f = fopen(file_path, "w");
    ck_assert(f != NULL);
    for (int32_t i = 0; i < 200; i++) {
        fprintf(f, "line %d match here\n", i);
    }
    fclose(f);

    char input[PATH_MAX + 256];
    snprintf(input, sizeof(input), "{\"pattern\": \"match\", \"path\": \"%s\"}", test_dir);
    char output[65536];
    int32_t exit_code = run_tool(input, output, sizeof(output));
    ck_assert_int_eq(exit_code, 0);
    ck_assert(strstr(output, "\"count\":200") != NULL);
}
END_TEST

START_TEST(test_nonexistent_path) {
    char input[PATH_MAX + 256];
    snprintf(input, sizeof(input), "{\"pattern\": \"test\", \"path\": \"/nonexistent/path\"}");
    char output[2048];
    int32_t exit_code = run_tool(input, output, sizeof(output));
    ck_assert_int_eq(exit_code, 0);
    ck_assert(strstr(output, "\"count\":0") != NULL);
}
END_TEST

START_TEST(test_large_input) {
    char *input = talloc_array(test_ctx, char, 20000);
    ck_assert(input != NULL);
    snprintf(input, 20000, "{\"pattern\": \"%0*d\", \"path\": \"%s\"}", 9000, 0, test_dir);
    char output[2048];
    int32_t exit_code = run_tool(input, output, sizeof(output));
    ck_assert_int_eq(exit_code, 0);
}
END_TEST

static Suite *grep_tool_suite(void)
{
    Suite *s = suite_create("GrepTool");
    TCase *tc_core = tcase_create("Core");

    tcase_add_checked_fixture(tc_core, setup, teardown);

    tcase_add_test(tc_core, test_schema_output);
    tcase_add_test(tc_core, test_empty_input);
    tcase_add_test(tc_core, test_invalid_json);
    tcase_add_test(tc_core, test_missing_pattern);
    tcase_add_test(tc_core, test_invalid_pattern_type);
    tcase_add_test(tc_core, test_invalid_regex);
    tcase_add_test(tc_core, test_simple_match);
    tcase_add_test(tc_core, test_no_matches);
    tcase_add_test(tc_core, test_with_glob_filter);
    tcase_add_test(tc_core, test_extended_regex);
    tcase_add_test(tc_core, test_multiple_files);
    tcase_add_test(tc_core, test_line_numbers);
    tcase_add_test(tc_core, test_special_chars_in_pattern);
    tcase_add_test(tc_core, test_skip_directory);
    tcase_add_test(tc_core, test_skip_unreadable_file);
    tcase_add_test(tc_core, test_many_matches);
    tcase_add_test(tc_core, test_nonexistent_path);
    tcase_add_test(tc_core, test_large_input);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = grep_tool_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/tools/grep/grep_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
