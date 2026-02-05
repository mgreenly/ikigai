// Input parser module unit tests - Arrow key escape sequences
#include <check.h>
#include <talloc.h>
#include "apps/ikigai/input.h"
#include "tests/helpers/test_utils_helper.h"

START_TEST(test_input_parse_arrow_up) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};
    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    ik_input_parse_byte(parser, 0x1B, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(parser->in_escape);

    ik_input_parse_byte(parser, '[', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, 'A', &action);
    ck_assert_int_eq(action.type, IK_INPUT_ARROW_UP);
    ck_assert(!parser->in_escape);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_input_parse_arrow_down) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};
    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    ik_input_parse_byte(parser, 0x1B, &action);
    ik_input_parse_byte(parser, '[', &action);
    ik_input_parse_byte(parser, 'B', &action);
    ck_assert_int_eq(action.type, IK_INPUT_ARROW_DOWN);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_input_parse_arrow_left) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};
    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    ik_input_parse_byte(parser, 0x1B, &action);
    ik_input_parse_byte(parser, '[', &action);
    ik_input_parse_byte(parser, 'D', &action);
    ck_assert_int_eq(action.type, IK_INPUT_ARROW_LEFT);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_input_parse_arrow_right) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};
    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    ik_input_parse_byte(parser, 0x1B, &action);
    ik_input_parse_byte(parser, '[', &action);
    ik_input_parse_byte(parser, 'C', &action);
    ck_assert_int_eq(action.type, IK_INPUT_ARROW_RIGHT);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_input_parse_arrow_up_numlock) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};
    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // ESC [ 1 ; 129 A (129 = 1 + 128 NumLock)
    ik_input_parse_byte(parser, 0x1B, &action);
    ik_input_parse_byte(parser, '[', &action);
    ik_input_parse_byte(parser, '1', &action);
    ik_input_parse_byte(parser, ';', &action);
    ik_input_parse_byte(parser, '1', &action);
    ik_input_parse_byte(parser, '2', &action);
    ik_input_parse_byte(parser, '9', &action);
    ik_input_parse_byte(parser, 'A', &action);
    ck_assert_int_eq(action.type, IK_INPUT_ARROW_UP);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_input_parse_arrow_down_numlock) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};
    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    ik_input_parse_byte(parser, 0x1B, &action);
    ik_input_parse_byte(parser, '[', &action);
    ik_input_parse_byte(parser, '1', &action);
    ik_input_parse_byte(parser, ';', &action);
    ik_input_parse_byte(parser, '1', &action);
    ik_input_parse_byte(parser, '2', &action);
    ik_input_parse_byte(parser, '9', &action);
    ik_input_parse_byte(parser, 'B', &action);
    ck_assert_int_eq(action.type, IK_INPUT_ARROW_DOWN);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_input_parse_arrow_left_numlock) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};
    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    ik_input_parse_byte(parser, 0x1B, &action);
    ik_input_parse_byte(parser, '[', &action);
    ik_input_parse_byte(parser, '1', &action);
    ik_input_parse_byte(parser, ';', &action);
    ik_input_parse_byte(parser, '1', &action);
    ik_input_parse_byte(parser, '2', &action);
    ik_input_parse_byte(parser, '9', &action);
    ik_input_parse_byte(parser, 'D', &action);
    ck_assert_int_eq(action.type, IK_INPUT_ARROW_LEFT);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_input_parse_arrow_right_numlock) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};
    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    ik_input_parse_byte(parser, 0x1B, &action);
    ik_input_parse_byte(parser, '[', &action);
    ik_input_parse_byte(parser, '1', &action);
    ik_input_parse_byte(parser, ';', &action);
    ik_input_parse_byte(parser, '1', &action);
    ik_input_parse_byte(parser, '2', &action);
    ik_input_parse_byte(parser, '9', &action);
    ik_input_parse_byte(parser, 'C', &action);
    ck_assert_int_eq(action.type, IK_INPUT_ARROW_RIGHT);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_input_parse_ctrl_arrow_numlock) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};
    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // ESC [ 1 ; 133 A (133 = 5 + 128 = Ctrl + NumLock)
    ik_input_parse_byte(parser, 0x1B, &action);
    ik_input_parse_byte(parser, '[', &action);
    ik_input_parse_byte(parser, '1', &action);
    ik_input_parse_byte(parser, ';', &action);
    ik_input_parse_byte(parser, '1', &action);
    ik_input_parse_byte(parser, '3', &action);
    ik_input_parse_byte(parser, '3', &action);
    ik_input_parse_byte(parser, 'A', &action);
    ck_assert_int_eq(action.type, IK_INPUT_NAV_PARENT);

    talloc_free(ctx);
}
END_TEST

static Suite *escape_arrow_suite(void)
{
    Suite *s = suite_create("Escape Arrow Keys");
    TCase *tc = tcase_create("Core");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);

    tcase_add_test(tc, test_input_parse_arrow_up);
    tcase_add_test(tc, test_input_parse_arrow_down);
    tcase_add_test(tc, test_input_parse_arrow_left);
    tcase_add_test(tc, test_input_parse_arrow_right);
    tcase_add_test(tc, test_input_parse_arrow_up_numlock);
    tcase_add_test(tc, test_input_parse_arrow_down_numlock);
    tcase_add_test(tc, test_input_parse_arrow_left_numlock);
    tcase_add_test(tc, test_input_parse_arrow_right_numlock);
    tcase_add_test(tc, test_input_parse_ctrl_arrow_numlock);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    Suite *s = escape_arrow_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/input/escape_arrow_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    int failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (failed == 0) ? 0 : 1;
}
