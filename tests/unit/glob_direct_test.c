#include "../../src/tools/glob/glob_tool.h"

#include "../../src/json_allocator.h"
#include "../../src/panic.h"

#include <check.h>
#include <glob.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

// Mock glob_ function prototypes
int glob_(const char *pattern, int flags, int (*errfunc)(const char *, int), glob_t *pglob);
void globfree_(glob_t *pglob);

int (*mock_glob_)(const char *pattern, int flags, int (*errfunc)(const char *, int), glob_t *pglob) = NULL;
void (*mock_globfree_)(glob_t *pglob) = NULL;
int32_t mock_glob_return = 0;
glob_t mock_glob_result;

int glob_(const char *pattern, int flags, int (*errfunc)(const char *, int), glob_t *pglob)
{
    if (mock_glob_ != NULL) {
        return mock_glob_(pattern, flags, errfunc, pglob);
    }
    if (mock_glob_return != 0) {
        return mock_glob_return;
    }
    *pglob = mock_glob_result;
    return 0;
}

void globfree_(glob_t *pglob)
{
    if (mock_globfree_ != NULL) {
        mock_globfree_(pglob);
        return;
    }
    // Do nothing for mock
}

START_TEST(test_glob_success_single_file) {
    void *ctx = talloc_new(NULL);

    // Mock glob to return a single file
    static char path1[] = "test.txt";
    static char *paths[] = {path1};
    mock_glob_result.gl_pathc = 1;
    mock_glob_result.gl_pathv = paths;
    mock_glob_return = 0;

    // Redirect stdout
    FILE *original_stdout = stdout;
    char output_buffer[4096];
    stdout = fmemopen(output_buffer, sizeof(output_buffer), "w");

    int32_t result = glob_execute(ctx, "*.txt", NULL);

    fflush(stdout);
    fclose(stdout);
    stdout = original_stdout;

    ck_assert_int_eq(result, 0);
    ck_assert(strstr(output_buffer, "test.txt") != NULL);
    ck_assert(strstr(output_buffer, "\"count\":1") != NULL);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_glob_success_multiple_files) {
    void *ctx = talloc_new(NULL);

    // Mock glob to return multiple files
    static char path1[] = "file1.c";
    static char path2[] = "file2.c";
    static char path3[] = "file3.c";
    static char *paths[] = {path1, path2, path3};
    mock_glob_result.gl_pathc = 3;
    mock_glob_result.gl_pathv = paths;
    mock_glob_return = 0;

    // Redirect stdout
    FILE *original_stdout = stdout;
    char output_buffer[4096];
    stdout = fmemopen(output_buffer, sizeof(output_buffer), "w");

    int32_t result = glob_execute(ctx, "*.c", NULL);

    fflush(stdout);
    fclose(stdout);
    stdout = original_stdout;

    ck_assert_int_eq(result, 0);
    ck_assert(strstr(output_buffer, "file1.c") != NULL);
    ck_assert(strstr(output_buffer, "file2.c") != NULL);
    ck_assert(strstr(output_buffer, "file3.c") != NULL);
    ck_assert(strstr(output_buffer, "\"count\":3") != NULL);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_glob_no_match) {
    void *ctx = talloc_new(NULL);

    // Mock glob to return no matches
    mock_glob_result.gl_pathc = 0;
    mock_glob_result.gl_pathv = NULL;
    mock_glob_return = GLOB_NOMATCH;

    // Redirect stdout
    FILE *original_stdout = stdout;
    char output_buffer[4096];
    stdout = fmemopen(output_buffer, sizeof(output_buffer), "w");

    int32_t result = glob_execute(ctx, "*.nonexistent", NULL);

    fflush(stdout);
    fclose(stdout);
    stdout = original_stdout;

    ck_assert_int_eq(result, 0);
    ck_assert(strstr(output_buffer, "\"count\":0") != NULL);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_glob_nospace_error) {
    void *ctx = talloc_new(NULL);

    // Mock glob to return GLOB_NOSPACE
    mock_glob_return = GLOB_NOSPACE;

    // Redirect stdout
    FILE *original_stdout = stdout;
    char output_buffer[4096];
    stdout = fmemopen(output_buffer, sizeof(output_buffer), "w");

    int32_t result = glob_execute(ctx, "*.txt", NULL);

    fflush(stdout);
    fclose(stdout);
    stdout = original_stdout;

    ck_assert_int_eq(result, 0);
    ck_assert(strstr(output_buffer, "Out of memory during glob") != NULL);
    ck_assert(strstr(output_buffer, "OUT_OF_MEMORY") != NULL);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_glob_aborted_error) {
    void *ctx = talloc_new(NULL);

    // Mock glob to return GLOB_ABORTED
    mock_glob_return = GLOB_ABORTED;

    // Redirect stdout
    FILE *original_stdout = stdout;
    char output_buffer[4096];
    stdout = fmemopen(output_buffer, sizeof(output_buffer), "w");

    int32_t result = glob_execute(ctx, "*.txt", NULL);

    fflush(stdout);
    fclose(stdout);
    stdout = original_stdout;

    ck_assert_int_eq(result, 0);
    ck_assert(strstr(output_buffer, "Read error during glob") != NULL);
    ck_assert(strstr(output_buffer, "READ_ERROR") != NULL);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_glob_invalid_pattern_error) {
    void *ctx = talloc_new(NULL);

    // Mock glob to return some other error
    mock_glob_return = 999;

    // Redirect stdout
    FILE *original_stdout = stdout;
    char output_buffer[4096];
    stdout = fmemopen(output_buffer, sizeof(output_buffer), "w");

    int32_t result = glob_execute(ctx, "[invalid", NULL);

    fflush(stdout);
    fclose(stdout);
    stdout = original_stdout;

    ck_assert_int_eq(result, 0);
    ck_assert(strstr(output_buffer, "Invalid glob pattern") != NULL);
    ck_assert(strstr(output_buffer, "INVALID_PATTERN") != NULL);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_glob_with_path) {
    void *ctx = talloc_new(NULL);

    // Mock glob to return files
    static char path1[] = "src/test/file.c";
    static char *paths[] = {path1};
    mock_glob_result.gl_pathc = 1;
    mock_glob_result.gl_pathv = paths;
    mock_glob_return = 0;

    // Redirect stdout
    FILE *original_stdout = stdout;
    char output_buffer[4096];
    stdout = fmemopen(output_buffer, sizeof(output_buffer), "w");

    int32_t result = glob_execute(ctx, "*.c", "src/test");

    fflush(stdout);
    fclose(stdout);
    stdout = original_stdout;

    ck_assert_int_eq(result, 0);
    ck_assert(strstr(output_buffer, "file.c") != NULL);

    talloc_free(ctx);
}
END_TEST

static Suite *glob_direct_suite(void)
{
    Suite *s = suite_create("glob_direct");
    TCase *tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_glob_success_single_file);
    tcase_add_test(tc_core, test_glob_success_multiple_files);
    tcase_add_test(tc_core, test_glob_no_match);
    tcase_add_test(tc_core, test_glob_nospace_error);
    tcase_add_test(tc_core, test_glob_aborted_error);
    tcase_add_test(tc_core, test_glob_invalid_pattern_error);
    tcase_add_test(tc_core, test_glob_with_path);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = glob_direct_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int32_t number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
