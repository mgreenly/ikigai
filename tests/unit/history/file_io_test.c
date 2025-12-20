/**
 * @file file_io_test.c
 * @brief Unit tests for history file I/O (JSONL load/save)
 */

#include "../../../src/history.h"
#include "../../../src/history_io.h"
#include "../../../src/error.h"
#include "../../../src/wrapper.h"
#include "../../test_utils.h"

#include <check.h>
#include <talloc.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

// Forward declaration for suite function
static Suite *history_file_io_suite(void);

// Test fixture
static void *ctx;
static char test_dir_buffer[256];
static ik_history_t *hist;

static void setup(void)
{
    ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create a temporary test directory
    snprintf(test_dir_buffer, sizeof(test_dir_buffer), "/tmp/ikigai-history-io-test-XXXXXX");
    char *result = mkdtemp(test_dir_buffer);
    ck_assert_ptr_nonnull(result);

    // Change to test directory for relative path tests
    ck_assert_int_eq(chdir(test_dir_buffer), 0);

    // Create history with capacity 10
    hist = ik_history_create(ctx, 10);
    ck_assert_ptr_nonnull(hist);
}

static void teardown(void)
{
    // Change back to original directory
    chdir("/");

    // Cleanup test directory
    if (test_dir_buffer[0] != '\0') {
        // Remove .ikigai/history file if it exists
        char history_path[512];
        snprintf(history_path, sizeof(history_path), "%s/.ikigai/history", test_dir_buffer);
        unlink(history_path);

        // Remove .ikigai directory if it exists
        char ikigai_path[512];
        snprintf(ikigai_path, sizeof(ikigai_path), "%s/.ikigai", test_dir_buffer);
        struct stat st;
        if (stat(ikigai_path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                rmdir(ikigai_path);
            } else {
                unlink(ikigai_path);
            }
        }

        // Remove test directory
        rmdir(test_dir_buffer);
    }

    talloc_free(ctx);
}

// Test: Load from empty file (creates file if missing)
START_TEST(test_history_load_empty_file) {
    // Load when file doesn't exist - should succeed with empty history
    res_t res = ik_history_load(ctx, hist, NULL);
    ck_assert(is_ok(&res));

    // Verify history is empty
    ck_assert_uint_eq(hist->count, 0);

    // Verify file was created
    struct stat st;
    ck_assert_int_eq(stat(".ikigai/history", &st), 0);
}
END_TEST
// Test: Load valid JSONL entries
START_TEST(test_history_load_valid_entries)
{
    // Create directory and file with valid entries
    ck_assert_int_eq(mkdir(".ikigai", 0755), 0);

    FILE *f = fopen(".ikigai/history", "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\"cmd\": \"/clear\", \"ts\": \"2025-01-15T10:30:00Z\"}\n");
    fprintf(f, "{\"cmd\": \"hello\\nworld\", \"ts\": \"2025-01-15T10:31:00Z\"}\n");
    fprintf(f, "{\"cmd\": \"/model gpt-4o\", \"ts\": \"2025-01-15T10:32:00Z\"}\n");
    fclose(f);

    // Load history
    res_t res = ik_history_load(ctx, hist, NULL);
    ck_assert(is_ok(&res));

    // Verify all entries loaded
    ck_assert_uint_eq(hist->count, 3);
    ck_assert_str_eq(hist->entries[0], "/clear");
    ck_assert_str_eq(hist->entries[1], "hello\nworld");  // Multi-line preserved
    ck_assert_str_eq(hist->entries[2], "/model gpt-4o");
}

END_TEST
// Test: Load respects capacity limit
START_TEST(test_history_load_respects_capacity)
{
    // Create directory and file with more entries than capacity
    ck_assert_int_eq(mkdir(".ikigai", 0755), 0);

    FILE *f = fopen(".ikigai/history", "w");
    ck_assert_ptr_nonnull(f);

    // Write 15 entries (capacity is 10)
    for (int i = 0; i < 15; i++) {
        fprintf(f, "{\"cmd\": \"command %d\", \"ts\": \"2025-01-15T10:%02d:00Z\"}\n", i, i);
    }
    fclose(f);

    // Load history
    res_t res = ik_history_load(ctx, hist, NULL);
    ck_assert(is_ok(&res));

    // Verify only last 10 entries loaded
    ck_assert_uint_eq(hist->count, 10);
    ck_assert_str_eq(hist->entries[0], "command 5");   // First kept entry
    ck_assert_str_eq(hist->entries[9], "command 14");  // Last entry
}

