#include "../../../src/paths.h"
#include "../../../src/error.h"
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
    talloc_free(test_ctx);
    test_paths_cleanup_env();
}

// Test 1: Development mode (via IKIGAI_DATA_DIR env var)
START_TEST(test_data_dir_development) {
    setenv("IKIGAI_BIN_DIR", "/tmp/test/bin", 1);
    setenv("IKIGAI_CONFIG_DIR", "/tmp/test/etc/ikigai", 1);
    setenv("IKIGAI_DATA_DIR", "/tmp/test/share/ikigai", 1);
    setenv("IKIGAI_LIBEXEC_DIR", "/tmp/test/libexec/ikigai", 1);
    setenv("IKIGAI_CACHE_DIR", "/tmp/test/cache", 1);
    setenv("HOME", "/home/testuser", 1);

    ik_paths_t *paths = NULL;
    res_t result = ik_paths_init(test_ctx, &paths);
    ck_assert(is_ok(&result));

    const char *data_dir = ik_paths_get_data_dir(paths);
    ck_assert_str_eq(data_dir, "/tmp/test/share/ikigai");
}
END_TEST

// Test 2: User install (XDG paths)
START_TEST(test_data_dir_user_install) {
    setenv("IKIGAI_BIN_DIR", "/home/user/.local/bin", 1);
    setenv("IKIGAI_CONFIG_DIR", "/home/user/.config/ikigai", 1);
    setenv("IKIGAI_DATA_DIR", "/home/user/.local/share/ikigai", 1);
    setenv("IKIGAI_LIBEXEC_DIR", "/home/user/.local/libexec/ikigai", 1);
    setenv("IKIGAI_CACHE_DIR", "/home/user/.cache/ikigai", 1);
    setenv("HOME", "/home/user", 1);

    ik_paths_t *paths = NULL;
    res_t result = ik_paths_init(test_ctx, &paths);
    ck_assert(is_ok(&result));

    const char *data_dir = ik_paths_get_data_dir(paths);
    ck_assert_str_eq(data_dir, "/home/user/.local/share/ikigai");
}
END_TEST

// Test 3: System install
START_TEST(test_data_dir_system_install) {
    setenv("IKIGAI_BIN_DIR", "/usr/local/bin", 1);
    setenv("IKIGAI_CONFIG_DIR", "/usr/local/etc/ikigai", 1);
    setenv("IKIGAI_DATA_DIR", "/usr/local/share/ikigai", 1);
    setenv("IKIGAI_LIBEXEC_DIR", "/usr/local/libexec/ikigai", 1);
    setenv("IKIGAI_CACHE_DIR", "/usr/local/cache/ikigai", 1);
    setenv("HOME", "/home/testuser", 1);

    ik_paths_t *paths = NULL;
    res_t result = ik_paths_init(test_ctx, &paths);
    ck_assert(is_ok(&result));

    const char *data_dir = ik_paths_get_data_dir(paths);
    ck_assert_str_eq(data_dir, "/usr/local/share/ikigai");
}
END_TEST

static Suite *data_dir_suite(void)
{
    Suite *s = suite_create("paths_data_dir");

    TCase *tc_data_dir = tcase_create("data_dir_resolution");
    tcase_add_checked_fixture(tc_data_dir, setup, teardown);
    tcase_add_test(tc_data_dir, test_data_dir_development);
    tcase_add_test(tc_data_dir, test_data_dir_user_install);
    tcase_add_test(tc_data_dir, test_data_dir_system_install);
    suite_add_tcase(s, tc_data_dir);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = data_dir_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/paths/data_dir_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
