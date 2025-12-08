#include <check.h>
#include <string.h>

#include "../../../src/ansi.h"

// Test: returns 0 for regular text (no escape)
START_TEST(test_ansi_skip_csi_regular_text) {
    const char *text = "Hello World";
    size_t len = strlen(text);

    // Not at escape sequence
    size_t skip = ik_ansi_skip_csi(text, len, 0);
    ck_assert_uint_eq(skip, 0);

    skip = ik_ansi_skip_csi(text, len, 5);
    ck_assert_uint_eq(skip, 0);
}
END_TEST

// Test: returns 0 for partial ESC (just \x1b)
START_TEST(test_ansi_skip_csi_partial_esc) {
    const char *text = "abc\x1b";
    size_t len = strlen(text);

    // ESC at end without '['
    size_t skip = ik_ansi_skip_csi(text, len, 3);
    ck_assert_uint_eq(skip, 0);
}
END_TEST

// Test: returns 0 for ESC without '[' (e.g., \x1bO)
START_TEST(test_ansi_skip_csi_esc_without_bracket) {
    const char *text = "\x1bOHello";
    size_t len = strlen(text);

    // ESC followed by 'O' (not CSI)
    size_t skip = ik_ansi_skip_csi(text, len, 0);
    ck_assert_uint_eq(skip, 0);
}
END_TEST

// Test: skips simple SGR: \x1b[0m (4 bytes)
START_TEST(test_ansi_skip_csi_simple_sgr) {
    const char *text = "abc\x1b[0mdef";
    size_t len = strlen(text);

    // CSI sequence at position 3
    size_t skip = ik_ansi_skip_csi(text, len, 3);
    ck_assert_uint_eq(skip, 4);  // \x1b[0m = 4 bytes
}
END_TEST

// Test: skips 256-color foreground: \x1b[38;5;242m (11 bytes)
START_TEST(test_ansi_skip_csi_256_color_fg) {
    const char *text = "abc\x1b[38;5;242mdef";
    size_t len = strlen(text);

    // CSI sequence at position 3
    size_t skip = ik_ansi_skip_csi(text, len, 3);
    ck_assert_uint_eq(skip, 11);  // \x1b[38;5;242m = 11 bytes
}
END_TEST

// Test: skips 256-color background: \x1b[48;5;249m (11 bytes)
START_TEST(test_ansi_skip_csi_256_color_bg) {
    const char *text = "abc\x1b[48;5;249mdef";
    size_t len = strlen(text);

    // CSI sequence at position 3
    size_t skip = ik_ansi_skip_csi(text, len, 3);
    ck_assert_uint_eq(skip, 11);  // \x1b[48;5;249m = 11 bytes
}
END_TEST

// Test: skips combined: \x1b[38;5;242;1m (bold + color)
START_TEST(test_ansi_skip_csi_combined) {
    const char *text = "abc\x1b[38;5;242;1mdef";
    size_t len = strlen(text);

    // CSI sequence at position 3
    size_t skip = ik_ansi_skip_csi(text, len, 3);
    ck_assert_uint_eq(skip, 13);  // \x1b[38;5;242;1m = 13 bytes
}
END_TEST

// Test: handles sequence at end of buffer
START_TEST(test_ansi_skip_csi_at_end) {
    const char *text = "abc\x1b[0m";
    size_t len = strlen(text);

    // CSI sequence at position 3 (end of buffer)
    size_t skip = ik_ansi_skip_csi(text, len, 3);
    ck_assert_uint_eq(skip, 4);  // \x1b[0m = 4 bytes
}
END_TEST

// Test: handles incomplete sequence (no terminal byte)
START_TEST(test_ansi_skip_csi_incomplete) {
    const char *text = "abc\x1b[38;5;242";
    size_t len = strlen(text);

    // CSI sequence at position 3 but no terminal 'm'
    size_t skip = ik_ansi_skip_csi(text, len, 3);
    ck_assert_uint_eq(skip, 0);  // Invalid sequence - no terminal byte
}
END_TEST

// Test: handles invalid character in sequence
START_TEST(test_ansi_skip_csi_invalid_char) {
    const char *text = "abc\x1b[38\x01""5m";
    size_t len = strlen(text);

    // CSI sequence at position 3 with invalid char \x01 (not param/intermediate/terminal)
    size_t skip = ik_ansi_skip_csi(text, len, 3);
    ck_assert_uint_eq(skip, 0);  // Invalid sequence - invalid character
}
END_TEST

// Test: handles intermediate bytes (0x20-0x2F range)
START_TEST(test_ansi_skip_csi_intermediate_bytes) {
    const char *text = "abc\x1b[ m";
    size_t len = strlen(text);

    // CSI sequence with space (0x20) as intermediate byte
    size_t skip = ik_ansi_skip_csi(text, len, 3);
    ck_assert_uint_eq(skip, 4);  // \x1b[ m = 4 bytes
}
END_TEST

// Test: handles terminal byte at upper bound (0x7E)
START_TEST(test_ansi_skip_csi_terminal_upper_bound) {
    const char *text = "abc\x1b[~";
    size_t len = strlen(text);

    // CSI sequence with tilde (0x7E) as terminal byte
    size_t skip = ik_ansi_skip_csi(text, len, 3);
    ck_assert_uint_eq(skip, 3);  // \x1b[~ = 3 bytes
}
END_TEST

// Test: handles terminal byte at lower bound (0x40)
START_TEST(test_ansi_skip_csi_terminal_lower_bound) {
    const char *text = "abc\x1b[@";
    size_t len = strlen(text);

    // CSI sequence with '@' (0x40) as terminal byte
    size_t skip = ik_ansi_skip_csi(text, len, 3);
    ck_assert_uint_eq(skip, 3);  // \x1b[@ = 3 bytes
}
END_TEST

// Test: handles character above terminal byte range (0x7F)
START_TEST(test_ansi_skip_csi_char_above_terminal) {
    const char *text = "abc\x1b[\x7F";
    size_t len = strlen(text);

    // CSI sequence with 0x7F (above terminal range) should be invalid
    size_t skip = ik_ansi_skip_csi(text, len, 3);
    ck_assert_uint_eq(skip, 0);  // Invalid sequence - char > 0x7E
}
END_TEST

static Suite *ansi_skip_suite(void) {
    Suite *s;
    TCase *tc_core;

    s = suite_create("ANSI Skip");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_ansi_skip_csi_regular_text);
    tcase_add_test(tc_core, test_ansi_skip_csi_partial_esc);
    tcase_add_test(tc_core, test_ansi_skip_csi_esc_without_bracket);
    tcase_add_test(tc_core, test_ansi_skip_csi_simple_sgr);
    tcase_add_test(tc_core, test_ansi_skip_csi_256_color_fg);
    tcase_add_test(tc_core, test_ansi_skip_csi_256_color_bg);
    tcase_add_test(tc_core, test_ansi_skip_csi_combined);
    tcase_add_test(tc_core, test_ansi_skip_csi_at_end);
    tcase_add_test(tc_core, test_ansi_skip_csi_incomplete);
    tcase_add_test(tc_core, test_ansi_skip_csi_invalid_char);
    tcase_add_test(tc_core, test_ansi_skip_csi_intermediate_bytes);
    tcase_add_test(tc_core, test_ansi_skip_csi_terminal_upper_bound);
    tcase_add_test(tc_core, test_ansi_skip_csi_terminal_lower_bound);
    tcase_add_test(tc_core, test_ansi_skip_csi_char_above_terminal);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void) {
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = ansi_skip_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
