// Unit tests for history file I/O error handling

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
#include <stdarg.h>

// Forward declaration for suite function
static Suite *history_file_io_errors_suite(void);

// Test fixture
static void *ctx;
static char test_dir_buffer[256];
static ik_history_t *hist;

static bool mock_mkdir_should_fail = false;
static int mock_mkdir_errno = EACCES;
static bool mock_mkdir_race_condition = false;
static bool mock_fopen_should_fail = false;
static const char *mock_fopen_fail_path = NULL;
static bool mock_fseek_should_fail = false;
static int mock_fseek_call_count = 0;
static int mock_fseek_fail_on_call = -1;
static bool mock_ftell_should_fail = false;
static bool mock_fread_should_fail = false;
static bool mock_rename_should_fail = false;

// Forward declarations for wrapper functions
int posix_mkdir_(const char *pathname, mode_t mode);
FILE *fopen_(const char *pathname, const char *mode);
int fseek_(FILE *stream, long offset, int whence);
long ftell_(FILE *stream);
size_t fread_(void *ptr, size_t size, size_t nmemb, FILE *stream);
int posix_rename_(const char *oldpath, const char *newpath);

int posix_mkdir_(const char *pathname, mode_t mode)
{
    if (mock_mkdir_race_condition) {
        errno = EEXIST;
        return -1;
    }
    if (mock_mkdir_should_fail) {
        errno = mock_mkdir_errno;
        return -1;
    }
    (void)pathname;
    (void)mode;
    return 0;
}

FILE *fopen_(const char *pathname, const char *mode)
{
    if (mock_fopen_should_fail && (mock_fopen_fail_path == NULL || strcmp(pathname, mock_fopen_fail_path) == 0)) {
        errno = EACCES;
        return NULL;
    }
    return fopen(pathname, mode);
}

int fseek_(FILE *stream, long offset, int whence)
{
    mock_fseek_call_count++;
    if (mock_fseek_should_fail && (mock_fseek_fail_on_call == -1 || mock_fseek_call_count == mock_fseek_fail_on_call)) {
        errno = EIO;
        return -1;
    }
    return fseek(stream, offset, whence);
}

long ftell_(FILE *stream)
{
    if (mock_ftell_should_fail) {
        errno = EIO;
        return -1;
    }
    return ftell(stream);
}

size_t fread_(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    if (mock_fread_should_fail) {
        return (nmemb > 0) ? nmemb - 1 : 0;
    }
    return fread(ptr, size, nmemb, stream);
}

int posix_rename_(const char *oldpath, const char *newpath)
{
    if (mock_rename_should_fail) {
        errno = EACCES;
        return -1;
    }
    return rename(oldpath, newpath);
}

static void setup(void)
{
    ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create a temporary test directory
    snprintf(test_dir_buffer, sizeof(test_dir_buffer), "/tmp/ikigai-history-err-test-XXXXXX");
    char *result = mkdtemp(test_dir_buffer);
    ck_assert_ptr_nonnull(result);

    // Change to test directory for relative path tests
    ck_assert_int_eq(chdir(test_dir_buffer), 0);

    // Create history with capacity 10
    hist = ik_history_create(ctx, 10);
    ck_assert_ptr_nonnull(hist);

    // Reset mock state
    mock_mkdir_should_fail = false;
    mock_mkdir_errno = EACCES;
    mock_mkdir_race_condition = false;
    mock_fopen_should_fail = false;
    mock_fopen_fail_path = NULL;
    mock_fseek_should_fail = false;
    mock_fseek_call_count = 0;
    mock_fseek_fail_on_call = -1;
    mock_ftell_should_fail = false;
    mock_fread_should_fail = false;
    mock_rename_should_fail = false;
}

