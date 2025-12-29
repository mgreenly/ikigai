/**
 * @file mouse_scroll_test.c
 * @brief Unit tests for mouse scroll event parsing
 */

#include <check.h>
#include <talloc.h>
#include "../../../src/input.h"
#include "../../test_utils.h"

// Test: Mouse scroll up sequence ESC [ < 64 ; col ; row M should generate IK_INPUT_SCROLL_UP
START_TEST(test_mouse_scroll_up_parsing) {
    void *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = ik_input_parser_create(ctx);
    ik_input_action_t action;

    // Mouse scroll up in SGR mode: ESC [ < 64 ; 1 ; 1 M
    const char sequence[] = "\x1b[<64;1;1M";

    for (size_t i = 0; i < sizeof(sequence) - 1; i++) {
        ik_input_parse_byte(parser, sequence[i], &action);

        // All bytes except the last should return UNKNOWN (incomplete sequence)
        if (i < sizeof(sequence) - 2) {
            ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
        }
    }

    // Final byte should complete the sequence and return SCROLL_UP
    ck_assert_int_eq(action.type, IK_INPUT_SCROLL_UP);

    talloc_free(ctx);
}
END_TEST
// Test: Mouse scroll down sequence ESC [ < 65 ; col ; row M should generate IK_INPUT_SCROLL_DOWN
START_TEST(test_mouse_scroll_down_parsing)
{
    void *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = ik_input_parser_create(ctx);
    ik_input_action_t action;

    // Mouse scroll down in SGR mode: ESC [ < 65 ; 1 ; 1 M
    const char sequence[] = "\x1b[<65;1;1M";

    for (size_t i = 0; i < sizeof(sequence) - 1; i++) {
        ik_input_parse_byte(parser, sequence[i], &action);

        // All bytes except the last should return UNKNOWN (incomplete sequence)
        if (i < sizeof(sequence) - 2) {
            ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);
        }
    }

    // Final byte should complete the sequence and return SCROLL_DOWN
    ck_assert_int_eq(action.type, IK_INPUT_SCROLL_DOWN);

    talloc_free(ctx);
}

END_TEST
// Test: Mouse sequence without button field separator should be discarded
START_TEST(test_mouse_sequence_missing_separator)
{
    void *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = ik_input_parser_create(ctx);
    ik_input_action_t action;

    // Malformed sequence: ESC [ < 64 M (missing semicolon separator)
    const char sequence[] = "\x1b[<64M";

    for (size_t i = 0; i < sizeof(sequence) - 1; i++) {
        ik_input_parse_byte(parser, sequence[i], &action);
    }

    // Should return UNKNOWN for malformed sequence
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    talloc_free(ctx);
}

END_TEST
// Test: Mouse click event (button 0) should be discarded
START_TEST(test_mouse_click_discarded)
{
    void *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = ik_input_parser_create(ctx);
    ik_input_action_t action;

    // Mouse click: ESC [ < 0 ; 1 ; 1 M
    const char sequence[] = "\x1b[<0;1;1M";

    for (size_t i = 0; i < sizeof(sequence) - 1; i++) {
        ik_input_parse_byte(parser, sequence[i], &action);
    }

    // Should return UNKNOWN for non-scroll mouse events
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    talloc_free(ctx);
}

END_TEST
// Test: Mouse release event (m) should be handled
START_TEST(test_mouse_release_event)
{
    void *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = ik_input_parser_create(ctx);
    ik_input_action_t action;

    // Mouse release: ESC [ < 0 ; 1 ; 1 m (lowercase m)
    const char sequence[] = "\x1b[<0;1;1m";

    for (size_t i = 0; i < sizeof(sequence) - 1; i++) {
        ik_input_parse_byte(parser, sequence[i], &action);
    }

    // Should return UNKNOWN for non-scroll mouse events
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    talloc_free(ctx);
}

END_TEST
// Test: Mouse button with single digit should be discarded
START_TEST(test_mouse_single_digit_button)
{
    void *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = ik_input_parser_create(ctx);
    ik_input_action_t action;

    // Single digit button: ESC [ < 1 ; 1 ; 1 M
    const char sequence[] = "\x1b[<1;1;1M";

    for (size_t i = 0; i < sizeof(sequence) - 1; i++) {
        ik_input_parse_byte(parser, sequence[i], &action);
    }

    // Should return UNKNOWN for non-2-digit buttons
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    talloc_free(ctx);
}

