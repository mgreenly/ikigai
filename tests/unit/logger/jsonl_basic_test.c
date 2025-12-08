// Unit tests for JSONL logger module

#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "../../../src/logger.h"

// Helper: setup temp directory and init logger
static char test_dir[256];
static char log_file_path[512];

static void setup_logger(void)
{
    snprintf(test_dir, sizeof(test_dir), "/tmp/ikigai_jsonl_test_%d", getpid());
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

// Test: ik_log_create returns non-NULL doc with empty root object
START_TEST(test_log_create_returns_doc)
{
    yyjson_mut_doc *doc = ik_log_create();
    ck_assert_ptr_nonnull(doc);

    yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);
    ck_assert_ptr_nonnull(root);
    ck_assert(yyjson_mut_is_obj(root));
    ck_assert_uint_eq(yyjson_mut_obj_size(root), 0);

    yyjson_mut_doc_free(doc);
}
END_TEST

// Test: ik_log_debug_json writes JSONL to file
START_TEST(test_log_debug_writes_jsonl)
{
    setup_logger();

    yyjson_mut_doc *doc = ik_log_create();
    yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);
    yyjson_mut_obj_add_str(doc, root, "event", "test");
    yyjson_mut_obj_add_int(doc, root, "value", 42);

    ik_log_debug_json(doc);

    char *output = read_log_file();
    ck_assert_ptr_nonnull(output);

    // Should have written something to file
    ck_assert(strlen(output) > 0);

    // Should end with newline
    ck_assert(output[strlen(output) - 1] == '\n');

    teardown_logger();
}
END_TEST

// Test: output has "level":"debug" field
START_TEST(test_log_debug_has_level_field)
{
    setup_logger();

    yyjson_mut_doc *doc = ik_log_create();
    yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);
    yyjson_mut_obj_add_str(doc, root, "event", "test");

    ik_log_debug_json(doc);

    char *output = read_log_file();
    ck_assert_ptr_nonnull(output);

    // Parse the output as JSON
    yyjson_doc *parsed = yyjson_read(output, strlen(output), 0);
    ck_assert_ptr_nonnull(parsed);

    yyjson_val *parsed_root = yyjson_doc_get_root(parsed);
    yyjson_val *level = yyjson_obj_get(parsed_root, "level");
    ck_assert_ptr_nonnull(level);
    ck_assert(yyjson_is_str(level));
    ck_assert_str_eq(yyjson_get_str(level), "debug");

    yyjson_doc_free(parsed);
    teardown_logger();
}
END_TEST

// Test: output has "timestamp" field
START_TEST(test_log_debug_has_timestamp_field)
{
    setup_logger();

    yyjson_mut_doc *doc = ik_log_create();
    yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);
    yyjson_mut_obj_add_str(doc, root, "event", "test");

    ik_log_debug_json(doc);

    char *output = read_log_file();
    ck_assert_ptr_nonnull(output);

    // Parse the output as JSON
    yyjson_doc *parsed = yyjson_read(output, strlen(output), 0);
    ck_assert_ptr_nonnull(parsed);

    yyjson_val *parsed_root = yyjson_doc_get_root(parsed);
    yyjson_val *timestamp = yyjson_obj_get(parsed_root, "timestamp");
    ck_assert_ptr_nonnull(timestamp);
    ck_assert(yyjson_is_str(timestamp));

    yyjson_doc_free(parsed);
    teardown_logger();
}
END_TEST

// Test: output has "logline" field containing original doc
START_TEST(test_log_debug_has_logline_field)
{
    setup_logger();

    yyjson_mut_doc *doc = ik_log_create();
    yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);
    yyjson_mut_obj_add_str(doc, root, "event", "test");
    yyjson_mut_obj_add_int(doc, root, "value", 42);

    ik_log_debug_json(doc);

    char *output = read_log_file();
    ck_assert_ptr_nonnull(output);

    // Parse the output as JSON
    yyjson_doc *parsed = yyjson_read(output, strlen(output), 0);
    ck_assert_ptr_nonnull(parsed);

    yyjson_val *parsed_root = yyjson_doc_get_root(parsed);
    yyjson_val *logline = yyjson_obj_get(parsed_root, "logline");
    ck_assert_ptr_nonnull(logline);
    ck_assert(yyjson_is_obj(logline));

    // Check that logline contains the original fields
    yyjson_val *event = yyjson_obj_get(logline, "event");
    ck_assert_ptr_nonnull(event);
    ck_assert_str_eq(yyjson_get_str(event), "test");

    yyjson_val *value = yyjson_obj_get(logline, "value");
    ck_assert_ptr_nonnull(value);
    ck_assert_int_eq(yyjson_get_int(value), 42);

    yyjson_doc_free(parsed);
    teardown_logger();
}
END_TEST

// Test: output is valid single-line JSON
START_TEST(test_log_debug_is_single_line_json)
{
    setup_logger();

    yyjson_mut_doc *doc = ik_log_create();
    yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);
    yyjson_mut_obj_add_str(doc, root, "event", "test");

    ik_log_debug_json(doc);

    char *output = read_log_file();
    ck_assert_ptr_nonnull(output);

    // Should not contain any newlines except the final one
    size_t len = strlen(output);
    ck_assert(len > 0);

    // Count newlines
    int newline_count = 0;
    for (size_t i = 0; i < len; i++) {
        if (output[i] == '\n') {
            newline_count++;
        }
    }

    // Should have exactly one newline at the end
    ck_assert_int_eq(newline_count, 1);
    ck_assert(output[len - 1] == '\n');

    // Should be valid JSON
    yyjson_doc *parsed = yyjson_read(output, len, 0);
    ck_assert_ptr_nonnull(parsed);
    yyjson_doc_free(parsed);

    teardown_logger();
}
END_TEST

static Suite *logger_jsonl_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Logger JSONL");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_log_create_returns_doc);
    tcase_add_test(tc_core, test_log_debug_writes_jsonl);
    tcase_add_test(tc_core, test_log_debug_has_level_field);
    tcase_add_test(tc_core, test_log_debug_has_timestamp_field);
    tcase_add_test(tc_core, test_log_debug_has_logline_field);
    tcase_add_test(tc_core, test_log_debug_is_single_line_json);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = logger_jsonl_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
