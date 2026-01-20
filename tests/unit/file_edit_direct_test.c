/**
 * @file file_edit_direct_test.c
 * @brief Direct unit tests for file_edit logic with mocking
 */
#include "../test_constants.h"

#include "tools/file_edit/file_edit.h"

#include <check.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <talloc.h>
#include <unistd.h>

#include "json_allocator.h"
#include "panic.h"
#include "wrapper.h"

static TALLOC_CTX *test_ctx;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

// Mock control variables
static FILE *(*mock_fopen_impl)(const char *path, const char *mode) = NULL;
static size_t (*mock_fwrite_impl)(const void *ptr, size_t size, size_t nmemb, FILE *stream) = NULL;

FILE *fopen_(const char *path, const char *mode)
{
    if (mock_fopen_impl != NULL) {
        return mock_fopen_impl(path, mode);
    }
    return fopen(path, mode);
}

size_t fwrite_(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    if (mock_fwrite_impl != NULL) {
        return mock_fwrite_impl(ptr, size, nmemb, stream);
    }
    return fwrite(ptr, size, nmemb, stream);
}

// Test 1: fopen read failure - ENOENT
static FILE *fopen_enoent(const char *path, const char *mode)
{
    (void)path;
    (void)mode;
    errno = ENOENT;
    return NULL;
}

START_TEST(test_fopen_read_enoent) {
    mock_fopen_impl = fopen_enoent;

    char output[4096] = {0};
    FILE *mem_stream = fmemopen(output, sizeof(output), "w");
    ck_assert_ptr_nonnull(mem_stream);

    FILE *old_stdout = stdout;
    stdout = mem_stream;

    file_edit_params_t params = {
        .file_path = "/nonexistent/file.txt",
        .old_string = "foo",
        .new_string = "bar",
        .replace_all = false,
    };

    int32_t result = file_edit_execute(test_ctx, &params);

    fclose(mem_stream);
    stdout = old_stdout;

    ck_assert_int_eq(result, 0);
    ck_assert(strstr(output, "\"error_code\":\"FILE_NOT_FOUND\"") != NULL);

    mock_fopen_impl = NULL;
}
END_TEST

// Test 2: Empty old_string validation
START_TEST(test_empty_old_string) {
    char output[4096] = {0};
    FILE *mem_stream = fmemopen(output, sizeof(output), "w");
    ck_assert_ptr_nonnull(mem_stream);

    FILE *old_stdout = stdout;
    stdout = mem_stream;

    file_edit_params_t params = {
        .file_path = "test.txt",
        .old_string = "",
        .new_string = "bar",
        .replace_all = false,
    };

    int32_t result = file_edit_execute(test_ctx, &params);

    fclose(mem_stream);
    stdout = old_stdout;

    ck_assert_int_eq(result, 0);
    ck_assert(strstr(output, "\"error_code\":\"INVALID_ARG\"") != NULL);
    ck_assert(strstr(output, "old_string cannot be empty") != NULL);
}
END_TEST

// Test 3: Identical old/new strings
START_TEST(test_identical_strings) {
    char output[4096] = {0};
    FILE *mem_stream = fmemopen(output, sizeof(output), "w");
    ck_assert_ptr_nonnull(mem_stream);

    FILE *old_stdout = stdout;
    stdout = mem_stream;

    file_edit_params_t params = {
        .file_path = "test.txt",
        .old_string = "foo",
        .new_string = "foo",
        .replace_all = false,
    };

    int32_t result = file_edit_execute(test_ctx, &params);

    fclose(mem_stream);
    stdout = old_stdout;

    ck_assert_int_eq(result, 0);
    ck_assert(strstr(output, "\"error_code\":\"INVALID_ARG\"") != NULL);
    ck_assert(strstr(output, "identical") != NULL);
}
END_TEST

// Test 4: fopen read failure - EACCES
static FILE *fopen_eacces(const char *path, const char *mode)
{
    (void)path;
    (void)mode;
    errno = EACCES;
    return NULL;
}

