#include "paths.h"
#include "error.h"
#include "../../test_utils.h"
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
    test_paths_cleanup_env();
    talloc_free(test_ctx);
}

START_TEST(test_expand_tilde_home_only)
{
    // Setup
    setenv("HOME", "/home/testuser", 1);

    // Execute
    char *expanded = NULL;
    res_t result = ik_paths_expand_tilde(test_ctx, "~", &expanded);

    // Assert
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(expanded);
    ck_assert_str_eq(expanded, "/home/testuser");
}
END_TEST

START_TEST(test_expand_tilde_with_path)
{
    // Setup
    setenv("HOME", "/home/testuser", 1);

    // Execute
    char *expanded = NULL;
    res_t result = ik_paths_expand_tilde(test_ctx, "~/foo/bar", &expanded);

    // Assert
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(expanded);
    ck_assert_str_eq(expanded, "/home/testuser/foo/bar");
}
END_TEST

START_TEST(test_expand_tilde_absolute_path)
{
    // Setup
    setenv("HOME", "/home/testuser", 1);

    // Execute
    char *expanded = NULL;
    res_t result = ik_paths_expand_tilde(test_ctx, "/absolute/path", &expanded);

    // Assert - should return unchanged
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(expanded);
    ck_assert_str_eq(expanded, "/absolute/path");
}
END_TEST

START_TEST(test_expand_tilde_relative_path)
{
    // Setup
    setenv("HOME", "/home/testuser", 1);

    // Execute
    char *expanded = NULL;
    res_t result = ik_paths_expand_tilde(test_ctx, "relative/path", &expanded);

    // Assert - should return unchanged
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(expanded);
    ck_assert_str_eq(expanded, "relative/path");
}
END_TEST

START_TEST(test_expand_tilde_not_at_start)
{
    // Setup
    setenv("HOME", "/home/testuser", 1);

    // Execute
    char *expanded = NULL;
    res_t result = ik_paths_expand_tilde(test_ctx, "foo~/bar", &expanded);

    // Assert - should return unchanged
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(expanded);
    ck_assert_str_eq(expanded, "foo~/bar");
}
END_TEST

START_TEST(test_expand_tilde_home_not_set)
{
    // Setup - unset HOME
    unsetenv("HOME");

    // Execute
    char *expanded = NULL;
    res_t result = ik_paths_expand_tilde(test_ctx, "~/foo", &expanded);

    // Assert - should return ERR_IO when HOME not set
    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_IO);
}
END_TEST

START_TEST(test_expand_tilde_null_input)
{
    // Execute
    char *expanded = NULL;
    res_t result = ik_paths_expand_tilde(test_ctx, NULL, &expanded);

    // Assert - should return ERR_INVALID_ARG
    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_INVALID_ARG);
}
END_TEST

START_TEST(test_expand_tilde_empty_string)
{
    // Setup
    setenv("HOME", "/home/testuser", 1);

    // Execute
    char *expanded = NULL;
    res_t result = ik_paths_expand_tilde(test_ctx, "", &expanded);

    // Assert - empty string should return empty string
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(expanded);
    ck_assert_str_eq(expanded, "");
}
END_TEST

static Suite *paths_tilde_suite(void)
{
    Suite *s = suite_create("Paths Tilde Expansion");
    TCase *tc = tcase_create("Tilde");
    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_expand_tilde_home_only);
    tcase_add_test(tc, test_expand_tilde_with_path);
    tcase_add_test(tc, test_expand_tilde_absolute_path);
    tcase_add_test(tc, test_expand_tilde_relative_path);
    tcase_add_test(tc, test_expand_tilde_not_at_start);
    tcase_add_test(tc, test_expand_tilde_home_not_set);
    tcase_add_test(tc, test_expand_tilde_null_input);
    tcase_add_test(tc, test_expand_tilde_empty_string);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    Suite *s = paths_tilde_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
