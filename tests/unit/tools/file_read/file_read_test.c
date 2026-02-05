/**
 * @file file_read_test.c
 * @brief Unit tests for file_read tool
 */
#include "tests/test_constants.h"

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
static const char *tool_path = "libexec/ikigai/file-read-tool";

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
    ck_assert(strstr(output, "\"name\": \"file_read\"") != NULL);
    ck_assert(strstr(output, "\"file_path\"") != NULL);
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

START_TEST(test_missing_file_path) {
    int32_t exit_code = run_tool_basic("{}");
    ck_assert_int_eq(exit_code, 1);
}
END_TEST

START_TEST(test_invalid_file_path) {
    int32_t exit_code = run_tool_basic("{\"file_path\": 123}");
    ck_assert_int_eq(exit_code, 1);
}
END_TEST

START_TEST(test_nonexistent_file) {
    char output[4096];
    int32_t exit_code = run_tool("{\"file_path\": \"/nonexistent/file.txt\"}", output, sizeof(output));
    ck_assert_int_eq(exit_code, 0);
    ck_assert(strstr(output, "\"error\"") != NULL);
    ck_assert(strstr(output, "\"error_code\":\"FILE_NOT_FOUND\"") != NULL);
}
END_TEST

START_TEST(test_simple_file) {
    const char *tmpfile = "/tmp/test_file_read_simple.txt";
    FILE *f = fopen(tmpfile, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "Hello, world!\n");
    fclose(f);

    char input[512];
    snprintf(input, sizeof(input), "{\"file_path\": \"%s\"}", tmpfile);

    char output[4096];
    int32_t exit_code = run_tool(input, output, sizeof(output));
    ck_assert_int_eq(exit_code, 0);
    ck_assert(strstr(output, "\"output\"") != NULL);
    ck_assert(strstr(output, "Hello, world!") != NULL);

    unlink(tmpfile);
}
END_TEST

START_TEST(test_multiline_file) {
    const char *tmpfile = "/tmp/test_file_read_multiline.txt";
    FILE *f = fopen(tmpfile, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "Line 1\nLine 2\nLine 3\n");
    fclose(f);

    char input[512];
    snprintf(input, sizeof(input), "{\"file_path\": \"%s\"}", tmpfile);

    char output[4096];
    int32_t exit_code = run_tool(input, output, sizeof(output));
    ck_assert_int_eq(exit_code, 0);
    ck_assert(strstr(output, "Line 1") != NULL);
    ck_assert(strstr(output, "Line 2") != NULL);
    ck_assert(strstr(output, "Line 3") != NULL);

    unlink(tmpfile);
}
END_TEST

START_TEST(test_with_offset) {
    const char *tmpfile = "/tmp/test_file_read_offset.txt";
    FILE *f = fopen(tmpfile, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "Line 1\nLine 2\nLine 3\nLine 4\nLine 5\n");
    fclose(f);

    char input[512];
    snprintf(input, sizeof(input), "{\"file_path\": \"%s\", \"offset\": 3}", tmpfile);

    char output[4096];
    int32_t exit_code = run_tool(input, output, sizeof(output));
    ck_assert_int_eq(exit_code, 0);
    ck_assert(strstr(output, "Line 1") == NULL);
    ck_assert(strstr(output, "Line 2") == NULL);
    ck_assert(strstr(output, "Line 3") != NULL);
    ck_assert(strstr(output, "Line 4") != NULL);
    ck_assert(strstr(output, "Line 5") != NULL);

    unlink(tmpfile);
}
END_TEST

START_TEST(test_with_limit) {
    const char *tmpfile = "/tmp/test_file_read_limit.txt";
    FILE *f = fopen(tmpfile, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "Line 1\nLine 2\nLine 3\nLine 4\nLine 5\n");
    fclose(f);

    char input[512];
    snprintf(input, sizeof(input), "{\"file_path\": \"%s\", \"limit\": 2}", tmpfile);

    char output[4096];
    int32_t exit_code = run_tool(input, output, sizeof(output));
    ck_assert_int_eq(exit_code, 0);
    ck_assert(strstr(output, "Line 1") != NULL);
    ck_assert(strstr(output, "Line 2") != NULL);
    ck_assert(strstr(output, "Line 3") == NULL);
    ck_assert(strstr(output, "Line 4") == NULL);
    ck_assert(strstr(output, "Line 5") == NULL);

    unlink(tmpfile);
}
END_TEST

