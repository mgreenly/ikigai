// Unit tests for JSONL logger file output

#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "../../../src/logger.h"

// Test: ik_log_init creates .ikigai/logs directory and current.log file
START_TEST(test_log_init_creates_log_file) {
    char test_dir[256];
    snprintf(test_dir, sizeof(test_dir), "/tmp/ikigai_log_test_%d", getpid());

    // Create test directory
    mkdir(test_dir, 0755);

    // Initialize logger with temp directory
    ik_log_init(test_dir);

    // Check that .ikigai/logs directory was created
    char logs_dir[512];
    snprintf(logs_dir, sizeof(logs_dir), "%s/.ikigai/logs", test_dir);
    struct stat st;
    ck_assert_int_eq(stat(logs_dir, &st), 0);
    ck_assert(S_ISDIR(st.st_mode));

    // Check that current.log file exists
    char log_file[512];
    snprintf(log_file, sizeof(log_file), "%s/.ikigai/logs/current.log", test_dir);
    ck_assert_int_eq(stat(log_file, &st), 0);
    ck_assert(S_ISREG(st.st_mode));

    // Cleanup
    ik_log_shutdown();
    unlink(log_file);
    rmdir(logs_dir);
    char ikigai_dir[512];
    snprintf(ikigai_dir, sizeof(ikigai_dir), "%s/.ikigai", test_dir);
    rmdir(ikigai_dir);
    rmdir(test_dir);
}
END_TEST
// Test: ik_log_debug_json writes to current.log file
START_TEST(test_log_writes_to_file) {
    char test_dir[256];
    snprintf(test_dir, sizeof(test_dir), "/tmp/ikigai_log_test_%d", getpid());

    // Create test directory
    mkdir(test_dir, 0755);

    // Initialize logger
    ik_log_init(test_dir);

    // Write a log entry
    yyjson_mut_doc *doc = ik_log_create();
    yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);
    yyjson_mut_obj_add_str(doc, root, "event", "test_event");
    yyjson_mut_obj_add_int(doc, root, "value", 42);
    ik_log_debug_json(doc);

    // Read the log file
    char log_file[512];
    snprintf(log_file, sizeof(log_file), "%s/.ikigai/logs/current.log", test_dir);
    FILE *f = fopen(log_file, "r");
    ck_assert_ptr_nonnull(f);

    char line[4096];
    ck_assert_ptr_nonnull(fgets(line, sizeof(line), f));
    fclose(f);

    // Verify the log entry contains expected fields
    ck_assert_ptr_nonnull(strstr(line, "\"level\":\"debug\""));
    ck_assert_ptr_nonnull(strstr(line, "\"timestamp\""));
    ck_assert_ptr_nonnull(strstr(line, "\"logline\""));
    ck_assert_ptr_nonnull(strstr(line, "\"event\":\"test_event\""));
    ck_assert_ptr_nonnull(strstr(line, "\"value\":42"));

    // Cleanup
    ik_log_shutdown();
    unlink(log_file);
    char logs_dir[512];
    snprintf(logs_dir, sizeof(logs_dir), "%s/.ikigai/logs", test_dir);
    rmdir(logs_dir);
    char ikigai_dir[512];
    snprintf(ikigai_dir, sizeof(ikigai_dir), "%s/.ikigai", test_dir);
    rmdir(ikigai_dir);
    rmdir(test_dir);
}

END_TEST
// Test: multiple log entries append correctly
START_TEST(test_multiple_log_entries_append) {
    char test_dir[256];
    snprintf(test_dir, sizeof(test_dir), "/tmp/ikigai_log_test_%d", getpid());

    // Create test directory
    mkdir(test_dir, 0755);

    // Initialize logger
    ik_log_init(test_dir);

    // Write first log entry
    yyjson_mut_doc *doc1 = ik_log_create();
    yyjson_mut_val *root1 = yyjson_mut_doc_get_root(doc1);
    yyjson_mut_obj_add_str(doc1, root1, "event", "first");
    ik_log_debug_json(doc1);

    // Write second log entry
    yyjson_mut_doc *doc2 = ik_log_create();
    yyjson_mut_val *root2 = yyjson_mut_doc_get_root(doc2);
    yyjson_mut_obj_add_str(doc2, root2, "event", "second");
    ik_log_debug_json(doc2);

    // Read the log file
    char log_file[512];
    snprintf(log_file, sizeof(log_file), "%s/.ikigai/logs/current.log", test_dir);
    FILE *f = fopen(log_file, "r");
    ck_assert_ptr_nonnull(f);

    char line1[4096];
    char line2[4096];
    ck_assert_ptr_nonnull(fgets(line1, sizeof(line1), f));
    ck_assert_ptr_nonnull(fgets(line2, sizeof(line2), f));
    fclose(f);

    // Verify both entries
    ck_assert_ptr_nonnull(strstr(line1, "\"event\":\"first\""));
    ck_assert_ptr_nonnull(strstr(line2, "\"event\":\"second\""));

    // Cleanup
    ik_log_shutdown();
    unlink(log_file);
    char logs_dir[512];
    snprintf(logs_dir, sizeof(logs_dir), "%s/.ikigai/logs", test_dir);
    rmdir(logs_dir);
    char ikigai_dir[512];
    snprintf(ikigai_dir, sizeof(ikigai_dir), "%s/.ikigai", test_dir);
    rmdir(ikigai_dir);
    rmdir(test_dir);
}

END_TEST
// Test: ik_log_shutdown closes file
START_TEST(test_log_shutdown_closes_file) {
    char test_dir[256];
    snprintf(test_dir, sizeof(test_dir), "/tmp/ikigai_log_test_%d", getpid());

    // Create test directory
    mkdir(test_dir, 0755);

    // Initialize and shutdown logger
    ik_log_init(test_dir);
    ik_log_shutdown();

    // The file should exist after shutdown
    char log_file[512];
    snprintf(log_file, sizeof(log_file), "%s/.ikigai/logs/current.log", test_dir);
    struct stat st;
    ck_assert_int_eq(stat(log_file, &st), 0);

    // Cleanup
    unlink(log_file);
    char logs_dir[512];
    snprintf(logs_dir, sizeof(logs_dir), "%s/.ikigai/logs", test_dir);
    rmdir(logs_dir);
    char ikigai_dir[512];
    snprintf(ikigai_dir, sizeof(ikigai_dir), "%s/.ikigai", test_dir);
    rmdir(ikigai_dir);
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
    srunner_set_xml(sr, "reports/check/unit/logger/jsonl_file_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
