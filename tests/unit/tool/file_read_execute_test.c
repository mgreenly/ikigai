#include <check.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <talloc.h>
#include <unistd.h>

#include "../../../src/error.h"
#include "../../../src/tool.h"
#include "../../../src/wrapper.h"

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

// Test: file_read with valid file returns file contents
START_TEST(test_file_read_exec_valid_file) {
    // Create temporary test file with known contents
    char test_file[] = "/tmp/ikigai-file-read-test-XXXXXX";
    int fd = mkstemp(test_file);
    ck_assert(fd >= 0);

    const char *contents = "# My Project\n\nA simple example project.";
    ssize_t written = write(fd, contents, strlen(contents));
    ck_assert_int_eq((long)written, (long)strlen(contents));
    close(fd);

    // Execute file_read
    res_t res = ik_tool_exec_file_read(ctx, test_file);
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

    // Verify output contains file contents
    yyjson_val *output = yyjson_obj_get(data, "output");
    ck_assert_ptr_nonnull(output);
    const char *output_str = yyjson_get_str(output);
    ck_assert_ptr_nonnull(output_str);
    ck_assert_str_eq(output_str, contents);

    yyjson_doc_free(doc);

    // Cleanup
    unlink(test_file);
}
END_TEST
// Test: file_read with non-existent file returns error
START_TEST(test_file_read_exec_file_not_found)
{
    const char *nonexistent = "/tmp/ikigai-file-read-nonexistent-xyz123.txt";

    // Execute file_read on non-existent file
    res_t res = ik_tool_exec_file_read(ctx, nonexistent);
    ck_assert(!res.is_err);

    char *json = res.ok;
    ck_assert_ptr_nonnull(json);

    // Parse result JSON
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);

    // Verify success: false
    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert_ptr_nonnull(success);
    ck_assert(yyjson_get_bool(success) == false);

    // Verify error message exists and mentions the file
    yyjson_val *error = yyjson_obj_get(root, "error");
    ck_assert_ptr_nonnull(error);
    const char *error_str = yyjson_get_str(error);
    ck_assert_ptr_nonnull(error_str);
    ck_assert(strstr(error_str, "File not found") != NULL || strstr(error_str, nonexistent) != NULL);

    yyjson_doc_free(doc);
}

END_TEST
// Test: file_read with unreadable file returns error
START_TEST(test_file_read_exec_permission_denied)
{
    // Create temporary test file
    char test_file[] = "/tmp/ikigai-file-read-test-XXXXXX";
    int fd = mkstemp(test_file);
    ck_assert(fd >= 0);

    const char *contents = "test data";
    write(fd, contents, strlen(contents));
    close(fd);

    // Make file unreadable
    chmod(test_file, 0000);

    // Execute file_read
    res_t res = ik_tool_exec_file_read(ctx, test_file);
    ck_assert(!res.is_err);

    char *json = res.ok;
    ck_assert_ptr_nonnull(json);

    // Parse result JSON
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);

    // Verify success: false
    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert_ptr_nonnull(success);
    ck_assert(yyjson_get_bool(success) == false);

    // Verify error message exists and mentions permission denied
    yyjson_val *error = yyjson_obj_get(root, "error");
    ck_assert_ptr_nonnull(error);
    const char *error_str = yyjson_get_str(error);
    ck_assert_ptr_nonnull(error_str);
    ck_assert(strstr(error_str, "Permission denied") != NULL || strstr(error_str, test_file) != NULL);

    yyjson_doc_free(doc);

    // Restore permissions for cleanup
    chmod(test_file, 0644);
    unlink(test_file);
}

END_TEST
// Test: file_read with empty file returns empty output
START_TEST(test_file_read_exec_empty_file)
{
    // Create temporary empty file
    char test_file[] = "/tmp/ikigai-file-read-test-XXXXXX";
    int fd = mkstemp(test_file);
    ck_assert(fd >= 0);
    close(fd);

    // Execute file_read
    res_t res = ik_tool_exec_file_read(ctx, test_file);
    ck_assert(!res.is_err);

    char *json = res.ok;
    ck_assert_ptr_nonnull(json);

    // Parse result JSON
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);

    // Verify success: true
    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert_ptr_nonnull(success);
    ck_assert(yyjson_get_bool(success) == true);

    // Verify data object exists
    yyjson_val *data = yyjson_obj_get(root, "data");
    ck_assert_ptr_nonnull(data);

    // Verify output is empty string
    yyjson_val *output = yyjson_obj_get(data, "output");
    ck_assert_ptr_nonnull(output);
    const char *output_str = yyjson_get_str(output);
    ck_assert_ptr_nonnull(output_str);
    ck_assert_str_eq(output_str, "");

    yyjson_doc_free(doc);

    // Cleanup
    unlink(test_file);
}

