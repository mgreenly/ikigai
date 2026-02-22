// Unit tests for JSONL logger thread-safety

#include <check.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "shared/logger.h"

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

// Test: concurrent logging from multiple threads does not crash (logger is a no-op)
START_TEST(test_concurrent_logging_no_corruption) {
    char test_dir[256];
    snprintf(test_dir, sizeof(test_dir), "/tmp/ikigai_thread_test_%d", getpid());
    mkdir(test_dir, 0755);

    ik_log_init(test_dir);

    int num_threads = 10;
    int entries_per_thread = 100;

    pthread_t *threads = malloc(sizeof(pthread_t) * (size_t)num_threads);
    ck_assert_ptr_nonnull(threads);

    for (int i = 0; i < num_threads; i++) {
        thread_worker_args_t *args = malloc(sizeof(thread_worker_args_t));
        ck_assert_ptr_nonnull(args);
        args->thread_id = i;
        args->entries_per_thread = entries_per_thread;
        ck_assert_int_eq(pthread_create(&threads[i], NULL, thread_worker_func, args), 0);
    }

    for (int i = 0; i < num_threads; i++) {
        ck_assert_int_eq(pthread_join(threads[i], NULL), 0);
    }

    free(threads);

    // Should not crash; logger is a no-op
    ik_log_shutdown();
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
    srunner_set_xml(sr, "reports/check/unit/shared/logger/jsonl_thread_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