END_TEST
// Test: Load skips malformed lines
START_TEST(test_history_load_malformed_line)
{
    // Create directory and file with mix of valid and malformed entries
    ck_assert_int_eq(mkdir(".ikigai", 0755), 0);

    FILE *f = fopen(".ikigai/history", "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\"cmd\": \"valid1\", \"ts\": \"2025-01-15T10:30:00Z\"}\n");
    fprintf(f, "not valid json\n");  // Malformed
    fprintf(f, "{\"cmd\": \"valid2\", \"ts\": \"2025-01-15T10:31:00Z\"}\n");
    fprintf(f, "{\"nocmd\": \"missing cmd field\"}\n");  // Missing cmd field
    fprintf(f, "{\"cmd\": \"valid3\", \"ts\": \"2025-01-15T10:32:00Z\"}\n");
    fclose(f);

    // Load history - should skip malformed lines
    res_t res = ik_history_load(ctx, hist, NULL);
    ck_assert(is_ok(&res));

    // Verify only valid entries loaded
    ck_assert_uint_eq(hist->count, 3);
    ck_assert_str_eq(hist->entries[0], "valid1");
    ck_assert_str_eq(hist->entries[1], "valid2");
    ck_assert_str_eq(hist->entries[2], "valid3");
}

END_TEST
// Test: Save writes atomic JSONL file
START_TEST(test_history_save_atomic_write)
{
    // Add entries to history
    ik_history_add(hist, "/clear");
    ik_history_add(hist, "hello\nworld");
    ik_history_add(hist, "/model gpt-4o");

    // Save history
    res_t res = ik_history_save(hist);
    ck_assert(is_ok(&res));

    // Verify file exists
    struct stat st;
    ck_assert_int_eq(stat(".ikigai/history", &st), 0);

    // Read file and verify JSONL format
    FILE *f = fopen(".ikigai/history", "r");
    ck_assert_ptr_nonnull(f);

    char line[512];

    // Read first line - should contain "/clear"
    ck_assert_ptr_nonnull(fgets(line, sizeof(line), f));
    ck_assert(strstr(line, "\"cmd\"") != NULL);
    ck_assert(strstr(line, "\"/clear\"") != NULL);
    ck_assert(strstr(line, "\"ts\"") != NULL);

    // Read second line - should contain "hello\nworld" with \n escaped
    ck_assert_ptr_nonnull(fgets(line, sizeof(line), f));
    ck_assert(strstr(line, "\"cmd\"") != NULL);
    ck_assert(strstr(line, "hello\\nworld") != NULL);  // JSON escaped \n

    // Read third line
    ck_assert_ptr_nonnull(fgets(line, sizeof(line), f));
    ck_assert(strstr(line, "\"/model gpt-4o\"") != NULL);

    // Verify no more lines
    ck_assert_ptr_null(fgets(line, sizeof(line), f));

    fclose(f);

    // Verify temp file was cleaned up
    ck_assert_int_eq(stat(".ikigai/history.tmp", &st), -1);
}

END_TEST
// Test: Append entry to file
START_TEST(test_history_append_entry)
{
    // Create directory and file with initial entries
    ck_assert_int_eq(mkdir(".ikigai", 0755), 0);

    FILE *f = fopen(".ikigai/history", "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\"cmd\": \"entry1\", \"ts\": \"2025-01-15T10:30:00Z\"}\n");
    fprintf(f, "{\"cmd\": \"entry2\", \"ts\": \"2025-01-15T10:31:00Z\"}\n");
    fclose(f);

    // Append new entry
    res_t res = ik_history_append_entry("entry3");
    ck_assert(is_ok(&res));

    // Read file and verify 3 entries
    f = fopen(".ikigai/history", "r");
    ck_assert_ptr_nonnull(f);

    char line[512];
    int line_count = 0;
    while (fgets(line, sizeof(line), f) != NULL) {
        line_count++;
        if (line_count == 3) {
            // Verify third line contains entry3
            ck_assert(strstr(line, "\"entry3\"") != NULL);
        }
    }

    ck_assert_int_eq(line_count, 3);
    fclose(f);
}

