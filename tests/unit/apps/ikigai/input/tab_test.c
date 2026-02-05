// Input parser module unit tests - Tab key parsing tests
#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "apps/ikigai/input.h"
#include "shared/error.h"
#include "tests/helpers/test_utils_helper.h"

// Test: parse Tab key (0x09)
START_TEST(test_input_parse_tab_key) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Tab is byte 0x09
    ik_input_parse_byte(parser, '\t', &action);

    ck_assert_int_eq(action.type, IK_INPUT_TAB);
    ck_assert_int_eq(action.codepoint, 0);  // No codepoint for TAB

    talloc_free(ctx);
}

END_TEST

// Test suite
static Suite *input_tab_suite(void)
{
    Suite *s = suite_create("Input Tab");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    tcase_add_test(tc_core, test_input_parse_tab_key);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = input_tab_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/input/tab_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