END_TEST
// Test: file_read with large file works correctly
START_TEST(test_file_read_exec_large_file)
{
    // Create temporary file with large content
    char test_file[] = "/tmp/ikigai-file-read-test-XXXXXX";
    int fd = mkstemp(test_file);
    ck_assert(fd >= 0);

    // Write 10KB of data
    char *large_content = talloc_array(ctx, char, 10240);
    for (int i = 0; i < 10239; i++) {
        large_content[i] = (char)('A' + (i % 26));
    }
    large_content[10239] = '\0';

    write(fd, large_content, 10239);
    close(fd);

    // Execute file_read
    res_t res = ik_tool_exec_file_read(ctx, test_file);
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
    yyjson_val *output = yyjson_obj_get(data, "output");
    const char *output_str = yyjson_get_str(output);
    ck_assert_ptr_nonnull(output_str);
    ck_assert_uint_eq(strlen(output_str), 10239);
    ck_assert_str_eq(output_str, large_content);

    yyjson_doc_free(doc);

    // Cleanup
    unlink(test_file);
}

END_TEST
// Test: file_read with file containing special characters
START_TEST(test_file_read_exec_special_characters)
{
    // Create temporary file with special characters
    char test_file[] = "/tmp/ikigai-file-read-test-XXXXXX";
    int fd = mkstemp(test_file);
    ck_assert(fd >= 0);

    const char *contents = "Line 1\nLine 2\tTabbed\r\nLine 3 with \"quotes\" and 'apostrophes'";
    write(fd, contents, strlen(contents));
    close(fd);

    // Execute file_read
    res_t res = ik_tool_exec_file_read(ctx, test_file);
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
    yyjson_val *output = yyjson_obj_get(data, "output");
    const char *output_str = yyjson_get_str(output);
    ck_assert_ptr_nonnull(output_str);
    ck_assert_str_eq(output_str, contents);

    yyjson_doc_free(doc);

    // Cleanup
    unlink(test_file);
}

END_TEST

// Test: file_read error paths via mocking
// Mock fseek to fail
static int mock_fseek_fail_count = 0;
static int mock_fseek_fail_on = -1;
int fseek_(FILE *stream, long offset, int whence)
{
    if (mock_fseek_fail_on >= 0 && mock_fseek_fail_count == mock_fseek_fail_on) {
        mock_fseek_fail_count++;
        return -1;
    }
    mock_fseek_fail_count++;
    return fseek(stream, offset, whence);
}

// Mock ftell to fail
static int mock_ftell_should_fail = 0;
long ftell_(FILE *stream)
{
    if (mock_ftell_should_fail) {
        return -1;
    }
    return ftell(stream);
}

// Mock fread to fail
static int mock_fread_should_fail = 0;
size_t fread_(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    if (mock_fread_should_fail) {
        return 0;  // Return 0 bytes read
    }
    return fread(ptr, size, nmemb, stream);
}

// Mock fopen to fail with specific errno
static int mock_fopen_errno = 0;
FILE *fopen_(const char *pathname, const char *mode)
{
    if (mock_fopen_errno != 0) {
        errno = mock_fopen_errno;
        return NULL;
    }
    return fopen(pathname, mode);
}

// Test: fseek error (first call)
START_TEST(test_file_read_exec_fseek_error) {
    char test_file[] = "/tmp/ikigai-file-read-test-XXXXXX";
    int fd = mkstemp(test_file);
    ck_assert(fd >= 0);
    write(fd, "test", 4);
    close(fd);

    mock_fseek_fail_count = 0;
    mock_fseek_fail_on = 0;  // Fail on first fseek call

    res_t res = ik_tool_exec_file_read(ctx, test_file);
    ck_assert(!res.is_err);

    char *json = res.ok;
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert(yyjson_get_bool(success) == false);

    yyjson_val *error = yyjson_obj_get(root, "error");
    const char *error_str = yyjson_get_str(error);
    ck_assert(strstr(error_str, "Cannot seek file") != NULL);

    yyjson_doc_free(doc);
    mock_fseek_fail_on = -1;
    unlink(test_file);
}
END_TEST
// Test: ftell error
START_TEST(test_file_read_exec_ftell_error)
{
    char test_file[] = "/tmp/ikigai-file-read-test-XXXXXX";
    int fd = mkstemp(test_file);
    ck_assert(fd >= 0);
    write(fd, "test", 4);
    close(fd);

    mock_ftell_should_fail = 1;

    res_t res = ik_tool_exec_file_read(ctx, test_file);
    ck_assert(!res.is_err);

    char *json = res.ok;
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert(yyjson_get_bool(success) == false);

    yyjson_val *error = yyjson_obj_get(root, "error");
    const char *error_str = yyjson_get_str(error);
    ck_assert(strstr(error_str, "Cannot get file size") != NULL);

    yyjson_doc_free(doc);
    mock_ftell_should_fail = 0;
    unlink(test_file);
}

