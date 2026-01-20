#include "../../../src/file_utils.h"
#include "../../../src/error.h"
#include "../../test_utils_helper.h"

#include <check.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <talloc.h>
#include <unistd.h>

static Suite *file_utils_suite(void);

// Test: Successfully read existing file
START_TEST(test_file_read_all_success) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    // Create temporary test file
    char template[] = "/tmp/test_file_XXXXXX";
    int fd = mkstemp(template);
    ck_assert_int_ge(fd, 0);

    const char *content = "Hello, World!\nThis is a test file.";
    write(fd, content, strlen(content));
    close(fd);

    // Read file
    char *out_content = NULL;
    size_t out_size = 0;
    res_t res = ik_file_read_all(ctx, template, &out_content, &out_size);

    ck_assert(!res.is_err);
    ck_assert_ptr_nonnull(out_content);
    ck_assert_uint_eq(out_size, strlen(content));
    ck_assert_str_eq(out_content, content);
    ck_assert_int_eq(out_content[out_size], '\0');  // Verify null termination

    // Cleanup
    unlink(template);
    talloc_free(ctx);
}
END_TEST
// Test: Return error for missing file
START_TEST(test_file_read_all_file_not_found) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    char *out_content = NULL;
    size_t out_size = 0;
    res_t res = ik_file_read_all(ctx, "/tmp/nonexistent_file_12345.txt", &out_content, &out_size);

    ck_assert(res.is_err);
    ck_assert_ptr_nonnull(res.err);

    talloc_free(ctx);
}

END_TEST
// Test: Handle empty file
START_TEST(test_file_read_all_empty_file) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    // Create empty temporary file
    char template[] = "/tmp/test_empty_XXXXXX";
    int fd = mkstemp(template);
    ck_assert_int_ge(fd, 0);
    close(fd);

    // Read empty file
    char *out_content = NULL;
    size_t out_size = 0;
    res_t res = ik_file_read_all(ctx, template, &out_content, &out_size);

    ck_assert(!res.is_err);
    ck_assert_ptr_nonnull(out_content);
    ck_assert_uint_eq(out_size, 0);
    ck_assert_int_eq(out_content[0], '\0');  // Should be null-terminated

    // Cleanup
    unlink(template);
    talloc_free(ctx);
}

END_TEST

#ifndef SKIP_SIGNAL_TESTS
// Test: Assert on NULL context
START_TEST(test_file_read_all_null_ctx) {
    char *out_content = NULL;
    size_t out_size = 0;

    // This should trigger an assertion failure
    ik_file_read_all(NULL, "/tmp/test.txt", &out_content, &out_size);
}

END_TEST
// Test: Assert on NULL path
START_TEST(test_file_read_all_null_path) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    char *out_content = NULL;
    size_t out_size = 0;

    // This should trigger an assertion failure
    ik_file_read_all(ctx, NULL, &out_content, &out_size);

    talloc_free(ctx);
}

END_TEST
// Test: Assert on NULL output pointer
START_TEST(test_file_read_all_null_out) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    size_t out_size = 0;

    // This should trigger an assertion failure
    ik_file_read_all(ctx, "/tmp/test.txt", NULL, &out_size);

    talloc_free(ctx);
}

END_TEST
#endif

static Suite *file_utils_suite(void)
{
    Suite *s = suite_create("File Utils");

    TCase *tc_success = tcase_create("Success Cases");
    tcase_set_timeout(tc_success, IK_TEST_TIMEOUT);
    tcase_add_test(tc_success, test_file_read_all_success);
    tcase_add_test(tc_success, test_file_read_all_empty_file);
    suite_add_tcase(s, tc_success);

    TCase *tc_error = tcase_create("Error Cases");
    tcase_set_timeout(tc_error, IK_TEST_TIMEOUT);
    tcase_add_test(tc_error, test_file_read_all_file_not_found);
    suite_add_tcase(s, tc_error);

    TCase *tc_assert = tcase_create("Assertion Cases");
    tcase_set_timeout(tc_assert, IK_TEST_TIMEOUT);
#ifndef SKIP_SIGNAL_TESTS
    tcase_add_test_raise_signal(tc_assert, test_file_read_all_null_ctx, SIGABRT);
    tcase_add_test_raise_signal(tc_assert, test_file_read_all_null_path, SIGABRT);
    tcase_add_test_raise_signal(tc_assert, test_file_read_all_null_out, SIGABRT);
#endif
    suite_add_tcase(s, tc_assert);

    return s;
}

int main(void)
{
    Suite *s = file_utils_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
