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

START_TEST(test_paths_init_success) {
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

START_TEST(test_paths_init_missing_bin_dir) {
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

START_TEST(test_paths_init_missing_config_dir) {
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

START_TEST(test_paths_init_missing_data_dir) {
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

START_TEST(test_paths_init_missing_libexec_dir) {
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

START_TEST(test_paths_init_empty_bin_dir) {
    // Setup environment (empty IKIGAI_BIN_DIR)
    setenv("IKIGAI_BIN_DIR", "", 1);
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
    unsetenv("IKIGAI_BIN_DIR");
    unsetenv("IKIGAI_CONFIG_DIR");
    unsetenv("IKIGAI_DATA_DIR");
    unsetenv("IKIGAI_LIBEXEC_DIR");
}
END_TEST

START_TEST(test_paths_init_empty_config_dir) {
    // Setup environment (empty IKIGAI_CONFIG_DIR)
    setenv("IKIGAI_BIN_DIR", "/test/bin", 1);
    setenv("IKIGAI_CONFIG_DIR", "", 1);
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
    unsetenv("IKIGAI_CONFIG_DIR");
    unsetenv("IKIGAI_DATA_DIR");
    unsetenv("IKIGAI_LIBEXEC_DIR");
}
END_TEST

START_TEST(test_paths_init_empty_data_dir) {
    // Setup environment (empty IKIGAI_DATA_DIR)
    setenv("IKIGAI_BIN_DIR", "/test/bin", 1);
    setenv("IKIGAI_CONFIG_DIR", "/test/config", 1);
    setenv("IKIGAI_DATA_DIR", "", 1);
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
    unsetenv("IKIGAI_DATA_DIR");
    unsetenv("IKIGAI_LIBEXEC_DIR");
}
END_TEST

START_TEST(test_paths_init_empty_libexec_dir) {
    // Setup environment (empty IKIGAI_LIBEXEC_DIR)
    setenv("IKIGAI_BIN_DIR", "/test/bin", 1);
    setenv("IKIGAI_CONFIG_DIR", "/test/config", 1);
    setenv("IKIGAI_DATA_DIR", "/test/data", 1);
    setenv("IKIGAI_LIBEXEC_DIR", "", 1);

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
    unsetenv("IKIGAI_LIBEXEC_DIR");
}
END_TEST

START_TEST(test_paths_init_no_home) {
    // Setup environment (no HOME for tilde expansion)
    setenv("IKIGAI_BIN_DIR", "/test/bin", 1);
    setenv("IKIGAI_CONFIG_DIR", "/test/config", 1);
    setenv("IKIGAI_DATA_DIR", "/test/data", 1);
    setenv("IKIGAI_LIBEXEC_DIR", "/test/libexec", 1);
    unsetenv("HOME");

    // Execute
    ik_paths_t *paths = NULL;
    res_t result = ik_paths_init(test_ctx, &paths);

    // Assert - should fail during tilde expansion
    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_IO);
    ck_assert_ptr_null(paths);

    // Cleanup
    unsetenv("IKIGAI_BIN_DIR");
    unsetenv("IKIGAI_CONFIG_DIR");
    unsetenv("IKIGAI_DATA_DIR");
    unsetenv("IKIGAI_LIBEXEC_DIR");
}
END_TEST

static Suite *paths_init_suite(void)
{
    Suite *s = suite_create("paths_init");

    TCase *tc_init = tcase_create("init");
    tcase_add_checked_fixture(tc_init, setup, teardown);
    tcase_add_test(tc_init, test_paths_init_success);
    tcase_add_test(tc_init, test_paths_init_missing_bin_dir);
    tcase_add_test(tc_init, test_paths_init_missing_config_dir);
    tcase_add_test(tc_init, test_paths_init_missing_data_dir);
    tcase_add_test(tc_init, test_paths_init_missing_libexec_dir);
    tcase_add_test(tc_init, test_paths_init_empty_bin_dir);
    tcase_add_test(tc_init, test_paths_init_empty_config_dir);
    tcase_add_test(tc_init, test_paths_init_empty_data_dir);
    tcase_add_test(tc_init, test_paths_init_empty_libexec_dir);
    tcase_add_test(tc_init, test_paths_init_no_home);
    suite_add_tcase(s, tc_init);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = paths_init_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/paths/paths_init_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
