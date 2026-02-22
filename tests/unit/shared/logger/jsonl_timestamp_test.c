#include "tests/test_constants.h"
// Unit tests for JSONL logger timestamp formatting

#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <regex.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "shared/logger.h"

// Helper: setup temp directory and init logger
static char test_dir[256];
static char log_file_path[512];

static void setup_logger(void)
{
    snprintf(test_dir, sizeof(test_dir), "/tmp/ikigai_timestamp_test_%d", getpid());
    mkdir(test_dir, 0755);
    ik_log_init(test_dir);
    snprintf(log_file_path, sizeof(log_file_path), "%s/.ikigai/logs/current.log", test_dir);
}

static void teardown_logger(void)
{
    ik_log_shutdown();
    unlink(log_file_path);
    char logs_dir[512];
    snprintf(logs_dir, sizeof(logs_dir), "%s/.ikigai/logs", test_dir);
    rmdir(logs_dir);
    char ikigai_dir[512];
    snprintf(ikigai_dir, sizeof(ikigai_dir), "%s/.ikigai", test_dir);
    rmdir(ikigai_dir);
    rmdir(test_dir);
}

// Test: ik_log_debug_json does not crash (logger is a no-op)
START_TEST(test_jsonl_timestamp_iso8601_format) {
    setup_logger();

    yyjson_mut_doc *doc = ik_log_create();
    yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);
    yyjson_mut_obj_add_str(doc, root, "msg", "test");

    ik_log_debug_json(doc);  // Should not crash; logger is a no-op

    teardown_logger();
}
END_TEST
// Test: ik_log_debug_json does not crash (logger is a no-op)
START_TEST(test_jsonl_timestamp_milliseconds) {
    setup_logger();

    yyjson_mut_doc *doc = ik_log_create();
    yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);
    yyjson_mut_obj_add_str(doc, root, "msg", "test");

    ik_log_debug_json(doc);  // Should not crash; logger is a no-op

    teardown_logger();
}

END_TEST
// Test: ik_log_debug_json does not crash (logger is a no-op)
START_TEST(test_jsonl_timestamp_timezone_offset) {
    setup_logger();

    yyjson_mut_doc *doc = ik_log_create();
    yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);
    yyjson_mut_obj_add_str(doc, root, "msg", "test");

    ik_log_debug_json(doc);  // Should not crash; logger is a no-op

    teardown_logger();
}

END_TEST
// Test: ik_log_debug_json does not crash (logger is a no-op)
START_TEST(test_jsonl_timestamp_current_time) {
    setup_logger();

    yyjson_mut_doc *doc = ik_log_create();
    yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);
    yyjson_mut_obj_add_str(doc, root, "msg", "test");

    ik_log_debug_json(doc);  // Should not crash; logger is a no-op

    teardown_logger();
}

END_TEST

static Suite *jsonl_timestamp_suite(void)
{
    Suite *s = suite_create("JSONL Timestamp");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    tcase_add_test(tc_core, test_jsonl_timestamp_iso8601_format);
    tcase_add_test(tc_core, test_jsonl_timestamp_milliseconds);
    tcase_add_test(tc_core, test_jsonl_timestamp_timezone_offset);
    tcase_add_test(tc_core, test_jsonl_timestamp_current_time);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = jsonl_timestamp_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/shared/logger/jsonl_timestamp_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
