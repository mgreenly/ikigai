// Unit tests for IKIGAI_LOG_DIR environment variable support

#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "../../../src/logger.h"

// Helper: setup temp directory and init logger with env var
static char test_dir[256];
static char env_log_dir[256];
static char log_file_path[512];

static void setup_with_env(void)
{
    snprintf(test_dir, sizeof(test_dir), "/tmp/ikigai_env_test_%d", getpid());
    snprintf(env_log_dir, sizeof(env_log_dir), "/tmp/ikigai_env_logs_%d", getpid());

    // Create directories
    mkdir(test_dir, 0755);
    mkdir(env_log_dir, 0755);

    // Set environment variable
    setenv("IKIGAI_LOG_DIR", env_log_dir, 1);

    // Initialize logger with working directory
    ik_log_init(test_dir);

    // Log file should be in env_log_dir, not test_dir
    snprintf(log_file_path, sizeof(log_file_path), "%s/current.log", env_log_dir);
}

static void teardown_with_env(void)
{
    ik_log_shutdown();

    // Unset environment variable
    unsetenv("IKIGAI_LOG_DIR");

    // Clean up log file and directories
    unlink(log_file_path);
    rmdir(env_log_dir);

    // Clean up test_dir (should not have .ikigai dir when env var is set)
    rmdir(test_dir);
}

static void setup_without_env(void)
{
    snprintf(test_dir, sizeof(test_dir), "/tmp/ikigai_env_test_%d", getpid());
    mkdir(test_dir, 0755);

    // Make sure env var is not set
    unsetenv("IKIGAI_LOG_DIR");

    // Initialize logger
    ik_log_init(test_dir);

    // Log file should be in default location
    snprintf(log_file_path, sizeof(log_file_path), "%s/.ikigai/logs/current.log", test_dir);
}

static void teardown_without_env(void)
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

// Test: when IKIGAI_LOG_DIR is set, logger uses that directory
START_TEST(test_env_log_dir_overrides_default) {
    setup_with_env();

    // Write a log entry
    yyjson_mut_doc *doc = ik_log_create();
    yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);
    yyjson_mut_obj_add_str(doc, root, "event", "test_env");
    ik_log_debug_json(doc);

    // Log file should exist in env_log_dir
    char *output = read_log_file();
    ck_assert_ptr_nonnull(output);
    ck_assert(strlen(output) > 0);

    // Parse and verify content
    yyjson_doc *parsed = yyjson_read(output, strlen(output), 0);
    ck_assert_ptr_nonnull(parsed);
    yyjson_val *parsed_root = yyjson_doc_get_root(parsed);
    yyjson_val *logline = yyjson_obj_get(parsed_root, "logline");
    yyjson_val *event = yyjson_obj_get(logline, "event");
    ck_assert_str_eq(yyjson_get_str(event), "test_env");
    yyjson_doc_free(parsed);

    teardown_with_env();
}
END_TEST
// Test: when IKIGAI_LOG_DIR is not set, logger uses default location
START_TEST(test_no_env_uses_default_location) {
    setup_without_env();

    // Write a log entry
    yyjson_mut_doc *doc = ik_log_create();
    yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);
    yyjson_mut_obj_add_str(doc, root, "event", "test_default");
    ik_log_debug_json(doc);

    // Log file should exist in default location
    char *output = read_log_file();
    ck_assert_ptr_nonnull(output);
    ck_assert(strlen(output) > 0);

    teardown_without_env();
}

END_TEST
// Test: when IKIGAI_LOG_DIR is empty string, logger uses default location
START_TEST(test_empty_env_uses_default_location) {
    snprintf(test_dir, sizeof(test_dir), "/tmp/ikigai_env_test_%d", getpid());
    mkdir(test_dir, 0755);

    // Set env var to empty string
    setenv("IKIGAI_LOG_DIR", "", 1);

    // Initialize logger
    ik_log_init(test_dir);

    // Log file should be in default location
    snprintf(log_file_path, sizeof(log_file_path), "%s/.ikigai/logs/current.log", test_dir);

    // Write a log entry
    yyjson_mut_doc *doc = ik_log_create();
    yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);
    yyjson_mut_obj_add_str(doc, root, "event", "test_empty");
    ik_log_debug_json(doc);

    // Log file should exist in default location
    char *output = read_log_file();
    ck_assert_ptr_nonnull(output);
    ck_assert(strlen(output) > 0);

    // Cleanup
    ik_log_shutdown();
    unsetenv("IKIGAI_LOG_DIR");
    unlink(log_file_path);

    char logs_dir[512];
    snprintf(logs_dir, sizeof(logs_dir), "%s/.ikigai/logs", test_dir);
    rmdir(logs_dir);

    char ikigai_dir[512];
    snprintf(ikigai_dir, sizeof(ikigai_dir), "%s/.ikigai", test_dir);
    rmdir(ikigai_dir);

    rmdir(test_dir);
}

END_TEST

static Suite *env_log_dir_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Logger Environment Variable");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_env_log_dir_overrides_default);
    tcase_add_test(tc_core, test_no_env_uses_default_location);
    tcase_add_test(tc_core, test_empty_env_uses_default_location);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = env_log_dir_suite();
    sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/logger/env_log_dir_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
