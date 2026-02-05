// Input parser module unit tests - Scroll input action type tests
#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "apps/ikigai/input.h"
#include "shared/error.h"
#include "tests/helpers/test_utils_helper.h"

// Test: IK_INPUT_SCROLL_UP enum value exists and is distinct
START_TEST(test_scroll_up_enum_exists) {
    // Compile-time verification: IK_INPUT_SCROLL_UP should exist
    // and be different from other action types
    ik_input_action_type_t scroll_up = IK_INPUT_SCROLL_UP;
    ik_input_action_type_t scroll_down = IK_INPUT_SCROLL_DOWN;
    ik_input_action_type_t page_down = IK_INPUT_PAGE_DOWN;

    // Verify scroll_up is distinct from page_down
    ck_assert_int_ne(scroll_up, page_down);
    // Verify scroll_up is distinct from scroll_down
    ck_assert_int_ne(scroll_up, scroll_down);
}

END_TEST
// Test: IK_INPUT_SCROLL_DOWN enum value exists and is distinct
START_TEST(test_scroll_down_enum_exists) {
    // Compile-time verification: IK_INPUT_SCROLL_DOWN should exist
    // and be different from other action types
    ik_input_action_type_t scroll_down = IK_INPUT_SCROLL_DOWN;
    ik_input_action_type_t page_down = IK_INPUT_PAGE_DOWN;
    ik_input_action_type_t unknown = IK_INPUT_UNKNOWN;

    // Verify scroll_down is distinct from page_down
    ck_assert_int_ne(scroll_down, page_down);
    // Verify scroll_down is distinct from unknown
    ck_assert_int_ne(scroll_down, unknown);
}

END_TEST

// Test suite
static Suite *input_scroll_suite(void)
{
    Suite *s = suite_create("Input Scroll");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    tcase_add_test(tc_core, test_scroll_up_enum_exists);
    tcase_add_test(tc_core, test_scroll_down_enum_exists);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = input_scroll_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/input/scroll_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
