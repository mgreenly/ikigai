/**
 * @file bash_direct_test.c
 * @brief Direct unit tests for bash logic with mocking
 */
#include "../test_constants.h"

#include "tools/bash/bash.h"

#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

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
static FILE *(*mock_popen_impl)(const char *command, const char *mode) = NULL;
static int (*mock_pclose_impl)(FILE *stream) = NULL;

FILE *popen_(const char *command, const char *mode)
{
    if (mock_popen_impl != NULL) {
        return mock_popen_impl(command, mode);
    }
    return popen(command, mode);
}

int pclose_(FILE *stream)
{
    if (mock_pclose_impl != NULL) {
        return mock_pclose_impl(stream);
    }
    return pclose(stream);
}

// Test 1: popen failure
static FILE *popen_fail(const char *cmd, const char *mode)
{
    (void)cmd;
    (void)mode;
    return NULL;
}

START_TEST(test_popen_failure) {
    mock_popen_impl = popen_fail;

    char output[4096] = {0};
    FILE *mem_stream = fmemopen(output, sizeof(output), "w");
    ck_assert_ptr_nonnull(mem_stream);

    FILE *old_stdout = stdout;
    stdout = mem_stream;

    int32_t result = bash_execute(test_ctx, "echo test");

    fclose(mem_stream);
    stdout = old_stdout;

    ck_assert_int_eq(result, 0);
    ck_assert(strstr(output, "\"exit_code\":127") != NULL);

    mock_popen_impl = NULL;
}
END_TEST

// Test 2: Success with output
START_TEST(test_success_with_output) {
    char output[4096] = {0};
    FILE *mem_stream = fmemopen(output, sizeof(output), "w");
    ck_assert_ptr_nonnull(mem_stream);

    FILE *old_stdout = stdout;
    stdout = mem_stream;

    int32_t result = bash_execute(test_ctx, "echo hello");

    fclose(mem_stream);
    stdout = old_stdout;

    ck_assert_int_eq(result, 0);
    ck_assert(strstr(output, "\"output\":\"hello\"") != NULL);
    ck_assert(strstr(output, "\"exit_code\":0") != NULL);
}
END_TEST

// Test 3: Non-zero exit code
START_TEST(test_nonzero_exit) {
    char output[4096] = {0};
    FILE *mem_stream = fmemopen(output, sizeof(output), "w");
    ck_assert_ptr_nonnull(mem_stream);

    FILE *old_stdout = stdout;
    stdout = mem_stream;

    int32_t result = bash_execute(test_ctx, "exit 42");

    fclose(mem_stream);
    stdout = old_stdout;

    ck_assert_int_eq(result, 0);
    ck_assert(strstr(output, "\"exit_code\":42") != NULL);
}
END_TEST

// Test 4: Large output (>4KB to trigger buffer growth)
START_TEST(test_large_output) {
    char output[32768] = {0};
    FILE *mem_stream = fmemopen(output, sizeof(output), "w");
    ck_assert_ptr_nonnull(mem_stream);

    FILE *old_stdout = stdout;
    stdout = mem_stream;

    // Generate >4KB of output (using perl for portability)
    int32_t result = bash_execute(test_ctx, "perl -e 'print \"x\" x 5000'");

    fclose(mem_stream);
    stdout = old_stdout;

    ck_assert_int_eq(result, 0);
    ck_assert(strstr(output, "\"exit_code\":0") != NULL);
    // Check that the output field contains a large string (5000 x's)
    ck_assert(strstr(output, "\"output\":") != NULL);
}
END_TEST

// Test 5: Empty output
START_TEST(test_empty_output) {
    char output[4096] = {0};
    FILE *mem_stream = fmemopen(output, sizeof(output), "w");
    ck_assert_ptr_nonnull(mem_stream);

    FILE *old_stdout = stdout;
    stdout = mem_stream;

    int32_t result = bash_execute(test_ctx, "true");

    fclose(mem_stream);
    stdout = old_stdout;

    ck_assert_int_eq(result, 0);
    ck_assert(strstr(output, "\"output\":\"\"") != NULL);
    ck_assert(strstr(output, "\"exit_code\":0") != NULL);
}
END_TEST

// Test 6: Exact buffer size (4096 bytes) to trigger realloc for null terminator
START_TEST(test_exact_buffer_size) {
    char output[32768] = {0};
    FILE *mem_stream = fmemopen(output, sizeof(output), "w");
    ck_assert_ptr_nonnull(mem_stream);

    FILE *old_stdout = stdout;
    stdout = mem_stream;

    // Generate exactly 4096 bytes of output (4096 'x' characters)
    int32_t result = bash_execute(test_ctx, "perl -e 'print \"x\" x 4096'");

    fclose(mem_stream);
    stdout = old_stdout;

    ck_assert_int_eq(result, 0);
    ck_assert(strstr(output, "\"exit_code\":0") != NULL);
}
END_TEST

static Suite *bash_direct_suite(void)
{
    Suite *s = suite_create("BashDirect");
    TCase *tc_core = tcase_create("Core");

    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_popen_failure);
    tcase_add_test(tc_core, test_success_with_output);
    tcase_add_test(tc_core, test_nonzero_exit);
    tcase_add_test(tc_core, test_large_output);
    tcase_add_test(tc_core, test_empty_output);
    tcase_add_test(tc_core, test_exact_buffer_size);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = bash_direct_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/bash_direct_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