END_TEST
// Test: Load when file missing creates empty history
START_TEST(test_history_load_file_missing)
{
    // Don't create file - load should succeed with empty history
    res_t res = ik_history_load(ctx, hist, NULL);
    ck_assert(is_ok(&res));

    // Verify history is empty
    ck_assert_uint_eq(hist->count, 0);

    // Verify directory and file were created
    struct stat st;
    ck_assert_int_eq(stat(".ikigai", &st), 0);
    ck_assert(S_ISDIR(st.st_mode));
    ck_assert_int_eq(stat(".ikigai/history", &st), 0);
}

END_TEST
// Test: Append creates file if missing
START_TEST(test_history_append_creates_file)
{
    // Append when file doesn't exist
    res_t res = ik_history_append_entry("first entry");
    ck_assert(is_ok(&res));

    // Verify file was created
    struct stat st;
    ck_assert_int_eq(stat(".ikigai/history", &st), 0);

    // Verify content
    FILE *f = fopen(".ikigai/history", "r");
    ck_assert_ptr_nonnull(f);

    char line[512];
    ck_assert_ptr_nonnull(fgets(line, sizeof(line), f));
    ck_assert(strstr(line, "\"first entry\"") != NULL);

    fclose(f);
}

END_TEST
// Test: Save empty history
START_TEST(test_history_save_empty)
{
    // Save empty history
    res_t res = ik_history_save(hist);
    ck_assert(is_ok(&res));

    // Verify file exists but is empty
    struct stat st;
    ck_assert_int_eq(stat(".ikigai/history", &st), 0);
    ck_assert_int_eq((int)st.st_size, 0);
}

END_TEST
// Test: Load from file with size 0 (truly empty file)
START_TEST(test_history_load_file_size_zero)
{
    // Create directory and empty file (0 bytes)
    ck_assert_int_eq(mkdir(".ikigai", 0755), 0);

    FILE *f = fopen(".ikigai/history", "w");
    ck_assert_ptr_nonnull(f);
    fclose(f);  // Close immediately without writing - creates 0-byte file

    // Verify file is truly empty
    struct stat st;
    ck_assert_int_eq(stat(".ikigai/history", &st), 0);
    ck_assert_int_eq((int)st.st_size, 0);

    // Load history - should succeed with empty history
    res_t res = ik_history_load(ctx, hist, NULL);
    ck_assert(is_ok(&res));

    // Verify history is empty
    ck_assert_uint_eq(hist->count, 0);
}

END_TEST
// Test: Load from file with last line without newline
START_TEST(test_history_load_last_line_no_newline)
{
    // Create directory and file with entries where last line has no newline
    ck_assert_int_eq(mkdir(".ikigai", 0755), 0);

    FILE *f = fopen(".ikigai/history", "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\"cmd\": \"entry1\", \"ts\": \"2025-01-15T10:30:00Z\"}\n");
    fprintf(f, "{\"cmd\": \"entry2\", \"ts\": \"2025-01-15T10:31:00Z\"}");  // No newline at end
    fclose(f);

    // Load history - should handle missing final newline
    res_t res = ik_history_load(ctx, hist, NULL);
    ck_assert(is_ok(&res));

    // Verify both entries loaded
    ck_assert_uint_eq(hist->count, 2);
    ck_assert_str_eq(hist->entries[0], "entry1");
    ck_assert_str_eq(hist->entries[1], "entry2");
}