START_TEST(test_fopen_read_eacces) {
    mock_fopen_impl = fopen_eacces;

    char output[4096] = {0};
    FILE *mem_stream = fmemopen(output, sizeof(output), "w");
    ck_assert_ptr_nonnull(mem_stream);

    FILE *old_stdout = stdout;
    stdout = mem_stream;

    file_edit_params_t params = {
        .file_path = "/root/secret.txt",
        .old_string = "foo",
        .new_string = "bar",
        .replace_all = false,
    };

    int32_t result = file_edit_execute(test_ctx, &params);

    fclose(mem_stream);
    stdout = old_stdout;

    ck_assert_int_eq(result, 0);
    ck_assert(strstr(output, "\"error_code\":\"PERMISSION_DENIED\"") != NULL);

    mock_fopen_impl = NULL;
}
END_TEST

// Test 5: fopen read failure - other error
static FILE *fopen_other(const char *path, const char *mode)
{
    (void)path;
    (void)mode;
    errno = EIO;
    return NULL;
}

START_TEST(test_fopen_read_other) {
    mock_fopen_impl = fopen_other;

    char output[4096] = {0};
    FILE *mem_stream = fmemopen(output, sizeof(output), "w");
    ck_assert_ptr_nonnull(mem_stream);

    FILE *old_stdout = stdout;
    stdout = mem_stream;

    file_edit_params_t params = {
        .file_path = "/some/file.txt",
        .old_string = "foo",
        .new_string = "bar",
        .replace_all = false,
    };

    int32_t result = file_edit_execute(test_ctx, &params);

    fclose(mem_stream);
    stdout = old_stdout;

    ck_assert_int_eq(result, 0);
    ck_assert(strstr(output, "\"error_code\":\"OPEN_FAILED\"") != NULL);

    mock_fopen_impl = NULL;
}
END_TEST

// Test 6: Success with single replacement
START_TEST(test_success_single_replacement) {
    // Create temp file with content
    char temp_path[] = "/tmp/file_edit_test_XXXXXX";
    int fd = mkstemp(temp_path);
    ck_assert(fd >= 0);

    const char *initial_content = "hello world";
    ssize_t written = write(fd, initial_content, strlen(initial_content));
    ck_assert_int_eq(written, (ssize_t)strlen(initial_content));
    close(fd);

    char output[4096] = {0};
    FILE *mem_stream = fmemopen(output, sizeof(output), "w");
    ck_assert_ptr_nonnull(mem_stream);

    FILE *old_stdout = stdout;
    stdout = mem_stream;

    file_edit_params_t params = {
        .file_path = temp_path,
        .old_string = "world",
        .new_string = "universe",
        .replace_all = false,
    };

    int32_t result = file_edit_execute(test_ctx, &params);

    fclose(mem_stream);
    stdout = old_stdout;

    ck_assert_int_eq(result, 0);
    ck_assert(strstr(output, "\"replacements\":1") != NULL);
    ck_assert(strstr(output, "\"output\":") != NULL);

    // Verify file contents changed
    FILE *fp = fopen(temp_path, "r");
    ck_assert_ptr_nonnull(fp);
    char buf[256];
    size_t n = fread(buf, 1, sizeof(buf), fp);
    buf[n] = '\0';
    fclose(fp);
    ck_assert_str_eq(buf, "hello universe");

    unlink(temp_path);
}
END_TEST

// Test 7: Success with multiple replacements
START_TEST(test_success_multiple_replacements) {
    // Create temp file with content
    char temp_path[] = "/tmp/file_edit_test_XXXXXX";
    int fd = mkstemp(temp_path);
    ck_assert(fd >= 0);

    const char *initial_content = "foo bar foo baz foo";
    ssize_t written = write(fd, initial_content, strlen(initial_content));
    ck_assert_int_eq(written, (ssize_t)strlen(initial_content));
    close(fd);

    char output[4096] = {0};
    FILE *mem_stream = fmemopen(output, sizeof(output), "w");
    ck_assert_ptr_nonnull(mem_stream);

    FILE *old_stdout = stdout;
    stdout = mem_stream;

    file_edit_params_t params = {
        .file_path = temp_path,
        .old_string = "foo",
        .new_string = "FOO",
        .replace_all = true,
    };

    int32_t result = file_edit_execute(test_ctx, &params);

    fclose(mem_stream);
    stdout = old_stdout;

    ck_assert_int_eq(result, 0);
    ck_assert(strstr(output, "\"replacements\":3") != NULL);

    // Verify file contents changed
    FILE *fp = fopen(temp_path, "r");
    ck_assert_ptr_nonnull(fp);
    char buf[256] = {0};
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);

    const char *expected = "FOO bar FOO baz FOO";
    ck_assert(n >= strlen(expected));
    ck_assert_int_eq(memcmp(buf, expected, strlen(expected)), 0);

    unlink(temp_path);
}
END_TEST

