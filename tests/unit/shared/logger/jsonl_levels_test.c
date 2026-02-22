// Unit tests for JSONL logger level functions

#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "shared/logger.h"

// Helper: setup temp directory and init logger
static char test_dir[256];
static char log_file_path[512];

static void setup_logger(void)
{
    snprintf(test_dir, sizeof(test_dir), "/tmp/ikigai_jsonl_levels_test_%d", getpid());
    mkdir(test_dir, 0755);
    ik_log_init(test_dir);
    snprintf(log_file_path, sizeof(log_file_path), "%s/.ikigai/logs/current.log", test_dir);
}

static void teardown_logger(void)
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

// Test: ik_log_info_json writes "info" level
START_TEST(test_log_info_has_info_level) {
    setup_logger();

    yyjson_mut_doc *doc = ik_log_create();
    yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);
    yyjson_mut_obj_add_str(doc, root, "event", "test");

    ik_log_info_json(doc);

    char *output = read_log_file();
    ck_assert_ptr_nonnull(output);

    // Parse the output as JSON
    yyjson_doc *parsed = yyjson_read(output, strlen(output), 0);
    ck_assert_ptr_nonnull(parsed);

    yyjson_val *parsed_root = yyjson_doc_get_root(parsed);
    yyjson_val *level = yyjson_obj_get(parsed_root, "level");
    ck_assert_ptr_nonnull(level);
    ck_assert(yyjson_is_str(level));
    ck_assert_str_eq(yyjson_get_str(level), "info");

    yyjson_doc_free(parsed);
    teardown_logger();
}
END_TEST
// Test: ik_log_warn_json writes "warn" level
START_TEST(test_log_warn_has_warn_level) {
    setup_logger();

    yyjson_mut_doc *doc = ik_log_create();
    yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);
    yyjson_mut_obj_add_str(doc, root, "event", "test");

    ik_log_warn_json(doc);

    char *output = read_log_file();
    ck_assert_ptr_nonnull(output);

    // Parse the output as JSON
    yyjson_doc *parsed = yyjson_read(output, strlen(output), 0);
    ck_assert_ptr_nonnull(parsed);

    yyjson_val *parsed_root = yyjson_doc_get_root(parsed);
    yyjson_val *level = yyjson_obj_get(parsed_root, "level");
    ck_assert_ptr_nonnull(level);
    ck_assert(yyjson_is_str(level));
    ck_assert_str_eq(yyjson_get_str(level), "warn");

    yyjson_doc_free(parsed);
    teardown_logger();
}

END_TEST

static Suite *logger_jsonl_levels_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Logger JSONL Levels");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_log_info_has_info_level);
    tcase_add_test(tc_core, test_log_warn_has_warn_level);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = logger_jsonl_levels_suite();
    sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/shared/logger/jsonl_levels_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
