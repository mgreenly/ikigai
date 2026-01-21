// Test: fopen() failure in ik_log_reinit causes PANIC (lines 339-340)

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

// Mock fopen_ to fail on second call (after reinit fclose)
static int mock_count = 0;
FILE *fopen_(const char *pathname, const char *mode)
{
    // Count calls to fopen for current.log
    if (strstr(pathname, "current.log") != NULL) {
        mock_count++;
        // Fail on the second call (during reinit)
        if (mock_count == 2) {
            errno = EACCES;
            return NULL;
        }
    }
    return fopen(pathname, mode);
}

#if !defined(SKIP_SIGNAL_TESTS)
START_TEST(test_fopen_reinit_fail_panics) {
    char test_dir[256];
    snprintf(test_dir, sizeof(test_dir), "/tmp/ikigai_log_test_%d", getpid());
    mkdir(test_dir, 0755);

    // Initialize logger
    mock_count = 0;
    ik_log_init(test_dir);

    // Now reinit with fopen failure
    ik_log_reinit(test_dir);
}
END_TEST
#endif

static Suite *test_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Logger Error: fopen reinit fail");
    tc_core = tcase_create("Core");

#if !defined(SKIP_SIGNAL_TESTS)
    tcase_add_test_raise_signal(tc_core, test_fopen_reinit_fail_panics, SIGABRT);
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
    srunner_set_xml(sr, "reports/check/unit/logger/jsonl_error_fopen_reinit_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
