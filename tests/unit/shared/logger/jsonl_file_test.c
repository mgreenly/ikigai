// Unit tests for JSONL logger file output

#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "shared/logger.h"

// Test: ik_log_init and ik_log_shutdown do not crash (logger is a no-op)
START_TEST(test_log_init_creates_log_file) {
    char test_dir[256];
    snprintf(test_dir, sizeof(test_dir), "/tmp/ikigai_log_test_%d", getpid());
    mkdir(test_dir, 0755);

    ik_log_init(test_dir);   // Should not crash
    ik_log_shutdown();

    rmdir(test_dir);
}
END_TEST
// Test: ik_log_debug_json does not crash (logger is a no-op)
START_TEST(test_log_writes_to_file) {
    char test_dir[256];
    snprintf(test_dir, sizeof(test_dir), "/tmp/ikigai_log_test_%d", getpid());
    mkdir(test_dir, 0755);

    ik_log_init(test_dir);

    yyjson_mut_doc *doc = ik_log_create();
    yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);
    yyjson_mut_obj_add_str(doc, root, "event", "test_event");
    yyjson_mut_obj_add_int(doc, root, "value", 42);
    ik_log_debug_json(doc);  // Should not crash; logger is a no-op

    ik_log_shutdown();
    rmdir(test_dir);
}

END_TEST
// Test: multiple ik_log_debug_json calls do not crash (logger is a no-op)
START_TEST(test_multiple_log_entries_append) {
    char test_dir[256];
    snprintf(test_dir, sizeof(test_dir), "/tmp/ikigai_log_test_%d", getpid());
    mkdir(test_dir, 0755);

    ik_log_init(test_dir);

    yyjson_mut_doc *doc1 = ik_log_create();
    yyjson_mut_val *root1 = yyjson_mut_doc_get_root(doc1);
    yyjson_mut_obj_add_str(doc1, root1, "event", "first");
    ik_log_debug_json(doc1);  // Should not crash

    yyjson_mut_doc *doc2 = ik_log_create();
    yyjson_mut_val *root2 = yyjson_mut_doc_get_root(doc2);
    yyjson_mut_obj_add_str(doc2, root2, "event", "second");
    ik_log_debug_json(doc2);  // Should not crash

    ik_log_shutdown();
    rmdir(test_dir);
}

END_TEST
// Test: ik_log_shutdown does not crash (logger is a no-op)
START_TEST(test_log_shutdown_closes_file) {
    char test_dir[256];
    snprintf(test_dir, sizeof(test_dir), "/tmp/ikigai_log_test_%d", getpid());
    mkdir(test_dir, 0755);

    ik_log_init(test_dir);
    ik_log_shutdown();  // Should not crash

    rmdir(test_dir);
}

END_TEST

static Suite *logger_jsonl_file_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Logger JSONL File");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_log_init_creates_log_file);
    tcase_add_test(tc_core, test_log_writes_to_file);
    tcase_add_test(tc_core, test_multiple_log_entries_append);
    tcase_add_test(tc_core, test_log_shutdown_closes_file);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = logger_jsonl_file_suite();
    sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/shared/logger/jsonl_file_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
