// State transition tests - testing rapid state changes and confusion attacks
// These tests adopt a hacker mindset to find vulnerabilities
#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "../../src/input.h"
#include "../../src/error.h"
#include "../test_utils_helper.h"

// ========================================================================
// State Confusion Tests
// ========================================================================

// Test: Start UTF-8 sequence, then send ESC - should this reset UTF-8 state?
START_TEST(test_state_confusion_utf8_then_escape) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Start 2-byte UTF-8 sequence (é = 0xC3 0xA9)
    ik_input_parse_byte(parser, (char)0xC3, &action);
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(parser->in_utf8);

    // Now send ESC - what happens?
    // Current implementation: ESC is treated as continuation byte, will fail validation
    ik_input_parse_byte(parser, 0x1B, &action);
    // This should return UNKNOWN and reset UTF-8 state (invalid continuation byte)
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    // Verify we can parse normally after
    ik_input_parse_byte(parser, 'a', &action);
    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 'a');

    talloc_free(ctx);
}
END_TEST
// Test: Start escape sequence, then send UTF-8 lead byte
START_TEST(test_state_confusion_escape_then_utf8) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Start escape sequence
    ik_input_parse_byte(parser, 0x1B, &action); // ESC
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(parser->in_escape);

    // Send UTF-8 lead byte (0xC3) - not '[', so invalid escape
    ik_input_parse_byte(parser, (char)0xC3, &action);
    // Should reset escape state (first byte after ESC must be '[')
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(!parser->in_escape);

    talloc_free(ctx);
}

END_TEST
// ========================================================================
// Rapid State Transition Tests
// ========================================================================

// Test: Alternating ESC and regular bytes
START_TEST(test_rapid_esc_transitions) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Send: ESC, 'x', ESC, 'y', ESC, '[', 'A'
    ik_input_parse_byte(parser, 0x1B, &action); // ESC

    ik_input_parse_byte(parser, 'x', &action); // Invalid escape
    ck_assert(!parser->in_escape);

    ik_input_parse_byte(parser, 0x1B, &action); // ESC again

    ik_input_parse_byte(parser, 'y', &action); // Invalid escape
    ck_assert(!parser->in_escape);

    // Now send valid arrow up
    ik_input_parse_byte(parser, 0x1B, &action); // ESC

    ik_input_parse_byte(parser, '[', &action);

    ik_input_parse_byte(parser, 'A', &action);
    ck_assert_int_eq(action.type, IK_INPUT_ARROW_UP); // Should work correctly

    talloc_free(ctx);
}

END_TEST
// Test: Multiple incomplete UTF-8 sequences in a row
START_TEST(test_multiple_incomplete_utf8) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_action_t action = {0};

    ik_input_parser_t *parser = ik_input_parser_create(ctx);

    // Send 3 different UTF-8 lead bytes without completions
    ik_input_parse_byte(parser, (char)0xC3, &action); // 2-byte lead
    ck_assert(parser->in_utf8);

    ik_input_parse_byte(parser, (char)0xE2, &action); // 3-byte lead (invalid continuation)
    // Should reset due to invalid continuation byte (0xE2 is not 10xxxxxx)
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
    ck_assert(!parser->in_utf8);

    talloc_free(ctx);
}

END_TEST

// ========================================================================
// Test Suite
// ========================================================================

static Suite *state_transition_suite(void)
{
    Suite *s = suite_create("StateTransition");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    // State confusion tests
    tcase_add_test(tc_core, test_state_confusion_utf8_then_escape);
    tcase_add_test(tc_core, test_state_confusion_escape_then_utf8);

    // Rapid state transitions
    tcase_add_test(tc_core, test_rapid_esc_transitions);
    tcase_add_test(tc_core, test_multiple_incomplete_utf8);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = state_transition_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
