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

    yyjson_doc *doc = NULL;
    yyjson_val *data = ik_test_tool_parse_success(res.ok, &doc);
    const char *output = ik_test_tool_get_output(data);

    // Verify output message
    ck_assert(strstr(output, "Wrote") != NULL);
    ck_assert(strstr(output, "20") != NULL);
    ck_assert(strstr(output, "bytes") != NULL);

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

    yyjson_doc *doc = NULL;
    yyjson_val *data = ik_test_tool_parse_success(res.ok, &doc);
    const char *output = ik_test_tool_get_output(data);

    // Verify bytes field is 0
    yyjson_val *bytes = yyjson_obj_get(data, "bytes");
    ck_assert_uint_eq(yyjson_get_uint(bytes), 0);

    // Verify output message mentions 0 bytes
    ck_assert(strstr(output, "Wrote 0 bytes") != NULL);

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

    yyjson_doc *doc = NULL;
    yyjson_val *data = ik_test_tool_parse_success(res.ok, &doc);

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

    yyjson_doc *doc = NULL;
    const char *error = ik_test_tool_parse_error(res.ok, &doc);

    // Verify error message mentions permission denied
    ck_assert(strstr(error, "Permission denied") != NULL || strstr(error, readonly_path) != NULL);

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

    yyjson_doc *doc = NULL;
    yyjson_val *data = ik_test_tool_parse_success(res.ok, &doc);

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

    yyjson_doc *doc = NULL;
    ik_test_tool_parse_success(res.ok, &doc);

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

    yyjson_doc *doc = NULL;
    const char *error = ik_test_tool_parse_error(res.ok, &doc);
    ck_assert(strstr(error, "Permission denied") != NULL);

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

    yyjson_doc *doc = NULL;
    const char *error = ik_test_tool_parse_error(res.ok, &doc);
    ck_assert(strstr(error, "No space left") != NULL);

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

    yyjson_doc *doc = NULL;
    const char *error = ik_test_tool_parse_error(res.ok, &doc);
    ck_assert(strstr(error, "Cannot open file") != NULL);

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

    yyjson_doc *doc = NULL;
    const char *error = ik_test_tool_parse_error(res.ok, &doc);
    ck_assert(strstr(error, "Failed to write file") != NULL);

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
    tcase_set_timeout(tc_file_write_exec, 30);
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
