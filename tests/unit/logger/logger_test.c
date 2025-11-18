// Unit tests for logger module

#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include "../../../src/logger.h"

// Helper: capture stdout to a buffer
static char stdout_buffer[4096];
static int stdout_pipe[2];
static int saved_stdout;

static void setup_stdout_capture(void)
{
    memset(stdout_buffer, 0, sizeof(stdout_buffer));
    pipe(stdout_pipe);
    saved_stdout = dup(STDOUT_FILENO);
    dup2(stdout_pipe[1], STDOUT_FILENO);
    close(stdout_pipe[1]);
}

static void finish_stdout_capture(void)
{
    fflush(stdout);
    read(stdout_pipe[0], stdout_buffer, sizeof(stdout_buffer) - 1);
    dup2(saved_stdout, STDOUT_FILENO);
    close(stdout_pipe[0]);
    close(saved_stdout);
}

// Helper: capture stderr to a buffer
static char stderr_buffer[4096];
static int stderr_pipe[2];
static int saved_stderr;

static void setup_stderr_capture(void)
{
    memset(stderr_buffer, 0, sizeof(stderr_buffer));
    pipe(stderr_pipe);
    saved_stderr = dup(STDERR_FILENO);
    dup2(stderr_pipe[1], STDERR_FILENO);
    close(stderr_pipe[1]);
}

static void finish_stderr_capture(void)
{
    fflush(stderr);
    read(stderr_pipe[0], stderr_buffer, sizeof(stderr_buffer) - 1);
    dup2(saved_stderr, STDERR_FILENO);
    close(stderr_pipe[0]);
    close(saved_stderr);
}

// Test: ik_log_info outputs to stdout with correct format
START_TEST(test_logger_info_stdout) {
    ik_log_reset_timestamp_check();
    setenv("JOURNAL_STREAM", "8:12345", 1);
    setup_stdout_capture();
    ik_log_info("test message");
    finish_stdout_capture();

    ck_assert_str_eq(stdout_buffer, "INFO: test message\n");
    unsetenv("JOURNAL_STREAM");
}

END_TEST
// Test: ik_log_debug outputs to stdout with correct format
START_TEST(test_logger_debug_stdout)
{
    ik_log_reset_timestamp_check();
    setenv("JOURNAL_STREAM", "8:12345", 1);
    setup_stdout_capture();
    ik_log_debug("debug message");
    finish_stdout_capture();

    ck_assert_str_eq(stdout_buffer, "DEBUG: debug message\n");
    unsetenv("JOURNAL_STREAM");
}

END_TEST
// Test: ik_log_warn outputs to stdout with correct format
START_TEST(test_logger_warn_stdout)
{
    ik_log_reset_timestamp_check();
    setenv("JOURNAL_STREAM", "8:12345", 1);
    setup_stdout_capture();
    ik_log_warn("warning message");
    finish_stdout_capture();

    ck_assert_str_eq(stdout_buffer, "WARN: warning message\n");
    unsetenv("JOURNAL_STREAM");
}

END_TEST
// Test: ik_log_error outputs to stderr with correct format
START_TEST(test_logger_error_stderr)
{
    ik_log_reset_timestamp_check();
    setenv("JOURNAL_STREAM", "8:12345", 1);
    setup_stderr_capture();
    ik_log_error("error message");
    finish_stderr_capture();

    ck_assert_str_eq(stderr_buffer, "ERROR: error message\n");
    unsetenv("JOURNAL_STREAM");
}

END_TEST
// Test: printf-style formatting works correctly
START_TEST(test_logger_formatting)
{
    ik_log_reset_timestamp_check();
    setenv("JOURNAL_STREAM", "8:12345", 1);
    setup_stdout_capture();
    ik_log_info("value=%d string=%s", 42, "test");
    finish_stdout_capture();

    ck_assert_str_eq(stdout_buffer, "INFO: value=42 string=test\n");
    unsetenv("JOURNAL_STREAM");
}

END_TEST
// Test: Multiple format specifiers work
START_TEST(test_logger_multiple_formats)
{
    ik_log_reset_timestamp_check();
    setenv("JOURNAL_STREAM", "8:12345", 1);
    setup_stderr_capture();
    ik_log_error("error %d: %s (code 0x%x)", 123, "failure", 0xAB);
    finish_stderr_capture();

    ck_assert_str_eq(stderr_buffer, "ERROR: error 123: failure (code 0xab)\n");
    unsetenv("JOURNAL_STREAM");
}

END_TEST
// Test: Timestamps are NOT added when JOURNAL_STREAM is set (systemd mode)
START_TEST(test_logger_no_timestamp_in_systemd)
{
    ik_log_reset_timestamp_check();
    setenv("JOURNAL_STREAM", "8:12345", 1);

    setup_stdout_capture();
    ik_log_info("test");
    finish_stdout_capture();

    // Should be just "INFO: test\n" without timestamp
    ck_assert_str_eq(stdout_buffer, "INFO: test\n");

    unsetenv("JOURNAL_STREAM");
}

END_TEST
// Test: Timestamps ARE added when JOURNAL_STREAM is not set (direct mode)
START_TEST(test_logger_timestamp_in_direct_mode)
{
    ik_log_reset_timestamp_check();
    unsetenv("JOURNAL_STREAM");

    setup_stdout_capture();
    ik_log_info("test");
    finish_stdout_capture();

    // Should start with timestamp like "2025-01-15 10:30:45 INFO: test\n"
    // We'll just check it has more than "INFO: test\n" and contains "INFO: test"
    ck_assert(strstr(stdout_buffer, "INFO: test\n") != NULL);
    ck_assert(strlen(stdout_buffer) > strlen("INFO: test\n"));
}

END_TEST static Suite *logger_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Logger");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_logger_no_timestamp_in_systemd);
    tcase_add_test(tc_core, test_logger_timestamp_in_direct_mode);
    tcase_add_test(tc_core, test_logger_info_stdout);
    tcase_add_test(tc_core, test_logger_debug_stdout);
    tcase_add_test(tc_core, test_logger_warn_stdout);
    tcase_add_test(tc_core, test_logger_error_stderr);
    tcase_add_test(tc_core, test_logger_formatting);
    tcase_add_test(tc_core, test_logger_multiple_formats);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = logger_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
