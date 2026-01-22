/**
 * @file file_read_direct_test.c
 * @brief Direct unit tests for file_read logic with mocking
 */
#include "../test_constants.h"

#include "tools/file_read/file_read.h"

#include <check.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <talloc.h>
#include <unistd.h>

#include "json_allocator.h"
#include "panic.h"
#include "wrapper.h"

static TALLOC_CTX *test_ctx;
static const char *test_file_path = "test_file_read_temp.txt";

static const char *test_file_large = "test_file_read_large.txt";

static void setup(void)
{
    test_ctx = talloc_new(NULL);

    // Create a test file with multiple lines for success tests
    FILE *f = fopen(test_file_path, "w");
    if (f != NULL) {
        fprintf(f, "line 1\nline 2\nline 3\nline 4\nline 5\nline 6\nline 7\nline 8\nline 9\nline 10\n");
        fclose(f);
    }

    // Create a large file with lines totaling >4096 bytes to test buffer growth
    f = fopen(test_file_large, "w");
    if (f != NULL) {
        for (int32_t i = 0; i < 100; i++) {
            fprintf(f,
                    "This is line %d with some extra text to make it longer and trigger buffer growth when reading multiple lines\n",
                    i);
        }
        fclose(f);
    }
}

static void teardown(void)
{
    talloc_free(test_ctx);
    unlink(test_file_path);
    unlink(test_file_large);
}

// Mock control variables
static FILE *(*mock_fopen_impl)(const char *path, const char *mode) = NULL;
static size_t (*mock_fread_impl)(void *ptr, size_t size, size_t nmemb, FILE *stream) = NULL;
static int (*mock_posix_stat_impl)(const char *pathname, struct stat *statbuf) = NULL;

FILE *fopen_(const char *path, const char *mode)
{
    if (mock_fopen_impl != NULL) {
        return mock_fopen_impl(path, mode);
    }
    return fopen(path, mode);
}

size_t fread_(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    if (mock_fread_impl != NULL) {
        return mock_fread_impl(ptr, size, nmemb, stream);
    }
    return fread(ptr, size, nmemb, stream);
}

int posix_stat_(const char *pathname, struct stat *statbuf)
{
    if (mock_posix_stat_impl != NULL) {
        return mock_posix_stat_impl(pathname, statbuf);
    }
    return stat(pathname, statbuf);
}

// fclose_ just calls fclose (no mocking needed)
int fclose_(FILE *stream)
{
    return fclose(stream);
}

// Mock implementations for error cases
static FILE *fopen_enoent(const char *path, const char *mode)
{
    (void)path;
    (void)mode;
    errno = ENOENT;
    return NULL;
}

static FILE *fopen_eacces(const char *path, const char *mode)
{
    (void)path;
    (void)mode;
    errno = EACCES;
    return NULL;
}

static FILE *fopen_eio(const char *path, const char *mode)
{
    (void)path;
    (void)mode;
    errno = EIO;
    return NULL;
}

static int posix_stat_fail(const char *pathname, struct stat *statbuf)
{
    (void)pathname;
    (void)statbuf;
    errno = EIO;
    return -1;
}

static size_t fread_short(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    (void)ptr;
    (void)size;
    (void)nmemb;
    (void)stream;
    return 5;  // Return less than expected
}

// Test 1: fopen failure - ENOENT
START_TEST(test_file_read_fopen_enoent) {
    mock_fopen_impl = fopen_enoent;

    char output[4096] = {0};
    FILE *mem_stream = fmemopen(output, sizeof(output), "w");
    ck_assert_ptr_nonnull(mem_stream);

    FILE *old_stdout = stdout;
    stdout = mem_stream;

    file_read_execute(test_ctx, "nonexistent.txt", false, 0, false, 0);

    fclose(mem_stream);
    stdout = old_stdout;

    ck_assert(strstr(output, "File not found") != NULL);
    ck_assert(strstr(output, "FILE_NOT_FOUND") != NULL);

    mock_fopen_impl = NULL;
}
END_TEST

// Test 2: fopen failure - EACCES
START_TEST(test_file_read_fopen_eacces) {
    mock_fopen_impl = fopen_eacces;

    char output[4096] = {0};
    FILE *mem_stream = fmemopen(output, sizeof(output), "w");
    ck_assert_ptr_nonnull(mem_stream);

    FILE *old_stdout = stdout;
    stdout = mem_stream;

    file_read_execute(test_ctx, "noperm.txt", false, 0, false, 0);

    fclose(mem_stream);
    stdout = old_stdout;

    ck_assert(strstr(output, "Permission denied") != NULL);
    ck_assert(strstr(output, "PERMISSION_DENIED") != NULL);

    mock_fopen_impl = NULL;
}
END_TEST

// Test 3: fopen failure - other error
START_TEST(test_file_read_fopen_other_error) {
    mock_fopen_impl = fopen_eio;

    char output[4096] = {0};
    FILE *mem_stream = fmemopen(output, sizeof(output), "w");
    ck_assert_ptr_nonnull(mem_stream);

    FILE *old_stdout = stdout;
    stdout = mem_stream;

    file_read_execute(test_ctx, "error.txt", false, 0, false, 0);

    fclose(mem_stream);
    stdout = old_stdout;

    ck_assert(strstr(output, "Cannot open file") != NULL);
    ck_assert(strstr(output, "OPEN_FAILED") != NULL);

    mock_fopen_impl = NULL;
}
END_TEST

