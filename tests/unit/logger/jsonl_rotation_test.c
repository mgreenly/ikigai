// Unit tests for JSONL logger file rotation

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

// Helper: Get first archived log file path
static bool get_first_archive(const char *logs_dir, char *archive_path, size_t path_len)
{
    DIR *dir = opendir(logs_dir);
    if (!dir) return false;

    bool found = false;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".log") &&
            strstr(entry->d_name, "T") &&
            strcmp(entry->d_name, "current.log") != 0) {
            snprintf(archive_path, path_len, "%s/%s", logs_dir, entry->d_name);
            found = true;
            break;
        }
    }
    closedir(dir);
    return found;
}

// Test: First init with no existing log - no rotation
START_TEST(test_init_no_existing_log_no_rotation)
{
    char test_dir[256];
    snprintf(test_dir, sizeof(test_dir), "/tmp/ikigai_rotation_test_%d", getpid());

    // Create test directory
    mkdir(test_dir, 0755);

    // Initialize logger (no existing current.log)
    ik_log_init(test_dir);

    // Check that current.log exists
    char log_file[512];
    snprintf(log_file, sizeof(log_file), "%s/.ikigai/logs/current.log", test_dir);
    struct stat st;
    ck_assert_int_eq(stat(log_file, &st), 0);

    // Check that no archives were created
    char logs_dir[512];
    snprintf(logs_dir, sizeof(logs_dir), "%s/.ikigai/logs", test_dir);
    ck_assert_int_eq(count_log_archives(logs_dir), 0);

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

// Test: Existing current.log gets rotated to timestamped archive
START_TEST(test_init_rotates_existing_log)
{
    char test_dir[256];
    snprintf(test_dir, sizeof(test_dir), "/tmp/ikigai_rotation_test_%d", getpid());

    // Create test directory and logs directory manually
    mkdir(test_dir, 0755);
    char ikigai_dir[512];
    snprintf(ikigai_dir, sizeof(ikigai_dir), "%s/.ikigai", test_dir);
    mkdir(ikigai_dir, 0755);
    char logs_dir[512];
    snprintf(logs_dir, sizeof(logs_dir), "%s/.ikigai/logs", test_dir);
    mkdir(logs_dir, 0755);

    // Create existing current.log with content
    char log_file[512];
    snprintf(log_file, sizeof(log_file), "%s/.ikigai/logs/current.log", test_dir);
    FILE *f = fopen(log_file, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\"event\":\"old_entry\"}\n");
    fclose(f);

    // Initialize logger - should rotate existing file
    ik_log_init(test_dir);

    // Check that current.log exists and is empty (new file)
    struct stat st;
    ck_assert_int_eq(stat(log_file, &st), 0);
    ck_assert_int_eq((int)st.st_size, 0);

    // Check that one archive was created
    ck_assert_int_eq(count_log_archives(logs_dir), 1);

    // Find the archive file
    char archive_path[512];
    ck_assert(get_first_archive(logs_dir, archive_path, sizeof(archive_path)));

    // Verify archive contains original content
    f = fopen(archive_path, "r");
    ck_assert_ptr_nonnull(f);
    char line[256];
    ck_assert_ptr_nonnull(fgets(line, sizeof(line), f));
    ck_assert_ptr_nonnull(strstr(line, "old_entry"));
    fclose(f);

    // Cleanup
    ik_log_shutdown();
    unlink(log_file);
    unlink(archive_path);
    rmdir(logs_dir);
    rmdir(ikigai_dir);
    rmdir(test_dir);
}
END_TEST

// Test: Multiple initializations create multiple archives
START_TEST(test_multiple_rotations_create_multiple_archives)
{
    char test_dir[256];
    snprintf(test_dir, sizeof(test_dir), "/tmp/ikigai_rotation_test_%d", getpid());

    // Create test directory
    mkdir(test_dir, 0755);
    char ikigai_dir[512];
    snprintf(ikigai_dir, sizeof(ikigai_dir), "%s/.ikigai", test_dir);
    char logs_dir[512];
    snprintf(logs_dir, sizeof(logs_dir), "%s/.ikigai/logs", test_dir);
    char log_file[512];
    snprintf(log_file, sizeof(log_file), "%s/.ikigai/logs/current.log", test_dir);

    // First init - creates current.log
    ik_log_init(test_dir);
    yyjson_mut_doc *doc1 = ik_log_create();
    yyjson_mut_val *root1 = yyjson_mut_doc_get_root(doc1);
    yyjson_mut_obj_add_str(doc1, root1, "event", "first");
    ik_log_debug_json(doc1);
    ik_log_shutdown();

    // Sleep briefly to ensure different timestamp
    usleep(10000); // 10ms

    // Second init - should rotate first current.log
    ik_log_init(test_dir);
    yyjson_mut_doc *doc2 = ik_log_create();
    yyjson_mut_val *root2 = yyjson_mut_doc_get_root(doc2);
    yyjson_mut_obj_add_str(doc2, root2, "event", "second");
    ik_log_debug_json(doc2);
    ik_log_shutdown();

    // Sleep briefly to ensure different timestamp
    usleep(10000); // 10ms

    // Third init - should rotate second current.log
    ik_log_init(test_dir);
    yyjson_mut_doc *doc3 = ik_log_create();
    yyjson_mut_val *root3 = yyjson_mut_doc_get_root(doc3);
    yyjson_mut_obj_add_str(doc3, root3, "event", "third");
    ik_log_debug_json(doc3);
    ik_log_shutdown();

    // Should have 2 archives (first and second logs were rotated)
    ck_assert_int_eq(count_log_archives(logs_dir), 2);

    // Current.log should contain third entry
    FILE *f = fopen(log_file, "r");
    ck_assert_ptr_nonnull(f);
    char line[512];
    ck_assert_ptr_nonnull(fgets(line, sizeof(line), f));
    ck_assert_ptr_nonnull(strstr(line, "third"));
    fclose(f);

    // Cleanup - remove all archive files
    DIR *dir = opendir(logs_dir);
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            char path[1024];
            snprintf(path, sizeof(path), "%s/%s", logs_dir, entry->d_name);
            unlink(path);
        }
    }
    closedir(dir);
    rmdir(logs_dir);
    rmdir(ikigai_dir);
    rmdir(test_dir);
}
END_TEST

