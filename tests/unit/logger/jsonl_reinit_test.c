// Unit tests for JSONL logger reinitialization

#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include "../../../src/logger.h"

// Helper: Count files matching pattern in directory
static int count_log_archives(const char *logs_dir)
{
    DIR *dir = opendir(logs_dir);
    if (!dir) return 0;

    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Match timestamped log files: YYYY-MM-DDTHH-MM-SS.sss±HH-MM.log
        if (strstr(entry->d_name, ".log") &&
            strstr(entry->d_name, "T") &&
            strcmp(entry->d_name, "current.log") != 0) {
            count++;
        }
    }
    closedir(dir);
    return count;
}

// Helper: Read single line from file
static bool read_single_line(const char *file_path, char *line, size_t line_len)
{
    FILE *f = fopen(file_path, "r");
    if (!f) return false;

    bool success = fgets(line, (int)line_len, f) != NULL;
    fclose(f);
    return success;
}

// Test: Initialize logger with dir1, write entries, reinit to dir2, write new entries
START_TEST(test_reinit_switches_directory) {
    char test_dir1[256];
    char test_dir2[256];
    snprintf(test_dir1, sizeof(test_dir1), "/tmp/ikigai_reinit_test1_%d", getpid());
    snprintf(test_dir2, sizeof(test_dir2), "/tmp/ikigai_reinit_test2_%d", getpid());

    // Create test directories
    mkdir(test_dir1, 0755);
    mkdir(test_dir2, 0755);

    // Initialize logger with dir1
    ik_log_init(test_dir1);

    // Write entry to dir1
    yyjson_mut_doc *doc1 = ik_log_create();
    yyjson_mut_val *root1 = yyjson_mut_doc_get_root(doc1);
    yyjson_mut_obj_add_str(doc1, root1, "event", "dir1_entry");
    ik_log_debug_json(doc1);

    // Reinit to dir2
    ik_log_reinit(test_dir2);

    // Write entry to dir2
    yyjson_mut_doc *doc2 = ik_log_create();
    yyjson_mut_val *root2 = yyjson_mut_doc_get_root(doc2);
    yyjson_mut_obj_add_str(doc2, root2, "event", "dir2_entry");
    ik_log_debug_json(doc2);

    // Verify dir1 log file still exists and contains original entry
    char dir1_log[512];
    snprintf(dir1_log, sizeof(dir1_log), "%s/.ikigai/logs/current.log", test_dir1);
    char dir1_line[4096];
    ck_assert(read_single_line(dir1_log, dir1_line, sizeof(dir1_line)));
    ck_assert_ptr_nonnull(strstr(dir1_line, "dir1_entry"));

    // Verify dir2 log file contains new entry
    char dir2_log[512];
    snprintf(dir2_log, sizeof(dir2_log), "%s/.ikigai/logs/current.log", test_dir2);
    char dir2_line[4096];
    ck_assert(read_single_line(dir2_log, dir2_line, sizeof(dir2_line)));
    ck_assert_ptr_nonnull(strstr(dir2_line, "dir2_entry"));

    // Cleanup
    ik_log_shutdown();
    unlink(dir1_log);
    char dir1_logs[512];
    snprintf(dir1_logs, sizeof(dir1_logs), "%s/.ikigai/logs", test_dir1);
    rmdir(dir1_logs);
    char dir1_ikigai[512];
    snprintf(dir1_ikigai, sizeof(dir1_ikigai), "%s/.ikigai", test_dir1);
    rmdir(dir1_ikigai);
    rmdir(test_dir1);

    unlink(dir2_log);
    char dir2_logs[512];
    snprintf(dir2_logs, sizeof(dir2_logs), "%s/.ikigai/logs", test_dir2);
    rmdir(dir2_logs);
    char dir2_ikigai[512];
    snprintf(dir2_ikigai, sizeof(dir2_ikigai), "%s/.ikigai", test_dir2);
    rmdir(dir2_ikigai);
    rmdir(test_dir2);
}
END_TEST
// Test: Reinit rotates existing current.log in new directory
START_TEST(test_reinit_rotates_existing_log_in_new_dir) {
    char test_dir1[256];
    char test_dir2[256];
    snprintf(test_dir1, sizeof(test_dir1), "/tmp/ikigai_reinit_test1_%d", getpid());
    snprintf(test_dir2, sizeof(test_dir2), "/tmp/ikigai_reinit_test2_%d", getpid());

    // Create test directories
    mkdir(test_dir1, 0755);
    mkdir(test_dir2, 0755);

    // Initialize logger with dir1
    ik_log_init(test_dir1);

    // Write entry to dir1
    yyjson_mut_doc *doc1 = ik_log_create();
    yyjson_mut_val *root1 = yyjson_mut_doc_get_root(doc1);
    yyjson_mut_obj_add_str(doc1, root1, "event", "dir1_entry");
    ik_log_debug_json(doc1);
    ik_log_shutdown();

    // Manually create dir2 with existing current.log
    char dir2_ikigai[512];
    snprintf(dir2_ikigai, sizeof(dir2_ikigai), "%s/.ikigai", test_dir2);
    mkdir(dir2_ikigai, 0755);
    char dir2_logs[512];
    snprintf(dir2_logs, sizeof(dir2_logs), "%s/.ikigai/logs", test_dir2);
    mkdir(dir2_logs, 0755);
    char dir2_log[512];
    snprintf(dir2_log, sizeof(dir2_log), "%s/.ikigai/logs/current.log", test_dir2);
    FILE *f = fopen(dir2_log, "w");
    fprintf(f, "{\"event\":\"old_dir2_entry\"}\n");
    fclose(f);

    // Reinit to dir2 (should close dir1, rotate dir2's existing log, open new dir2 log)
    ik_log_reinit(test_dir2);

    // Check that one archive was created in dir2
    ck_assert_int_eq(count_log_archives(dir2_logs), 1);

    // Check that current.log in dir2 is empty (new file)
    struct stat st;
    ck_assert_int_eq(stat(dir2_log, &st), 0);
    ck_assert_int_eq((int)st.st_size, 0);

    // Write entry to new dir2 log
    yyjson_mut_doc *doc2 = ik_log_create();
    yyjson_mut_val *root2 = yyjson_mut_doc_get_root(doc2);
    yyjson_mut_obj_add_str(doc2, root2, "event", "new_dir2_entry");
    ik_log_debug_json(doc2);

    // Verify current.log contains new entry
    char dir2_line[4096];
    ck_assert(read_single_line(dir2_log, dir2_line, sizeof(dir2_line)));
    ck_assert_ptr_nonnull(strstr(dir2_line, "new_dir2_entry"));

    // Cleanup
    ik_log_shutdown();

    // Remove all files from dir2/logs
    DIR *dir = opendir(dir2_logs);
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            char path[1024];
            snprintf(path, sizeof(path), "%s/%s", dir2_logs, entry->d_name);
            unlink(path);
        }
    }
    closedir(dir);
    rmdir(dir2_logs);
    rmdir(dir2_ikigai);
    rmdir(test_dir2);

    // Clean up dir1
    char dir1_log[512];
    snprintf(dir1_log, sizeof(dir1_log), "%s/.ikigai/logs/current.log", test_dir1);
    unlink(dir1_log);
    char dir1_logs[512];
    snprintf(dir1_logs, sizeof(dir1_logs), "%s/.ikigai/logs", test_dir1);
    rmdir(dir1_logs);
    char dir1_ikigai[512];
    snprintf(dir1_ikigai, sizeof(dir1_ikigai), "%s/.ikigai", test_dir1);
    rmdir(dir1_ikigai);
    rmdir(test_dir1);
}

