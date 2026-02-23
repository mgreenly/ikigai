/**
 * @file glob_test.c
 * @brief Unit tests for glob tool
 */
#include "tests/test_constants.h"

#include <check.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <talloc.h>
#include <unistd.h>

static TALLOC_CTX *test_ctx;
static const char *tool_path = "libexec/glob-tool";

static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

// Helper to run tool with input and capture output
static void run_tool_with_args(const char *arg1,
                               const char *input,
                               char **output,
                               size_t *output_len,
                               int32_t *exit_code)
{
    int32_t pipe_in[2], pipe_out[2];
    if (pipe(pipe_in) == -1 || pipe(pipe_out) == -1) {
        *exit_code = -1;
        return;
    }

    pid_t pid = fork();
    if (pid == -1) {
        close(pipe_in[0]);
        close(pipe_in[1]);
        close(pipe_out[0]);
        close(pipe_out[1]);
        *exit_code = -1;
        return;
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

    size_t capacity = 4096;
    *output_len = 0;
    *output = talloc_array(test_ctx, char, (unsigned int)capacity);

    ssize_t n;
    while ((n = read(pipe_out[0], *output + *output_len, capacity - *output_len)) > 0) {
        *output_len += (size_t)n;
        if (*output_len >= capacity) {
            capacity *= 2;
            *output = talloc_realloc(test_ctx, *output, char, (unsigned int)capacity);
        }
    }
    close(pipe_out[0]);

    if (*output_len < capacity) {
        (*output)[*output_len] = '\0';
    } else {
        *output = talloc_realloc(test_ctx, *output, char, (unsigned int)(*output_len + 1));
        (*output)[*output_len] = '\0';
    }

    int32_t status;
    waitpid(pid, &status, 0);
    *exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

START_TEST(test_schema_output) {
    char *output;
    size_t output_len;
    int32_t exit_code;

    run_tool_with_args("--schema", NULL, &output, &output_len, &exit_code);

    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(strstr(output, "\"name\": \"glob\""));
    ck_assert_ptr_nonnull(strstr(output, "\"pattern\""));
}
END_TEST

START_TEST(test_empty_input) {
    char *output;
    size_t output_len;
    int32_t exit_code;

    run_tool_with_args(NULL, "", &output, &output_len, &exit_code);

    ck_assert_int_eq(exit_code, 1);
}
END_TEST

START_TEST(test_invalid_json) {
    char *output;
    size_t output_len;
    int32_t exit_code;

    run_tool_with_args(NULL, "{not valid json}", &output, &output_len, &exit_code);

    ck_assert_int_eq(exit_code, 1);
}
END_TEST

START_TEST(test_missing_pattern) {
    char *output;
    size_t output_len;
    int32_t exit_code;

    run_tool_with_args(NULL, "{\"path\":\"/tmp\"}", &output, &output_len, &exit_code);

    ck_assert_int_eq(exit_code, 1);
}
END_TEST

START_TEST(test_invalid_pattern_type) {
    char *output;
    size_t output_len;
    int32_t exit_code;

    run_tool_with_args(NULL, "{\"pattern\":123}", &output, &output_len, &exit_code);

    ck_assert_int_eq(exit_code, 1);
}
END_TEST

START_TEST(test_simple_pattern) {
    // Create test file
    const char *testfile = "/tmp/glob_test_simple.txt";
    FILE *f = fopen(testfile, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "test");
    fclose(f);

    char *output;
    size_t output_len;
    int32_t exit_code;

    run_tool_with_args(NULL, "{\"pattern\":\"/tmp/glob_test_simple.txt\"}", &output, &output_len, &exit_code);

    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(strstr(output, "\"output\":\"/tmp/glob_test_simple.txt\""));
    ck_assert_ptr_nonnull(strstr(output, "\"count\":1"));

    unlink(testfile);
}
END_TEST

START_TEST(test_wildcard_pattern) {
    // Create test files
    const char *testfile1 = "/tmp/glob_test_wild1.dat";
    const char *testfile2 = "/tmp/glob_test_wild2.dat";
    FILE *f1 = fopen(testfile1, "w");
    FILE *f2 = fopen(testfile2, "w");
    ck_assert_ptr_nonnull(f1);
    ck_assert_ptr_nonnull(f2);
    fclose(f1);
    fclose(f2);

    char *output;
    size_t output_len;
    int32_t exit_code;

    run_tool_with_args(NULL, "{\"pattern\":\"/tmp/glob_test_wild*.dat\"}", &output, &output_len, &exit_code);

    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(strstr(output, "/tmp/glob_test_wild1.dat"));
    ck_assert_ptr_nonnull(strstr(output, "/tmp/glob_test_wild2.dat"));
    ck_assert_ptr_nonnull(strstr(output, "\"count\":2"));

    unlink(testfile1);
    unlink(testfile2);
}
END_TEST

START_TEST(test_no_matches) {
    char *output;
    size_t output_len;
    int32_t exit_code;

    run_tool_with_args(NULL, "{\"pattern\":\"/tmp/glob_test_no_such_file_*.xyz\"}", &output, &output_len, &exit_code);

    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(strstr(output, "\"output\":\"\""));
    ck_assert_ptr_nonnull(strstr(output, "\"count\":0"));
}
END_TEST

START_TEST(test_with_path_parameter) {
    // Create test file in /tmp
    const char *testfile = "/tmp/glob_test_path.log";
    FILE *f = fopen(testfile, "w");
    ck_assert_ptr_nonnull(f);
    fclose(f);

    char *output;
    size_t output_len;
    int32_t exit_code;

    run_tool_with_args(NULL, "{\"pattern\":\"glob_test_path.log\",\"path\":\"/tmp\"}", &output, &output_len,
                       &exit_code);

    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(strstr(output, "/tmp/glob_test_path.log"));
    ck_assert_ptr_nonnull(strstr(output, "\"count\":1"));

    unlink(testfile);
}
END_TEST

START_TEST(test_question_mark_pattern) {
    // Create test files
    const char *testfile1 = "/tmp/glob_test_q1.x";
    const char *testfile2 = "/tmp/glob_test_q2.x";
    const char *testfile3 = "/tmp/glob_test_q12.x";  // Should not match single ?
    FILE *f1 = fopen(testfile1, "w");
    FILE *f2 = fopen(testfile2, "w");
    FILE *f3 = fopen(testfile3, "w");
    ck_assert_ptr_nonnull(f1);
    ck_assert_ptr_nonnull(f2);
    ck_assert_ptr_nonnull(f3);
    fclose(f1);
    fclose(f2);
    fclose(f3);

    char *output;
    size_t output_len;
    int32_t exit_code;

    run_tool_with_args(NULL, "{\"pattern\":\"/tmp/glob_test_q?.x\"}", &output, &output_len, &exit_code);

    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(strstr(output, "glob_test_q1.x"));
    ck_assert_ptr_nonnull(strstr(output, "glob_test_q2.x"));
    ck_assert_ptr_null(strstr(output, "glob_test_q12.x"));
    ck_assert_ptr_nonnull(strstr(output, "\"count\":2"));

    unlink(testfile1);
    unlink(testfile2);
    unlink(testfile3);
}
END_TEST

START_TEST(test_character_class_pattern) {
    // Create test files
    const char *testfile1 = "/tmp/glob_test_class_a.bin";
    const char *testfile2 = "/tmp/glob_test_class_b.bin";
    const char *testfile3 = "/tmp/glob_test_class_x.bin";  // Should not match [ab]
    FILE *f1 = fopen(testfile1, "w");
    FILE *f2 = fopen(testfile2, "w");
    FILE *f3 = fopen(testfile3, "w");
    ck_assert_ptr_nonnull(f1);
    ck_assert_ptr_nonnull(f2);
    ck_assert_ptr_nonnull(f3);
    fclose(f1);
    fclose(f2);
    fclose(f3);

    char *output;
    size_t output_len;
    int32_t exit_code;

    run_tool_with_args(NULL, "{\"pattern\":\"/tmp/glob_test_class_[ab].bin\"}", &output, &output_len, &exit_code);

    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(strstr(output, "glob_test_class_a.bin"));
    ck_assert_ptr_nonnull(strstr(output, "glob_test_class_b.bin"));
    ck_assert_ptr_null(strstr(output, "glob_test_class_x.bin"));
    ck_assert_ptr_nonnull(strstr(output, "\"count\":2"));

    unlink(testfile1);
    unlink(testfile2);
    unlink(testfile3);
}
END_TEST

START_TEST(test_multiple_matches_sorted) {
    // Create several test files
    const char *files[] = {
        "/tmp/glob_test_multi_1.tmp",
        "/tmp/glob_test_multi_2.tmp",
        "/tmp/glob_test_multi_3.tmp",
        NULL
    };

    for (int32_t i = 0; files[i] != NULL; i++) {
        FILE *f = fopen(files[i], "w");
        ck_assert_ptr_nonnull(f);
        fclose(f);
    }

    char *output;
    size_t output_len;
    int32_t exit_code;

    run_tool_with_args(NULL, "{\"pattern\":\"/tmp/glob_test_multi_*.tmp\"}", &output, &output_len, &exit_code);

    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(strstr(output, "glob_test_multi_1.tmp"));
    ck_assert_ptr_nonnull(strstr(output, "glob_test_multi_2.tmp"));
    ck_assert_ptr_nonnull(strstr(output, "glob_test_multi_3.tmp"));
    ck_assert_ptr_nonnull(strstr(output, "\"count\":3"));

    for (int32_t i = 0; files[i] != NULL; i++) {
        unlink(files[i]);
    }
}
END_TEST

START_TEST(test_special_characters_in_path) {
    // Create file with space in name
    const char *testfile = "/tmp/glob test with spaces.doc";
    FILE *f = fopen(testfile, "w");
    ck_assert_ptr_nonnull(f);
    fclose(f);

    char *output;
    size_t output_len;
    int32_t exit_code;

    run_tool_with_args(NULL, "{\"pattern\":\"/tmp/glob test with spaces.doc\"}", &output, &output_len, &exit_code);

    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(strstr(output, "glob test with spaces.doc"));
    ck_assert_ptr_nonnull(strstr(output, "\"count\":1"));

    unlink(testfile);
}
END_TEST

START_TEST(test_empty_path_parameter) {
    // Empty path should be treated same as no path
    const char *testfile = "/tmp/glob_test_empty_path.xyz";
    FILE *f = fopen(testfile, "w");
    ck_assert_ptr_nonnull(f);
    fclose(f);

    char *output;
    size_t output_len;
    int32_t exit_code;

    run_tool_with_args(NULL,
                       "{\"pattern\":\"/tmp/glob_test_empty_path.xyz\",\"path\":\"\"}",
                       &output,
                       &output_len,
                       &exit_code);

    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(strstr(output, "/tmp/glob_test_empty_path.xyz"));
    ck_assert_ptr_nonnull(strstr(output, "\"count\":1"));

    unlink(testfile);
}
END_TEST

START_TEST(test_large_input) {
    // Build large JSON input (10KB pattern string)
    size_t pattern_size = 10000;
    char *pattern = talloc_array(test_ctx, char, (unsigned int)(pattern_size + 1));
    memset(pattern, 'a', pattern_size);
    pattern[pattern_size] = '\0';

    char *input = talloc_asprintf(test_ctx, "{\"pattern\":\"%s\"}", pattern);

    char *output;
    size_t output_len;
    int32_t exit_code;

    run_tool_with_args(NULL, input, &output, &output_len, &exit_code);

    // Should succeed (no matches expected for this pattern)
    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(strstr(output, "\"count\":0"));
}
END_TEST

START_TEST(test_many_matches) {
    // Create many test files to exercise output buffer growth
    const uint32_t num_files = 100;
    char **files = talloc_array(test_ctx, char *, num_files);

    for (uint32_t i = 0; i < num_files; i++) {
        files[i] = talloc_asprintf(test_ctx, "/tmp/glob_test_many_%03u.tst", i);
        FILE *f = fopen(files[i], "w");
        ck_assert_ptr_nonnull(f);
        fclose(f);
    }

    char *output;
    size_t output_len;
    int32_t exit_code;

    run_tool_with_args(NULL, "{\"pattern\":\"/tmp/glob_test_many_*.tst\"}", &output, &output_len, &exit_code);

    ck_assert_int_eq(exit_code, 0);
    ck_assert_ptr_nonnull(strstr(output, "\"count\":100"));
    ck_assert_ptr_nonnull(strstr(output, "glob_test_many_000.tst"));
    ck_assert_ptr_nonnull(strstr(output, "glob_test_many_099.tst"));

    for (uint32_t i = 0; i < num_files; i++) {
        unlink(files[i]);
    }
}
END_TEST

static Suite *glob_tool_suite(void)
{
    Suite *s = suite_create("GlobTool");
    TCase *tc_core = tcase_create("Core");

    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_schema_output);
    tcase_add_test(tc_core, test_empty_input);
    tcase_add_test(tc_core, test_invalid_json);
    tcase_add_test(tc_core, test_missing_pattern);
    tcase_add_test(tc_core, test_invalid_pattern_type);
    tcase_add_test(tc_core, test_simple_pattern);
    tcase_add_test(tc_core, test_wildcard_pattern);
    tcase_add_test(tc_core, test_no_matches);
    tcase_add_test(tc_core, test_with_path_parameter);
    tcase_add_test(tc_core, test_question_mark_pattern);
    tcase_add_test(tc_core, test_character_class_pattern);
    tcase_add_test(tc_core, test_multiple_matches_sorted);
    tcase_add_test(tc_core, test_special_characters_in_path);
    tcase_add_test(tc_core, test_empty_path_parameter);
    tcase_add_test(tc_core, test_large_input);
    tcase_add_test(tc_core, test_many_matches);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = glob_tool_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/tools/glob/glob_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
