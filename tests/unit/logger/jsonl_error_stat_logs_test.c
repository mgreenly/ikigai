// Test: stat() error other than ENOENT for logs directory causes PANIC (line 261)

#include <check.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../../../src/logger.h"
#include "../../../src/wrapper.h"

// Mock posix_stat_ to fail with EACCES for logs directory
static int mock_count = 0;
int posix_stat_(const char *pathname, struct stat *statbuf)
{
    // First call is for .ikigai directory - fail with ENOENT (will create it)
    if (mock_count == 0) {
        mock_count++;
        errno = ENOENT;
        return -1;
    }
    // Second call is for logs directory - fail with EACCES
    if (mock_count == 1) {
        mock_count++;
        errno = EACCES;
        return -1;
    }
    // Subsequent calls use real stat
    return stat(pathname, statbuf);
}

#if !defined(SKIP_SIGNAL_TESTS)
START_TEST(test_stat_eacces_logs_panics) {
    char test_dir[256];
    snprintf(test_dir, sizeof(test_dir), "/tmp/ikigai_log_test_%d", getpid());
    mkdir(test_dir, 0755);

    mock_count = 0;
    ik_log_init(test_dir);
}
END_TEST
#endif

static Suite *test_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Logger Error: stat logs EACCES");
    tc_core = tcase_create("Core");

#if !defined(SKIP_SIGNAL_TESTS)
    tcase_add_test_raise_signal(tc_core, test_stat_eacces_logs_panics, SIGABRT);
#endif

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = test_suite();
    sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/logger/jsonl_error_stat_logs_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
