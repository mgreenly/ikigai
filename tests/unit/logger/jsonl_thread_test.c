// Unit tests for JSONL logger thread-safety

#include <check.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "../../../src/logger.h"

// Thread worker function arguments
typedef struct {
    int thread_id;
    int entries_per_thread;
} thread_worker_args_t;

// Thread function: each thread logs entries (forward declaration)
static void *thread_worker_func(void *arg);

// Implementation
static void *thread_worker_func(void *arg)
{
    thread_worker_args_t *args = (thread_worker_args_t *)arg;
    int thread_id = args->thread_id;
    int entries_per_thread = args->entries_per_thread;

    for (int i = 0; i < entries_per_thread; i++) {
        yyjson_mut_doc *doc = ik_log_create();
        yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);
        yyjson_mut_obj_add_int(doc, root, "thread", thread_id);
        yyjson_mut_obj_add_int(doc, root, "entry", i);
        yyjson_mut_obj_add_str(doc, root, "message", "test");
        ik_log_debug_json(doc);
    }

    free(args);
    return NULL;
}

// Test: concurrent logging from multiple threads doesn't corrupt output
START_TEST(test_concurrent_logging_no_corruption) {
    char test_dir[256];
    snprintf(test_dir, sizeof(test_dir), "/tmp/ikigai_thread_test_%d", getpid());

    // Create test directory
    mkdir(test_dir, 0755);

    // Initialize logger
    ik_log_init(test_dir);

    // Number of threads and log entries per thread
    int num_threads = 10;
    int entries_per_thread = 100;

    // Create threads array (allocate dynamically to avoid VLA)
    pthread_t *threads = malloc(sizeof(pthread_t) * (size_t)num_threads);
    ck_assert_ptr_nonnull(threads);

    for (int i = 0; i < num_threads; i++) {
        thread_worker_args_t *args = malloc(sizeof(thread_worker_args_t));
        ck_assert_ptr_nonnull(args);
        args->thread_id = i;
        args->entries_per_thread = entries_per_thread;
        ck_assert_int_eq(pthread_create(&threads[i], NULL, thread_worker_func, args), 0);
    }

    // Wait for all threads to complete
    for (int i = 0; i < num_threads; i++) {
        ck_assert_int_eq(pthread_join(threads[i], NULL), 0);
    }

    free(threads);

    // Read log file and verify
    char log_file[512];
    snprintf(log_file, sizeof(log_file), "%s/.ikigai/logs/current.log", test_dir);
    FILE *f = fopen(log_file, "r");
    ck_assert_ptr_nonnull(f);

    // Count lines and verify each is valid JSON
    int line_count = 0;
    char line[8192];
    while (fgets(line, sizeof(line), f) != NULL) {
        line_count++;

        // Remove trailing newline
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }

        // Each line should be valid JSON
        yyjson_doc *parsed = yyjson_read(line, strlen(line), 0);
        ck_assert_ptr_nonnull(parsed);

        // Verify structure
        yyjson_val *root = yyjson_doc_get_root(parsed);
        ck_assert(yyjson_is_obj(root));

        // Check for level field
        yyjson_val *level = yyjson_obj_get(root, "level");
        ck_assert_ptr_nonnull(level);
        ck_assert_str_eq(yyjson_get_str(level), "debug");

        // Check for timestamp field
        yyjson_val *timestamp = yyjson_obj_get(root, "timestamp");
        ck_assert_ptr_nonnull(timestamp);

        // Check for logline field
        yyjson_val *logline = yyjson_obj_get(root, "logline");
        ck_assert_ptr_nonnull(logline);
        ck_assert(yyjson_is_obj(logline));

        yyjson_doc_free(parsed);
    }

    fclose(f);

    // Verify we have exactly 1000 lines (10 threads * 100 entries each)
    int expected_lines = num_threads * entries_per_thread;
    ck_assert_int_eq(line_count, expected_lines);

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

static Suite *logger_thread_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Logger Thread Safety");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_concurrent_logging_no_corruption);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = logger_thread_suite();
    sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/logger/jsonl_thread_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
