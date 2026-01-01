// Escape sequence edge case tests - testing boundary conditions and malformed sequences
// These tests adopt a hacker mindset to find vulnerabilities
#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "../../src/input.h"
#include "../../src/error.h"
#include "../test_utils.h"

// ========================================================================
// Escape Sequence Edge Cases
// ========================================================================

// Test: ESC [ followed by null byte
START_TEST(test_escape_sequence_null_byte) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    ik_input_parse_byte(parser, 0x1B, &action); // ESC
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '[', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    // Send null byte - what happens?
    ik_input_parse_byte(parser, '\0', &action);
    // Should treat as incomplete (waiting for more)
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(parser->in_escape); // Still in escape mode

    talloc_free(ctx);
}

END_TEST
// Test: ESC [ followed by control character (e.g., Ctrl+C)
START_TEST(test_escape_sequence_control_char) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    ik_input_parse_byte(parser, 0x1B, &action); // ESC
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    ik_input_parse_byte(parser, '[', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    // Send Ctrl+C (0x03)
    ik_input_parse_byte(parser, 0x03, &action);
    // Should treat as incomplete/unknown (not a recognized sequence)
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    talloc_free(ctx);
}

END_TEST
// Test: Very long escape sequence (just before buffer overflow)
START_TEST(test_escape_sequence_nearly_full_buffer) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    ik_input_parse_byte(parser, 0x1B, &action); // ESC

    ik_input_parse_byte(parser, '[', &action);

    // Send 13 more bytes (total 15 in buffer: '[' + 13 '1's)
    // Buffer is 16 bytes, esc_len will be 14, which is < 15 (sizeof - 1)
    for (int i = 0; i < 13; i++) {
        ik_input_parse_byte(parser, '1', &action);
        ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    }

    // Should still be in escape mode (esc_len == 14, still < 15)
    ck_assert(parser->in_escape);
    ck_assert_uint_eq(parser->esc_len, 14);

    // One more byte brings us to esc_len == 15 (sizeof - 1), triggering overflow protection
    ik_input_parse_byte(parser, '1', &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(!parser->in_escape); // Should have reset

    talloc_free(ctx);
}

END_TEST

// ========================================================================
// Test Suite
// ========================================================================

static Suite *escape_edge_suite(void)
{
    Suite *s = suite_create("EscapeEdge");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);

    // Escape sequence edge cases
    tcase_add_test(tc_core, test_escape_sequence_null_byte);
    tcase_add_test(tc_core, test_escape_sequence_control_char);
    tcase_add_test(tc_core, test_escape_sequence_nearly_full_buffer);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = escape_edge_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
