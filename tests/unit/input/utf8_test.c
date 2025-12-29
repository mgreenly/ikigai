// Input parser module unit tests - UTF-8 parsing tests
#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "../../../src/input.h"
#include "../../../src/error.h"
#include "../../test_utils.h"

// Test: parse 2-byte UTF-8 character (é = U+00E9 = 0xC3 0xA9)
START_TEST(test_input_parse_utf8_2byte) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse é (0xC3 0xA9) - 2 byte UTF-8
    // First byte (lead byte)
    ik_input_parse_byte(parser, (char)0xC3, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN); // Incomplete sequence

    // Second byte (continuation byte)
    ik_input_parse_byte(parser, (char)0xA9, &action);
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 0x00E9); // U+00E9 (é)

    talloc_free(ctx);
}

END_TEST
// Test: parse 3-byte UTF-8 character (☃ = U+2603 = 0xE2 0x98 0x83)
START_TEST(test_input_parse_utf8_3byte)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse ☃ (0xE2 0x98 0x83) - 3 byte UTF-8
    // First byte (lead byte)
    ik_input_parse_byte(parser, (char)0xE2, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN); // Incomplete

    // Second byte (continuation)
    ik_input_parse_byte(parser, (char)0x98, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN); // Still incomplete

    // Third byte (continuation)
    ik_input_parse_byte(parser, (char)0x83, &action);
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 0x2603); // U+2603 (☃)

    talloc_free(ctx);
}

END_TEST
// Test: parse 4-byte UTF-8 character (🎉 = U+1F389 = 0xF0 0x9F 0x8E 0x89)
START_TEST(test_input_parse_utf8_4byte)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse 🎉 (0xF0 0x9F 0x8E 0x89) - 4 byte UTF-8
    // First byte (lead byte)
    ik_input_parse_byte(parser, (char)0xF0, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN); // Incomplete

    // Second byte (continuation)
    ik_input_parse_byte(parser, (char)0x9F, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN); // Still incomplete

    // Third byte (continuation)
    ik_input_parse_byte(parser, (char)0x8E, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN); // Still incomplete

    // Fourth byte (continuation)
    ik_input_parse_byte(parser, (char)0x89, &action);
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 0x1F389); // U+1F389 (🎉)

    talloc_free(ctx);
}

END_TEST
// Test: incomplete UTF-8 sequence (only lead byte)
START_TEST(test_input_parse_utf8_incomplete_eof)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse only lead byte of 2-byte sequence
    ik_input_parse_byte(parser, (char)0xC3, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN); // Incomplete
    ck_assert(parser->in_utf8); // Should be in UTF-8 mode

    talloc_free(ctx);
}

END_TEST
// Test: invalid UTF-8 sequence (invalid continuation byte)
START_TEST(test_input_parse_utf8_invalid_continuation)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Start 2-byte sequence
    ik_input_parse_byte(parser, (char)0xC3, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(parser->in_utf8);

    // Send invalid continuation byte (not 10xxxxxx pattern)
    // Using 0xFF which is 11111111 (not a valid continuation byte)
    ik_input_parse_byte(parser, (char)0xFF, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(!parser->in_utf8); // Should reset

    // Verify parser can handle next input correctly
    ik_input_parse_byte(parser, 'a', &action);
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 'a');

    talloc_free(ctx);
}

END_TEST

// Test suite
static Suite *input_utf8_suite(void)
{
    Suite *s = suite_create("Input UTF-8");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);
    tcase_set_timeout(tc_core, 30);
    tcase_set_timeout(tc_core, 30);
    tcase_set_timeout(tc_core, 30);
    tcase_set_timeout(tc_core, 30);

    tcase_add_test(tc_core, test_input_parse_utf8_2byte);
    tcase_add_test(tc_core, test_input_parse_utf8_3byte);
    tcase_add_test(tc_core, test_input_parse_utf8_4byte);
    tcase_add_test(tc_core, test_input_parse_utf8_incomplete_eof);
    tcase_add_test(tc_core, test_input_parse_utf8_invalid_continuation);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = input_utf8_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
