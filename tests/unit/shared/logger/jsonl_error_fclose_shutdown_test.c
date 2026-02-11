// Test: fclose() failure in ik_log_shutdown causes PANIC (lines 305-306)

#include <check.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "shared/logger.h"
#include "shared/wrapper.h"

// Mock fclose_ to fail
int fclose_(FILE *stream)
{
    errno = EIO;
    (void)stream;
    return EOF;
}

#if !defined(SKIP_SIGNAL_TESTS)
START_TEST(test_fclose_shutdown_fail_panics) {
    char test_dir[256];
    snprintf(test_dir, sizeof(test_dir), "/tmp/ikigai_log_test_%d", getpid());
    mkdir(test_dir, 0755);

    // Initialize logger
    ik_log_init(test_dir);

    // Now shutdown with fclose failure
    ik_log_shutdown();
}
END_TEST
#endif

static Suite *test_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Logger Error: fclose shutdown fail");
    tc_core = tcase_create("Core");

#if !defined(SKIP_SIGNAL_TESTS)
    tcase_add_test_raise_signal(tc_core, test_fclose_shutdown_fail_panics, SIGABRT);
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
    srunner_set_xml(sr, "reports/check/unit/shared/logger/jsonl_error_fclose_shutdown_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
