// Test: fclose() failure in ik_log_reinit causes PANIC (lines 327-328)

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

// Mock fclose_ to fail
static int mock_count = 0;
int fclose_(FILE *stream)
{
    // Fail on first fclose (during reinit)
    if (mock_count == 0) {
        mock_count++;
        errno = EIO;
        (void)stream;
        return EOF;
    }
    // Subsequent calls use real fclose
    return fclose(stream);
}

#if !defined(SKIP_SIGNAL_TESTS)
START_TEST(test_fclose_reinit_fail_panics) {
    char test_dir[256];
    snprintf(test_dir, sizeof(test_dir), "/tmp/ikigai_log_test_%d", getpid());
    mkdir(test_dir, 0755);

    // Initialize logger
    ik_log_init(test_dir);

    // Now reinit with fclose failure
    mock_count = 0;
    ik_log_reinit(test_dir);
}
END_TEST
#endif

static Suite *test_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Logger Error: fclose reinit fail");
    tc_core = tcase_create("Core");

#if !defined(SKIP_SIGNAL_TESTS)
    tcase_add_test_raise_signal(tc_core, test_fclose_reinit_fail_panics, SIGABRT);
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
    srunner_set_xml(sr, "reports/check/unit/logger/jsonl_error_fclose_reinit_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
