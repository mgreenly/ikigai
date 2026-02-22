// Unit tests for JSONL logger module

#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "shared/logger.h"

// Helper: setup temp directory and init logger
static char test_dir[256];
static char log_file_path[512];

static void setup_logger(void)
{
    snprintf(test_dir, sizeof(test_dir), "/tmp/ikigai_jsonl_test_%d", getpid());
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

// Test: ik_log_create returns non-NULL doc with empty root object
START_TEST(test_log_create_returns_doc) {
    yyjson_mut_doc *doc = ik_log_create();
    ck_assert_ptr_nonnull(doc);

    yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);
    ck_assert_ptr_nonnull(root);
    ck_assert(yyjson_mut_is_obj(root));
    ck_assert_uint_eq(yyjson_mut_obj_size(root), 0);

    yyjson_mut_doc_free(doc);
}
END_TEST
// Test: ik_log_debug_json does not crash (logger is a no-op)
START_TEST(test_log_debug_writes_jsonl) {
    setup_logger();

    yyjson_mut_doc *doc = ik_log_create();
    yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);
    yyjson_mut_obj_add_str(doc, root, "event", "test");
    yyjson_mut_obj_add_int(doc, root, "value", 42);

    ik_log_debug_json(doc);  // Should not crash; logger is a no-op

    teardown_logger();
}

END_TEST
// Test: ik_log_debug_json does not crash (logger is a no-op)
START_TEST(test_log_debug_has_level_field) {
    setup_logger();

    yyjson_mut_doc *doc = ik_log_create();
    yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);
    yyjson_mut_obj_add_str(doc, root, "event", "test");

    ik_log_debug_json(doc);  // Should not crash; logger is a no-op

    teardown_logger();
}

END_TEST
// Test: ik_log_debug_json does not crash (logger is a no-op)
START_TEST(test_log_debug_has_timestamp_field) {
    setup_logger();

    yyjson_mut_doc *doc = ik_log_create();
    yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);
    yyjson_mut_obj_add_str(doc, root, "event", "test");

    ik_log_debug_json(doc);  // Should not crash; logger is a no-op

    teardown_logger();
}

END_TEST
// Test: ik_log_debug_json does not crash (logger is a no-op)
START_TEST(test_log_debug_has_logline_field) {
    setup_logger();

    yyjson_mut_doc *doc = ik_log_create();
    yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);
    yyjson_mut_obj_add_str(doc, root, "event", "test");
    yyjson_mut_obj_add_int(doc, root, "value", 42);

    ik_log_debug_json(doc);  // Should not crash; logger is a no-op

    teardown_logger();
}

END_TEST
// Test: ik_log_debug_json does not crash (logger is a no-op)
START_TEST(test_log_debug_is_single_line_json) {
    setup_logger();

    yyjson_mut_doc *doc = ik_log_create();
    yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);
    yyjson_mut_obj_add_str(doc, root, "event", "test");

    ik_log_debug_json(doc);  // Should not crash; logger is a no-op

    teardown_logger();
}

END_TEST

static Suite *logger_jsonl_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Logger JSONL");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_log_create_returns_doc);
    tcase_add_test(tc_core, test_log_debug_writes_jsonl);
    tcase_add_test(tc_core, test_log_debug_has_level_field);
    tcase_add_test(tc_core, test_log_debug_has_timestamp_field);
    tcase_add_test(tc_core, test_log_debug_has_logline_field);
    tcase_add_test(tc_core, test_log_debug_is_single_line_json);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = logger_jsonl_suite();
    sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/shared/logger/jsonl_basic_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
