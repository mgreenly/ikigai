#include <check.h>
#include <errno.h>
#include <limits.h>
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

    yyjson_doc *doc = NULL;
    yyjson_val *data = ik_test_tool_parse_success(res.ok, &doc);
    const char *output = ik_test_tool_get_output(data);
    ck_assert_str_eq(output, contents);

    yyjson_doc_free(doc);

    // Cleanup
    unlink(test_file);
}
END_TEST
// Test: file_read with non-existent file returns error
START_TEST(test_file_read_exec_file_not_found) {
    const char *nonexistent = "/tmp/ikigai-file-read-nonexistent-xyz123.txt";

    // Execute file_read on non-existent file
    res_t res = ik_tool_exec_file_read(ctx, nonexistent);
    ck_assert(!res.is_err);

    yyjson_doc *doc = NULL;
    const char *error = ik_test_tool_parse_error(res.ok, &doc);
    ck_assert(strstr(error, "File not found") != NULL || strstr(error, nonexistent) != NULL);

    yyjson_doc_free(doc);
}

END_TEST
// Test: file_read with unreadable file returns error
START_TEST(test_file_read_exec_permission_denied) {
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

    yyjson_doc *doc = NULL;
    const char *error = ik_test_tool_parse_error(res.ok, &doc);
    ck_assert(strstr(error, "Permission denied") != NULL || strstr(error, test_file) != NULL);

    yyjson_doc_free(doc);

    // Restore permissions for cleanup
    chmod(test_file, 0644);
    unlink(test_file);
}

END_TEST
// Test: file_read with empty file returns empty output
START_TEST(test_file_read_exec_empty_file) {
    // Create temporary empty file
    char test_file[] = "/tmp/ikigai-file-read-test-XXXXXX";
    int fd = mkstemp(test_file);
    ck_assert(fd >= 0);
    close(fd);

    // Execute file_read
    res_t res = ik_tool_exec_file_read(ctx, test_file);
    ck_assert(!res.is_err);

    yyjson_doc *doc = NULL;
    yyjson_val *data = ik_test_tool_parse_success(res.ok, &doc);
    const char *output = ik_test_tool_get_output(data);
    ck_assert_str_eq(output, "");

    yyjson_doc_free(doc);

    // Cleanup
    unlink(test_file);
}

END_TEST
// Test: file_read with large file works correctly
START_TEST(test_file_read_exec_large_file) {
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

    yyjson_doc *doc = NULL;
    yyjson_val *data = ik_test_tool_parse_success(res.ok, &doc);
    const char *output = ik_test_tool_get_output(data);
    ck_assert_uint_eq(strlen(output), 10239);
    ck_assert_str_eq(output, large_content);

    yyjson_doc_free(doc);

    // Cleanup
    unlink(test_file);
}