// Test 8: String not found
START_TEST(test_string_not_found) {
    // Create temp file with content
    char temp_path[] = "/tmp/file_edit_test_XXXXXX";
    int fd = mkstemp(temp_path);
    ck_assert(fd >= 0);

    const char *initial_content = "hello world";
    ssize_t written = write(fd, initial_content, strlen(initial_content));
    ck_assert_int_eq(written, (ssize_t)strlen(initial_content));
    close(fd);

    char output[4096] = {0};
    FILE *mem_stream = fmemopen(output, sizeof(output), "w");
    ck_assert_ptr_nonnull(mem_stream);

    FILE *old_stdout = stdout;
    stdout = mem_stream;

    file_edit_params_t params = {
        .file_path = temp_path,
        .old_string = "notfound",
        .new_string = "replacement",
        .replace_all = false,
    };

    int32_t result = file_edit_execute(test_ctx, &params);

    fclose(mem_stream);
    stdout = old_stdout;

    ck_assert_int_eq(result, 0);
    ck_assert(strstr(output, "\"error_code\":\"NOT_FOUND\"") != NULL);

    unlink(temp_path);
}
END_TEST

// Test 9: Multiple matches without replace_all
START_TEST(test_multiple_without_replace_all) {
    // Create temp file with content
    char temp_path[] = "/tmp/file_edit_test_XXXXXX";
    int fd = mkstemp(temp_path);
    ck_assert(fd >= 0);

    const char *initial_content = "foo bar foo baz";
    ssize_t written = write(fd, initial_content, strlen(initial_content));
    ck_assert_int_eq(written, (ssize_t)strlen(initial_content));
    close(fd);

    char output[4096] = {0};
    FILE *mem_stream = fmemopen(output, sizeof(output), "w");
    ck_assert_ptr_nonnull(mem_stream);

    FILE *old_stdout = stdout;
    stdout = mem_stream;

    file_edit_params_t params = {
        .file_path = temp_path,
        .old_string = "foo",
        .new_string = "FOO",
        .replace_all = false,
    };

    int32_t result = file_edit_execute(test_ctx, &params);

    fclose(mem_stream);
    stdout = old_stdout;

    ck_assert_int_eq(result, 0);
    ck_assert(strstr(output, "\"error_code\":\"NOT_UNIQUE\"") != NULL);
    ck_assert(strstr(output, "found 2 times") != NULL);

    unlink(temp_path);
}
END_TEST

// Test 10: fopen write failure - EACCES
static int fopen_write_call_count = 0;
static FILE *fopen_write_eacces(const char *path, const char *mode)
{
    fopen_write_call_count++;
    // First call (read) succeeds, second call (write) fails
    if (fopen_write_call_count == 1) {
        return fopen(path, mode);
    }
    errno = EACCES;
    return NULL;
}

START_TEST(test_fopen_write_eacces) {
    // Create temp file with content
    char temp_path[] = "/tmp/file_edit_test_XXXXXX";
    int fd = mkstemp(temp_path);
    ck_assert(fd >= 0);

    const char *initial_content = "hello world";
    ssize_t written = write(fd, initial_content, strlen(initial_content));
    ck_assert_int_eq(written, (ssize_t)strlen(initial_content));
    close(fd);

    fopen_write_call_count = 0;
    mock_fopen_impl = fopen_write_eacces;

    char output[4096] = {0};
    FILE *mem_stream = fmemopen(output, sizeof(output), "w");
    ck_assert_ptr_nonnull(mem_stream);

    FILE *old_stdout = stdout;
    stdout = mem_stream;

    file_edit_params_t params = {
        .file_path = temp_path,
        .old_string = "world",
        .new_string = "universe",
        .replace_all = false,
    };

    int32_t result = file_edit_execute(test_ctx, &params);

    fclose(mem_stream);
    stdout = old_stdout;

    ck_assert_int_eq(result, 0);
    ck_assert(strstr(output, "\"error_code\":\"PERMISSION_DENIED\"") != NULL);
    ck_assert(strstr(output, "Permission denied") != NULL);

    mock_fopen_impl = NULL;
    unlink(temp_path);
}
END_TEST

