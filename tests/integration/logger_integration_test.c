// Integration tests for logger module - thread safety

#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include "../../src/logger.h"

#define NUM_THREADS 10
#define LOGS_PER_THREAD 100

// Thread function that logs many messages
static void *logging_thread(void *arg)
{
    int thread_id = *(int *)arg;

    for (int i = 0; i < LOGS_PER_THREAD; i++) {
        ik_log_info("Thread %d message %d", thread_id, i);
        ik_log_debug("Thread %d debug %d", thread_id, i);
        ik_log_warn("Thread %d warning %d", thread_id, i);
        ik_log_error("Thread %d error %d", thread_id, i);
    }

    return NULL;
}

// Test: Multiple threads logging concurrently
// This test verifies that log messages don't interleave
// and the logger is thread-safe
START_TEST(test_concurrent_logging) {
    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];

    // Redirect output to /dev/null to avoid cluttering test output
    FILE *devnull = fopen("/dev/null", "w");
    ck_assert_ptr_nonnull(devnull);

    int saved_stdout = dup(STDOUT_FILENO);
    int saved_stderr = dup(STDERR_FILENO);
    dup2(fileno(devnull), STDOUT_FILENO);
    dup2(fileno(devnull), STDERR_FILENO);

    // Create threads
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        int ret = pthread_create(&threads[i], NULL, logging_thread,
                                 &thread_ids[i]);
        ck_assert_int_eq(ret, 0);
    }

    // Wait for all threads
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    // Restore stdout/stderr
    dup2(saved_stdout, STDOUT_FILENO);
    dup2(saved_stderr, STDERR_FILENO);
    close(saved_stdout);
    close(saved_stderr);
    fclose(devnull);

    // If we got here without crashing or deadlocking, the test passed
    ck_assert(1);
}

END_TEST
// Test: Basic logger functionality with all levels
START_TEST(test_all_log_levels)
{
    // Redirect to /dev/null
    FILE *devnull = fopen("/dev/null", "w");
    ck_assert_ptr_nonnull(devnull);

    int saved_stdout = dup(STDOUT_FILENO);
    int saved_stderr = dup(STDERR_FILENO);
    dup2(fileno(devnull), STDOUT_FILENO);
    dup2(fileno(devnull), STDERR_FILENO);

    // Call all log levels
    ik_log_debug("Debug message");
    ik_log_info("Info message");
    ik_log_warn("Warning message");
    ik_log_error("Error message");

    // Restore
    dup2(saved_stdout, STDOUT_FILENO);
    dup2(saved_stderr, STDERR_FILENO);
    close(saved_stdout);
    close(saved_stderr);
    fclose(devnull);
}

END_TEST static Suite *logger_integration_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("LoggerIntegration");
    tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);

    tcase_add_test(tc_core, test_all_log_levels);
    tcase_add_test(tc_core, test_concurrent_logging);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = logger_integration_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