END_TEST
// Test: second fseek error (rewind)
START_TEST(test_file_read_exec_rewind_error)
{
    char test_file[] = "/tmp/ikigai-file-read-test-XXXXXX";
    int fd = mkstemp(test_file);
    ck_assert(fd >= 0);
    write(fd, "test", 4);
    close(fd);

    mock_fseek_fail_count = 0;
    mock_fseek_fail_on = 1;  // Fail on second fseek call (rewind)

    res_t res = ik_tool_exec_file_read(ctx, test_file);
    ck_assert(!res.is_err);

    char *json = res.ok;
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert(yyjson_get_bool(success) == false);

    yyjson_val *error = yyjson_obj_get(root, "error");
    const char *error_str = yyjson_get_str(error);
    ck_assert(strstr(error_str, "Cannot seek file") != NULL);

    yyjson_doc_free(doc);
    mock_fseek_fail_on = -1;
    unlink(test_file);
}

END_TEST
// Test: fread error
START_TEST(test_file_read_exec_fread_error)
{
    char test_file[] = "/tmp/ikigai-file-read-test-XXXXXX";
    int fd = mkstemp(test_file);
    ck_assert(fd >= 0);
    write(fd, "test", 4);
    close(fd);

    mock_fread_should_fail = 1;

    res_t res = ik_tool_exec_file_read(ctx, test_file);
    ck_assert(!res.is_err);

    char *json = res.ok;
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert(yyjson_get_bool(success) == false);

    yyjson_val *error = yyjson_obj_get(root, "error");
    const char *error_str = yyjson_get_str(error);
    ck_assert(strstr(error_str, "Failed to read file") != NULL);

    yyjson_doc_free(doc);
    mock_fread_should_fail = 0;
    unlink(test_file);
}

END_TEST
// Test: fopen error with generic errno (not ENOENT or EACCES)
START_TEST(test_file_read_exec_generic_fopen_error)
{
    const char *test_file = "/tmp/test";

    mock_fopen_errno = ENOMEM;  // Out of memory error

    res_t res = ik_tool_exec_file_read(ctx, test_file);
    ck_assert(!res.is_err);

    char *json = res.ok;
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert(yyjson_get_bool(success) == false);

    yyjson_val *error = yyjson_obj_get(root, "error");
    const char *error_str = yyjson_get_str(error);
    ck_assert(strstr(error_str, "Cannot open file") != NULL);

    yyjson_doc_free(doc);
    mock_fopen_errno = 0;
}

END_TEST

// Test suite
static Suite *file_read_execute_suite(void)
{
    Suite *s = suite_create("File Read Execution");

    TCase *tc_file_read_exec = tcase_create("File Read Execution");
    tcase_add_checked_fixture(tc_file_read_exec, setup, teardown);
    tcase_add_test(tc_file_read_exec, test_file_read_exec_valid_file);
    tcase_add_test(tc_file_read_exec, test_file_read_exec_file_not_found);
    tcase_add_test(tc_file_read_exec, test_file_read_exec_permission_denied);
    tcase_add_test(tc_file_read_exec, test_file_read_exec_empty_file);
    tcase_add_test(tc_file_read_exec, test_file_read_exec_large_file);
    tcase_add_test(tc_file_read_exec, test_file_read_exec_special_characters);
    tcase_add_test(tc_file_read_exec, test_file_read_exec_fseek_error);
    tcase_add_test(tc_file_read_exec, test_file_read_exec_ftell_error);
    tcase_add_test(tc_file_read_exec, test_file_read_exec_rewind_error);
    tcase_add_test(tc_file_read_exec, test_file_read_exec_fread_error);
    tcase_add_test(tc_file_read_exec, test_file_read_exec_generic_fopen_error);
    suite_add_tcase(s, tc_file_read_exec);

    return s;
}

int main(void)
{
    Suite *s = file_read_execute_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int32_t number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
