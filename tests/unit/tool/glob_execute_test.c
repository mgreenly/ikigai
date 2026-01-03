#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>
#include <unistd.h>

#include "../../../src/error.h"
#include "../../../src/tool.h"
#include "../../test_utils.h"

// Test fixtures
static TALLOC_CTX *ctx = NULL;

static void setup(void)
{
    ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(ctx);
    ctx = NULL;
}

// Glob execution tests
START_TEST(test_glob_exec_with_matches) {
    // Create temporary test directory with known files
    char test_dir[] = "/tmp/ikigai-glob-test-XXXXXX";
    char *dir = mkdtemp(test_dir);
    ck_assert_ptr_nonnull(dir);

    // Create test files
    char file1[256];
    char file2[256];
    char file3[256];
    snprintf(file1, sizeof(file1), "%s/test1.c", dir);
    snprintf(file2, sizeof(file2), "%s/test2.c", dir);
    snprintf(file3, sizeof(file3), "%s/test.txt", dir);

    FILE *f1 = fopen(file1, "w");
    ck_assert_ptr_nonnull(f1);
    fclose(f1);

    FILE *f2 = fopen(file2, "w");
    ck_assert_ptr_nonnull(f2);
    fclose(f2);

    FILE *f3 = fopen(file3, "w");
    ck_assert_ptr_nonnull(f3);
    fclose(f3);

    // Execute glob with pattern "*.c"
    res_t res = ik_tool_exec_glob(ctx, "*.c", dir);
    ck_assert(!res.is_err);

    yyjson_doc *doc = NULL;
    yyjson_val *data = ik_test_tool_parse_success(res.ok, &doc);
    const char *output_str = ik_test_tool_get_output(data);

    // Verify count is 2
    yyjson_val *count = yyjson_obj_get(data, "count");
    ck_assert_ptr_nonnull(count);
    ck_assert_int_eq(yyjson_get_int(count), 2);

    // Verify output contains both .c files
    ck_assert(strstr(output_str, "test1.c") != NULL);
    ck_assert(strstr(output_str, "test2.c") != NULL);
    ck_assert(strstr(output_str, "test.txt") == NULL);

    yyjson_doc_free(doc);

    // Cleanup
    unlink(file1);
    unlink(file2);
    unlink(file3);
    rmdir(dir);
}
END_TEST

START_TEST(test_glob_exec_no_matches) {
    // Create temporary test directory with no matching files
    char test_dir[] = "/tmp/ikigai-glob-test-XXXXXX";
    char *dir = mkdtemp(test_dir);
    ck_assert_ptr_nonnull(dir);

    // Create a non-matching file
    char file1[256];
    snprintf(file1, sizeof(file1), "%s/test.txt", dir);
    FILE *f1 = fopen(file1, "w");
    ck_assert_ptr_nonnull(f1);
    fclose(f1);

    // Execute glob with pattern "*.c" (no matches)
    res_t res = ik_tool_exec_glob(ctx, "*.c", dir);
    ck_assert(!res.is_err);

    yyjson_doc *doc = NULL;
    yyjson_val *data = ik_test_tool_parse_success(res.ok, &doc);
    const char *output_str = ik_test_tool_get_output(data);

    yyjson_val *count = yyjson_obj_get(data, "count");
    ck_assert_int_eq(yyjson_get_int(count), 0);

    ck_assert_str_eq(output_str, "");

    yyjson_doc_free(doc);

    // Cleanup
    unlink(file1);
    rmdir(dir);
}

END_TEST

START_TEST(test_glob_exec_no_matches_treated_as_success) {
    // POSIX glob treats patterns like "[unclosed" as literal patterns that don't match
    // This is not an error - just no matches
    res_t res = ik_tool_exec_glob(ctx, "[unclosed", "/tmp");
    ck_assert(!res.is_err);

    yyjson_doc *doc = NULL;
    yyjson_val *data = ik_test_tool_parse_success(res.ok, &doc);

    // Verify count is 0
    yyjson_val *count = yyjson_obj_get(data, "count");
    ck_assert_int_eq(yyjson_get_int(count), 0);

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_glob_exec_with_null_path) {
    // Test with NULL path - should use pattern as-is
    // Create a temp file in /tmp
    char tmpfile[] = "/tmp/ikigai-test-XXXXXX.txt";
    int fd = mkstemps(tmpfile, 4);
    ck_assert(fd >= 0);
    close(fd);

    // Execute glob with NULL path
    res_t res = ik_tool_exec_glob(ctx, tmpfile, NULL);
    ck_assert(!res.is_err);

    yyjson_doc *doc = NULL;
    yyjson_val *data = ik_test_tool_parse_success(res.ok, &doc);
    const char *output_str = ik_test_tool_get_output(data);

    yyjson_val *count = yyjson_obj_get(data, "count");
    ck_assert_int_eq(yyjson_get_int(count), 1);

    ck_assert(strstr(output_str, tmpfile) != NULL);

    yyjson_doc_free(doc);
    unlink(tmpfile);
}