START_TEST(test_with_offset_and_limit) {
    const char *tmpfile = "/tmp/test_file_read_offset_limit.txt";
    FILE *f = fopen(tmpfile, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "Line 1\nLine 2\nLine 3\nLine 4\nLine 5\n");
    fclose(f);

    char input[512];
    snprintf(input, sizeof(input), "{\"file_path\": \"%s\", \"offset\": 2, \"limit\": 2}", tmpfile);

    char output[4096];
    int32_t exit_code = run_tool(input, output, sizeof(output));
    ck_assert_int_eq(exit_code, 0);
    ck_assert(strstr(output, "Line 1") == NULL);
    ck_assert(strstr(output, "Line 2") != NULL);
    ck_assert(strstr(output, "Line 3") != NULL);
    ck_assert(strstr(output, "Line 4") == NULL);
    ck_assert(strstr(output, "Line 5") == NULL);

    unlink(tmpfile);
}
END_TEST

START_TEST(test_offset_beyond_file) {
    const char *tmpfile = "/tmp/test_file_read_offset_beyond.txt";
    FILE *f = fopen(tmpfile, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "Line 1\nLine 2\n");
    fclose(f);

    char input[512];
    snprintf(input, sizeof(input), "{\"file_path\": \"%s\", \"offset\": 100}", tmpfile);

    char output[4096];
    int32_t exit_code = run_tool(input, output, sizeof(output));
    ck_assert_int_eq(exit_code, 0);
    ck_assert(strstr(output, "\"output\":\"\"") != NULL);

    unlink(tmpfile);
}
END_TEST

START_TEST(test_large_file) {
    const char *tmpfile = "/tmp/test_file_read_large.txt";
    FILE *f = fopen(tmpfile, "w");
    ck_assert_ptr_nonnull(f);
    for (int32_t i = 0; i < 1000; i++) {
        fprintf(f, "Line %d with some content to make it longer\n", i);
    }
    fclose(f);

    char input[512];
    snprintf(input, sizeof(input), "{\"file_path\": \"%s\"}", tmpfile);

    char *output = malloc(100000);
    ck_assert_ptr_nonnull(output);
    int32_t exit_code = run_tool(input, output, 100000);
    ck_assert_int_eq(exit_code, 0);
    ck_assert(strstr(output, "Line 0") != NULL);
    ck_assert(strstr(output, "Line 999") != NULL);

    free(output);
    unlink(tmpfile);
}
END_TEST

START_TEST(test_special_characters) {
    const char *tmpfile = "/tmp/test_file_read_special.txt";
    FILE *f = fopen(tmpfile, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "Special: \"quotes\" and \\backslash\\ and \nnewline\n");
    fclose(f);

    char input[512];
    snprintf(input, sizeof(input), "{\"file_path\": \"%s\"}", tmpfile);

    char output[4096];
    int32_t exit_code = run_tool(input, output, sizeof(output));
    ck_assert_int_eq(exit_code, 0);
    ck_assert(strstr(output, "\"output\"") != NULL);

    unlink(tmpfile);
}
END_TEST

START_TEST(test_empty_file) {
    const char *tmpfile = "/tmp/test_file_read_empty.txt";
    FILE *f = fopen(tmpfile, "w");
    ck_assert_ptr_nonnull(f);
    fclose(f);

    char input[512];
    snprintf(input, sizeof(input), "{\"file_path\": \"%s\"}", tmpfile);

    char output[4096];
    int32_t exit_code = run_tool(input, output, sizeof(output));
    ck_assert_int_eq(exit_code, 0);
    ck_assert(strstr(output, "\"output\":\"\"") != NULL);

    unlink(tmpfile);
}
END_TEST

