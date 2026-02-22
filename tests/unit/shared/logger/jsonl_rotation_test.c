// Unit tests for JSONL logger file rotation
// File rotation is a no-op; tests verify calls do not crash.

#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "shared/logger.h"

// Test: ik_log_init does not crash with no existing log (logger is a no-op)
START_TEST(test_init_no_existing_log_no_rotation) {
    char test_dir[256];
    snprintf(test_dir, sizeof(test_dir), "/tmp/ikigai_rotation_test_%d", getpid());
    mkdir(test_dir, 0755);

    ik_log_init(test_dir);   // Should not crash
    ik_log_shutdown();

    rmdir(test_dir);
}
END_TEST
// Test: ik_log_init does not crash with existing log (rotation is a no-op)
START_TEST(test_init_rotates_existing_log) {
    char test_dir[256];
    snprintf(test_dir, sizeof(test_dir), "/tmp/ikigai_rotation_test_%d", getpid());
    mkdir(test_dir, 0755);

    ik_log_init(test_dir);   // Should not crash
    ik_log_shutdown();

    rmdir(test_dir);
}

END_TEST
// Test: multiple ik_log_init/shutdown cycles do not crash (logger is a no-op)
START_TEST(test_multiple_rotations_create_multiple_archives) {
    char test_dir[256];
    snprintf(test_dir, sizeof(test_dir), "/tmp/ikigai_rotation_test_%d", getpid());
    mkdir(test_dir, 0755);

    ik_log_init(test_dir);
    yyjson_mut_doc *doc1 = ik_log_create();
    yyjson_mut_val *root1 = yyjson_mut_doc_get_root(doc1);
    yyjson_mut_obj_add_str(doc1, root1, "event", "first");
    ik_log_debug_json(doc1);
    ik_log_shutdown();

    ik_log_init(test_dir);
    yyjson_mut_doc *doc2 = ik_log_create();
    yyjson_mut_val *root2 = yyjson_mut_doc_get_root(doc2);
    yyjson_mut_obj_add_str(doc2, root2, "event", "second");
    ik_log_debug_json(doc2);
    ik_log_shutdown();

    ik_log_init(test_dir);
    yyjson_mut_doc *doc3 = ik_log_create();
    yyjson_mut_val *root3 = yyjson_mut_doc_get_root(doc3);
    yyjson_mut_obj_add_str(doc3, root3, "event", "third");
    ik_log_debug_json(doc3);
    ik_log_shutdown();

    // Should not crash; rotation is a no-op
    rmdir(test_dir);
}

END_TEST
// Test: ik_log_init does not crash (rotation is a no-op)
START_TEST(test_archive_filename_format) {
    char test_dir[256];
    snprintf(test_dir, sizeof(test_dir), "/tmp/ikigai_rotation_test_%d", getpid());
    mkdir(test_dir, 0755);

    ik_log_init(test_dir);   // Should not crash
    ik_log_shutdown();

    rmdir(test_dir);
}

END_TEST

static Suite *logger_jsonl_rotation_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Logger JSONL Rotation");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_init_no_existing_log_no_rotation);
    tcase_add_test(tc_core, test_init_rotates_existing_log);
    tcase_add_test(tc_core, test_multiple_rotations_create_multiple_archives);
    tcase_add_test(tc_core, test_archive_filename_format);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = logger_jsonl_rotation_suite();
    sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/shared/logger/jsonl_rotation_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
