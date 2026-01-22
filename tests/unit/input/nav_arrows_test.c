// Input parser module unit tests - Navigation arrow key tests
#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "../../../src/input.h"
#include "../../../src/error.h"
#include "../../test_utils_helper.h"

// Test: Ctrl+Left generates NAV_PREV_SIBLING
START_TEST(test_ctrl_left_arrow) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse Ctrl+Left: ESC [ 1 ; 5 D
    ik_input_parse_byte(parser, 0x1B, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '[', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '1', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, ';', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '5', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, 'D', &action);
    ck_assert_int_eq(action.type, IK_INPUT_NAV_PREV_SIBLING);

    talloc_free(ctx);
}
END_TEST
// Test: Ctrl+Right generates NAV_NEXT_SIBLING
START_TEST(test_ctrl_right_arrow) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse Ctrl+Right: ESC [ 1 ; 5 C
    ik_input_parse_byte(parser, 0x1B, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '[', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '1', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, ';', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '5', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, 'C', &action);
    ck_assert_int_eq(action.type, IK_INPUT_NAV_NEXT_SIBLING);

    talloc_free(ctx);
}

END_TEST
// Test: Ctrl+Up generates NAV_PARENT
START_TEST(test_ctrl_up_arrow) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse Ctrl+Up: ESC [ 1 ; 5 A
    ik_input_parse_byte(parser, 0x1B, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '[', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '1', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, ';', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '5', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, 'A', &action);
    ck_assert_int_eq(action.type, IK_INPUT_NAV_PARENT);

    talloc_free(ctx);
}

END_TEST
// Test: Ctrl+Down generates NAV_CHILD
START_TEST(test_ctrl_down_arrow) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse Ctrl+Down: ESC [ 1 ; 5 B
    ik_input_parse_byte(parser, 0x1B, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '[', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '1', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, ';', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '5', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, 'B', &action);
    ck_assert_int_eq(action.type, IK_INPUT_NAV_CHILD);

    talloc_free(ctx);
}

END_TEST
// Test: plain arrow left still works (no regression)
START_TEST(test_plain_left_arrow) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse plain Left: ESC [ D
    ik_input_parse_byte(parser, 0x1B, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '[', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, 'D', &action);
    ck_assert_int_eq(action.type, IK_INPUT_ARROW_LEFT);

    talloc_free(ctx);
}

END_TEST
// Test: plain arrow right still works (no regression)
START_TEST(test_plain_right_arrow) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse plain Right: ESC [ C
    ik_input_parse_byte(parser, 0x1B, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '[', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, 'C', &action);
    ck_assert_int_eq(action.type, IK_INPUT_ARROW_RIGHT);

    talloc_free(ctx);
}

END_TEST
// Test: plain arrow up still works (no regression)
START_TEST(test_plain_up_arrow) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse plain Up: ESC [ A
    ik_input_parse_byte(parser, 0x1B, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '[', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, 'A', &action);
    ck_assert_int_eq(action.type, IK_INPUT_ARROW_UP);

    talloc_free(ctx);
}

END_TEST
// Test: plain arrow down still works (no regression)
START_TEST(test_plain_down_arrow) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse plain Down: ESC [ B
    ik_input_parse_byte(parser, 0x1B, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '[', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, 'B', &action);
    ck_assert_int_eq(action.type, IK_INPUT_ARROW_DOWN);

    talloc_free(ctx);
}

END_TEST
// Test: invalid Ctrl+Arrow pattern (wrong modifier)
START_TEST(test_invalid_ctrl_pattern) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse ESC [ 1 ; 3 A (Alt+Up, not Ctrl+Up)
    // This should not match the Ctrl pattern [1;5]
    ik_input_parse_byte(parser, 0x1B, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '[', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '1', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, ';', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '3', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    // Final byte - should not trigger navigation actions
    ik_input_parse_byte(parser, 'A', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    talloc_free(ctx);
}

END_TEST
// Test: Ctrl pattern with invalid arrow key
START_TEST(test_ctrl_pattern_invalid_key) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse ESC [ 1 ; 5 E (Ctrl+E, not a valid arrow)
    ik_input_parse_byte(parser, 0x1B, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '[', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '1', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, ';', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '5', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    // 'E' is not a valid arrow key (not A, B, C, or D)
    ik_input_parse_byte(parser, 'E', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    talloc_free(ctx);
}

END_TEST

// Test suite
static Suite *input_nav_arrows_suite(void)
{
    Suite *s = suite_create("Input Navigation Arrows");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    tcase_add_test(tc_core, test_ctrl_left_arrow);
    tcase_add_test(tc_core, test_ctrl_right_arrow);
    tcase_add_test(tc_core, test_ctrl_up_arrow);
    tcase_add_test(tc_core, test_ctrl_down_arrow);
    tcase_add_test(tc_core, test_plain_left_arrow);
    tcase_add_test(tc_core, test_plain_right_arrow);
    tcase_add_test(tc_core, test_plain_up_arrow);
    tcase_add_test(tc_core, test_plain_down_arrow);
    tcase_add_test(tc_core, test_invalid_ctrl_pattern);
    tcase_add_test(tc_core, test_ctrl_pattern_invalid_key);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = input_nav_arrows_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/input/nav_arrows_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
