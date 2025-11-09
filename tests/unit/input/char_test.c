// Input parser module unit tests - Regular character parsing tests
#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "../../../src/input.h"
#include "../../../src/error.h"
#include "../../test_utils.h"

// Test: parse regular ASCII characters
START_TEST(test_input_parse_regular_char)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;
    ik_input_action_t action = {0};

    res_t res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    // Parse 'a'
    res = ik_input_parse_byte(parser, 'a', &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 'a');

    // Parse 'Z'
    res = ik_input_parse_byte(parser, 'Z', &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 'Z');

    // Parse '5'
    res = ik_input_parse_byte(parser, '5', &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, '5');

    talloc_free(ctx);
}

END_TEST
// Test: parse non-printable characters return UNKNOWN
START_TEST(test_input_parse_nonprintable)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;
    ik_input_action_t action = {0};

    res_t res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    // Parse non-printable character below 0x20 (except recognized control chars)
    res = ik_input_parse_byte(parser, 0x01, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    // Parse non-printable character above 0x7E (high byte)
    res = ik_input_parse_byte(parser, (char)0x80, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    talloc_free(ctx);
}

END_TEST
// Test: parse newline character
START_TEST(test_input_parse_newline)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;
    ik_input_action_t action = {0};

    res_t res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    // Parse '\n' (0x0A)
    res = ik_input_parse_byte(parser, '\n', &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_NEWLINE);

    talloc_free(ctx);
}

END_TEST
// Test: parse carriage return (Enter key in raw mode)
START_TEST(test_input_parse_carriage_return)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;
    ik_input_action_t action = {0};

    res_t res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    // Parse '\r' (0x0D) - Enter key sends this in raw mode
    res = ik_input_parse_byte(parser, '\r', &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_NEWLINE);

    talloc_free(ctx);
}

END_TEST
// Test: parse backspace character
START_TEST(test_input_parse_backspace)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;
    ik_input_action_t action = {0};

    res_t res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    // Parse DEL (0x7F)
    res = ik_input_parse_byte(parser, 0x7F, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_BACKSPACE);

    talloc_free(ctx);
}

END_TEST
// Test: parse Ctrl+C character
START_TEST(test_input_parse_ctrl_c)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = NULL;
    ik_input_action_t action = {0};

    res_t res = ik_input_parser_create(ctx, &parser);
    ck_assert(is_ok(&res));

    // Parse Ctrl+C (0x03)
    res = ik_input_parse_byte(parser, 0x03, &action);
    ck_assert(is_ok(&res));
    ck_assert_int_eq(action.type, IK_INPUT_CTRL_C);

    talloc_free(ctx);
}

END_TEST

// Test suite
static Suite *input_char_suite(void)
{
    Suite *s = suite_create("Input Char");
    TCase *tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_input_parse_regular_char);
    tcase_add_test(tc_core, test_input_parse_nonprintable);
    tcase_add_test(tc_core, test_input_parse_newline);
    tcase_add_test(tc_core, test_input_parse_carriage_return);
    tcase_add_test(tc_core, test_input_parse_backspace);
    tcase_add_test(tc_core, test_input_parse_ctrl_c);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = input_char_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
