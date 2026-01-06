#include "paths.h"
#include "error.h"
#include "panic.h"
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

START_TEST(test_paths_init_success)
{
    // Setup environment
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
    ck_assert_str_eq(ik_paths_get_bin_dir(paths), "/test/bin");
    ck_assert_str_eq(ik_paths_get_config_dir(paths), "/test/config");
    ck_assert_str_eq(ik_paths_get_data_dir(paths), "/test/data");
    ck_assert_str_eq(ik_paths_get_libexec_dir(paths), "/test/libexec");

    // Cleanup
    unsetenv("IKIGAI_BIN_DIR");
    unsetenv("IKIGAI_CONFIG_DIR");
    unsetenv("IKIGAI_DATA_DIR");
    unsetenv("IKIGAI_LIBEXEC_DIR");
}
END_TEST

START_TEST(test_paths_init_missing_bin_dir)
{
    // Setup environment (missing IKIGAI_BIN_DIR)
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

START_TEST(test_paths_init_missing_config_dir)
{
    // Setup environment (missing IKIGAI_CONFIG_DIR)
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

START_TEST(test_paths_init_missing_data_dir)
{
    // Setup environment (missing IKIGAI_DATA_DIR)
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

START_TEST(test_paths_init_missing_libexec_dir)
{
    // Setup environment (missing IKIGAI_LIBEXEC_DIR)
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

START_TEST(test_paths_get_bin_dir)
{
    // Setup environment
    setenv("IKIGAI_BIN_DIR", "/usr/local/bin", 1);
    setenv("IKIGAI_CONFIG_DIR", "/usr/local/etc/ikigai", 1);
    setenv("IKIGAI_DATA_DIR", "/usr/local/share/ikigai", 1);
    setenv("IKIGAI_LIBEXEC_DIR", "/usr/local/libexec/ikigai", 1);
    setenv("HOME", "/home/testuser", 1);

    ik_paths_t *paths = NULL;
    res_t result = ik_paths_init(test_ctx, &paths);
    ck_assert(is_ok(&result));

    // Test
    const char *bin_dir = ik_paths_get_bin_dir(paths);
    ck_assert_ptr_nonnull(bin_dir);
    ck_assert_str_eq(bin_dir, "/usr/local/bin");

    // Cleanup
    unsetenv("IKIGAI_BIN_DIR");
    unsetenv("IKIGAI_CONFIG_DIR");
    unsetenv("IKIGAI_DATA_DIR");
    unsetenv("IKIGAI_LIBEXEC_DIR");
}
END_TEST

START_TEST(test_paths_get_config_dir)
{
    // Setup environment
    setenv("IKIGAI_BIN_DIR", "/usr/local/bin", 1);
    setenv("IKIGAI_CONFIG_DIR", "/usr/local/etc/ikigai", 1);
    setenv("IKIGAI_DATA_DIR", "/usr/local/share/ikigai", 1);
    setenv("IKIGAI_LIBEXEC_DIR", "/usr/local/libexec/ikigai", 1);
    setenv("HOME", "/home/testuser", 1);

    ik_paths_t *paths = NULL;
    res_t result = ik_paths_init(test_ctx, &paths);
    ck_assert(is_ok(&result));

    // Test
    const char *config_dir = ik_paths_get_config_dir(paths);
    ck_assert_ptr_nonnull(config_dir);
    ck_assert_str_eq(config_dir, "/usr/local/etc/ikigai");

    // Cleanup
    unsetenv("IKIGAI_BIN_DIR");
    unsetenv("IKIGAI_CONFIG_DIR");
    unsetenv("IKIGAI_DATA_DIR");
    unsetenv("IKIGAI_LIBEXEC_DIR");
}
END_TEST

START_TEST(test_paths_get_data_dir)
{
    // Setup environment
    setenv("IKIGAI_BIN_DIR", "/usr/local/bin", 1);
    setenv("IKIGAI_CONFIG_DIR", "/usr/local/etc/ikigai", 1);
    setenv("IKIGAI_DATA_DIR", "/usr/local/share/ikigai", 1);
    setenv("IKIGAI_LIBEXEC_DIR", "/usr/local/libexec/ikigai", 1);
    setenv("HOME", "/home/testuser", 1);

    ik_paths_t *paths = NULL;
    res_t result = ik_paths_init(test_ctx, &paths);
    ck_assert(is_ok(&result));

    // Test
    const char *data_dir = ik_paths_get_data_dir(paths);
    ck_assert_ptr_nonnull(data_dir);
    ck_assert_str_eq(data_dir, "/usr/local/share/ikigai");

    // Cleanup
    unsetenv("IKIGAI_BIN_DIR");
    unsetenv("IKIGAI_CONFIG_DIR");
    unsetenv("IKIGAI_DATA_DIR");
    unsetenv("IKIGAI_LIBEXEC_DIR");
}
END_TEST

START_TEST(test_paths_get_libexec_dir)
{
    // Setup environment
    setenv("IKIGAI_BIN_DIR", "/usr/local/bin", 1);
    setenv("IKIGAI_CONFIG_DIR", "/usr/local/etc/ikigai", 1);
    setenv("IKIGAI_DATA_DIR", "/usr/local/share/ikigai", 1);
    setenv("IKIGAI_LIBEXEC_DIR", "/usr/local/libexec/ikigai", 1);
    setenv("HOME", "/home/testuser", 1);

    ik_paths_t *paths = NULL;
    res_t result = ik_paths_init(test_ctx, &paths);
    ck_assert(is_ok(&result));

    // Test
    const char *libexec_dir = ik_paths_get_libexec_dir(paths);
    ck_assert_ptr_nonnull(libexec_dir);
    ck_assert_str_eq(libexec_dir, "/usr/local/libexec/ikigai");

    // Cleanup
    unsetenv("IKIGAI_BIN_DIR");
    unsetenv("IKIGAI_CONFIG_DIR");
    unsetenv("IKIGAI_DATA_DIR");
    unsetenv("IKIGAI_LIBEXEC_DIR");
}
END_TEST

START_TEST(test_paths_get_tools_system_dir)
{
    // Setup environment
    setenv("IKIGAI_BIN_DIR", "/usr/local/bin", 1);
    setenv("IKIGAI_CONFIG_DIR", "/usr/local/etc/ikigai", 1);
    setenv("IKIGAI_DATA_DIR", "/usr/local/share/ikigai", 1);
    setenv("IKIGAI_LIBEXEC_DIR", "/usr/local/libexec/ikigai", 1);
    setenv("HOME", "/home/testuser", 1);

    ik_paths_t *paths = NULL;
    res_t result = ik_paths_init(test_ctx, &paths);
    ck_assert(is_ok(&result));

    // Test - tools_system_dir should return same as libexec_dir
    const char *tools_system_dir = ik_paths_get_tools_system_dir(paths);
    const char *libexec_dir = ik_paths_get_libexec_dir(paths);
    ck_assert_ptr_nonnull(tools_system_dir);
    ck_assert_str_eq(tools_system_dir, libexec_dir);

    // Cleanup
    unsetenv("IKIGAI_BIN_DIR");
    unsetenv("IKIGAI_CONFIG_DIR");
    unsetenv("IKIGAI_DATA_DIR");
    unsetenv("IKIGAI_LIBEXEC_DIR");
}
END_TEST

START_TEST(test_paths_get_tools_user_dir)
{
    // Setup environment
    setenv("IKIGAI_BIN_DIR", "/usr/local/bin", 1);
    setenv("IKIGAI_CONFIG_DIR", "/usr/local/etc/ikigai", 1);
    setenv("IKIGAI_DATA_DIR", "/usr/local/share/ikigai", 1);
    setenv("IKIGAI_LIBEXEC_DIR", "/usr/local/libexec/ikigai", 1);
    setenv("HOME", "/home/testuser", 1);

    ik_paths_t *paths = NULL;
    res_t result = ik_paths_init(test_ctx, &paths);
    ck_assert(is_ok(&result));

    // Test - tools_user_dir should be ~/.ikigai/tools/ (expanded)
    const char *tools_user_dir = ik_paths_get_tools_user_dir(paths);
    ck_assert_ptr_nonnull(tools_user_dir);
    ck_assert_str_eq(tools_user_dir, "/home/testuser/.ikigai/tools/");

    // Cleanup
    unsetenv("IKIGAI_BIN_DIR");
    unsetenv("IKIGAI_CONFIG_DIR");
    unsetenv("IKIGAI_DATA_DIR");
    unsetenv("IKIGAI_LIBEXEC_DIR");
}
END_TEST

START_TEST(test_paths_get_tools_project_dir)
{
    // Setup environment
    setenv("IKIGAI_BIN_DIR", "/usr/local/bin", 1);
    setenv("IKIGAI_CONFIG_DIR", "/usr/local/etc/ikigai", 1);
    setenv("IKIGAI_DATA_DIR", "/usr/local/share/ikigai", 1);
    setenv("IKIGAI_LIBEXEC_DIR", "/usr/local/libexec/ikigai", 1);
    setenv("HOME", "/home/testuser", 1);

    ik_paths_t *paths = NULL;
    res_t result = ik_paths_init(test_ctx, &paths);
    ck_assert(is_ok(&result));

    // Test - tools_project_dir should be .ikigai/tools/
    const char *tools_project_dir = ik_paths_get_tools_project_dir(paths);
    ck_assert_ptr_nonnull(tools_project_dir);
    ck_assert_str_eq(tools_project_dir, ".ikigai/tools/");

    // Cleanup
    unsetenv("IKIGAI_BIN_DIR");
    unsetenv("IKIGAI_CONFIG_DIR");
    unsetenv("IKIGAI_DATA_DIR");
    unsetenv("IKIGAI_LIBEXEC_DIR");
}
END_TEST

START_TEST(test_paths_expand_tilde_home)
{
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

START_TEST(test_paths_expand_tilde_alone)
{
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

START_TEST(test_paths_expand_tilde_not_at_start)
{
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

START_TEST(test_paths_expand_tilde_absolute)
{
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

START_TEST(test_paths_expand_tilde_relative)
{
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

START_TEST(test_paths_expand_tilde_no_home)
{
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

START_TEST(test_paths_expand_tilde_null_input)
{
    // Test
    char *expanded = NULL;
    res_t result = ik_paths_expand_tilde(test_ctx, NULL, &expanded);

    // Assert
    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_INVALID_ARG);
}
END_TEST

static Suite *paths_suite(void)
{
    Suite *s = suite_create("paths");

    TCase *tc_init = tcase_create("init");
    tcase_add_checked_fixture(tc_init, setup, teardown);
    tcase_add_test(tc_init, test_paths_init_success);
    tcase_add_test(tc_init, test_paths_init_missing_bin_dir);
    tcase_add_test(tc_init, test_paths_init_missing_config_dir);
    tcase_add_test(tc_init, test_paths_init_missing_data_dir);
    tcase_add_test(tc_init, test_paths_init_missing_libexec_dir);
    suite_add_tcase(s, tc_init);

    TCase *tc_getters = tcase_create("getters");
    tcase_add_checked_fixture(tc_getters, setup, teardown);
    tcase_add_test(tc_getters, test_paths_get_bin_dir);
    tcase_add_test(tc_getters, test_paths_get_config_dir);
    tcase_add_test(tc_getters, test_paths_get_data_dir);
    tcase_add_test(tc_getters, test_paths_get_libexec_dir);
    tcase_add_test(tc_getters, test_paths_get_tools_system_dir);
    tcase_add_test(tc_getters, test_paths_get_tools_user_dir);
    tcase_add_test(tc_getters, test_paths_get_tools_project_dir);
    suite_add_tcase(s, tc_getters);

    TCase *tc_tilde = tcase_create("tilde_expansion");
    tcase_add_checked_fixture(tc_tilde, setup, teardown);
    tcase_add_test(tc_tilde, test_paths_expand_tilde_home);
    tcase_add_test(tc_tilde, test_paths_expand_tilde_alone);
    tcase_add_test(tc_tilde, test_paths_expand_tilde_not_at_start);
    tcase_add_test(tc_tilde, test_paths_expand_tilde_absolute);
    tcase_add_test(tc_tilde, test_paths_expand_tilde_relative);
    tcase_add_test(tc_tilde, test_paths_expand_tilde_no_home);
    tcase_add_test(tc_tilde, test_paths_expand_tilde_null_input);
    suite_add_tcase(s, tc_tilde);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = paths_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
