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

// Test: file_write with valid path and content creates file successfully
START_TEST(test_file_write_exec_valid) {
    char test_file[] = "/tmp/ikigai-file-write-test-XXXXXX";
    int fd = mkstemp(test_file);
    ck_assert(fd >= 0);
    close(fd);
    unlink(test_file);  // Remove so we can test creation

    const char *content = "Remember to refactor";

    // Execute file_write
    res_t res = ik_tool_exec_file_write(ctx, test_file, content);
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

    // Verify output message
    yyjson_val *output = yyjson_obj_get(data, "output");
    ck_assert_ptr_nonnull(output);
    const char *output_str = yyjson_get_str(output);
    ck_assert_ptr_nonnull(output_str);
    ck_assert(strstr(output_str, "Wrote") != NULL);
    ck_assert(strstr(output_str, "20") != NULL);
    ck_assert(strstr(output_str, "bytes") != NULL);

    // Verify bytes field
    yyjson_val *bytes = yyjson_obj_get(data, "bytes");
    ck_assert_ptr_nonnull(bytes);
    ck_assert_uint_eq(yyjson_get_uint(bytes), 20);

    yyjson_doc_free(doc);

    // Verify file was actually created with correct contents
    FILE *f = fopen(test_file, "r");
    ck_assert_ptr_nonnull(f);
    char buffer[100];
    size_t read_bytes = fread(buffer, 1, sizeof(buffer), f);
    buffer[read_bytes] = '\0';
    ck_assert_str_eq(buffer, content);
    fclose(f);

    // Cleanup
    unlink(test_file);
}
END_TEST
// Test: file_write with empty content creates empty file
START_TEST(test_file_write_exec_empty_content)
{
    char test_file[] = "/tmp/ikigai-file-write-test-XXXXXX";
    int fd = mkstemp(test_file);
    ck_assert(fd >= 0);
    close(fd);
    unlink(test_file);

    const char *content = "";

    // Execute file_write
    res_t res = ik_tool_exec_file_write(ctx, test_file, content);
    ck_assert(!res.is_err);

    char *json = res.ok;
    ck_assert_ptr_nonnull(json);

    // Parse result JSON
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    // Verify success: true
    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert(yyjson_get_bool(success) == true);

    // Verify bytes field is 0
    yyjson_val *data = yyjson_obj_get(root, "data");
    yyjson_val *bytes = yyjson_obj_get(data, "bytes");
    ck_assert_uint_eq(yyjson_get_uint(bytes), 0);

    // Verify output message mentions 0 bytes
    yyjson_val *output = yyjson_obj_get(data, "output");
    const char *output_str = yyjson_get_str(output);
    ck_assert(strstr(output_str, "Wrote 0 bytes") != NULL);

    yyjson_doc_free(doc);

    // Verify file was created and is empty
    struct stat st;
    ck_assert_int_eq(stat(test_file, &st), 0);
    ck_assert_int_eq(st.st_size, 0);

    // Cleanup
    unlink(test_file);
}

END_TEST
// Test: file_write overwrites existing file
START_TEST(test_file_write_exec_overwrite)
{
    char test_file[] = "/tmp/ikigai-file-write-test-XXXXXX";
    int fd = mkstemp(test_file);
    ck_assert(fd >= 0);

    // Write initial content
    const char *old_content = "Old content that will be replaced";
    write(fd, old_content, strlen(old_content));
    close(fd);

    const char *new_content = "New content";

    // Execute file_write
    res_t res = ik_tool_exec_file_write(ctx, test_file, new_content);
    ck_assert(!res.is_err);

    char *json = res.ok;
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert(yyjson_get_bool(success) == true);

    yyjson_val *data = yyjson_obj_get(root, "data");
    yyjson_val *bytes = yyjson_obj_get(data, "bytes");
    ck_assert_uint_eq(yyjson_get_uint(bytes), strlen(new_content));

    yyjson_doc_free(doc);

    // Verify file contains new content only
    FILE *f = fopen(test_file, "r");
    char buffer[100];
    size_t read_bytes = fread(buffer, 1, sizeof(buffer), f);
    buffer[read_bytes] = '\0';
    ck_assert_str_eq(buffer, new_content);
    fclose(f);

    // Cleanup
    unlink(test_file);
}

END_TEST
// Test: file_write to read-only location returns error
START_TEST(test_file_write_exec_permission_denied)
{
    const char *readonly_path = "/proc/version";  // Read-only system file

    const char *content = "This should fail";

    // Execute file_write
    res_t res = ik_tool_exec_file_write(ctx, readonly_path, content);
    ck_assert(!res.is_err);

    char *json = res.ok;
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    // Verify success: false
    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert_ptr_nonnull(success);
    ck_assert(yyjson_get_bool(success) == false);

    // Verify error message mentions permission denied
    yyjson_val *error = yyjson_obj_get(root, "error");
    ck_assert_ptr_nonnull(error);
    const char *error_str = yyjson_get_str(error);
    ck_assert_ptr_nonnull(error_str);
    ck_assert(strstr(error_str, "Permission denied") != NULL || strstr(error_str, readonly_path) != NULL);

    yyjson_doc_free(doc);
}

