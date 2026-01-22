// Input parser module unit tests - CSI u edge cases
#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "../../../src/input.h"
#include "../../../src/error.h"
#include "../../test_utils_helper.h"

// Test: CSI u sequence too short should be ignored
START_TEST(test_csi_u_too_short) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = ik_input_parser_create(ctx);
    ik_input_action_t action;

    // ESC[u = too short (esc_len == 1, needs >= 3)
    const char seq[] = "\x1b[u";
    for (size_t i = 0; i < sizeof(seq) - 1; i++) {
        ik_input_parse_byte(parser, seq[i], &action);
    }

    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    talloc_free(ctx);
}
END_TEST
// Test: Sequence ending with 'u' but not valid CSI u
START_TEST(test_csi_u_invalid_not_ending_with_u) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = ik_input_parser_create(ctx);
    ik_input_action_t action;

    // ESC[97x = not ending with 'u'
    const char seq[] = "\x1b[97x";
    for (size_t i = 0; i < sizeof(seq) - 1; i++) {
        ik_input_parse_byte(parser, seq[i], &action);
    }

    // Should be treated as unknown CSI sequence
    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    talloc_free(ctx);
}

END_TEST
// Test: CSI u with modified Tab should return UNKNOWN
START_TEST(test_csi_u_modified_tab_unknown) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = ik_input_parser_create(ctx);
    ik_input_action_t action;

    // ESC[9;5u = Tab with Ctrl (modified Tab not currently handled)
    const char seq[] = "\x1b[9;5u";
    for (size_t i = 0; i < sizeof(seq) - 1; i++) {
        ik_input_parse_byte(parser, seq[i], &action);
    }

    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    talloc_free(ctx);
}

END_TEST
// Test: CSI u with modifier should return UNKNOWN
START_TEST(test_csi_u_modified_key_unknown) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = ik_input_parser_create(ctx);
    ik_input_action_t action;

    // ESC[97;4u = 'a' with Alt modifier (not handled, should be UNKNOWN)
    const char seq[] = "\x1b[97;4u";
    for (size_t i = 0; i < sizeof(seq) - 1; i++) {
        ik_input_parse_byte(parser, seq[i], &action);
    }

    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    talloc_free(ctx);
}

END_TEST
// Test: CSI u with Ctrl+C but wrong keycode should return UNKNOWN
START_TEST(test_csi_u_ctrl_wrong_keycode) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = ik_input_parser_create(ctx);
    ik_input_action_t action;

    // ESC[98;5u = 'b' with Ctrl (not Ctrl+C, should be UNKNOWN)
    const char seq[] = "\x1b[98;5u";
    for (size_t i = 0; i < sizeof(seq) - 1; i++) {
        ik_input_parse_byte(parser, seq[i], &action);
    }

    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    talloc_free(ctx);
}

END_TEST
// Test: CSI u with 'c' keycode but wrong modifier (not Ctrl) should return UNKNOWN
START_TEST(test_csi_u_c_wrong_modifier) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = ik_input_parser_create(ctx);
    ik_input_action_t action;

    // ESC[99;6u = 'c' with Shift+Ctrl (modifiers=6, not just Ctrl=5)
    const char seq[] = "\x1b[99;6u";
    for (size_t i = 0; i < sizeof(seq) - 1; i++) {
        ik_input_parse_byte(parser, seq[i], &action);
    }

    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    talloc_free(ctx);
}

END_TEST
// Test: CSI u with modified Backspace should return UNKNOWN
START_TEST(test_csi_u_modified_backspace) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = ik_input_parser_create(ctx);
    ik_input_action_t action;

    // ESC[127;2u = Backspace with Shift (not handled, should be UNKNOWN)
    const char seq[] = "\x1b[127;2u";
    for (size_t i = 0; i < sizeof(seq) - 1; i++) {
        ik_input_parse_byte(parser, seq[i], &action);
    }

    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    talloc_free(ctx);
}

END_TEST
// Test: CSI u with modified Escape should return UNKNOWN
START_TEST(test_csi_u_modified_escape) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = ik_input_parser_create(ctx);
    ik_input_action_t action;

    // ESC[27;2u = Escape with Shift (not handled, should be UNKNOWN)
    const char seq[] = "\x1b[27;2u";
    for (size_t i = 0; i < sizeof(seq) - 1; i++) {
        ik_input_parse_byte(parser, seq[i], &action);
    }

    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    talloc_free(ctx);
}

END_TEST
// Test: CSI u with Unicode and modifiers should return UNKNOWN
START_TEST(test_csi_u_unicode_with_modifiers) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = ik_input_parser_create(ctx);
    ik_input_action_t action;

    // ESC[233;2u = 'é' with Shift modifier (not handled, should be UNKNOWN)
    const char seq[] = "\x1b[233;2u";
    for (size_t i = 0; i < sizeof(seq) - 1; i++) {
        ik_input_parse_byte(parser, seq[i], &action);
    }

    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    talloc_free(ctx);
}

END_TEST
// Test: CSI u with large Unicode codepoint (4-digit hex)
START_TEST(test_csi_u_large_unicode) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = ik_input_parser_create(ctx);
    ik_input_action_t action;

    // ESC[8364;1u = € symbol (U+20AC = 8364 decimal)
    const char seq[] = "\x1b[8364;1u";
    for (size_t i = 0; i < sizeof(seq) - 1; i++) {
        ik_input_parse_byte(parser, seq[i], &action);
    }

    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 8364);

    talloc_free(ctx);
}

END_TEST
// Test: CSI u with codepoint beyond valid Unicode range
START_TEST(test_csi_u_beyond_unicode_range) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = ik_input_parser_create(ctx);
    ik_input_action_t action;

    // ESC[1114112;1u = beyond U+10FFFF (max valid Unicode)
    const char seq[] = "\x1b[1114112;1u";
    for (size_t i = 0; i < sizeof(seq) - 1; i++) {
        ik_input_parse_byte(parser, seq[i], &action);
    }

    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    talloc_free(ctx);
}

END_TEST

// Test suite
static Suite *input_csi_u_edge_suite(void)
{
    Suite *s = suite_create("Input CSI u Edge Cases");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    tcase_add_test(tc_core, test_csi_u_too_short);
    tcase_add_test(tc_core, test_csi_u_invalid_not_ending_with_u);
    tcase_add_test(tc_core, test_csi_u_modified_tab_unknown);
    tcase_add_test(tc_core, test_csi_u_modified_key_unknown);
    tcase_add_test(tc_core, test_csi_u_ctrl_wrong_keycode);
    tcase_add_test(tc_core, test_csi_u_c_wrong_modifier);
    tcase_add_test(tc_core, test_csi_u_modified_backspace);
    tcase_add_test(tc_core, test_csi_u_modified_escape);
    tcase_add_test(tc_core, test_csi_u_unicode_with_modifiers);
    tcase_add_test(tc_core, test_csi_u_large_unicode);
    tcase_add_test(tc_core, test_csi_u_beyond_unicode_range);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = input_csi_u_edge_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/input/csi_u_edge_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
