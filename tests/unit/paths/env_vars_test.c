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

START_TEST(test_env_all_vars_set)
{
    // Setup
    setenv("IKIGAI_BIN_DIR", "/test/bin", 1);
    setenv("IKIGAI_CONFIG_DIR", "/test/config", 1);
    setenv("IKIGAI_DATA_DIR", "/test/data", 1);
    setenv("IKIGAI_LIBEXEC_DIR", "/test/libexec", 1);
    setenv("HOME", "/home/testuser", 1);

    // Execute
    ik_paths_t *paths = NULL;
    res_t result = ik_paths_init(test_ctx, &paths);

    // Assert
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(paths);

    // Cleanup
    unsetenv("IKIGAI_BIN_DIR");
    unsetenv("IKIGAI_CONFIG_DIR");
    unsetenv("IKIGAI_DATA_DIR");
    unsetenv("IKIGAI_LIBEXEC_DIR");
}
END_TEST

START_TEST(test_env_missing_bin_dir)
{
    // Setup (missing IKIGAI_BIN_DIR)
    unsetenv("IKIGAI_BIN_DIR");
    setenv("IKIGAI_CONFIG_DIR", "/test/config", 1);
    setenv("IKIGAI_DATA_DIR", "/test/data", 1);
    setenv("IKIGAI_LIBEXEC_DIR", "/test/libexec", 1);

    // Execute
    ik_paths_t *paths = NULL;
    res_t result = ik_paths_init(test_ctx, &paths);

    // Assert
    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_INVALID_ARG);
    ck_assert_ptr_null(paths);

    // Cleanup
    unsetenv("IKIGAI_CONFIG_DIR");
    unsetenv("IKIGAI_DATA_DIR");
    unsetenv("IKIGAI_LIBEXEC_DIR");
}
END_TEST

START_TEST(test_env_missing_config_dir)
{
    // Setup (missing IKIGAI_CONFIG_DIR)
    setenv("IKIGAI_BIN_DIR", "/test/bin", 1);
    unsetenv("IKIGAI_CONFIG_DIR");
    setenv("IKIGAI_DATA_DIR", "/test/data", 1);
    setenv("IKIGAI_LIBEXEC_DIR", "/test/libexec", 1);

    // Execute
    ik_paths_t *paths = NULL;
    res_t result = ik_paths_init(test_ctx, &paths);

    // Assert
    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_INVALID_ARG);
    ck_assert_ptr_null(paths);

    // Cleanup
    unsetenv("IKIGAI_BIN_DIR");
    unsetenv("IKIGAI_DATA_DIR");
    unsetenv("IKIGAI_LIBEXEC_DIR");
}
END_TEST

START_TEST(test_env_missing_data_dir)
{
    // Setup (missing IKIGAI_DATA_DIR)
    setenv("IKIGAI_BIN_DIR", "/test/bin", 1);
    setenv("IKIGAI_CONFIG_DIR", "/test/config", 1);
    unsetenv("IKIGAI_DATA_DIR");
    setenv("IKIGAI_LIBEXEC_DIR", "/test/libexec", 1);

    // Execute
    ik_paths_t *paths = NULL;
    res_t result = ik_paths_init(test_ctx, &paths);

    // Assert
    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_INVALID_ARG);
    ck_assert_ptr_null(paths);

    // Cleanup
    unsetenv("IKIGAI_BIN_DIR");
    unsetenv("IKIGAI_CONFIG_DIR");
    unsetenv("IKIGAI_LIBEXEC_DIR");
}
END_TEST

START_TEST(test_env_missing_libexec_dir)
{
    // Setup (missing IKIGAI_LIBEXEC_DIR)
    setenv("IKIGAI_BIN_DIR", "/test/bin", 1);
    setenv("IKIGAI_CONFIG_DIR", "/test/config", 1);
    setenv("IKIGAI_DATA_DIR", "/test/data", 1);
    unsetenv("IKIGAI_LIBEXEC_DIR");

    // Execute
    ik_paths_t *paths = NULL;
    res_t result = ik_paths_init(test_ctx, &paths);

    // Assert
    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_INVALID_ARG);
    ck_assert_ptr_null(paths);

    // Cleanup
    unsetenv("IKIGAI_BIN_DIR");
    unsetenv("IKIGAI_CONFIG_DIR");
    unsetenv("IKIGAI_DATA_DIR");
}
END_TEST

