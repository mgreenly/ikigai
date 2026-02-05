// Test: ik_log_shutdown when ik_log_file is NULL (line 303)

#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include "shared/logger.h"

START_TEST(test_shutdown_when_null) {
    // Call shutdown without init - should handle NULL gracefully
    ik_log_shutdown();
    // Should not crash
    ck_assert(1);
}
END_TEST

static Suite *test_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Logger Error: shutdown when NULL");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_shutdown_when_null);

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
    srunner_set_xml(sr, "reports/check/unit/shared/logger/jsonl_error_shutdown_null_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