END_TEST
// Test: file_read with file containing special characters
START_TEST(test_file_read_exec_special_characters) {
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

    yyjson_doc *doc = NULL;
    yyjson_val *data = ik_test_tool_parse_success(res.ok, &doc);
    const char *output = ik_test_tool_get_output(data);
    ck_assert_str_eq(output, contents);

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

// Mock ftell to fail or return large value
static int mock_ftell_should_fail = 0;
static long mock_ftell_large_value = -1;
long ftell_(FILE *stream)
{
    if (mock_ftell_should_fail) {
        return -1;
    }
    if (mock_ftell_large_value >= 0) {
        return mock_ftell_large_value;
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

    yyjson_doc *doc = NULL;
    const char *error = ik_test_tool_parse_error(res.ok, &doc);
    ck_assert(strstr(error, "Cannot seek file") != NULL);

    yyjson_doc_free(doc);
    mock_fseek_fail_on = -1;
    unlink(test_file);
}
END_TEST
// Test: ftell error
START_TEST(test_file_read_exec_ftell_error) {
    char test_file[] = "/tmp/ikigai-file-read-test-XXXXXX";
    int fd = mkstemp(test_file);
    ck_assert(fd >= 0);
    write(fd, "test", 4);
    close(fd);

    mock_ftell_should_fail = 1;

    res_t res = ik_tool_exec_file_read(ctx, test_file);
    ck_assert(!res.is_err);

    yyjson_doc *doc = NULL;
    const char *error = ik_test_tool_parse_error(res.ok, &doc);
    ck_assert(strstr(error, "Cannot get file size") != NULL);

    yyjson_doc_free(doc);
    mock_ftell_should_fail = 0;
    unlink(test_file);
}

END_TEST
// Test: second fseek error (rewind)
START_TEST(test_file_read_exec_rewind_error) {
    char test_file[] = "/tmp/ikigai-file-read-test-XXXXXX";
    int fd = mkstemp(test_file);
    ck_assert(fd >= 0);
    write(fd, "test", 4);
    close(fd);

    mock_fseek_fail_count = 0;
    mock_fseek_fail_on = 1;  // Fail on second fseek call (rewind)

    res_t res = ik_tool_exec_file_read(ctx, test_file);
    ck_assert(!res.is_err);

    yyjson_doc *doc = NULL;
    const char *error = ik_test_tool_parse_error(res.ok, &doc);
    ck_assert(strstr(error, "Cannot seek file") != NULL);

    yyjson_doc_free(doc);
    mock_fseek_fail_on = -1;
    unlink(test_file);
}

END_TEST
// Test: fread error
START_TEST(test_file_read_exec_fread_error) {
    char test_file[] = "/tmp/ikigai-file-read-test-XXXXXX";
    int fd = mkstemp(test_file);
    ck_assert(fd >= 0);
    write(fd, "test", 4);
    close(fd);

    mock_fread_should_fail = 1;

    res_t res = ik_tool_exec_file_read(ctx, test_file);
    ck_assert(!res.is_err);

    yyjson_doc *doc = NULL;
    const char *error = ik_test_tool_parse_error(res.ok, &doc);
    ck_assert(strstr(error, "Failed to read file") != NULL);

    yyjson_doc_free(doc);
    mock_fread_should_fail = 0;
    unlink(test_file);
}

END_TEST
// Test: fopen error with generic errno (not ENOENT or EACCES)
START_TEST(test_file_read_exec_generic_fopen_error) {
    const char *test_file = "/tmp/test";

    mock_fopen_errno = ENOMEM;  // Out of memory error

    res_t res = ik_tool_exec_file_read(ctx, test_file);
    ck_assert(!res.is_err);

    yyjson_doc *doc = NULL;
    const char *error = ik_test_tool_parse_error(res.ok, &doc);
    ck_assert(strstr(error, "Cannot open file") != NULL);

    yyjson_doc_free(doc);
    mock_fopen_errno = 0;
}

END_TEST
// Test: file too large error (generic error path)
START_TEST(test_file_read_exec_file_too_large) {
    char test_file[] = "/tmp/ikigai-file-read-test-XXXXXX";
    int fd = mkstemp(test_file);
    ck_assert(fd >= 0);
    write(fd, "test", 4);
    close(fd);

    // Mock ftell to return a size that triggers "File too large"
    // UINT_MAX is typically 4294967295, so we return that value
    mock_ftell_large_value = (long)UINT_MAX;

    res_t res = ik_tool_exec_file_read(ctx, test_file);
    ck_assert(!res.is_err);

    yyjson_doc *doc = NULL;
    const char *error = ik_test_tool_parse_error(res.ok, &doc);
    // This should hit the generic error path and pass through the original message
    ck_assert(strstr(error, "File too large") != NULL);

    yyjson_doc_free(doc);
    mock_ftell_large_value = -1;
    unlink(test_file);
}

END_TEST

// Test suite
static Suite *file_read_execute_suite(void)
{
    Suite *s = suite_create("File Read Execution");

    TCase *tc_file_read_exec = tcase_create("File Read Execution");
    tcase_set_timeout(tc_file_read_exec, IK_TEST_TIMEOUT);
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
    tcase_add_test(tc_file_read_exec, test_file_read_exec_file_too_large);
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
