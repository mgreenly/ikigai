#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <talloc.h>
#include <unistd.h>

#include "../../../src/error.h"
#include "../../../src/tool.h"

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

// Grep execution tests
START_TEST(test_grep_exec_with_matches) {
    // Create temporary test directory with known files
    char test_dir[] = "/tmp/ikigai-grep-test-XXXXXX";
    char *dir = mkdtemp(test_dir);
    ck_assert_ptr_nonnull(dir);

    // Create test files with TODO comments
    char file1[256];
    char file2[256];
    char file3[256];
    snprintf(file1, sizeof(file1), "%s/test1.c", dir);
    snprintf(file2, sizeof(file2), "%s/test2.c", dir);
    snprintf(file3, sizeof(file3), "%s/test.txt", dir);

    FILE *f1 = fopen(file1, "w");
    ck_assert_ptr_nonnull(f1);
    fprintf(f1, "// This is a test file\n// TODO: add error handling\nint main() {}\n");
    fclose(f1);

    FILE *f2 = fopen(file2, "w");
    ck_assert_ptr_nonnull(f2);
    fprintf(f2, "// TODO: implement history\nvoid func() {}\n");
    fclose(f2);

    FILE *f3 = fopen(file3, "w");
    ck_assert_ptr_nonnull(f3);
    fprintf(f3, "This is a text file\n");
    fclose(f3);

    // Execute grep with pattern "TODO"
    res_t res = ik_tool_exec_grep(ctx, "TODO", NULL, dir);
    ck_assert(!res.is_err);

    char *json = res.ok;
    ck_assert_ptr_nonnull(json);

    // Parse result JSON
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert(yyjson_is_obj(root));

    // Verify success: true
    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert_ptr_nonnull(success);
    ck_assert(yyjson_get_bool(success) == true);

    // Verify data object exists
    yyjson_val *data = yyjson_obj_get(root, "data");
    ck_assert_ptr_nonnull(data);
    ck_assert(yyjson_is_obj(data));

    // Verify count is 2
    yyjson_val *count = yyjson_obj_get(data, "count");
    ck_assert_ptr_nonnull(count);
    ck_assert_int_eq(yyjson_get_int(count), 2);

    // Verify output contains both matches with correct format
    yyjson_val *output = yyjson_obj_get(data, "output");
    ck_assert_ptr_nonnull(output);
    const char *output_str = yyjson_get_str(output);
    ck_assert_ptr_nonnull(output_str);
    ck_assert(strstr(output_str, "test1.c:2:") != NULL);
    ck_assert(strstr(output_str, "TODO: add error handling") != NULL);
    ck_assert(strstr(output_str, "test2.c:1:") != NULL);
    ck_assert(strstr(output_str, "TODO: implement history") != NULL);

    yyjson_doc_free(doc);

    // Cleanup
    unlink(file1);
    unlink(file2);
    unlink(file3);
    rmdir(dir);
}
END_TEST START_TEST(test_grep_exec_no_matches)
{
    // Create temporary test directory with no matching content
    char test_dir[] = "/tmp/ikigai-grep-test-XXXXXX";
    char *dir = mkdtemp(test_dir);
    ck_assert_ptr_nonnull(dir);

    // Create a file without the pattern
    char file1[256];
    snprintf(file1, sizeof(file1), "%s/test.txt", dir);
    FILE *f1 = fopen(file1, "w");
    ck_assert_ptr_nonnull(f1);
    fprintf(f1, "This file has no matching text\n");
    fclose(f1);

    // Execute grep with pattern "TODO" (no matches)
    res_t res = ik_tool_exec_grep(ctx, "TODO", NULL, dir);
    ck_assert(!res.is_err);

    char *json = res.ok;
    ck_assert_ptr_nonnull(json);

    // Parse result JSON
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert(yyjson_get_bool(success) == true);

    yyjson_val *data = yyjson_obj_get(root, "data");
    yyjson_val *count = yyjson_obj_get(data, "count");
    ck_assert_int_eq(yyjson_get_int(count), 0);

    yyjson_val *output = yyjson_obj_get(data, "output");
    const char *output_str = yyjson_get_str(output);
    ck_assert_str_eq(output_str, "");

    yyjson_doc_free(doc);

    // Cleanup
    unlink(file1);
    rmdir(dir);
}

