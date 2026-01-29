#include "paths.h"
#include "error.h"
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

START_TEST(test_translate_ik_uri_to_path_basic) {
    char *output = NULL;
    res_t result = ik_paths_translate_ik_uri_to_path(test_ctx, paths, "ik://shared/notes.txt", &output);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(output);
    ck_assert_str_eq(output, "/home/user/projects/ikigai/state/shared/notes.txt");
}
END_TEST

START_TEST(test_translate_ik_uri_to_path_root) {
    char *output = NULL;
    res_t result = ik_paths_translate_ik_uri_to_path(test_ctx, paths, "ik://config.json", &output);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(output);
    ck_assert_str_eq(output, "/home/user/projects/ikigai/state/config.json");
}
END_TEST

START_TEST(test_translate_ik_uri_to_path_trailing_slash) {
    char *output = NULL;
    res_t result = ik_paths_translate_ik_uri_to_path(test_ctx, paths, "ik://shared/", &output);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(output);
    ck_assert_str_eq(output, "/home/user/projects/ikigai/state/shared/");
}
END_TEST

START_TEST(test_translate_ik_uri_to_path_multiple) {
    char *output = NULL;
    res_t result = ik_paths_translate_ik_uri_to_path(test_ctx, paths,
                                                      "Copy ik://a.txt to ik://b.txt", &output);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(output);
    ck_assert_str_eq(output, "Copy /home/user/projects/ikigai/state/a.txt to /home/user/projects/ikigai/state/b.txt");
}
END_TEST

START_TEST(test_translate_ik_uri_to_path_mixed) {
    char *output = NULL;
    res_t result = ik_paths_translate_ik_uri_to_path(test_ctx, paths,
                                                      "Move ik://notes.txt to ./local.txt", &output);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(output);
    ck_assert_str_eq(output, "Move /home/user/projects/ikigai/state/notes.txt to ./local.txt");
}
END_TEST

START_TEST(test_translate_ik_uri_to_path_no_false_positive) {
    char *output = NULL;
    res_t result = ik_paths_translate_ik_uri_to_path(test_ctx, paths,
                                                      "myik://test should not translate", &output);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(output);
    ck_assert_str_eq(output, "myik://test should not translate");
}
END_TEST

START_TEST(test_translate_ik_uri_to_path_no_match) {
    char *output = NULL;
    res_t result = ik_paths_translate_ik_uri_to_path(test_ctx, paths,
                                                      "No URI here at all", &output);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(output);
    ck_assert_str_eq(output, "No URI here at all");
}
END_TEST

START_TEST(test_translate_ik_uri_to_path_null_paths) {
    char *output = NULL;
    res_t result = ik_paths_translate_ik_uri_to_path(test_ctx, NULL, "ik://test", &output);

    ck_assert(is_err(&result));
    ck_assert_ptr_nonnull(result.err);
    ck_assert_int_eq(result.err->code, ERR_INVALID_ARG);
}
END_TEST

START_TEST(test_translate_ik_uri_to_path_null_input) {
    char *output = NULL;
    res_t result = ik_paths_translate_ik_uri_to_path(test_ctx, paths, NULL, &output);

    ck_assert(is_err(&result));
    ck_assert_ptr_nonnull(result.err);
    ck_assert_int_eq(result.err->code, ERR_INVALID_ARG);
}
END_TEST

START_TEST(test_translate_path_to_ik_uri_basic) {
    char *output = NULL;
    res_t result = ik_paths_translate_path_to_ik_uri(test_ctx, paths,
                                                      "/home/user/projects/ikigai/state/shared/notes.txt", &output);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(output);
    ck_assert_str_eq(output, "ik://shared/notes.txt");
}
END_TEST

START_TEST(test_translate_path_to_ik_uri_root) {
    char *output = NULL;
    res_t result = ik_paths_translate_path_to_ik_uri(test_ctx, paths,
                                                      "/home/user/projects/ikigai/state/config.json", &output);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(output);
    ck_assert_str_eq(output, "ik://config.json");
}
END_TEST

START_TEST(test_translate_path_to_ik_uri_trailing_slash) {
    char *output = NULL;
    res_t result = ik_paths_translate_path_to_ik_uri(test_ctx, paths,
                                                      "/home/user/projects/ikigai/state/shared/", &output);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(output);
    ck_assert_str_eq(output, "ik://shared/");
}
END_TEST

START_TEST(test_translate_path_to_ik_uri_multiple) {
    char *output = NULL;
    res_t result = ik_paths_translate_path_to_ik_uri(test_ctx, paths,
                                                      "Error in /home/user/projects/ikigai/state/a.txt and /home/user/projects/ikigai/state/b.txt",
                                                      &output);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(output);
    ck_assert_str_eq(output, "Error in ik://a.txt and ik://b.txt");
}
END_TEST