START_TEST(test_env_empty_string)
{
    // Setup (IKIGAI_BIN_DIR set to empty string)
    setenv("IKIGAI_BIN_DIR", "", 1);
    setenv("IKIGAI_CONFIG_DIR", "/test/config", 1);
    setenv("IKIGAI_DATA_DIR", "/test/data", 1);
    setenv("IKIGAI_LIBEXEC_DIR", "/test/libexec", 1);

    // Execute
    ik_paths_t *paths = NULL;
    res_t result = ik_paths_init(test_ctx, &paths);

    // Assert - empty string should be treated as missing
    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_INVALID_ARG);

    // Cleanup
    unsetenv("IKIGAI_BIN_DIR");
    unsetenv("IKIGAI_CONFIG_DIR");
    unsetenv("IKIGAI_DATA_DIR");
    unsetenv("IKIGAI_LIBEXEC_DIR");
}
END_TEST

START_TEST(test_env_with_spaces)
{
    // Setup
    setenv("IKIGAI_BIN_DIR", "/test/path with spaces/bin", 1);
    setenv("IKIGAI_CONFIG_DIR", "/test/config", 1);
    setenv("IKIGAI_DATA_DIR", "/test/data", 1);
    setenv("IKIGAI_LIBEXEC_DIR", "/test/libexec", 1);
    setenv("HOME", "/home/testuser", 1);

    // Execute
    ik_paths_t *paths = NULL;
    res_t result = ik_paths_init(test_ctx, &paths);

    // Assert
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(paths);
    ck_assert_str_eq(ik_paths_get_bin_dir(paths), "/test/path with spaces/bin");

    // Cleanup
    unsetenv("IKIGAI_BIN_DIR");
    unsetenv("IKIGAI_CONFIG_DIR");
    unsetenv("IKIGAI_DATA_DIR");
    unsetenv("IKIGAI_LIBEXEC_DIR");
}
END_TEST

START_TEST(test_env_with_trailing_slash)
{
    // Setup
    setenv("IKIGAI_BIN_DIR", "/test/bin/", 1);
    setenv("IKIGAI_CONFIG_DIR", "/test/config/", 1);
    setenv("IKIGAI_DATA_DIR", "/test/data/", 1);
    setenv("IKIGAI_LIBEXEC_DIR", "/test/libexec/", 1);
    setenv("HOME", "/home/testuser", 1);

    // Execute
    ik_paths_t *paths = NULL;
    res_t result = ik_paths_init(test_ctx, &paths);

    // Assert - trailing slashes should be preserved
    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(paths);
    ck_assert_str_eq(ik_paths_get_bin_dir(paths), "/test/bin/");
    ck_assert_str_eq(ik_paths_get_config_dir(paths), "/test/config/");

    // Cleanup
    unsetenv("IKIGAI_BIN_DIR");
    unsetenv("IKIGAI_CONFIG_DIR");
    unsetenv("IKIGAI_DATA_DIR");
    unsetenv("IKIGAI_LIBEXEC_DIR");
}
END_TEST

static Suite *paths_env_vars_suite(void)
{
    Suite *s = suite_create("Paths Environment Variables");
    TCase *tc = tcase_create("Environment");
    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_env_all_vars_set);
    tcase_add_test(tc, test_env_missing_bin_dir);
    tcase_add_test(tc, test_env_missing_config_dir);
    tcase_add_test(tc, test_env_missing_data_dir);
    tcase_add_test(tc, test_env_missing_libexec_dir);
    tcase_add_test(tc, test_env_empty_string);
    tcase_add_test(tc, test_env_with_spaces);
    tcase_add_test(tc, test_env_with_trailing_slash);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    Suite *s = paths_env_vars_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
