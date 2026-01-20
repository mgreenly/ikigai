// Unit tests for logger rotation failure path

#include <check.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <talloc.h>
#include "../../../src/logger.h"
#include "../../../src/wrapper.h"
#include "../../test_utils_helper.h"

// Helper: setup temp directory
static char test_dir[256];
static char log_file_path[512];
static TALLOC_CTX *test_ctx = NULL;

static void setup_test(void)
{
    snprintf(test_dir, sizeof(test_dir), "/tmp/ikigai_logger_rotation_test_%d", getpid());
    mkdir(test_dir, 0755);

    // When IKIGAI_LOG_DIR is set by suite_setup, use that path
    const char *log_dir = getenv("IKIGAI_LOG_DIR");
    if (log_dir != NULL) {
        snprintf(log_file_path, sizeof(log_file_path), "%s/current.log", log_dir);
    } else {
        snprintf(log_file_path, sizeof(log_file_path), "%s/.ikigai/logs/current.log", test_dir);
    }
    test_ctx = talloc_new(NULL);
}

static void teardown_test(void)
{
    if (test_ctx != NULL) {
        talloc_free(test_ctx);
        test_ctx = NULL;
    }
    unlink(log_file_path);

    // Only clean up test_dir if IKIGAI_LOG_DIR is not set
    const char *log_dir = getenv("IKIGAI_LOG_DIR");
    if (log_dir == NULL) {
        char logs_dir[512];
        snprintf(logs_dir, sizeof(logs_dir), "%s/.ikigai/logs", test_dir);
        rmdir(logs_dir);
        char ikigai_dir[512];
        snprintf(ikigai_dir, sizeof(ikigai_dir), "%s/.ikigai", test_dir);
        rmdir(ikigai_dir);
    }
    rmdir(test_dir);
}

static char *read_log_file(void)
{
    FILE *f = fopen(log_file_path, "r");
    if (!f) return NULL;

    static char buffer[4096];
    size_t len = fread(buffer, 1, sizeof(buffer) - 1, f);
    buffer[len] = '\0';
    fclose(f);
    return buffer;
}

// Mock posix_rename_ to always fail
int posix_rename_(const char *old, const char *new)
{
    (void)old;
    (void)new;
    errno = EACCES;
    return -1;
}

// Test: log file rotation failure is handled gracefully
START_TEST(test_logger_rotation_failure_ignored) {
    setup_test();

    // Create first logger and write to it
    ik_logger_t *logger1 = ik_logger_create(test_ctx, test_dir);
    ck_assert_ptr_nonnull(logger1);

    yyjson_mut_doc *doc1 = ik_log_create();
    yyjson_mut_val *root1 = yyjson_mut_doc_get_root(doc1);
    yyjson_mut_obj_add_str(doc1, root1, "event", "before_failed_rotation");
    ik_logger_info_json(logger1, doc1);

    // Close first logger
    talloc_free(logger1);

    // Create second logger - rotation will fail due to mock, but should continue
    TALLOC_CTX *ctx2 = talloc_new(NULL);
    ik_logger_t *logger2 = ik_logger_create(ctx2, test_dir);
    ck_assert_ptr_nonnull(logger2);

    yyjson_mut_doc *doc2 = ik_log_create();
    yyjson_mut_val *root2 = yyjson_mut_doc_get_root(doc2);
    yyjson_mut_obj_add_str(doc2, root2, "event", "after_failed_rotation");
    ik_logger_info_json(logger2, doc2);

    // Log file should exist and contain new log (old one was truncated by "w" mode)
    char *output = read_log_file();
    ck_assert_ptr_nonnull(output);
    ck_assert(strstr(output, "after_failed_rotation") != NULL);

    talloc_free(ctx2);
    teardown_test();
}
END_TEST

// Suite-level setup: Set log directory
static void suite_setup(void)
{
    ik_test_set_log_dir(__FILE__);
}

static Suite *logger_rotation_failure_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Logger Rotation Failure");
    tc_core = tcase_create("Core");
    tcase_add_unchecked_fixture(tc_core, suite_setup, NULL);

    tcase_add_test(tc_core, test_logger_rotation_failure_ignored);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = logger_rotation_failure_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
