// Unit tests for DI-based logger API (ik_logger_t context)

#include <check.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <talloc.h>
#include "../../../src/logger.h"

// Helper: setup temp directory
static char test_dir[256];
static char log_file_path[512];
static TALLOC_CTX *test_ctx = NULL;

static void setup_test(void)
{
    snprintf(test_dir, sizeof(test_dir), "/tmp/ikigai_logger_di_test_%d", getpid());
    mkdir(test_dir, 0755);
    snprintf(log_file_path, sizeof(log_file_path), "%s/.ikigai/logs/current.log", test_dir);
    test_ctx = talloc_new(NULL);
}

static void teardown_test(void)
{
    if (test_ctx != NULL) {
        talloc_free(test_ctx);
        test_ctx = NULL;
    }
    unlink(log_file_path);
    char logs_dir[512];
    snprintf(logs_dir, sizeof(logs_dir), "%s/.ikigai/logs", test_dir);
    rmdir(logs_dir);
    char ikigai_dir[512];
    snprintf(ikigai_dir, sizeof(ikigai_dir), "%s/.ikigai", test_dir);
    rmdir(ikigai_dir);
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

// Test: ik_logger_create returns non-NULL logger
START_TEST(test_logger_create_returns_logger)
{
    setup_test();

    ik_logger_t *logger = ik_logger_create(test_ctx, test_dir);
    ck_assert_ptr_nonnull(logger);

    teardown_test();
}
END_TEST

// Test: ik_logger_debug_json writes to log file
START_TEST(test_logger_debug_writes_jsonl)
{
    setup_test();

    ik_logger_t *logger = ik_logger_create(test_ctx, test_dir);
    ck_assert_ptr_nonnull(logger);

    yyjson_mut_doc *doc = ik_log_create();
    yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);
    yyjson_mut_obj_add_str(doc, root, "event", "test_di");
    yyjson_mut_obj_add_int(doc, root, "value", 123);

    ik_logger_debug_json(logger, doc);

    char *output = read_log_file();
    ck_assert_ptr_nonnull(output);
    ck_assert(strlen(output) > 0);

    teardown_test();
}
END_TEST

// Test: logger output has correct level field
START_TEST(test_logger_has_level_field)
{
    setup_test();

    ik_logger_t *logger = ik_logger_create(test_ctx, test_dir);

    yyjson_mut_doc *doc = ik_log_create();
    yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);
    yyjson_mut_obj_add_str(doc, root, "event", "test");

    ik_logger_warn_json(logger, doc);

    char *output = read_log_file();
    ck_assert_ptr_nonnull(output);

    // Parse and check level
    yyjson_doc *parsed = yyjson_read(output, strlen(output), 0);
    ck_assert_ptr_nonnull(parsed);

    yyjson_val *parsed_root = yyjson_doc_get_root(parsed);
    yyjson_val *level = yyjson_obj_get(parsed_root, "level");
    ck_assert_ptr_nonnull(level);
    ck_assert_str_eq(yyjson_get_str(level), "warn");

    yyjson_doc_free(parsed);
    teardown_test();
}
END_TEST

// Test: logger output has timestamp field
START_TEST(test_logger_has_timestamp_field)
{
    setup_test();

    ik_logger_t *logger = ik_logger_create(test_ctx, test_dir);

    yyjson_mut_doc *doc = ik_log_create();
    yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);
    yyjson_mut_obj_add_str(doc, root, "event", "test");

    ik_logger_info_json(logger, doc);

    char *output = read_log_file();
    ck_assert_ptr_nonnull(output);

    yyjson_doc *parsed = yyjson_read(output, strlen(output), 0);
    ck_assert_ptr_nonnull(parsed);

    yyjson_val *parsed_root = yyjson_doc_get_root(parsed);
    yyjson_val *timestamp = yyjson_obj_get(parsed_root, "timestamp");
    ck_assert_ptr_nonnull(timestamp);
    ck_assert(yyjson_is_str(timestamp));

    yyjson_doc_free(parsed);
    teardown_test();
}
END_TEST

// Test: logger output has logline field with original content
START_TEST(test_logger_has_logline_field)
{
    setup_test();

    ik_logger_t *logger = ik_logger_create(test_ctx, test_dir);

    yyjson_mut_doc *doc = ik_log_create();
    yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);
    yyjson_mut_obj_add_str(doc, root, "event", "di_test");
    yyjson_mut_obj_add_int(doc, root, "code", 42);

    ik_logger_error_json(logger, doc);

    char *output = read_log_file();
    ck_assert_ptr_nonnull(output);

    yyjson_doc *parsed = yyjson_read(output, strlen(output), 0);
    ck_assert_ptr_nonnull(parsed);

    yyjson_val *parsed_root = yyjson_doc_get_root(parsed);
    yyjson_val *logline = yyjson_obj_get(parsed_root, "logline");
    ck_assert_ptr_nonnull(logline);
    ck_assert(yyjson_is_obj(logline));

    yyjson_val *event = yyjson_obj_get(logline, "event");
    ck_assert_str_eq(yyjson_get_str(event), "di_test");

    yyjson_val *code = yyjson_obj_get(logline, "code");
    ck_assert_int_eq(yyjson_get_int(code), 42);

    yyjson_doc_free(parsed);
    teardown_test();
}
END_TEST