START_TEST(test_large_input_json) {
    const char *tmpfile = "/tmp/test_file_read_large_input.txt";
    FILE *f = fopen(tmpfile, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "Test content\n");
    fclose(f);

    // Create input JSON larger than 4096 bytes to trigger buffer reallocation
    char *input = talloc_array(test_ctx, char, 5000);
    ck_assert_ptr_nonnull(input);
    int32_t offset = snprintf(input, 5000, "{\"file_path\": \"%s\", \"padding\": \"", tmpfile);
    // Fill with padding to exceed 4096 bytes
    for (int32_t i = 0; i < 4000; i++) {
        input[offset + i] = 'x';
    }
    offset += 4000;
    snprintf(input + offset, 5000 - (size_t)offset, "\"}");

    char output[8192];
    int32_t exit_code = run_tool(input, output, sizeof(output));
    ck_assert_int_eq(exit_code, 0);
    ck_assert(strstr(output, "Test content") != NULL);

    unlink(tmpfile);
}
END_TEST

START_TEST(test_permission_denied) {
    const char *tmpfile = "/tmp/test_file_read_perms.txt";

    // Clean up any leftover file from previous run
    chmod(tmpfile, 0644);
    unlink(tmpfile);

    FILE *f = fopen(tmpfile, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "Secret content\n");
    fclose(f);

    chmod(tmpfile, 0000);

    char input[512];
    snprintf(input, sizeof(input), "{\"file_path\": \"%s\"}", tmpfile);

    char output[4096];
    int32_t exit_code = run_tool(input, output, sizeof(output));
    ck_assert_int_eq(exit_code, 0);
    ck_assert(strstr(output, "\"error\"") != NULL);
    ck_assert(strstr(output, "\"error_code\":\"PERMISSION_DENIED\"") != NULL);

    chmod(tmpfile, 0644);
    unlink(tmpfile);
}
END_TEST

START_TEST(test_line_offset_and_limit_buffer_growth) {
    const char *tmpfile = "/tmp/test_file_read_growth.txt";
    FILE *f = fopen(tmpfile, "w");
    ck_assert_ptr_nonnull(f);
    for (int32_t i = 0; i < 200; i++) {
        fprintf(f, "Line %d: ", i);
        for (int32_t j = 0; j < 50; j++) {
            fprintf(f, "word%d ", j);
        }
        fprintf(f, "\n");
    }
    fclose(f);

    // offset=10 means start from line 10 (1-based), which contains "Line 9:"
    // limit=50 means read 50 lines, so lines 10-59, containing "Line 9:" through "Line 58:"
    char input[512];
    snprintf(input, sizeof(input), "{\"file_path\": \"%s\", \"offset\": 10, \"limit\": 50}", tmpfile);

    char *output = malloc(100000);
    ck_assert_ptr_nonnull(output);
    int32_t exit_code = run_tool(input, output, 100000);
    ck_assert_int_eq(exit_code, 0);
    ck_assert(strstr(output, "Line 8:") == NULL);
    ck_assert(strstr(output, "Line 9:") != NULL);
    ck_assert(strstr(output, "Line 58:") != NULL);
    ck_assert(strstr(output, "Line 59:") == NULL);

    free(output);
    unlink(tmpfile);
}
END_TEST

static Suite *file_read_suite(void)
{
    Suite *s = suite_create("file_read");
    TCase *tc = tcase_create("core");

    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_schema_output);
    tcase_add_test(tc, test_empty_input);
    tcase_add_test(tc, test_invalid_json);
    tcase_add_test(tc, test_missing_file_path);
    tcase_add_test(tc, test_invalid_file_path);
    tcase_add_test(tc, test_nonexistent_file);
    tcase_add_test(tc, test_simple_file);
    tcase_add_test(tc, test_multiline_file);
    tcase_add_test(tc, test_with_offset);
    tcase_add_test(tc, test_with_limit);
    tcase_add_test(tc, test_with_offset_and_limit);
    tcase_add_test(tc, test_offset_beyond_file);
    tcase_add_test(tc, test_large_file);
    tcase_add_test(tc, test_special_characters);
    tcase_add_test(tc, test_empty_file);
    tcase_add_test(tc, test_large_input_json);
    tcase_add_test(tc, test_permission_denied);
    tcase_add_test(tc, test_line_offset_and_limit_buffer_growth);

    suite_add_tcase(s, tc);
    return s;
}

int32_t main(void)
{
    Suite *s = file_read_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/tools/file_read/file_read_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    int32_t number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