END_TEST
// Test: Mouse button with triple digits should be discarded
START_TEST(test_mouse_triple_digit_button)
{
    void *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = ik_input_parser_create(ctx);
    ik_input_action_t action;

    // Triple digit button: ESC [ < 100 ; 1 ; 1 M
    const char sequence[] = "\x1b[<100;1;1M";

    for (size_t i = 0; i < sizeof(sequence) - 1; i++) {
        ik_input_parse_byte(parser, sequence[i], &action);
    }

    // Should return UNKNOWN for non-2-digit buttons
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    talloc_free(ctx);
}

END_TEST
// Test: Mouse button 63 (not scroll) should be discarded
START_TEST(test_mouse_button_63)
{
    void *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = ik_input_parser_create(ctx);
    ik_input_action_t action;

    // Button 63: ESC [ < 63 ; 1 ; 1 M
    const char sequence[] = "\x1b[<63;1;1M";

    for (size_t i = 0; i < sizeof(sequence) - 1; i++) {
        ik_input_parse_byte(parser, sequence[i], &action);
    }

    // Should return UNKNOWN for non-scroll buttons
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    talloc_free(ctx);
}

END_TEST
// Test: Mouse button 66 (not scroll) should be discarded
START_TEST(test_mouse_button_66)
{
    void *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = ik_input_parser_create(ctx);
    ik_input_action_t action;

    // Button 66: ESC [ < 66 ; 1 ; 1 M
    const char sequence[] = "\x1b[<66;1;1M";

    for (size_t i = 0; i < sizeof(sequence) - 1; i++) {
        ik_input_parse_byte(parser, sequence[i], &action);
    }

    // Should return UNKNOWN for non-scroll buttons
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    talloc_free(ctx);
}

END_TEST
// Test: Mouse button 60 (b0='6' but b1='0', not scroll) should be discarded
START_TEST(test_mouse_button_60)
{
    void *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = ik_input_parser_create(ctx);
    ik_input_action_t action;

    // Button 60: ESC [ < 60 ; 1 ; 1 M
    const char sequence[] = "\x1b[<60;1;1M";

    for (size_t i = 0; i < sizeof(sequence) - 1; i++) {
        ik_input_parse_byte(parser, sequence[i], &action);
    }

    // Should return UNKNOWN for non-scroll buttons (tests b0=='6' && b1!='4' branch)
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    talloc_free(ctx);
}

END_TEST
// Test: Mouse button 62 (b0='6' but b1='2', not scroll) should be discarded
START_TEST(test_mouse_button_62)
{
    void *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = ik_input_parser_create(ctx);
    ik_input_action_t action;

    // Button 62: ESC [ < 62 ; 1 ; 1 M
    const char sequence[] = "\x1b[<62;1;1M";

    for (size_t i = 0; i < sizeof(sequence) - 1; i++) {
        ik_input_parse_byte(parser, sequence[i], &action);
    }

    // Should return UNKNOWN for non-scroll buttons (tests b0=='6' && b1!='5' branch)
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    talloc_free(ctx);
}

END_TEST

// Create test suite
static Suite *mouse_scroll_suite(void)
{
    Suite *s = suite_create("Input Mouse Scroll");

    TCase *tc_parse = tcase_create("Parsing");
    tcase_set_timeout(tc_parse, 30);
    tcase_set_timeout(tc_parse, 30);
    tcase_set_timeout(tc_parse, 30);
    tcase_set_timeout(tc_parse, 30);
    tcase_set_timeout(tc_parse, 30);
    tcase_add_test(tc_parse, test_mouse_scroll_up_parsing);
    tcase_add_test(tc_parse, test_mouse_scroll_down_parsing);
    tcase_add_test(tc_parse, test_mouse_sequence_missing_separator);
    tcase_add_test(tc_parse, test_mouse_click_discarded);
    tcase_add_test(tc_parse, test_mouse_release_event);
    tcase_add_test(tc_parse, test_mouse_single_digit_button);
    tcase_add_test(tc_parse, test_mouse_triple_digit_button);
    tcase_add_test(tc_parse, test_mouse_button_63);
    tcase_add_test(tc_parse, test_mouse_button_66);
    tcase_add_test(tc_parse, test_mouse_button_60);
    tcase_add_test(tc_parse, test_mouse_button_62);
    suite_add_tcase(s, tc_parse);

    return s;
}

int main(void)
{
    Suite *s = mouse_scroll_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
