/**
 * @file file_write_direct_test.c
 * @brief Direct unit tests for file_write logic with mocking
 */
#include "tests/test_constants.h"

#include "tools/file_write/file_write_logic.h"

#include <check.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

#include "shared/json_allocator.h"
#include "shared/panic.h"
#include "shared/wrapper.h"

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
static int (*mock_fclose_impl)(FILE *stream) = NULL;

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

int fclose_(FILE *stream)
{
    if (mock_fclose_impl != NULL) {
        return mock_fclose_impl(stream);
    }
    return fclose(stream);
}

// Test 1: ENOSPC error from fopen
static FILE *fopen_enospc(const char *path, const char *mode)
{
    (void)path;
    (void)mode;
    errno = ENOSPC;
    return NULL;
}

START_TEST(test_enospc_error) {
    mock_fopen_impl = fopen_enospc;

    char output[4096] = {0};
    FILE *mem_stream = fmemopen(output, sizeof(output), "w");
    ck_assert_ptr_nonnull(mem_stream);

    FILE *old_stdout = stdout;
    stdout = mem_stream;

    int32_t result = do_file_write(test_ctx, "/tmp/test.txt", "content", 7);

    fclose(mem_stream);
    stdout = old_stdout;

    ck_assert_int_eq(result, 0);
    ck_assert(strstr(output, "NO_SPACE") != NULL);
    ck_assert(strstr(output, "No space left on device") != NULL);

    mock_fopen_impl = NULL;
}
END_TEST

// Test 2: EACCES error from fopen
static FILE *fopen_eacces(const char *path, const char *mode)
{
    (void)path;
    (void)mode;
    errno = EACCES;
    return NULL;
}

START_TEST(test_eacces_error) {
    mock_fopen_impl = fopen_eacces;

    char output[4096] = {0};
    FILE *mem_stream = fmemopen(output, sizeof(output), "w");
    ck_assert_ptr_nonnull(mem_stream);

    FILE *old_stdout = stdout;
    stdout = mem_stream;

    int32_t result = do_file_write(test_ctx, "/tmp/test.txt", "content", 7);

    fclose(mem_stream);
    stdout = old_stdout;

    ck_assert_int_eq(result, 0);
    ck_assert(strstr(output, "PERMISSION_DENIED") != NULL);
    ck_assert(strstr(output, "Permission denied") != NULL);

    mock_fopen_impl = NULL;
}
END_TEST

// Test 3: Other error from fopen
static FILE *fopen_other(const char *path, const char *mode)
{
    (void)path;
    (void)mode;
    errno = EIO;
    return NULL;
}

START_TEST(test_open_other_error) {
    mock_fopen_impl = fopen_other;

    char output[4096] = {0};
    FILE *mem_stream = fmemopen(output, sizeof(output), "w");
    ck_assert_ptr_nonnull(mem_stream);

    FILE *old_stdout = stdout;
    stdout = mem_stream;

    int32_t result = do_file_write(test_ctx, "/tmp/test.txt", "content", 7);

    fclose(mem_stream);
    stdout = old_stdout;

    ck_assert_int_eq(result, 0);
    ck_assert(strstr(output, "OPEN_FAILED") != NULL);
    ck_assert(strstr(output, "Cannot open file") != NULL);

    mock_fopen_impl = NULL;
}
END_TEST

// Test 4: fwrite failure (partial write)
static FILE * fake_file_handle = (FILE *)0x1234;

static FILE *fopen_success(const char *path, const char *mode)
{
    (void)path;
    (void)mode;
    return fake_file_handle;
}

static size_t fwrite_partial(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    (void)ptr;
    (void)size;
    (void)nmemb;
    (void)stream;
    return 0;  // Return less than expected
}

static int fclose_noop(FILE *stream)
{
    (void)stream;
    return 0;
}

START_TEST(test_fwrite_failure) {
    mock_fopen_impl = fopen_success;
    mock_fwrite_impl = fwrite_partial;
    mock_fclose_impl = fclose_noop;

    char output[4096] = {0};
    FILE *mem_stream = fmemopen(output, sizeof(output), "w");
    ck_assert_ptr_nonnull(mem_stream);

    FILE *old_stdout = stdout;
    stdout = mem_stream;

    int32_t result = do_file_write(test_ctx, "/tmp/test.txt", "content", 7);

    fclose(mem_stream);
    stdout = old_stdout;

    ck_assert_int_eq(result, 0);
    ck_assert(strstr(output, "WRITE_FAILED") != NULL);
    ck_assert(strstr(output, "Failed to write file") != NULL);

    mock_fopen_impl = NULL;
    mock_fwrite_impl = NULL;
    mock_fclose_impl = NULL;
}
END_TEST

static Suite *file_write_direct_suite(void)
{
    Suite *s = suite_create("FileWriteDirect");
    TCase *tc_core = tcase_create("Core");

    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_enospc_error);
    tcase_add_test(tc_core, test_eacces_error);
    tcase_add_test(tc_core, test_open_other_error);
    tcase_add_test(tc_core, test_fwrite_failure);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = file_write_direct_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/tools/file_write/file_write_direct_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
