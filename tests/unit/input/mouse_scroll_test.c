/**
 * @file mouse_scroll_test.c
 * @brief Unit tests for mouse scroll event parsing
 */

#include <check.h>
#include <talloc.h>
#include "../../../src/input.h"
#include "../../test_utils.h"

// Test: Mouse scroll up sequence ESC [ < 64 ; col ; row M should generate IK_INPUT_SCROLL_UP
START_TEST(test_mouse_scroll_up_parsing)
{
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

// Create test suite
static Suite *mouse_scroll_suite(void)
{
    Suite *s = suite_create("Input Mouse Scroll");

    TCase *tc_parse = tcase_create("Parsing");
    tcase_set_timeout(tc_parse, 30);
    tcase_add_test(tc_parse, test_mouse_scroll_up_parsing);
    tcase_add_test(tc_parse, test_mouse_scroll_down_parsing);
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