START_TEST(test_translate_path_to_ik_uri_mixed) {
    char *output = NULL;
    res_t result = ik_paths_translate_path_to_ik_uri(test_ctx, paths,
                                                      "File /home/user/projects/ikigai/state/notes.txt and /tmp/other.txt",
                                                      &output);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(output);
    ck_assert_str_eq(output, "File ik://notes.txt and /tmp/other.txt");
}
END_TEST

START_TEST(test_translate_path_to_ik_uri_no_match) {
    char *output = NULL;
    res_t result = ik_paths_translate_path_to_ik_uri(test_ctx, paths,
                                                      "No state path here: /tmp/test.txt", &output);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(output);
    ck_assert_str_eq(output, "No state path here: /tmp/test.txt");
}
END_TEST

START_TEST(test_translate_path_to_ik_uri_null_paths) {
    char *output = NULL;
    res_t result = ik_paths_translate_path_to_ik_uri(test_ctx, NULL, "/tmp/test", &output);

    ck_assert(is_err(&result));
    ck_assert_ptr_nonnull(result.err);
    ck_assert_int_eq(result.err->code, ERR_INVALID_ARG);
}
END_TEST

START_TEST(test_translate_path_to_ik_uri_null_input) {
    char *output = NULL;
    res_t result = ik_paths_translate_path_to_ik_uri(test_ctx, paths, NULL, &output);

    ck_assert(is_err(&result));
    ck_assert_ptr_nonnull(result.err);
    ck_assert_int_eq(result.err->code, ERR_INVALID_ARG);
}
END_TEST

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

START_TEST(test_translate_false_positive_with_real) {
    char *output = NULL;
    res_t result = ik_paths_translate_ik_uri_to_path(test_ctx, paths,
                                                      "prefix_ik://fake and ik://real/path.txt", &output);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(output);
    ck_assert_str_eq(output, "prefix_ik://fake and /home/user/projects/ikigai/state/real/path.txt");
}
END_TEST

START_TEST(test_translate_uri_with_leading_slash) {
    char *output = NULL;
    res_t result = ik_paths_translate_ik_uri_to_path(test_ctx, paths, "ik:///path.txt", &output);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(output);
    ck_assert_str_eq(output, "/home/user/projects/ikigai/state/path.txt");
}
END_TEST

START_TEST(test_translate_uri_empty_after) {
    char *output = NULL;
    res_t result = ik_paths_translate_ik_uri_to_path(test_ctx, paths, "ik://", &output);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(output);
    ck_assert_str_eq(output, "/home/user/projects/ikigai/state");
}
END_TEST

START_TEST(test_translate_system_uri_variations) {
    char *out = NULL;
    res_t r = ik_paths_translate_ik_uri_to_path(test_ctx, paths, "ik://system", &out);
    ck_assert(is_ok(&r));
    ck_assert_str_eq(out, "/usr/local/share/ikigai/system");

    r = ik_paths_translate_ik_uri_to_path(test_ctx, paths, "ik://system/prompt.md", &out);
    ck_assert(is_ok(&r));
    ck_assert_str_eq(out, "/usr/local/share/ikigai/system/prompt.md");

    r = ik_paths_translate_ik_uri_to_path(test_ctx, paths, "ik://system/", &out);
    ck_assert(is_ok(&r));
    ck_assert_str_eq(out, "/usr/local/share/ikigai/system/");
}
END_TEST

START_TEST(test_translate_systemd_not_system) {
    char *output = NULL;
    res_t result = ik_paths_translate_ik_uri_to_path(test_ctx, paths, "ik://systemd/foo", &output);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(output);
    ck_assert_str_eq(output, "/home/user/projects/ikigai/state/systemd/foo");
}
END_TEST

START_TEST(test_translate_system_prefix_not_namespace) {
    char *output = NULL;
    res_t result = ik_paths_translate_ik_uri_to_path(test_ctx, paths, "ik://system_config/file", &output);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(output);
    ck_assert_str_eq(output, "/home/user/projects/ikigai/state/system_config/file");
}
END_TEST

START_TEST(test_translate_mixed_system_and_generic) {
    char *output = NULL;
    res_t result = ik_paths_translate_ik_uri_to_path(test_ctx, paths,
                                                      "Copy ik://system/prompt.md to ik://notes.txt", &output);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(output);
    ck_assert_str_eq(output, "Copy /usr/local/share/ikigai/system/prompt.md to /home/user/projects/ikigai/state/notes.txt");
}
END_TEST