static void teardown(void)
{
    // Reset mock state
    mock_mkdir_should_fail = false;
    mock_mkdir_race_condition = false;
    mock_fopen_should_fail = false;
    mock_fopen_fail_path = NULL;
    mock_fseek_should_fail = false;
    mock_fseek_call_count = 0;
    mock_fseek_fail_on_call = -1;
    mock_ftell_should_fail = false;
    mock_fread_should_fail = false;
    mock_rename_should_fail = false;

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

START_TEST(test_history_ensure_directory_race_condition) {
    mock_mkdir_race_condition = true;
    res_t res = ik_history_ensure_directory(ctx);
    ck_assert(is_ok(&res));
}
END_TEST

START_TEST(test_history_ensure_directory_mkdir_failure) {
    mock_mkdir_should_fail = true;
    mock_mkdir_errno = EACCES;
    res_t res = ik_history_ensure_directory(ctx);
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    ck_assert(strstr(error_message(res.err), "Failed to create") != NULL);
}

END_TEST

START_TEST(test_history_load_ensure_directory_failure) {
    mock_mkdir_should_fail = true;
    mock_mkdir_errno = EACCES;
    res_t res = ik_history_load(ctx, hist, NULL);
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
}

END_TEST

START_TEST(test_history_load_fopen_create_failure) {
    mock_fopen_should_fail = true;
    mock_fopen_fail_path = ".ikigai/history";
    res_t res = ik_history_load(ctx, hist, NULL);
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    ck_assert(strstr(error_message(res.err), "Failed to create") != NULL);
}

END_TEST

START_TEST(test_history_load_fopen_read_failure) {
    ck_assert_int_eq(mkdir(".ikigai", 0755), 0);
    FILE *f = fopen(".ikigai/history", "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\"cmd\": \"test\", \"ts\": \"2025-01-15T10:30:00Z\"}\n");
    fclose(f);
    mock_fopen_should_fail = true;
    mock_fopen_fail_path = ".ikigai/history";
    res_t res = ik_history_load(ctx, hist, NULL);
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    ck_assert(strstr(error_message(res.err), "Failed to open") != NULL);
}

END_TEST

START_TEST(test_history_load_fseek_to_end_failure) {
    ck_assert_int_eq(mkdir(".ikigai", 0755), 0);
    FILE *f = fopen(".ikigai/history", "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\"cmd\": \"test\", \"ts\": \"2025-01-15T10:30:00Z\"}\n");
    fclose(f);
    mock_fseek_should_fail = true;
    mock_fseek_fail_on_call = 1;
    res_t res = ik_history_load(ctx, hist, NULL);
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    ck_assert(strstr(error_message(res.err), "Failed to seek") != NULL);
}

END_TEST

START_TEST(test_history_load_ftell_failure) {
    ck_assert_int_eq(mkdir(".ikigai", 0755), 0);
    FILE *f = fopen(".ikigai/history", "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\"cmd\": \"test\", \"ts\": \"2025-01-15T10:30:00Z\"}\n");
    fclose(f);
    mock_ftell_should_fail = true;
    res_t res = ik_history_load(ctx, hist, NULL);
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    ck_assert(strstr(error_message(res.err), "Failed to get size") != NULL);
}

END_TEST

START_TEST(test_history_load_fseek_to_beginning_failure) {
    ck_assert_int_eq(mkdir(".ikigai", 0755), 0);
    FILE *f = fopen(".ikigai/history", "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\"cmd\": \"test\", \"ts\": \"2025-01-15T10:30:00Z\"}\n");
    fclose(f);
    mock_fseek_should_fail = true;
    mock_fseek_fail_on_call = 2;
    res_t res = ik_history_load(ctx, hist, NULL);
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    ck_assert(strstr(error_message(res.err), "Failed to seek") != NULL);
}

END_TEST

START_TEST(test_history_load_fread_incomplete) {
    ck_assert_int_eq(mkdir(".ikigai", 0755), 0);
    FILE *f = fopen(".ikigai/history", "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "{\"cmd\": \"test\", \"ts\": \"2025-01-15T10:30:00Z\"}\n");
    fclose(f);
    mock_fread_should_fail = true;
    res_t res = ik_history_load(ctx, hist, NULL);
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    ck_assert(strstr(error_message(res.err), "Failed to read") != NULL);
    ck_assert(strstr(error_message(res.err), "incomplete") != NULL);
}

END_TEST

START_TEST(test_history_load_exceeds_max_entries) {
    ck_assert_int_eq(mkdir(".ikigai", 0755), 0);
    FILE *f = fopen(".ikigai/history", "w");
    ck_assert_ptr_nonnull(f);
    for (int i = 0; i < 21; i++) {
        fprintf(f, "{\"cmd\": \"command%d\", \"ts\": \"2025-01-15T10:30:00Z\"}\n", i);
    }
    fclose(f);
    res_t res = ik_history_load(ctx, hist, NULL);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(hist->count, 10);
}

END_TEST

START_TEST(test_history_save_ensure_directory_failure) {
    ik_history_add(hist, "command1");
    ik_history_add(hist, "command2");
    mock_mkdir_should_fail = true;
    mock_mkdir_errno = EACCES;
    res_t res = ik_history_save(hist);
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    talloc_free(res.err);
}

END_TEST

START_TEST(test_history_save_fopen_failure) {
    ck_assert_int_eq(mkdir(".ikigai", 0755), 0);
    ik_history_add(hist, "command1");
    ik_history_add(hist, "command2");
    mock_fopen_should_fail = true;
    mock_fopen_fail_path = ".ikigai/history.tmp";
    res_t res = ik_history_save(hist);
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    ck_assert(strstr(error_message(res.err), "Failed to create") != NULL);
    ck_assert(strstr(error_message(res.err), "history.tmp") != NULL);
    talloc_free(res.err);
}

END_TEST

START_TEST(test_history_save_rename_failure) {
    ck_assert_int_eq(mkdir(".ikigai", 0755), 0);
    ik_history_add(hist, "command1");
    ik_history_add(hist, "command2");
    mock_rename_should_fail = true;
    res_t res = ik_history_save(hist);
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    ck_assert(strstr(error_message(res.err), "Failed to rename") != NULL);
    talloc_free(res.err);
}

END_TEST

START_TEST(test_history_append_ensure_directory_failure) {
    mock_mkdir_should_fail = true;
    mock_mkdir_errno = EACCES;
    res_t res = ik_history_append_entry("test command");
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    talloc_free(res.err);
}

END_TEST

START_TEST(test_history_append_fopen_failure) {
    ck_assert_int_eq(mkdir(".ikigai", 0755), 0);
    mock_fopen_should_fail = true;
    mock_fopen_fail_path = ".ikigai/history";
    res_t res = ik_history_append_entry("test command");
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    ck_assert(strstr(error_message(res.err), "Failed to open") != NULL);
    talloc_free(res.err);
}

END_TEST

static Suite *history_file_io_errors_suite(void)
{
    Suite *s = suite_create("History File I/O Errors");
    TCase *tc = tcase_create("file_io_errors");
    tcase_set_timeout(tc, 30);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_history_ensure_directory_race_condition);
    tcase_add_test(tc, test_history_ensure_directory_mkdir_failure);
    tcase_add_test(tc, test_history_load_ensure_directory_failure);
    tcase_add_test(tc, test_history_load_fopen_create_failure);
    tcase_add_test(tc, test_history_load_fopen_read_failure);
    tcase_add_test(tc, test_history_load_fseek_to_end_failure);
    tcase_add_test(tc, test_history_load_ftell_failure);
    tcase_add_test(tc, test_history_load_fseek_to_beginning_failure);
    tcase_add_test(tc, test_history_load_fread_incomplete);
    tcase_add_test(tc, test_history_load_exceeds_max_entries);
    tcase_add_test(tc, test_history_save_ensure_directory_failure);
    tcase_add_test(tc, test_history_save_fopen_failure);
    tcase_add_test(tc, test_history_save_rename_failure);
    tcase_add_test(tc, test_history_append_ensure_directory_failure);
    tcase_add_test(tc, test_history_append_fopen_failure);
    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    Suite *s = history_file_io_errors_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
