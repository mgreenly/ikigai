#include "../../../test_constants.h"

#include "tools/web_search_google/error_output.h"

#include <check.h>
#include <stdlib.h>
#include <talloc.h>

static TALLOC_CTX *test_ctx;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

START_TEST(test_output_error_basic) {
    output_error(test_ctx, "Test error", "TEST_ERROR");
}

END_TEST

START_TEST(test_output_error_with_event_auth_missing) {
    output_error_with_event(test_ctx, "Auth required", "AUTH_MISSING");
}

END_TEST

START_TEST(test_output_error_with_event_other_error) {
    output_error_with_event(test_ctx, "Some error", "OTHER_ERROR");
}

END_TEST

static Suite *error_output_suite(void)
{
    Suite *s = suite_create("WebSearchGoogleErrorOutput");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_core, setup, teardown);

    tcase_add_test(tc_core, test_output_error_basic);
    tcase_add_test(tc_core, test_output_error_with_event_auth_missing);
    tcase_add_test(tc_core, test_output_error_with_event_other_error);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = error_output_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/tools/web_search_google/error_output_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