END_TEST
// Test: file_write with large content
START_TEST(test_file_write_exec_large_content)
{
    char test_file[] = "/tmp/ikigai-file-write-test-XXXXXX";
    int fd = mkstemp(test_file);
    ck_assert(fd >= 0);
    close(fd);
    unlink(test_file);

    // Create large content (10KB)
    char *large_content = talloc_array(ctx, char, 10240);
    for (int i = 0; i < 10239; i++) {
        large_content[i] = (char)('A' + (i % 26));
    }
    large_content[10239] = '\0';

    // Execute file_write
    res_t res = ik_tool_exec_file_write(ctx, test_file, large_content);
    ck_assert(!res.is_err);

    char *json = res.ok;
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert(yyjson_get_bool(success) == true);

    yyjson_val *data = yyjson_obj_get(root, "data");
    yyjson_val *bytes = yyjson_obj_get(data, "bytes");
    ck_assert_uint_eq(yyjson_get_uint(bytes), 10239);

    yyjson_doc_free(doc);

    // Verify file size
    struct stat st;
    ck_assert_int_eq(stat(test_file, &st), 0);
    ck_assert_int_eq(st.st_size, 10239);

    // Cleanup
    unlink(test_file);
}

END_TEST
// Test: file_write with special characters
START_TEST(test_file_write_exec_special_characters)
{
    char test_file[] = "/tmp/ikigai-file-write-test-XXXXXX";
    int fd = mkstemp(test_file);
    ck_assert(fd >= 0);
    close(fd);
    unlink(test_file);

    const char *content = "Line 1\nLine 2\tTabbed\r\nLine 3 with \"quotes\" and 'apostrophes'";

    // Execute file_write
    res_t res = ik_tool_exec_file_write(ctx, test_file, content);
    ck_assert(!res.is_err);

    char *json = res.ok;
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert(yyjson_get_bool(success) == true);

    yyjson_doc_free(doc);

    // Verify file contains exact content
    FILE *f = fopen(test_file, "r");
    char buffer[200];
    size_t read_bytes = fread(buffer, 1, sizeof(buffer), f);
    buffer[read_bytes] = '\0';
    ck_assert_str_eq(buffer, content);
    fclose(f);

    // Cleanup
    unlink(test_file);
}

END_TEST

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

// Test: fopen error with EACCES
START_TEST(test_file_write_exec_eacces_error) {
    const char *test_file = "/tmp/test";

    mock_fopen_errno = EACCES;

    res_t res = ik_tool_exec_file_write(ctx, test_file, "content");
    ck_assert(!res.is_err);

    char *json = res.ok;
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert(yyjson_get_bool(success) == false);

    yyjson_val *error = yyjson_obj_get(root, "error");
    const char *error_str = yyjson_get_str(error);
    ck_assert(strstr(error_str, "Permission denied") != NULL);

    yyjson_doc_free(doc);
    mock_fopen_errno = 0;
}
END_TEST
// Test: fopen error with ENOSPC
START_TEST(test_file_write_exec_enospc_error)
{
    const char *test_file = "/tmp/test";

    mock_fopen_errno = ENOSPC;

    res_t res = ik_tool_exec_file_write(ctx, test_file, "content");
    ck_assert(!res.is_err);

    char *json = res.ok;
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert(yyjson_get_bool(success) == false);

    yyjson_val *error = yyjson_obj_get(root, "error");
    const char *error_str = yyjson_get_str(error);
    ck_assert(strstr(error_str, "No space left") != NULL);

    yyjson_doc_free(doc);
    mock_fopen_errno = 0;
}

END_TEST
// Test: fopen error with generic errno
START_TEST(test_file_write_exec_generic_error)
{
    const char *test_file = "/tmp/test";

    mock_fopen_errno = ENOMEM;

    res_t res = ik_tool_exec_file_write(ctx, test_file, "content");
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

// Mock fwrite to fail
static int mock_fwrite_should_fail = 0;
size_t fwrite_(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    if (mock_fwrite_should_fail) {
        return 0;  // Return 0 bytes written
    }
    return fwrite(ptr, size, nmemb, stream);
}

// Test: fwrite error
START_TEST(test_file_write_exec_fwrite_error) {
    char test_file[] = "/tmp/ikigai-file-write-test-XXXXXX";
    int fd = mkstemp(test_file);
    ck_assert(fd >= 0);
    close(fd);
    unlink(test_file);

    mock_fwrite_should_fail = 1;

    res_t res = ik_tool_exec_file_write(ctx, test_file, "test content");
    ck_assert(!res.is_err);

    char *json = res.ok;
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert(yyjson_get_bool(success) == false);

    yyjson_val *error = yyjson_obj_get(root, "error");
    const char *error_str = yyjson_get_str(error);
    ck_assert(strstr(error_str, "Failed to write file") != NULL);

    yyjson_doc_free(doc);
    mock_fwrite_should_fail = 0;

    // Cleanup
    unlink(test_file);
}
END_TEST

// Test suite
static Suite *file_write_execute_suite(void)
{
    Suite *s = suite_create("File Write Execution");

    TCase *tc_file_write_exec = tcase_create("File Write Execution");
    tcase_add_checked_fixture(tc_file_write_exec, setup, teardown);
    tcase_add_test(tc_file_write_exec, test_file_write_exec_valid);
    tcase_add_test(tc_file_write_exec, test_file_write_exec_empty_content);
    tcase_add_test(tc_file_write_exec, test_file_write_exec_overwrite);
    tcase_add_test(tc_file_write_exec, test_file_write_exec_permission_denied);
    tcase_add_test(tc_file_write_exec, test_file_write_exec_large_content);
    tcase_add_test(tc_file_write_exec, test_file_write_exec_special_characters);
    tcase_add_test(tc_file_write_exec, test_file_write_exec_eacces_error);
    tcase_add_test(tc_file_write_exec, test_file_write_exec_enospc_error);
    tcase_add_test(tc_file_write_exec, test_file_write_exec_generic_error);
    tcase_add_test(tc_file_write_exec, test_file_write_exec_fwrite_error);
    suite_add_tcase(s, tc_file_write_exec);

    return s;
}

int main(void)
{
    Suite *s = file_write_execute_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int32_t number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
