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

static Suite *paths_getters_suite(void)
{
    Suite *s = suite_create("paths_getters");

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

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = paths_getters_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}