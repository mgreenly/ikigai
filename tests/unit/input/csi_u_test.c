// Input parser module unit tests - CSI u sequence tests
#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "../../../src/input.h"
#include "../../../src/error.h"
#include "../../test_utils.h"

// Test: CSI u plain Enter emits NEWLINE (submit)
START_TEST(test_csi_u_plain_enter) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = ik_input_parser_create(ctx);
    ik_input_action_t action;

    // ESC[13;1u = plain Enter
    const char seq[] = "\x1b[13;1u";
    for (size_t i = 0; i < sizeof(seq) - 1; i++) {
        ik_input_parse_byte(parser, seq[i], &action);
    }

    ck_assert_int_eq(action.type, IK_INPUT_NEWLINE);

    talloc_free(ctx);
}
END_TEST
// Test: CSI u Shift+Enter emits INSERT_NEWLINE
START_TEST(test_csi_u_shift_enter) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = ik_input_parser_create(ctx);
    ik_input_action_t action;

    // ESC[13;2u = Shift+Enter
    const char seq[] = "\x1b[13;2u";
    for (size_t i = 0; i < sizeof(seq) - 1; i++) {
        ik_input_parse_byte(parser, seq[i], &action);
    }

    ck_assert_int_eq(action.type, IK_INPUT_INSERT_NEWLINE);

    talloc_free(ctx);
}

END_TEST
// Test: CSI u Ctrl+Enter emits INSERT_NEWLINE
START_TEST(test_csi_u_ctrl_enter) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = ik_input_parser_create(ctx);
    ik_input_action_t action;

    // ESC[13;5u = Ctrl+Enter
    const char seq[] = "\x1b[13;5u";
    for (size_t i = 0; i < sizeof(seq) - 1; i++) {
        ik_input_parse_byte(parser, seq[i], &action);
    }

    ck_assert_int_eq(action.type, IK_INPUT_INSERT_NEWLINE);

    talloc_free(ctx);
}

END_TEST
// Test: CSI u Alt+Enter emits INSERT_NEWLINE
START_TEST(test_csi_u_alt_enter) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = ik_input_parser_create(ctx);
    ik_input_action_t action;

    // ESC[13;3u = Alt+Enter
    const char seq[] = "\x1b[13;3u";
    for (size_t i = 0; i < sizeof(seq) - 1; i++) {
        ik_input_parse_byte(parser, seq[i], &action);
    }

    ck_assert_int_eq(action.type, IK_INPUT_INSERT_NEWLINE);

    talloc_free(ctx);
}

END_TEST
// Test: CSI u Ctrl+Shift+Enter emits INSERT_NEWLINE
START_TEST(test_csi_u_ctrl_shift_enter) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = ik_input_parser_create(ctx);
    ik_input_action_t action;

    // ESC[13;6u = Ctrl+Shift+Enter (1 + 1 + 4)
    const char seq[] = "\x1b[13;6u";
    for (size_t i = 0; i < sizeof(seq) - 1; i++) {
        ik_input_parse_byte(parser, seq[i], &action);
    }

    ck_assert_int_eq(action.type, IK_INPUT_INSERT_NEWLINE);

    talloc_free(ctx);
}

END_TEST
// Test: Alacritty modifier-only events are ignored
START_TEST(test_csi_u_modifier_only_ignored) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = ik_input_parser_create(ctx);
    ik_input_action_t action;

    // ESC[57441;2u = Shift key alone (Alacritty)
    const char seq[] = "\x1b[57441;2u";
    for (size_t i = 0; i < sizeof(seq) - 1; i++) {
        ik_input_parse_byte(parser, seq[i], &action);
    }

    ck_assert_int_eq(action.type, IK_INPUT_UNKNOWN);

    talloc_free(ctx);
}

END_TEST
// Test: Ctrl+J still works (not CSI u)
START_TEST(test_ctrl_j_still_works) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = ik_input_parser_create(ctx);
    ik_input_action_t action;

    // Ctrl+J = 0x0A (LF)
    ik_input_parse_byte(parser, 0x0A, &action);

    ck_assert_int_eq(action.type, IK_INPUT_INSERT_NEWLINE);

    talloc_free(ctx);
}