END_TEST
// Test: Reinit with no existing log in new directory doesn't create archives
START_TEST(test_reinit_no_existing_log_in_new_dir) {
    char test_dir1[256];
    char test_dir2[256];
    snprintf(test_dir1, sizeof(test_dir1), "/tmp/ikigai_reinit_test1_%d", getpid());
    snprintf(test_dir2, sizeof(test_dir2), "/tmp/ikigai_reinit_test2_%d", getpid());

    // Create test directories
    mkdir(test_dir1, 0755);
    mkdir(test_dir2, 0755);

    // Initialize logger with dir1
    ik_log_init(test_dir1);

    // Write entry to dir1
    yyjson_mut_doc *doc1 = ik_log_create();
    yyjson_mut_val *root1 = yyjson_mut_doc_get_root(doc1);
    yyjson_mut_obj_add_str(doc1, root1, "event", "dir1_entry");
    ik_log_debug_json(doc1);

    // Reinit to dir2 (dir2 has no existing log)
    ik_log_reinit(test_dir2);

    // Check that no archives were created in dir2
    char dir2_logs[512];
    snprintf(dir2_logs, sizeof(dir2_logs), "%s/.ikigai/logs", test_dir2);
    ck_assert_int_eq(count_log_archives(dir2_logs), 0);

    // Check that current.log exists and is empty in dir2
    char dir2_log[512];
    snprintf(dir2_log, sizeof(dir2_log), "%s/.ikigai/logs/current.log", test_dir2);
    struct stat st;
    ck_assert_int_eq(stat(dir2_log, &st), 0);
    ck_assert_int_eq((int)st.st_size, 0);

    // Write entry to dir2
    yyjson_mut_doc *doc2 = ik_log_create();
    yyjson_mut_val *root2 = yyjson_mut_doc_get_root(doc2);
    yyjson_mut_obj_add_str(doc2, root2, "event", "dir2_entry");
    ik_log_debug_json(doc2);

    // Verify dir2 log contains new entry
    char dir2_line[4096];
    ck_assert(read_single_line(dir2_log, dir2_line, sizeof(dir2_line)));
    ck_assert_ptr_nonnull(strstr(dir2_line, "dir2_entry"));

    // Cleanup
    ik_log_shutdown();
    unlink(dir2_log);
    rmdir(dir2_logs);
    char dir2_ikigai[512];
    snprintf(dir2_ikigai, sizeof(dir2_ikigai), "%s/.ikigai", test_dir2);
    rmdir(dir2_ikigai);
    rmdir(test_dir2);

    char dir1_log[512];
    snprintf(dir1_log, sizeof(dir1_log), "%s/.ikigai/logs/current.log", test_dir1);
    unlink(dir1_log);
    char dir1_logs[512];
    snprintf(dir1_logs, sizeof(dir1_logs), "%s/.ikigai/logs", test_dir1);
    rmdir(dir1_logs);
    char dir1_ikigai[512];
    snprintf(dir1_ikigai, sizeof(dir1_ikigai), "%s/.ikigai", test_dir1);
    rmdir(dir1_ikigai);
    rmdir(test_dir1);
}

END_TEST

static Suite *logger_jsonl_reinit_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Logger JSONL Reinit");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_reinit_switches_directory);
    tcase_add_test(tc_core, test_reinit_rotates_existing_log_in_new_dir);
    tcase_add_test(tc_core, test_reinit_no_existing_log_in_new_dir);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = logger_jsonl_reinit_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
