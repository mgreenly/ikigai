#include <check.h>
#include <string.h>

#include "../../../src/ansi.h"

// Test: IK_ANSI_RESET macro equals correct escape sequence
START_TEST(test_ansi_reset_macro) {
    const char *expected = "\x1b[0m";
    ck_assert_str_eq(IK_ANSI_RESET, expected);
}
END_TEST

// Test: ik_ansi_fg_256() produces correct sequence for gray subdued (242)
START_TEST(test_ansi_fg_256_gray_subdued) {
    char buf[12];
    size_t written = ik_ansi_fg_256(buf, sizeof(buf), 242);

    ck_assert_uint_eq(written, 11);  // \x1b[38;5;242m = 11 bytes (excluding null)
    ck_assert_str_eq(buf, "\x1b[38;5;242m");
}
END_TEST

// Test: ik_ansi_fg_256() produces correct sequence for gray light (249)
START_TEST(test_ansi_fg_256_gray_light) {
    char buf[12];
    size_t written = ik_ansi_fg_256(buf, sizeof(buf), 249);

    ck_assert_uint_eq(written, 11);  // \x1b[38;5;249m = 11 bytes (excluding null)
    ck_assert_str_eq(buf, "\x1b[38;5;249m");
}
END_TEST

// Test: ik_ansi_fg_256() produces correct sequence for single digit color (0)
START_TEST(test_ansi_fg_256_single_digit) {
    char buf[12];
    size_t written = ik_ansi_fg_256(buf, sizeof(buf), 0);

    ck_assert_uint_eq(written, 9);  // \x1b[38;5;0m = 9 bytes (excluding null)
    ck_assert_str_eq(buf, "\x1b[38;5;0m");
}
END_TEST

// Test: ik_ansi_fg_256() produces correct sequence for max color (255)
START_TEST(test_ansi_fg_256_max_color) {
    char buf[12];
    size_t written = ik_ansi_fg_256(buf, sizeof(buf), 255);

    ck_assert_uint_eq(written, 11);  // \x1b[38;5;255m = 11 bytes (excluding null)
    ck_assert_str_eq(buf, "\x1b[38;5;255m");
}
END_TEST

// Test: ik_ansi_fg_256() returns 0 if buffer too small
START_TEST(test_ansi_fg_256_buffer_too_small) {
    char buf[8];  // Not enough space for 11 bytes + null
    size_t written = ik_ansi_fg_256(buf, sizeof(buf), 242);

    ck_assert_uint_eq(written, 0);  // Should return 0 on error
}
END_TEST

// Test: ik_ansi_fg_256() handles buffer exactly the right size
START_TEST(test_ansi_fg_256_exact_buffer_size) {
    char buf[10];  // Exactly enough for single digit + null
    size_t written = ik_ansi_fg_256(buf, sizeof(buf), 0);

    ck_assert_uint_eq(written, 9);  // \x1b[38;5;0m = 9 bytes (excluding null)
    ck_assert_str_eq(buf, "\x1b[38;5;0m");
}
END_TEST

// Test: ik_ansi_fg_256() handles two-digit color
START_TEST(test_ansi_fg_256_two_digit) {
    char buf[12];
    size_t written = ik_ansi_fg_256(buf, sizeof(buf), 42);

    ck_assert_uint_eq(written, 10);  // \x1b[38;5;42m = 10 bytes (excluding null)
    ck_assert_str_eq(buf, "\x1b[38;5;42m");
}
END_TEST

// Test: Color constants have correct values
START_TEST(test_ansi_color_constants) {
    ck_assert_uint_eq(IK_ANSI_GRAY_SUBDUED, 242);
    ck_assert_uint_eq(IK_ANSI_GRAY_LIGHT, 249);
}
END_TEST

static Suite *ansi_color_suite(void) {
    Suite *s;
    TCase *tc_core;

    s = suite_create("ANSI Color");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_ansi_reset_macro);
    tcase_add_test(tc_core, test_ansi_fg_256_gray_subdued);
    tcase_add_test(tc_core, test_ansi_fg_256_gray_light);
    tcase_add_test(tc_core, test_ansi_fg_256_single_digit);
    tcase_add_test(tc_core, test_ansi_fg_256_max_color);
    tcase_add_test(tc_core, test_ansi_fg_256_buffer_too_small);
    tcase_add_test(tc_core, test_ansi_fg_256_exact_buffer_size);
    tcase_add_test(tc_core, test_ansi_fg_256_two_digit);
    tcase_add_test(tc_core, test_ansi_color_constants);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void) {
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = ansi_color_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