END_TEST
// Test: CSI u character 'a' (keycode 97)
START_TEST(test_csi_u_char_a) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = ik_input_parser_create(ctx);
    ik_input_action_t action;

    // ESC[97;1u = 'a' with no modifiers
    const char seq[] = "\x1b[97;1u";
    for (size_t i = 0; i < sizeof(seq) - 1; i++) {
        ik_input_parse_byte(parser, seq[i], &action);
    }

    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 97);  // 'a'

    talloc_free(ctx);
}

END_TEST
// Test: CSI u space character (keycode 32)
START_TEST(test_csi_u_char_space) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = ik_input_parser_create(ctx);
    ik_input_action_t action;

    // ESC[32;1u = space with no modifiers
    const char seq[] = "\x1b[32;1u";
    for (size_t i = 0; i < sizeof(seq) - 1; i++) {
        ik_input_parse_byte(parser, seq[i], &action);
    }

    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 32);  // space

    talloc_free(ctx);
}

END_TEST
// Test: CSI u Tab key (keycode 9)
START_TEST(test_csi_u_tab) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = ik_input_parser_create(ctx);
    ik_input_action_t action;

    // ESC[9;1u = Tab with no modifiers
    const char seq[] = "\x1b[9;1u";
    for (size_t i = 0; i < sizeof(seq) - 1; i++) {
        ik_input_parse_byte(parser, seq[i], &action);
    }

    ck_assert_int_eq(action.type, IK_INPUT_TAB);

    talloc_free(ctx);
}

END_TEST
// Test: CSI u Backspace key (keycode 127)
START_TEST(test_csi_u_backspace) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = ik_input_parser_create(ctx);
    ik_input_action_t action;

    // ESC[127;1u = Backspace with no modifiers
    const char seq[] = "\x1b[127;1u";
    for (size_t i = 0; i < sizeof(seq) - 1; i++) {
        ik_input_parse_byte(parser, seq[i], &action);
    }

    ck_assert_int_eq(action.type, IK_INPUT_BACKSPACE);

    talloc_free(ctx);
}

END_TEST
// Test: CSI u Escape key (keycode 27)
START_TEST(test_csi_u_escape) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = ik_input_parser_create(ctx);
    ik_input_action_t action;

    // ESC[27;1u = Escape with no modifiers
    const char seq[] = "\x1b[27;1u";
    for (size_t i = 0; i < sizeof(seq) - 1; i++) {
        ik_input_parse_byte(parser, seq[i], &action);
    }

    ck_assert_int_eq(action.type, IK_INPUT_ESCAPE);

    talloc_free(ctx);
}

END_TEST
// Test: CSI u Unicode character (e.g., é = U+00E9 = 233)
START_TEST(test_csi_u_unicode) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = ik_input_parser_create(ctx);
    ik_input_action_t action;

    // ESC[233;1u = 'é' with no modifiers
    const char seq[] = "\x1b[233;1u";
    for (size_t i = 0; i < sizeof(seq) - 1; i++) {
        ik_input_parse_byte(parser, seq[i], &action);
    }

    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 233);  // é

    talloc_free(ctx);
}

END_TEST
// Test: CSI u Ctrl+C (keycode 99 = 'c', modifiers 5 = Ctrl)
START_TEST(test_csi_u_ctrl_c) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = ik_input_parser_create(ctx);
    ik_input_action_t action;

    // ESC[99;5u = Ctrl+C (keycode 99 = 'c', modifiers 5 = 1 + 4 = Ctrl)
    const char seq[] = "\x1b[99;5u";
    for (size_t i = 0; i < sizeof(seq) - 1; i++) {
        ik_input_parse_byte(parser, seq[i], &action);
    }

    ck_assert_int_eq(action.type, IK_INPUT_CTRL_C);

    talloc_free(ctx);
}

END_TEST
// Test: Legacy Ctrl+C (0x03) still works
START_TEST(test_legacy_ctrl_c) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = ik_input_parser_create(ctx);
    ik_input_action_t action;

    // Legacy Ctrl+C = 0x03
    ik_input_parse_byte(parser, 0x03, &action);

    ck_assert_int_eq(action.type, IK_INPUT_CTRL_C);

    talloc_free(ctx);
}