// Test 11: fopen write failure - other error
static FILE *fopen_write_other(const char *path, const char *mode)
{
    fopen_write_call_count++;
    // First call (read) succeeds, second call (write) fails
    if (fopen_write_call_count == 1) {
        return fopen(path, mode);
    }
    errno = EIO;
    return NULL;
}

START_TEST(test_fopen_write_other) {
    // Create temp file with content
    char temp_path[] = "/tmp/file_edit_test_XXXXXX";
    int fd = mkstemp(temp_path);
    ck_assert(fd >= 0);

    const char *initial_content = "hello world";
    ssize_t written = write(fd, initial_content, strlen(initial_content));
    ck_assert_int_eq(written, (ssize_t)strlen(initial_content));
    close(fd);

    fopen_write_call_count = 0;
    mock_fopen_impl = fopen_write_other;

    char output[4096] = {0};
    FILE *mem_stream = fmemopen(output, sizeof(output), "w");
    ck_assert_ptr_nonnull(mem_stream);

    FILE *old_stdout = stdout;
    stdout = mem_stream;

    file_edit_params_t params = {
        .file_path = temp_path,
        .old_string = "world",
        .new_string = "universe",
        .replace_all = false,
    };

    int32_t result = file_edit_execute(test_ctx, &params);

    fclose(mem_stream);
    stdout = old_stdout;

    ck_assert_int_eq(result, 0);
    ck_assert(strstr(output, "\"error_code\":\"OPEN_FAILED\"") != NULL);

    mock_fopen_impl = NULL;
    unlink(temp_path);
}
END_TEST

// Test 12: fwrite failure
static size_t fwrite_fail(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    (void)ptr;
    (void)size;
    (void)nmemb;
    (void)stream;
    return 0;  // Write failure
}

START_TEST(test_fwrite_failure) {
    // Create temp file with content
    char temp_path[] = "/tmp/file_edit_test_XXXXXX";
    int fd = mkstemp(temp_path);
    ck_assert(fd >= 0);

    const char *initial_content = "hello world";
    ssize_t written = write(fd, initial_content, strlen(initial_content));
    ck_assert_int_eq(written, (ssize_t)strlen(initial_content));
    close(fd);

    mock_fwrite_impl = fwrite_fail;

    char output[4096] = {0};
    FILE *mem_stream = fmemopen(output, sizeof(output), "w");
    ck_assert_ptr_nonnull(mem_stream);

    FILE *old_stdout = stdout;
    stdout = mem_stream;

    file_edit_params_t params = {
        .file_path = temp_path,
        .old_string = "world",
        .new_string = "universe",
        .replace_all = false,
    };

    int32_t result = file_edit_execute(test_ctx, &params);

    fclose(mem_stream);
    stdout = old_stdout;

    ck_assert_int_eq(result, 0);
    ck_assert(strstr(output, "\"error_code\":\"WRITE_FAILED\"") != NULL);

    mock_fwrite_impl = NULL;
    unlink(temp_path);
}
END_TEST

static Suite *file_edit_direct_suite(void)
{
    Suite *s = suite_create("FileEditDirect");
    TCase *tc_core = tcase_create("Core");

    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_fopen_read_enoent);
    tcase_add_test(tc_core, test_empty_old_string);
    tcase_add_test(tc_core, test_identical_strings);
    tcase_add_test(tc_core, test_fopen_read_eacces);
    tcase_add_test(tc_core, test_fopen_read_other);
    tcase_add_test(tc_core, test_success_single_replacement);
    tcase_add_test(tc_core, test_success_multiple_replacements);
    tcase_add_test(tc_core, test_string_not_found);
    tcase_add_test(tc_core, test_multiple_without_replace_all);
    tcase_add_test(tc_core, test_fopen_write_eacces);
    tcase_add_test(tc_core, test_fopen_write_other);
    tcase_add_test(tc_core, test_fwrite_failure);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = file_edit_direct_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
