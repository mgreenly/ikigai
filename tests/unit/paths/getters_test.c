#include "paths.h"
#include "error.h"
#include "../../test_utils_helper.h"
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

START_TEST(test_get_bin_dir) {
    // Setup
    setenv("IKIGAI_BIN_DIR", "/custom/bin", 1);
    setenv("IKIGAI_CONFIG_DIR", "/custom/config", 1);
    setenv("IKIGAI_DATA_DIR", "/custom/data", 1);
    setenv("IKIGAI_LIBEXEC_DIR", "/custom/libexec", 1);
    setenv("IKIGAI_CACHE_DIR", "/custom/cache", 1);
    setenv("HOME", "/home/testuser", 1);

    // Execute
    ik_paths_t *paths = NULL;
    res_t result = ik_paths_init(test_ctx, &paths);
    ck_assert(is_ok(&result));

    // Assert
    const char *bin_dir = ik_paths_get_bin_dir(paths);
    ck_assert_ptr_nonnull(bin_dir);
    ck_assert_str_eq(bin_dir, "/custom/bin");

    // Cleanup
    unsetenv("IKIGAI_BIN_DIR");
    unsetenv("IKIGAI_CONFIG_DIR");
    unsetenv("IKIGAI_DATA_DIR");
    unsetenv("IKIGAI_LIBEXEC_DIR");
    unsetenv("IKIGAI_CACHE_DIR");
}
END_TEST

START_TEST(test_get_config_dir) {
    // Setup
    setenv("IKIGAI_BIN_DIR", "/custom/bin", 1);
    setenv("IKIGAI_CONFIG_DIR", "/custom/config", 1);
    setenv("IKIGAI_DATA_DIR", "/custom/data", 1);
    setenv("IKIGAI_LIBEXEC_DIR", "/custom/libexec", 1);
    setenv("IKIGAI_CACHE_DIR", "/custom/cache", 1);
    setenv("HOME", "/home/testuser", 1);

    // Execute
    ik_paths_t *paths = NULL;
    res_t result = ik_paths_init(test_ctx, &paths);
    ck_assert(is_ok(&result));

    // Assert
    const char *config_dir = ik_paths_get_config_dir(paths);
    ck_assert_ptr_nonnull(config_dir);
    ck_assert_str_eq(config_dir, "/custom/config");

    // Cleanup
    unsetenv("IKIGAI_BIN_DIR");
    unsetenv("IKIGAI_CONFIG_DIR");
    unsetenv("IKIGAI_DATA_DIR");
    unsetenv("IKIGAI_LIBEXEC_DIR");
    unsetenv("IKIGAI_CACHE_DIR");
}
END_TEST

START_TEST(test_get_data_dir) {
    // Setup
    setenv("IKIGAI_BIN_DIR", "/custom/bin", 1);
    setenv("IKIGAI_CONFIG_DIR", "/custom/config", 1);
    setenv("IKIGAI_DATA_DIR", "/custom/data", 1);
    setenv("IKIGAI_LIBEXEC_DIR", "/custom/libexec", 1);
    setenv("IKIGAI_CACHE_DIR", "/custom/cache", 1);
    setenv("HOME", "/home/testuser", 1);

    // Execute
    ik_paths_t *paths = NULL;
    res_t result = ik_paths_init(test_ctx, &paths);
    ck_assert(is_ok(&result));

    // Assert
    const char *data_dir = ik_paths_get_data_dir(paths);
    ck_assert_ptr_nonnull(data_dir);
    ck_assert_str_eq(data_dir, "/custom/data");

    // Cleanup
    unsetenv("IKIGAI_BIN_DIR");
    unsetenv("IKIGAI_CONFIG_DIR");
    unsetenv("IKIGAI_DATA_DIR");
    unsetenv("IKIGAI_LIBEXEC_DIR");
    unsetenv("IKIGAI_CACHE_DIR");
}
END_TEST

START_TEST(test_get_libexec_dir) {
    // Setup
    setenv("IKIGAI_BIN_DIR", "/custom/bin", 1);
    setenv("IKIGAI_CONFIG_DIR", "/custom/config", 1);
    setenv("IKIGAI_DATA_DIR", "/custom/data", 1);
    setenv("IKIGAI_LIBEXEC_DIR", "/custom/libexec", 1);
    setenv("IKIGAI_CACHE_DIR", "/custom/cache", 1);
    setenv("HOME", "/home/testuser", 1);

    // Execute
    ik_paths_t *paths = NULL;
    res_t result = ik_paths_init(test_ctx, &paths);
    ck_assert(is_ok(&result));

    // Assert
    const char *libexec_dir = ik_paths_get_libexec_dir(paths);
    ck_assert_ptr_nonnull(libexec_dir);
    ck_assert_str_eq(libexec_dir, "/custom/libexec");

    // Cleanup
    unsetenv("IKIGAI_BIN_DIR");
    unsetenv("IKIGAI_CONFIG_DIR");
    unsetenv("IKIGAI_DATA_DIR");
    unsetenv("IKIGAI_LIBEXEC_DIR");
    unsetenv("IKIGAI_CACHE_DIR");
}
END_TEST

START_TEST(test_getters_not_null) {
    // Setup
    test_paths_setup_env();
    setenv("HOME", "/home/testuser", 1);

    // Execute
    ik_paths_t *paths = NULL;
    res_t result = ik_paths_init(test_ctx, &paths);
    ck_assert(is_ok(&result));

    // Assert - all getters should never return NULL when initialized
    ck_assert_ptr_nonnull(ik_paths_get_bin_dir(paths));
    ck_assert_ptr_nonnull(ik_paths_get_config_dir(paths));
    ck_assert_ptr_nonnull(ik_paths_get_data_dir(paths));
    ck_assert_ptr_nonnull(ik_paths_get_libexec_dir(paths));
    ck_assert_ptr_nonnull(ik_paths_get_tools_system_dir(paths));
    ck_assert_ptr_nonnull(ik_paths_get_tools_user_dir(paths));
    ck_assert_ptr_nonnull(ik_paths_get_tools_project_dir(paths));

    test_paths_cleanup_env();
}
END_TEST

START_TEST(test_getters_const_strings) {
    // Setup
    test_paths_setup_env();
    setenv("HOME", "/home/testuser", 1);

    // Execute
    ik_paths_t *paths = NULL;
    res_t result = ik_paths_init(test_ctx, &paths);
    ck_assert(is_ok(&result));

    // Assert - strings should remain valid while paths instance alive
    // Get pointers multiple times and verify they're stable
    const char *bin_dir_1 = ik_paths_get_bin_dir(paths);
    const char *bin_dir_2 = ik_paths_get_bin_dir(paths);
    ck_assert_ptr_eq(bin_dir_1, bin_dir_2);  // Should return same pointer

    const char *config_dir_1 = ik_paths_get_config_dir(paths);
    const char *config_dir_2 = ik_paths_get_config_dir(paths);
    ck_assert_ptr_eq(config_dir_1, config_dir_2);

    test_paths_cleanup_env();
}
END_TEST

static Suite *paths_getters_suite(void)
{
    Suite *s = suite_create("Paths Getters");
    TCase *tc = tcase_create("Getters");
    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_get_bin_dir);
    tcase_add_test(tc, test_get_config_dir);
    tcase_add_test(tc, test_get_data_dir);
    tcase_add_test(tc, test_get_libexec_dir);
    tcase_add_test(tc, test_getters_not_null);
    tcase_add_test(tc, test_getters_const_strings);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    Suite *s = paths_getters_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/paths/getters_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