// Test: Archived filename has correct timestamp format (no colons for filesystem safety)
START_TEST(test_archive_filename_format)
{
    char test_dir[256];
    snprintf(test_dir, sizeof(test_dir), "/tmp/ikigai_rotation_test_%d", getpid());

    // Create test directory and existing log
    mkdir(test_dir, 0755);
    char ikigai_dir[512];
    snprintf(ikigai_dir, sizeof(ikigai_dir), "%s/.ikigai", test_dir);
    mkdir(ikigai_dir, 0755);
    char logs_dir[512];
    snprintf(logs_dir, sizeof(logs_dir), "%s/.ikigai/logs", test_dir);
    mkdir(logs_dir, 0755);
    char log_file[512];
    snprintf(log_file, sizeof(log_file), "%s/.ikigai/logs/current.log", test_dir);
    FILE *f = fopen(log_file, "w");
    fprintf(f, "test\n");
    fclose(f);

    // Initialize - should rotate
    ik_log_init(test_dir);

    // Get archive filename
    char archive_path[512];
    ck_assert(get_first_archive(logs_dir, archive_path, sizeof(archive_path)));

    // Extract just the filename
    char *filename = strrchr(archive_path, '/');
    ck_assert_ptr_nonnull(filename);
    filename++; // Skip the '/'

    // Verify format: YYYY-MM-DDTHH-MM-SS.sss±HH-MM.log
    // Should have no colons (replaced with hyphens)
    ck_assert_ptr_null(strchr(filename, ':'));

    // Should have the 'T' separator
    ck_assert_ptr_nonnull(strchr(filename, 'T'));

    // Should end with .log
    ck_assert_ptr_nonnull(strstr(filename, ".log"));

    // Should have date format YYYY-MM-DD at start
    ck_assert(strlen(filename) > 10);
    ck_assert_int_eq(filename[4], '-');
    ck_assert_int_eq(filename[7], '-');

    // Cleanup
    ik_log_shutdown();
    unlink(log_file);
    unlink(archive_path);
    rmdir(logs_dir);
    rmdir(ikigai_dir);
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

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
