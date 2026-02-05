#include "apps/ikigai/paths.h"
#include "shared/error.h"
#include "shared/panic.h"
#include <check.h>
#include <stdlib.h>
#include <talloc.h>

static TALLOC_CTX *test_ctx;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

START_TEST(test_paths_expand_tilde_home) {
    // Setup
    setenv("HOME", "/home/testuser", 1);

    // Test
    char *expanded = NULL;
    res_t result = ik_paths_expand_tilde(test_ctx, "~/foo", &expanded);

    // Assert
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(expanded);
    ck_assert_str_eq(expanded, "/home/testuser/foo");
}
END_TEST

START_TEST(test_paths_expand_tilde_alone) {
    // Setup
    setenv("HOME", "/home/testuser", 1);

    // Test
    char *expanded = NULL;
    res_t result = ik_paths_expand_tilde(test_ctx, "~", &expanded);

    // Assert
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(expanded);
    ck_assert_str_eq(expanded, "/home/testuser");
}
END_TEST

START_TEST(test_paths_expand_tilde_not_at_start) {
    // Setup
    setenv("HOME", "/home/testuser", 1);

    // Test
    char *expanded = NULL;
    res_t result = ik_paths_expand_tilde(test_ctx, "foo~/bar", &expanded);

    // Assert
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(expanded);
    ck_assert_str_eq(expanded, "foo~/bar");
}
END_TEST

START_TEST(test_paths_expand_tilde_absolute) {
    // Setup
    setenv("HOME", "/home/testuser", 1);

    // Test
    char *expanded = NULL;
    res_t result = ik_paths_expand_tilde(test_ctx, "/absolute/path", &expanded);

    // Assert
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(expanded);
    ck_assert_str_eq(expanded, "/absolute/path");
}
END_TEST

START_TEST(test_paths_expand_tilde_relative) {
    // Setup
    setenv("HOME", "/home/testuser", 1);

    // Test
    char *expanded = NULL;
    res_t result = ik_paths_expand_tilde(test_ctx, "relative/path", &expanded);

    // Assert
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(expanded);
    ck_assert_str_eq(expanded, "relative/path");
}
END_TEST

START_TEST(test_paths_expand_tilde_no_home) {
    // Setup - unset HOME
    unsetenv("HOME");

    // Test
    char *expanded = NULL;
    res_t result = ik_paths_expand_tilde(test_ctx, "~/foo", &expanded);

    // Assert
    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_IO);
}
END_TEST

START_TEST(test_paths_expand_tilde_null_input) {
    // Test
    char *expanded = NULL;
    res_t result = ik_paths_expand_tilde(test_ctx, NULL, &expanded);

    // Assert
    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_INVALID_ARG);
}
END_TEST

START_TEST(test_paths_expand_tilde_tilde_non_slash) {
    // Setup
    setenv("HOME", "/home/testuser", 1);

    // Test - tilde followed by non-slash character
    char *expanded = NULL;
    res_t result = ik_paths_expand_tilde(test_ctx, "~user", &expanded);

    // Assert - should not expand, just copy as-is
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(expanded);
    ck_assert_str_eq(expanded, "~user");
}
END_TEST

static Suite *paths_tilde_suite(void)
{
    Suite *s = suite_create("paths_tilde");

    TCase *tc_tilde = tcase_create("tilde_expansion");
    tcase_add_checked_fixture(tc_tilde, setup, teardown);
    tcase_add_test(tc_tilde, test_paths_expand_tilde_home);
    tcase_add_test(tc_tilde, test_paths_expand_tilde_alone);
    tcase_add_test(tc_tilde, test_paths_expand_tilde_not_at_start);
    tcase_add_test(tc_tilde, test_paths_expand_tilde_absolute);
    tcase_add_test(tc_tilde, test_paths_expand_tilde_relative);
    tcase_add_test(tc_tilde, test_paths_expand_tilde_no_home);
    tcase_add_test(tc_tilde, test_paths_expand_tilde_null_input);
    tcase_add_test(tc_tilde, test_paths_expand_tilde_tilde_non_slash);
    suite_add_tcase(s, tc_tilde);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = paths_tilde_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/paths/paths_tilde_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