// Test: talloc cleanup properly closes logger
START_TEST(test_logger_cleanup_on_talloc_free)
{
    setup_test();

    // Create a separate context for the logger
    TALLOC_CTX *logger_ctx = talloc_new(NULL);
    ik_logger_t *logger = ik_logger_create(logger_ctx, test_dir);
    ck_assert_ptr_nonnull(logger);

    // Write a log entry
    yyjson_mut_doc *doc = ik_log_create();
    yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);
    yyjson_mut_obj_add_str(doc, root, "event", "before_free");
    ik_logger_debug_json(logger, doc);

    // Free should trigger destructor and close file
    talloc_free(logger_ctx);

    // Verify file was written
    char *output = read_log_file();
    ck_assert_ptr_nonnull(output);
    ck_assert(strstr(output, "before_free") != NULL);

    teardown_test();
}
END_TEST

// Test: ik_logger_reinit changes log file location
START_TEST(test_logger_reinit_changes_location)
{
    setup_test();

    ik_logger_t *logger = ik_logger_create(test_ctx, test_dir);
    ck_assert_ptr_nonnull(logger);

    // Write initial log
    yyjson_mut_doc *doc1 = ik_log_create();
    yyjson_mut_val *root1 = yyjson_mut_doc_get_root(doc1);
    yyjson_mut_obj_add_str(doc1, root1, "event", "before_reinit");
    ik_logger_info_json(logger, doc1);

    // Verify initial log was written
    char *output1 = read_log_file();
    ck_assert_ptr_nonnull(output1);
    ck_assert(strstr(output1, "before_reinit") != NULL);

    // Create new directory for reinit
    char new_dir[256];
    snprintf(new_dir, sizeof(new_dir), "/tmp/ikigai_logger_di_test_new_%d", getpid());
    mkdir(new_dir, 0755);

    char new_log_file[512];
    snprintf(new_log_file, sizeof(new_log_file), "%s/.ikigai/logs/current.log", new_dir);

    // Reinit to new location
    ik_logger_reinit(logger, new_dir);

    // Write log to new location
    yyjson_mut_doc *doc2 = ik_log_create();
    yyjson_mut_val *root2 = yyjson_mut_doc_get_root(doc2);
    yyjson_mut_obj_add_str(doc2, root2, "event", "after_reinit");
    ik_logger_info_json(logger, doc2);

    // Verify new log file exists and contains new log
    FILE *f = fopen(new_log_file, "r");
    ck_assert_ptr_nonnull(f);

    char buffer[4096];
    size_t len = fread(buffer, 1, sizeof(buffer) - 1, f);
    buffer[len] = '\0';
    fclose(f);

    ck_assert(strstr(buffer, "after_reinit") != NULL);
    ck_assert(strstr(buffer, "before_reinit") == NULL); // Old log not in new file

    // Cleanup new directory
    unlink(new_log_file);
    char new_logs_dir[512];
    snprintf(new_logs_dir, sizeof(new_logs_dir), "%s/.ikigai/logs", new_dir);
    rmdir(new_logs_dir);
    char new_ikigai_dir[512];
    snprintf(new_ikigai_dir, sizeof(new_ikigai_dir), "%s/.ikigai", new_dir);
    rmdir(new_ikigai_dir);
    rmdir(new_dir);

    teardown_test();
}
END_TEST

// Test: ik_logger_fatal_json exits process
START_TEST(test_logger_fatal_exits)
{
    setup_test();

    ik_logger_t *logger = ik_logger_create(test_ctx, test_dir);
    ck_assert_ptr_nonnull(logger);

    yyjson_mut_doc *doc = ik_log_create();
    yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);
    yyjson_mut_obj_add_str(doc, root, "event", "fatal_error");

    // This should exit(1) - test framework will catch it
    ik_logger_fatal_json(logger, doc);

    // Should not reach here
    ck_abort_msg("Should have exited");

    teardown_test();
}
END_TEST


static Suite *logger_di_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Logger DI");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_logger_create_returns_logger);
    tcase_add_test(tc_core, test_logger_debug_writes_jsonl);
    tcase_add_test(tc_core, test_logger_has_level_field);
    tcase_add_test(tc_core, test_logger_has_timestamp_field);
    tcase_add_test(tc_core, test_logger_has_logline_field);
    tcase_add_test(tc_core, test_logger_cleanup_on_talloc_free);
    tcase_add_test(tc_core, test_logger_reinit_changes_location);
    tcase_add_exit_test(tc_core, test_logger_fatal_exits, 1);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = logger_di_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
