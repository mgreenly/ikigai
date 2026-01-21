/**
 * @file log_dir_test.c
 * @brief Tests for ik_test_set_log_dir helper function
 */

#include "../../test_utils_helper.h"
#include <check.h>
#include <stdlib.h>
#include <string.h>

// Test: ik_test_set_log_dir sets IKIGAI_LOG_DIR from file path
START_TEST(test_set_log_dir_from_file_path) {
    // Clear environment variable first
    unsetenv("IKIGAI_LOG_DIR");

    ik_test_set_log_dir("tests/unit/logger/jsonl_basic_test.c");

    const char *log_dir = getenv("IKIGAI_LOG_DIR");
    ck_assert_ptr_nonnull(log_dir);
    ck_assert_str_eq(log_dir, "/tmp/ikigai_logs_jsonl_basic_test");
}
END_TEST
// Test: ik_test_set_log_dir handles nested paths
START_TEST(test_set_log_dir_from_nested_path) {
    unsetenv("IKIGAI_LOG_DIR");

    ik_test_set_log_dir("tests/unit/commands/mark_db_test.c");

    const char *log_dir = getenv("IKIGAI_LOG_DIR");
    ck_assert_ptr_nonnull(log_dir);
    ck_assert_str_eq(log_dir, "/tmp/ikigai_logs_mark_db_test");
}

END_TEST
// Test: ik_test_set_log_dir handles simple filename
START_TEST(test_set_log_dir_simple_file) {
    unsetenv("IKIGAI_LOG_DIR");

    ik_test_set_log_dir("foo_test.c");

    const char *log_dir = getenv("IKIGAI_LOG_DIR");
    ck_assert_ptr_nonnull(log_dir);
    ck_assert_str_eq(log_dir, "/tmp/ikigai_logs_foo_test");
}

END_TEST
// Test: ik_test_set_log_dir handles NULL gracefully
START_TEST(test_set_log_dir_null) {
    unsetenv("IKIGAI_LOG_DIR");

    ik_test_set_log_dir(NULL);

    const char *log_dir = getenv("IKIGAI_LOG_DIR");
    ck_assert_ptr_null(log_dir);
}

END_TEST
// Test: ik_test_set_log_dir handles file without extension
START_TEST(test_set_log_dir_no_extension) {
    unsetenv("IKIGAI_LOG_DIR");

    ik_test_set_log_dir("tests/unit/test_file");

    const char *log_dir = getenv("IKIGAI_LOG_DIR");
    ck_assert_ptr_nonnull(log_dir);
    ck_assert_str_eq(log_dir, "/tmp/ikigai_logs_test_file");
}

END_TEST

static Suite *log_dir_suite(void)
{
    Suite *s = suite_create("Test Utils Log Dir");

    TCase *tc = tcase_create("set_log_dir");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_test(tc, test_set_log_dir_from_file_path);
    tcase_add_test(tc, test_set_log_dir_from_nested_path);
    tcase_add_test(tc, test_set_log_dir_simple_file);
    tcase_add_test(tc, test_set_log_dir_null);
    tcase_add_test(tc, test_set_log_dir_no_extension);
    suite_add_tcase(s, tc);

    return s;
}

int32_t main(void)
{
    Suite *s = log_dir_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/test_utils/log_dir_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int32_t number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
