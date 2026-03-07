#include "apps/ikigai/paths.h"
#include "shared/error.h"
#include <check.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

static TALLOC_CTX *test_ctx;
static ik_paths_t *paths;

static void setup(void)
{
    test_ctx = talloc_new(NULL);

    // Setup environment
    setenv("IKIGAI_BIN_DIR", "/usr/local/bin", 1);
    setenv("IKIGAI_CONFIG_DIR", "/usr/local/etc/ikigai", 1);
    setenv("IKIGAI_DATA_DIR", "/usr/local/share/ikigai", 1);
    setenv("IKIGAI_LIBEXEC_DIR", "/usr/local/libexec/ikigai", 1);
    setenv("IKIGAI_CACHE_DIR", "/tmp/cache", 1);
    setenv("IKIGAI_STATE_DIR", "/home/user/projects/ikigai/state", 1);
    setenv("IKIGAI_RUNTIME_DIR", "/run/user/1000", 1);
    setenv("HOME", "/home/testuser", 1);

    res_t result = ik_paths_init(test_ctx, &paths);
    ck_assert(is_ok(&result));
}

static void teardown(void)
{
    talloc_free(test_ctx);

    unsetenv("IKIGAI_BIN_DIR");
    unsetenv("IKIGAI_CONFIG_DIR");
    unsetenv("IKIGAI_DATA_DIR");
    unsetenv("IKIGAI_LIBEXEC_DIR");
    unsetenv("IKIGAI_CACHE_DIR");
    unsetenv("IKIGAI_STATE_DIR");
}

START_TEST(test_round_trip_translation) {
    // Test: ik:// -> path -> ik:// should return original
    const char *original = "ik://shared/notes.txt";

    char *path = NULL;
    res_t result1 = ik_paths_translate_ik_uri_to_path(test_ctx, paths, original, &path);
    ck_assert(is_ok(&result1));
    ck_assert_ptr_nonnull(path);

    char *uri = NULL;
    res_t result2 = ik_paths_translate_path_to_ik_uri(test_ctx, paths, path, &uri);
    ck_assert(is_ok(&result2));
    ck_assert_ptr_nonnull(uri);

    ck_assert_str_eq(uri, original);
}
END_TEST

START_TEST(test_round_trip_system_translation) {
    // Test: ik://system -> path -> ik://system should return original
    const char *original = "ik://system/prompt.md";

    char *path = NULL;
    res_t result1 = ik_paths_translate_ik_uri_to_path(test_ctx, paths, original, &path);
    ck_assert(is_ok(&result1));
    ck_assert_ptr_nonnull(path);

    char *uri = NULL;
    res_t result2 = ik_paths_translate_path_to_ik_uri(test_ctx, paths, path, &uri);
    ck_assert(is_ok(&result2));
    ck_assert_ptr_nonnull(uri);

    ck_assert_str_eq(uri, original);
}
END_TEST

static Suite *paths_translate_round_trip_suite(void)
{
    Suite *s = suite_create("paths_translate_round_trip");

    TCase *tc_round_trip = tcase_create("round_trip");
    tcase_add_checked_fixture(tc_round_trip, setup, teardown);
    tcase_add_test(tc_round_trip, test_round_trip_translation);
    tcase_add_test(tc_round_trip, test_round_trip_system_translation);
    suite_add_tcase(s, tc_round_trip);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = paths_translate_round_trip_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/paths/paths_translate_round_trip_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