END_TEST

START_TEST(test_glob_exec_with_empty_path) {
    // Test with empty path - should use pattern as-is
    char test_dir[] = "/tmp/ikigai-glob-test-XXXXXX";
    char *dir = mkdtemp(test_dir);
    ck_assert_ptr_nonnull(dir);

    char file1[256];
    snprintf(file1, sizeof(file1), "%s/test.c", dir);
    FILE *f1 = fopen(file1, "w");
    ck_assert_ptr_nonnull(f1);
    fclose(f1);

    // Change to test directory to test empty path
    char oldcwd[256];
    getcwd(oldcwd, sizeof(oldcwd));
    chdir(dir);

    // Execute glob with empty path
    res_t res = ik_tool_exec_glob(ctx, "*.c", "");
    ck_assert(!res.is_err);

    yyjson_doc *doc = NULL;
    yyjson_val *data = ik_test_tool_parse_success(res.ok, &doc);

    yyjson_val *count = yyjson_obj_get(data, "count");
    ck_assert_int_eq(yyjson_get_int(count), 1);

    yyjson_doc_free(doc);

    // Restore directory
    chdir(oldcwd);
    unlink(file1);
    rmdir(dir);
}

END_TEST

START_TEST(test_glob_exec_multiple_files_output_format) {
    // Test that multiple files are separated by newlines
    char test_dir[] = "/tmp/ikigai-glob-test-XXXXXX";
    char *dir = mkdtemp(test_dir);
    ck_assert_ptr_nonnull(dir);

    // Create exactly 3 matching files to test iteration logic
    char file1[256], file2[256], file3[256];
    snprintf(file1, sizeof(file1), "%s/a.c", dir);
    snprintf(file2, sizeof(file2), "%s/b.c", dir);
    snprintf(file3, sizeof(file3), "%s/c.c", dir);

    FILE *f1 = fopen(file1, "w");
    ck_assert_ptr_nonnull(f1);
    fclose(f1);

    FILE *f2 = fopen(file2, "w");
    ck_assert_ptr_nonnull(f2);
    fclose(f2);

    FILE *f3 = fopen(file3, "w");
    ck_assert_ptr_nonnull(f3);
    fclose(f3);

    res_t res = ik_tool_exec_glob(ctx, "*.c", dir);
    ck_assert(!res.is_err);

    yyjson_doc *doc = NULL;
    yyjson_val *data = ik_test_tool_parse_success(res.ok, &doc);
    const char *output_str = ik_test_tool_get_output(data);

    yyjson_val *count = yyjson_obj_get(data, "count");
    ck_assert_int_eq(yyjson_get_int(count), 3);

    // Verify output has newlines separating files
    // Count newlines - should be 2 (between 3 files)
    int newline_count = 0;
    for (const char *p = output_str; *p; p++) {
        if (*p == '\n') newline_count++;
    }
    ck_assert_int_eq(newline_count, 2);

    yyjson_doc_free(doc);

    unlink(file1);
    unlink(file2);
    unlink(file3);
    rmdir(dir);
}

END_TEST

// Test suite
static Suite *glob_execute_suite(void)
{
    Suite *s = suite_create("Glob Execution");

    TCase *tc_glob_exec = tcase_create("Glob Execution");
    tcase_set_timeout(tc_glob_exec, 30);
    tcase_add_checked_fixture(tc_glob_exec, setup, teardown);
    tcase_add_test(tc_glob_exec, test_glob_exec_with_matches);
    tcase_add_test(tc_glob_exec, test_glob_exec_no_matches);
    tcase_add_test(tc_glob_exec, test_glob_exec_no_matches_treated_as_success);
    tcase_add_test(tc_glob_exec, test_glob_exec_with_null_path);
    tcase_add_test(tc_glob_exec, test_glob_exec_with_empty_path);
    tcase_add_test(tc_glob_exec, test_glob_exec_multiple_files_output_format);
    suite_add_tcase(s, tc_glob_exec);

    return s;
}

int main(void)
{
    Suite *s = glob_execute_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int32_t number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