END_TEST
// Test: Load from file with empty lines
START_TEST(test_history_load_empty_lines)
{
    // Create directory and file with blank lines between entries
    ck_assert_int_eq(mkdir(".ikigai", 0755), 0);

    FILE *f = fopen(".ikigai/history", "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\"cmd\": \"entry1\", \"ts\": \"2025-01-15T10:30:00Z\"}\n");
    fprintf(f, "\n");  // Empty line
    fprintf(f, "\n");  // Another empty line
    fprintf(f, "{\"cmd\": \"entry2\", \"ts\": \"2025-01-15T10:31:00Z\"}\n");
    fprintf(f, "\n");  // Empty line at end
    fprintf(f, "{\"cmd\": \"entry3\", \"ts\": \"2025-01-15T10:32:00Z\"}\n");
    fclose(f);

    // Load history - should skip empty lines
    res_t res = ik_history_load(ctx, hist, NULL);
    ck_assert(is_ok(&res));

    // Verify only valid entries loaded (empty lines skipped)
    ck_assert_uint_eq(hist->count, 3);
    ck_assert_str_eq(hist->entries[0], "entry1");
    ck_assert_str_eq(hist->entries[1], "entry2");
    ck_assert_str_eq(hist->entries[2], "entry3");
}

END_TEST
// Test: Load from file with non-object JSON
START_TEST(test_history_load_non_object_json)
{
    // Create directory and file with valid JSON that is not an object
    ck_assert_int_eq(mkdir(".ikigai", 0755), 0);

    FILE *f = fopen(".ikigai/history", "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\"cmd\": \"entry1\", \"ts\": \"2025-01-15T10:30:00Z\"}\n");
    fprintf(f, "\"just a string\"\n");  // Valid JSON, but not an object
    fprintf(f, "123\n");  // Valid JSON number, but not an object
    fprintf(f, "[1, 2, 3]\n");  // Valid JSON array, but not an object
    fprintf(f, "{\"cmd\": \"entry2\", \"ts\": \"2025-01-15T10:31:00Z\"}\n");
    fclose(f);

    // Load history - should skip non-object JSON
    res_t res = ik_history_load(ctx, hist, NULL);
    ck_assert(is_ok(&res));

    // Verify only object entries loaded
    ck_assert_uint_eq(hist->count, 2);
    ck_assert_str_eq(hist->entries[0], "entry1");
    ck_assert_str_eq(hist->entries[1], "entry2");
}

END_TEST
// Test: Load from file with cmd field that is not a string
START_TEST(test_history_load_cmd_not_string)
{
    // Create directory and file with cmd field that is not a string
    ck_assert_int_eq(mkdir(".ikigai", 0755), 0);

    FILE *f = fopen(".ikigai/history", "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\"cmd\": \"entry1\", \"ts\": \"2025-01-15T10:30:00Z\"}\n");
    fprintf(f, "{\"cmd\": 123}\n");  // cmd is a number, not a string
    fprintf(f, "{\"cmd\": [\"array\"]}\n");  // cmd is an array, not a string
    fprintf(f, "{\"cmd\": {\"obj\": \"val\"}}\n");  // cmd is an object, not a string
    fprintf(f, "{\"cmd\": \"entry2\", \"ts\": \"2025-01-15T10:31:00Z\"}\n");
    fclose(f);

    // Load history - should skip entries with non-string cmd
    res_t res = ik_history_load(ctx, hist, NULL);
    ck_assert(is_ok(&res));

    // Verify only valid entries loaded
    ck_assert_uint_eq(hist->count, 2);
    ck_assert_str_eq(hist->entries[0], "entry1");
    ck_assert_str_eq(hist->entries[1], "entry2");
}

END_TEST

static Suite *history_file_io_suite(void)
{
    Suite *s = suite_create("History File I/O");
    TCase *tc = tcase_create("file_io");

    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_history_load_empty_file);
    tcase_add_test(tc, test_history_load_valid_entries);
    tcase_add_test(tc, test_history_load_respects_capacity);
    tcase_add_test(tc, test_history_load_malformed_line);
    tcase_add_test(tc, test_history_save_atomic_write);
    tcase_add_test(tc, test_history_append_entry);
    tcase_add_test(tc, test_history_load_file_missing);
    tcase_add_test(tc, test_history_append_creates_file);
    tcase_add_test(tc, test_history_save_empty);
    tcase_add_test(tc, test_history_load_file_size_zero);
    tcase_add_test(tc, test_history_load_last_line_no_newline);
    tcase_add_test(tc, test_history_load_empty_lines);
    tcase_add_test(tc, test_history_load_non_object_json);
    tcase_add_test(tc, test_history_load_cmd_not_string);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    Suite *s = history_file_io_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