END_TEST
// Test: CSI u Shift+= produces '+'
START_TEST(test_csi_u_shift_equals_produces_plus) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = ik_input_parser_create(ctx);
    ik_input_action_t action;

    // ESC[61;2u = '=' (keycode 61) with Shift modifier (2)
    // Should produce '+' after xkbcommon translation
    const char seq[] = "\x1b[61;2u";
    for (size_t i = 0; i < sizeof(seq) - 1; i++) {
        ik_input_parse_byte(parser, seq[i], &action);
    }

    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, '+');

    talloc_free(ctx);
}

END_TEST
// Test: CSI u Shift+a produces 'A'
START_TEST(test_csi_u_shift_a_produces_uppercase) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = ik_input_parser_create(ctx);
    ik_input_action_t action;

    // ESC[97;2u = 'a' (keycode 97) with Shift modifier (2)
    // Should produce 'A' after xkbcommon translation
    const char seq[] = "\x1b[97;2u";
    for (size_t i = 0; i < sizeof(seq) - 1; i++) {
        ik_input_parse_byte(parser, seq[i], &action);
    }

    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, 'A');

    talloc_free(ctx);
}

END_TEST
// Test: CSI u Shift+1 produces '!'
START_TEST(test_csi_u_shift_1_produces_exclamation) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = ik_input_parser_create(ctx);
    ik_input_action_t action;

    // ESC[49;2u = '1' (keycode 49) with Shift modifier (2)
    // Should produce '!' after xkbcommon translation
    const char seq[] = "\x1b[49;2u";
    for (size_t i = 0; i < sizeof(seq) - 1; i++) {
        ik_input_parse_byte(parser, seq[i], &action);
    }

    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, '!');

    talloc_free(ctx);
}

END_TEST
// Test: CSI u Shift+2 produces '@'
START_TEST(test_csi_u_shift_2_produces_at) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_input_parser_t *parser = ik_input_parser_create(ctx);
    ik_input_action_t action;

    // ESC[50;2u = '2' (keycode 50) with Shift modifier (2)
    // Should produce '@' after xkbcommon translation
    const char seq[] = "\x1b[50;2u";
    for (size_t i = 0; i < sizeof(seq) - 1; i++) {
        ik_input_parse_byte(parser, seq[i], &action);
    }

    ck_assert_int_eq(action.type, IK_INPUT_CHAR);
    ck_assert_uint_eq(action.codepoint, '@');

    talloc_free(ctx);
}

END_TEST

// Test suite
static Suite *input_csi_u_suite(void)
{
    Suite *s = suite_create("Input CSI u");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    tcase_add_test(tc_core, test_csi_u_plain_enter);
    tcase_add_test(tc_core, test_csi_u_shift_enter);
    tcase_add_test(tc_core, test_csi_u_ctrl_enter);
    tcase_add_test(tc_core, test_csi_u_alt_enter);
    tcase_add_test(tc_core, test_csi_u_ctrl_shift_enter);
    tcase_add_test(tc_core, test_csi_u_modifier_only_ignored);
    tcase_add_test(tc_core, test_ctrl_j_still_works);
    tcase_add_test(tc_core, test_csi_u_char_a);
    tcase_add_test(tc_core, test_csi_u_char_space);
    tcase_add_test(tc_core, test_csi_u_tab);
    tcase_add_test(tc_core, test_csi_u_backspace);
    tcase_add_test(tc_core, test_csi_u_escape);
    tcase_add_test(tc_core, test_csi_u_unicode);
    tcase_add_test(tc_core, test_csi_u_ctrl_c);
    tcase_add_test(tc_core, test_legacy_ctrl_c);
    tcase_add_test(tc_core, test_csi_u_shift_equals_produces_plus);
    tcase_add_test(tc_core, test_csi_u_shift_a_produces_uppercase);
    tcase_add_test(tc_core, test_csi_u_shift_1_produces_exclamation);
    tcase_add_test(tc_core, test_csi_u_shift_2_produces_at);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = input_csi_u_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
