// Input parser module unit tests - Regular character parsing tests
#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "../../../src/input.h"
#include "../../../src/error.h"
#include "../../test_utils.h"

// Test: parse regular ASCII characters
START_TEST(test_input_parse_regular_char) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse 'a'
    ik_input_parse_byte(parser, 'a', &action);
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 'a');

    // Parse 'Z'
    ik_input_parse_byte(parser, 'Z', &action);
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 'Z');

    // Parse '5'
    ik_input_parse_byte(parser, '5', &action);
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, '5');

    talloc_free(ctx);
}

END_TEST
// Test: parse non-printable characters return UNKNOWN
START_TEST(test_input_parse_nonprintable)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse non-printable character below 0x20 (except recognized control chars)
    // Use 0x02 (Ctrl+B) which is not recognized
    ik_input_parse_byte(parser, 0x02, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    // Parse non-printable character above 0x7E (high byte)
    ik_input_parse_byte(parser, (char)0x80, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    talloc_free(ctx);
}

END_TEST
// Test: parse Ctrl+J (insert newline without submitting)
START_TEST(test_input_parse_newline)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse '\n' (0x0A) - Ctrl+J inserts newline without submitting
    ik_input_parse_byte(parser, '\n', &action);
    ck_assert_int_eq(action.type, IK_INPUT_INSERT_NEWLINE);

    talloc_free(ctx);
}

END_TEST
// Test: parse carriage return (Enter key in raw mode)
START_TEST(test_input_parse_carriage_return)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse '\r' (0x0D) - Enter key sends this in raw mode
    ik_input_parse_byte(parser, '\r', &action);
    ck_assert_int_eq(action.type, IK_INPUT_NEWLINE);

    talloc_free(ctx);
}

END_TEST
// Test: parse backspace character
START_TEST(test_input_parse_backspace)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse DEL (0x7F)
    ik_input_parse_byte(parser, 0x7F, &action);
    ck_assert_int_eq(action.type, IK_INPUT_BACKSPACE);

    talloc_free(ctx);
}

END_TEST
// Test: parse Ctrl+C character
START_TEST(test_input_parse_ctrl_c)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse Ctrl+C (0x03)
    ik_input_parse_byte(parser, 0x03, &action);
    ck_assert_int_eq(action.type, IK_INPUT_CTRL_C);

    talloc_free(ctx);
}

END_TEST
// Test: parse Ctrl+A character (beginning of line)
START_TEST(test_input_parse_ctrl_a)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse Ctrl+A (0x01)
    ik_input_parse_byte(parser, 0x01, &action);
    ck_assert_int_eq(action.type, IK_INPUT_CTRL_A);

    talloc_free(ctx);
}

END_TEST
// Test: parse Ctrl+E character (end of line)
START_TEST(test_input_parse_ctrl_e)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse Ctrl+E (0x05)
    ik_input_parse_byte(parser, 0x05, &action);
    ck_assert_int_eq(action.type, IK_INPUT_CTRL_E);

    talloc_free(ctx);
}

END_TEST
// Test: parse Ctrl+K character (kill to end of line)
START_TEST(test_input_parse_ctrl_k)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse Ctrl+K (0x0B)
    ik_input_parse_byte(parser, 0x0B, &action);
    ck_assert_int_eq(action.type, IK_INPUT_CTRL_K);

    talloc_free(ctx);
}

END_TEST
// Test: parse Ctrl+U character (kill line)
START_TEST(test_input_parse_ctrl_u)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse Ctrl+U (0x15)
    ik_input_parse_byte(parser, 0x15, &action);
    ck_assert_int_eq(action.type, IK_INPUT_CTRL_U);

    talloc_free(ctx);
}

END_TEST
// Test: parse Ctrl+W character (delete word backward)
START_TEST(test_input_parse_ctrl_w)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse Ctrl+W (0x17)
    ik_input_parse_byte(parser, 0x17, &action);
    ck_assert_int_eq(action.type, IK_INPUT_CTRL_W);

    talloc_free(ctx);
}

END_TEST

// Test suite
static Suite *input_char_suite(void)
{
    Suite *s = suite_create("Input Char");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);

    tcase_add_test(tc_core, test_input_parse_regular_char);
    tcase_add_test(tc_core, test_input_parse_nonprintable);
    tcase_add_test(tc_core, test_input_parse_newline);
    tcase_add_test(tc_core, test_input_parse_carriage_return);
    tcase_add_test(tc_core, test_input_parse_backspace);
    tcase_add_test(tc_core, test_input_parse_ctrl_c);
    tcase_add_test(tc_core, test_input_parse_ctrl_a);
    tcase_add_test(tc_core, test_input_parse_ctrl_e);
    tcase_add_test(tc_core, test_input_parse_ctrl_k);
    tcase_add_test(tc_core, test_input_parse_ctrl_u);
    tcase_add_test(tc_core, test_input_parse_ctrl_w);

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
