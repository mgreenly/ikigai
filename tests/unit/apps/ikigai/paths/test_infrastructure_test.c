#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "tests/helpers/test_utils_helper.h"

// Test: Verify all 4 directories exist after setup
START_TEST(test_paths_setup_creates_directories) {
    const char *prefix = test_paths_setup_env();
    ck_assert_msg(prefix != NULL, "test_paths_setup_env returned NULL");

    // Check each directory exists
    char path[512];
    struct stat st;

    snprintf(path, sizeof(path), "%s/bin", prefix);
    ck_assert_msg(stat(path, &st) == 0, "bin directory does not exist: %s", path);
    ck_assert_msg(S_ISDIR(st.st_mode), "bin is not a directory");

    snprintf(path, sizeof(path), "%s/config", prefix);
    ck_assert_msg(stat(path, &st) == 0, "config directory does not exist: %s", path);
    ck_assert_msg(S_ISDIR(st.st_mode), "config is not a directory");

    snprintf(path, sizeof(path), "%s/share", prefix);
    ck_assert_msg(stat(path, &st) == 0, "share directory does not exist: %s", path);
    ck_assert_msg(S_ISDIR(st.st_mode), "share is not a directory");

    snprintf(path, sizeof(path), "%s/libexec", prefix);
    ck_assert_msg(stat(path, &st) == 0, "libexec directory does not exist: %s", path);
    ck_assert_msg(S_ISDIR(st.st_mode), "libexec is not a directory");

    test_paths_cleanup_env();
}
END_TEST

// Test: Verify all 4 env vars set correctly
START_TEST(test_paths_setup_sets_environment) {
    const char *prefix = test_paths_setup_env();
    ck_assert_msg(prefix != NULL, "test_paths_setup_env returned NULL");

    const char *bin_dir = getenv("IKIGAI_BIN_DIR");
    const char *config_dir = getenv("IKIGAI_CONFIG_DIR");
    const char *data_dir = getenv("IKIGAI_DATA_DIR");
    const char *libexec_dir = getenv("IKIGAI_LIBEXEC_DIR");

    ck_assert_msg(bin_dir != NULL, "IKIGAI_BIN_DIR not set");
    ck_assert_msg(config_dir != NULL, "IKIGAI_CONFIG_DIR not set");
    ck_assert_msg(data_dir != NULL, "IKIGAI_DATA_DIR not set");
    ck_assert_msg(libexec_dir != NULL, "IKIGAI_LIBEXEC_DIR not set");

    // Verify they match the expected pattern
    char expected[512];

    snprintf(expected, sizeof(expected), "%s/bin", prefix);
    ck_assert_str_eq(bin_dir, expected);

    snprintf(expected, sizeof(expected), "%s/config", prefix);
    ck_assert_str_eq(config_dir, expected);

    snprintf(expected, sizeof(expected), "%s/share", prefix);
    ck_assert_str_eq(data_dir, expected);

    snprintf(expected, sizeof(expected), "%s/libexec", prefix);
    ck_assert_str_eq(libexec_dir, expected);

    test_paths_cleanup_env();
}
END_TEST

// Test: Verify return value matches pattern /tmp/ikigai_test_${PID}
START_TEST(test_paths_setup_returns_prefix) {
    const char *prefix = test_paths_setup_env();
    ck_assert_msg(prefix != NULL, "test_paths_setup_env returned NULL");

    // Verify prefix starts with /tmp/ikigai_test_
    const char *expected_start = "/tmp/ikigai_test_";
    ck_assert_msg(strncmp(prefix, expected_start, strlen(expected_start)) == 0,
                  "Prefix does not start with '%s', got '%s'", expected_start, prefix);

    // Verify prefix ends with a number (PID)
    const char *pid_part = prefix + strlen(expected_start);
    ck_assert_msg(strlen(pid_part) > 0, "No PID in prefix");
    for (size_t i = 0; i < strlen(pid_part); i++) {
        ck_assert_msg(pid_part[i] >= '0' && pid_part[i] <= '9',
                      "Non-numeric character in PID: %c", pid_part[i]);
    }

    test_paths_cleanup_env();
}
END_TEST

// Test: Verify prefix contains actual PID
START_TEST(test_paths_setup_pid_isolation) {
    const char *prefix = test_paths_setup_env();
    ck_assert_msg(prefix != NULL, "test_paths_setup_env returned NULL");

    // Extract PID from prefix
    char expected[256];
    snprintf(expected, sizeof(expected), "/tmp/ikigai_test_%d", getpid());
    ck_assert_str_eq(prefix, expected);

    test_paths_cleanup_env();
}
END_TEST

// Test: Verify env vars unset after cleanup
START_TEST(test_paths_cleanup_unsets_environment) {
    test_paths_setup_env();
    test_paths_cleanup_env();

    ck_assert_msg(getenv("IKIGAI_BIN_DIR") == NULL, "IKIGAI_BIN_DIR still set after cleanup");
    ck_assert_msg(getenv("IKIGAI_CONFIG_DIR") == NULL, "IKIGAI_CONFIG_DIR still set after cleanup");
    ck_assert_msg(getenv("IKIGAI_DATA_DIR") == NULL, "IKIGAI_DATA_DIR still set after cleanup");
    ck_assert_msg(getenv("IKIGAI_LIBEXEC_DIR") == NULL, "IKIGAI_LIBEXEC_DIR still set after cleanup");
}
END_TEST

// Test: Verify directories removed after cleanup
START_TEST(test_paths_cleanup_removes_directories) {
    const char *prefix = test_paths_setup_env();
    ck_assert_msg(prefix != NULL, "test_paths_setup_env returned NULL");

    // Save prefix before cleanup
    char saved_prefix[256];
    snprintf(saved_prefix, sizeof(saved_prefix), "%s", prefix);

    test_paths_cleanup_env();

    // Verify directory tree is gone
    struct stat st;
    ck_assert_msg(stat(saved_prefix, &st) != 0, "Test directory still exists after cleanup: %s", saved_prefix);
}
END_TEST

// Test: Verify calling cleanup twice doesn't crash
START_TEST(test_paths_cleanup_idempotent) {
    test_paths_setup_env();
    test_paths_cleanup_env();
    test_paths_cleanup_env();  // Should not crash

    // Verify env vars still unset
    ck_assert_msg(getenv("IKIGAI_BIN_DIR") == NULL, "IKIGAI_BIN_DIR set after double cleanup");
}
END_TEST

static Suite *test_infrastructure_suite(void)
{
    Suite *s = suite_create("Test Infrastructure");
    TCase *tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_paths_setup_creates_directories);
    tcase_add_test(tc_core, test_paths_setup_sets_environment);
    tcase_add_test(tc_core, test_paths_setup_returns_prefix);
    tcase_add_test(tc_core, test_paths_setup_pid_isolation);
    tcase_add_test(tc_core, test_paths_cleanup_unsets_environment);
    tcase_add_test(tc_core, test_paths_cleanup_removes_directories);
    tcase_add_test(tc_core, test_paths_cleanup_idempotent);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = test_infrastructure_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/paths/test_infrastructure_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