// Test 4: posix_stat failure
START_TEST(test_file_read_stat_failure) {
    mock_posix_stat_impl = posix_stat_fail;

    char output[4096] = {0};
    FILE *mem_stream = fmemopen(output, sizeof(output), "w");
    ck_assert_ptr_nonnull(mem_stream);

    FILE *old_stdout = stdout;
    stdout = mem_stream;

    file_read_execute(test_ctx, test_file_path, false, 0, false, 0);

    fclose(mem_stream);
    stdout = old_stdout;

    ck_assert(strstr(output, "Cannot get file size") != NULL);
    ck_assert(strstr(output, "SIZE_FAILED") != NULL);

    mock_posix_stat_impl = NULL;
}
END_TEST

// Test 5: fread returns less than expected
START_TEST(test_file_read_fread_short) {
    mock_fread_impl = fread_short;

    char output[4096] = {0};
    FILE *mem_stream = fmemopen(output, sizeof(output), "w");
    ck_assert_ptr_nonnull(mem_stream);

    FILE *old_stdout = stdout;
    stdout = mem_stream;

    file_read_execute(test_ctx, test_file_path, false, 0, false, 0);

    fclose(mem_stream);
    stdout = old_stdout;

    ck_assert(strstr(output, "Failed to read file") != NULL);
    ck_assert(strstr(output, "READ_FAILED") != NULL);

    mock_fread_impl = NULL;
}
END_TEST

// Test 6: Success - whole file
START_TEST(test_file_read_success_whole_file) {
    char output[4096] = {0};
    FILE *mem_stream = fmemopen(output, sizeof(output), "w");
    ck_assert_ptr_nonnull(mem_stream);

    FILE *old_stdout = stdout;
    stdout = mem_stream;

    file_read_execute(test_ctx, test_file_path, false, 0, false, 0);

    fclose(mem_stream);
    stdout = old_stdout;

    ck_assert(strstr(output, "\"output\"") != NULL);
}
END_TEST

// Test 7: Success - with offset
START_TEST(test_file_read_success_with_offset) {
    char output[4096] = {0};
    FILE *mem_stream = fmemopen(output, sizeof(output), "w");
    ck_assert_ptr_nonnull(mem_stream);

    FILE *old_stdout = stdout;
    stdout = mem_stream;

    file_read_execute(test_ctx, test_file_path, true, 5, false, 0);

    fclose(mem_stream);
    stdout = old_stdout;

    ck_assert(strstr(output, "\"output\"") != NULL);
}
END_TEST

// Test 8: Success - with limit
START_TEST(test_file_read_success_with_limit) {
    char output[4096] = {0};
    FILE *mem_stream = fmemopen(output, sizeof(output), "w");
    ck_assert_ptr_nonnull(mem_stream);

    FILE *old_stdout = stdout;
    stdout = mem_stream;

    file_read_execute(test_ctx, test_file_path, false, 0, true, 3);

    fclose(mem_stream);
    stdout = old_stdout;

    ck_assert(strstr(output, "\"output\"") != NULL);
}
END_TEST

// Test 9: Success - with offset and limit
START_TEST(test_file_read_success_with_offset_and_limit) {
    char output[4096] = {0};
    FILE *mem_stream = fmemopen(output, sizeof(output), "w");
    ck_assert_ptr_nonnull(mem_stream);

    FILE *old_stdout = stdout;
    stdout = mem_stream;

    file_read_execute(test_ctx, test_file_path, true, 2, true, 5);

    fclose(mem_stream);
    stdout = old_stdout;

    ck_assert(strstr(output, "\"output\"") != NULL);
}
END_TEST

// Test 10: Large file with limit to trigger buffer growth
START_TEST(test_file_read_large_buffer_growth) {
    char output[16384] = {0};
    FILE *mem_stream = fmemopen(output, sizeof(output), "w");
    ck_assert_ptr_nonnull(mem_stream);

    FILE *old_stdout = stdout;
    stdout = mem_stream;

    // Read 50 lines from the large file - this should exceed 4096 bytes and trigger buffer growth
    file_read_execute(test_ctx, test_file_large, false, 0, true, 50);

    fclose(mem_stream);
    stdout = old_stdout;

    ck_assert(strstr(output, "\"output\"") != NULL);
}
END_TEST

static Suite *file_read_direct_suite(void)
{
    Suite *s = suite_create("file_read_direct");

    TCase *tc_fopen = tcase_create("fopen_errors");
    tcase_add_checked_fixture(tc_fopen, setup, teardown);
    tcase_add_test(tc_fopen, test_file_read_fopen_enoent);
    tcase_add_test(tc_fopen, test_file_read_fopen_eacces);
    tcase_add_test(tc_fopen, test_file_read_fopen_other_error);
    suite_add_tcase(s, tc_fopen);

    TCase *tc_io = tcase_create("io_errors");
    tcase_add_checked_fixture(tc_io, setup, teardown);
    tcase_add_test(tc_io, test_file_read_stat_failure);
    tcase_add_test(tc_io, test_file_read_fread_short);
    suite_add_tcase(s, tc_io);

    TCase *tc_success = tcase_create("success");
    tcase_add_checked_fixture(tc_success, setup, teardown);
    tcase_add_test(tc_success, test_file_read_success_whole_file);
    tcase_add_test(tc_success, test_file_read_success_with_offset);
    tcase_add_test(tc_success, test_file_read_success_with_limit);
    tcase_add_test(tc_success, test_file_read_success_with_offset_and_limit);
    tcase_add_test(tc_success, test_file_read_large_buffer_growth);
    suite_add_tcase(s, tc_success);

    return s;
}

int32_t main(void)
{
    Suite *s = file_read_direct_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/file_read_direct_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int32_t number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