END_TEST START_TEST(test_grep_exec_with_glob_filter)
{
    // Create temporary test directory
    char test_dir[] = "/tmp/ikigai-grep-test-XXXXXX";
    char *dir = mkdtemp(test_dir);
    ck_assert_ptr_nonnull(dir);

    // Create test files
    char file1[256];
    char file2[256];
    snprintf(file1, sizeof(file1), "%s/test.c", dir);
    snprintf(file2, sizeof(file2), "%s/test.txt", dir);

    FILE *f1 = fopen(file1, "w");
    ck_assert_ptr_nonnull(f1);
    fprintf(f1, "// TODO in C file\n");
    fclose(f1);

    FILE *f2 = fopen(file2, "w");
    ck_assert_ptr_nonnull(f2);
    fprintf(f2, "TODO in text file\n");
    fclose(f2);

    // Execute grep with glob filter "*.c" - should only match C file
    res_t res = ik_tool_exec_grep(ctx, "TODO", "*.c", dir);
    ck_assert(!res.is_err);

    char *json = res.ok;
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert(yyjson_get_bool(success) == true);

    yyjson_val *data = yyjson_obj_get(root, "data");
    yyjson_val *count = yyjson_obj_get(data, "count");
    ck_assert_int_eq(yyjson_get_int(count), 1);

    yyjson_val *output = yyjson_obj_get(data, "output");
    const char *output_str = yyjson_get_str(output);
    ck_assert(strstr(output_str, "test.c") != NULL);
    ck_assert(strstr(output_str, "test.txt") == NULL);

    yyjson_doc_free(doc);

    // Cleanup
    unlink(file1);
    unlink(file2);
    rmdir(dir);
}

END_TEST START_TEST(test_grep_exec_null_path_uses_cwd)
{
    // Create a temp file in current directory
    char oldcwd[256];
    getcwd(oldcwd, sizeof(oldcwd));

    char test_dir[] = "/tmp/ikigai-grep-test-XXXXXX";
    char *dir = mkdtemp(test_dir);
    ck_assert_ptr_nonnull(dir);
    chdir(dir);

    char file1[] = "test.c";
    FILE *f1 = fopen(file1, "w");
    ck_assert_ptr_nonnull(f1);
    fprintf(f1, "TODO: test\n");
    fclose(f1);

    // Execute grep with NULL path - should search current directory
    res_t res = ik_tool_exec_grep(ctx, "TODO", NULL, NULL);
    ck_assert(!res.is_err);

    char *json = res.ok;
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert(yyjson_get_bool(success) == true);

    yyjson_val *data = yyjson_obj_get(root, "data");
    yyjson_val *count = yyjson_obj_get(data, "count");
    ck_assert_int_eq(yyjson_get_int(count), 1);

    yyjson_doc_free(doc);

    // Restore directory and cleanup
    chdir(oldcwd);
    char file_path[512];
    snprintf(file_path, sizeof(file_path), "%s/%s", dir, file1);
    unlink(file_path);
    rmdir(dir);
}

END_TEST START_TEST(test_grep_exec_multiline_match)
{
    // Test that line numbers are correct
    char test_dir[] = "/tmp/ikigai-grep-test-XXXXXX";
    char *dir = mkdtemp(test_dir);
    ck_assert_ptr_nonnull(dir);

    char file1[256];
    snprintf(file1, sizeof(file1), "%s/test.c", dir);
    FILE *f1 = fopen(file1, "w");
    ck_assert_ptr_nonnull(f1);
    fprintf(f1, "Line 1\n");
    fprintf(f1, "Line 2 with TODO\n");
    fprintf(f1, "Line 3\n");
    fprintf(f1, "Line 4 with TODO again\n");
    fclose(f1);

    res_t res = ik_tool_exec_grep(ctx, "TODO", NULL, dir);
    ck_assert(!res.is_err);

    char *json = res.ok;
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *data = yyjson_obj_get(root, "data");
    yyjson_val *count = yyjson_obj_get(data, "count");
    ck_assert_int_eq(yyjson_get_int(count), 2);

    yyjson_val *output = yyjson_obj_get(data, "output");
    const char *output_str = yyjson_get_str(output);
    ck_assert(strstr(output_str, ":2:") != NULL);
    ck_assert(strstr(output_str, ":4:") != NULL);

    yyjson_doc_free(doc);

    // Cleanup
    unlink(file1);
    rmdir(dir);
}

END_TEST

// Test suite
static Suite *grep_execute_suite(void)
{
    Suite *s = suite_create("Grep Execution");

    TCase *tc_grep_exec = tcase_create("Grep Execution");
    tcase_add_checked_fixture(tc_grep_exec, setup, teardown);
    tcase_add_test(tc_grep_exec, test_grep_exec_with_matches);
    tcase_add_test(tc_grep_exec, test_grep_exec_no_matches);
    tcase_add_test(tc_grep_exec, test_grep_exec_with_glob_filter);
    tcase_add_test(tc_grep_exec, test_grep_exec_null_path_uses_cwd);
    tcase_add_test(tc_grep_exec, test_grep_exec_multiline_match);
    suite_add_tcase(s, tc_grep_exec);

    return s;
}

int main(void)
{
    Suite *s = grep_execute_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int32_t number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