START_TEST(test_translate_path_with_leading_slash) {
    char *output = NULL;
    res_t result = ik_paths_translate_path_to_ik_uri(test_ctx, paths,
                                                      "/home/user/projects/ikigai/state/path.txt", &output);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(output);
    ck_assert_str_eq(output, "ik://path.txt");
}
END_TEST

START_TEST(test_translate_system_path_variations) {
    char *out = NULL;
    res_t r = ik_paths_translate_path_to_ik_uri(test_ctx, paths,
                                                 "/usr/local/share/ikigai/system", &out);
    ck_assert(is_ok(&r));
    ck_assert_str_eq(out, "ik://system");

    r = ik_paths_translate_path_to_ik_uri(test_ctx, paths,
                                          "/usr/local/share/ikigai/system/prompt.md", &out);
    ck_assert(is_ok(&r));
    ck_assert_str_eq(out, "ik://system/prompt.md");

    r = ik_paths_translate_path_to_ik_uri(test_ctx, paths,
                                          "/usr/local/share/ikigai/system/", &out);
    ck_assert(is_ok(&r));
    ck_assert_str_eq(out, "ik://system/");
}
END_TEST

START_TEST(test_translate_mixed_system_and_generic_paths) {
    char *output = NULL;
    res_t result = ik_paths_translate_path_to_ik_uri(test_ctx, paths,
                                                      "Error in /usr/local/share/ikigai/system/prompt.md and /home/user/projects/ikigai/state/notes.txt",
                                                      &output);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(output);
    ck_assert_str_eq(output, "Error in ik://system/prompt.md and ik://notes.txt");
}
END_TEST

static Suite *paths_translate_suite(void)
{
    Suite *s = suite_create("paths_translate");

    TCase *tc_uri_to_path = tcase_create("ik_uri_to_path");
    tcase_add_checked_fixture(tc_uri_to_path, setup, teardown);
    tcase_add_test(tc_uri_to_path, test_translate_ik_uri_to_path_basic);
    tcase_add_test(tc_uri_to_path, test_translate_ik_uri_to_path_root);
    tcase_add_test(tc_uri_to_path, test_translate_ik_uri_to_path_trailing_slash);
    tcase_add_test(tc_uri_to_path, test_translate_ik_uri_to_path_multiple);
    tcase_add_test(tc_uri_to_path, test_translate_ik_uri_to_path_mixed);
    tcase_add_test(tc_uri_to_path, test_translate_ik_uri_to_path_no_false_positive);
    tcase_add_test(tc_uri_to_path, test_translate_ik_uri_to_path_no_match);
    tcase_add_test(tc_uri_to_path, test_translate_ik_uri_to_path_null_paths);
    tcase_add_test(tc_uri_to_path, test_translate_ik_uri_to_path_null_input);
    tcase_add_test(tc_uri_to_path, test_translate_false_positive_with_real);
    tcase_add_test(tc_uri_to_path, test_translate_uri_with_leading_slash);
    tcase_add_test(tc_uri_to_path, test_translate_uri_empty_after);
    tcase_add_test(tc_uri_to_path, test_translate_system_uri_variations);
    tcase_add_test(tc_uri_to_path, test_translate_systemd_not_system);
    tcase_add_test(tc_uri_to_path, test_translate_system_prefix_not_namespace);
    tcase_add_test(tc_uri_to_path, test_translate_mixed_system_and_generic);
    suite_add_tcase(s, tc_uri_to_path);

    TCase *tc_path_to_uri = tcase_create("path_to_ik_uri");
    tcase_add_checked_fixture(tc_path_to_uri, setup, teardown);
    tcase_add_test(tc_path_to_uri, test_translate_path_to_ik_uri_basic);
    tcase_add_test(tc_path_to_uri, test_translate_path_to_ik_uri_root);
    tcase_add_test(tc_path_to_uri, test_translate_path_to_ik_uri_trailing_slash);
    tcase_add_test(tc_path_to_uri, test_translate_path_to_ik_uri_multiple);
    tcase_add_test(tc_path_to_uri, test_translate_path_to_ik_uri_mixed);
    tcase_add_test(tc_path_to_uri, test_translate_path_to_ik_uri_no_match);
    tcase_add_test(tc_path_to_uri, test_translate_path_to_ik_uri_null_paths);
    tcase_add_test(tc_path_to_uri, test_translate_path_to_ik_uri_null_input);
    tcase_add_test(tc_path_to_uri, test_translate_path_with_leading_slash);
    tcase_add_test(tc_path_to_uri, test_translate_system_path_variations);
    tcase_add_test(tc_path_to_uri, test_translate_mixed_system_and_generic_paths);
    suite_add_tcase(s, tc_path_to_uri);

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
    Suite *s = paths_translate_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/paths/paths_translate_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
