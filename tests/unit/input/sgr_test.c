// Input parser module unit tests - SGR sequence stripping tests
#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "../../../src/input.h"
#include "../../../src/error.h"
#include "../../test_utils.h"

// Test: SGR reset sequence is discarded
START_TEST(test_input_parse_sgr_reset) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse ESC [ 0 m (SGR reset)
    ik_input_parse_byte(parser, 0x1B, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '[', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '0', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, 'm', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN); // Discarded - no action
    ck_assert(!parser->in_escape); // Should reset

    talloc_free(ctx);
}

END_TEST
// Test: 256-color foreground SGR sequence is discarded
START_TEST(test_input_parse_sgr_256_color)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse ESC [ 3 8 ; 5 ; 2 4 2 m (256-color foreground)
    ik_input_parse_byte(parser, 0x1B, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '[', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '3', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '8', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, ';', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '5', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, ';', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '2', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '4', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '2', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, 'm', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN); // Discarded - no action
    ck_assert(!parser->in_escape); // Should reset

    talloc_free(ctx);
}

END_TEST
// Test: pasting text with SGR before and after
START_TEST(test_input_parse_text_with_sgr)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse "before\x1b[0mafter"
    // First: "before"
    ik_input_parse_byte(parser, 'b', &action);
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 'b');

    ik_input_parse_byte(parser, 'e', &action);
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 'e');

    ik_input_parse_byte(parser, 'f', &action);
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 'f');

    ik_input_parse_byte(parser, 'o', &action);
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 'o');

    ik_input_parse_byte(parser, 'r', &action);
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 'r');

    ik_input_parse_byte(parser, 'e', &action);
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 'e');

    // Now: \x1b[0m (SGR reset - should be discarded)
    ik_input_parse_byte(parser, 0x1B, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '[', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '0', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, 'm', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN); // Discarded
    ck_assert(!parser->in_escape);

    // Finally: "after"
    ik_input_parse_byte(parser, 'a', &action);
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 'a');

    ik_input_parse_byte(parser, 'f', &action);
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 'f');

    ik_input_parse_byte(parser, 't', &action);
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 't');

    ik_input_parse_byte(parser, 'e', &action);
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 'e');

    ik_input_parse_byte(parser, 'r', &action);
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 'r');

    talloc_free(ctx);
}

END_TEST
// Test: pasting text with SGR prefix
START_TEST(test_input_parse_text_with_sgr_prefix)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse "\x1b[38;5;242mhello"
    // First: \x1b[38;5;242m (256-color - should be discarded)
    ik_input_parse_byte(parser, 0x1B, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '[', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '3', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '8', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, ';', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '5', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, ';', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '2', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '4', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '2', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, 'm', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN); // Discarded
    ck_assert(!parser->in_escape);

    // Now: "hello"
    ik_input_parse_byte(parser, 'h', &action);
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 'h');

    ik_input_parse_byte(parser, 'e', &action);
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 'e');

    ik_input_parse_byte(parser, 'l', &action);
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 'l');

    ik_input_parse_byte(parser, 'l', &action);
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 'l');

    ik_input_parse_byte(parser, 'o', &action);
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 'o');

    talloc_free(ctx);
}

END_TEST
// Test: non-SGR escapes still work after SGR implementation
START_TEST(test_input_parse_arrow_still_works_with_sgr)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse ESC [ A (arrow up - should still work)
    ik_input_parse_byte(parser, 0x1B, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '[', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, 'A', &action);
    ck_assert_int_eq(action.type, IK_INPUT_ARROW_UP);
    ck_assert(!parser->in_escape);

    talloc_free(ctx);
}

END_TEST

// Test: ESC m with no '[' should reset (esc_len == 0 when checking 'm')
START_TEST(test_input_parse_sgr_no_bracket)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Parse ESC m (invalid - missing '[')
    ik_input_parse_byte(parser, 0x1B, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(parser->in_escape);

    // 'm' without '[' - should reset (esc_len == 0 at this point)
    ik_input_parse_byte(parser, 'm', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(!parser->in_escape); // Should reset

    // Verify parser can handle next input
    ik_input_parse_byte(parser, 'a', &action);
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 'a');

    talloc_free(ctx);
}

END_TEST

// Test suite
static Suite *input_sgr_suite(void)
{
    Suite *s = suite_create("Input SGR");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);

    tcase_add_test(tc_core, test_input_parse_sgr_reset);
    tcase_add_test(tc_core, test_input_parse_sgr_256_color);
    tcase_add_test(tc_core, test_input_parse_text_with_sgr);
    tcase_add_test(tc_core, test_input_parse_text_with_sgr_prefix);
    tcase_add_test(tc_core, test_input_parse_arrow_still_works_with_sgr);
    tcase_add_test(tc_core, test_input_parse_sgr_no_bracket);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = input_sgr_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
