// cursor position calculation unit tests
#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "../../../src/render.h"
#include "../../../src/error.h"
#include "../../test_utils.h"

// Internal function for testing cursor position calculation
typedef struct {
    int32_t screen_row;
    int32_t screen_col;
} cursor_screen_pos_t;

res_t calculate_cursor_screen_position(void *ctx,
                                       const char *text,
                                       size_t text_len,
                                       size_t cursor_byte_offset,
                                       int32_t terminal_width,
                                       cursor_screen_pos_t *pos_out);

// Test: simple ASCII positioning
START_TEST(test_cursor_position_simple_ascii) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *text = "hello";
    cursor_screen_pos_t pos = {0};

    res_t res = calculate_cursor_screen_position(ctx, text, 5, 3, 80, &pos);

    ck_assert(is_ok(&res));
    ck_assert_int_eq(pos.screen_row, 0);
    ck_assert_int_eq(pos.screen_col, 3);

    talloc_free(ctx);
}
END_TEST
// Test: cursor at start
START_TEST(test_cursor_position_at_start) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *text = "hello world";
    cursor_screen_pos_t pos = {0};

    res_t res = calculate_cursor_screen_position(ctx, text, 11, 0, 80, &pos);

    ck_assert(is_ok(&res));
    ck_assert_int_eq(pos.screen_row, 0);
    ck_assert_int_eq(pos.screen_col, 0);

    talloc_free(ctx);
}

END_TEST
// Test: cursor at end
START_TEST(test_cursor_position_at_end) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *text = "hello";
    cursor_screen_pos_t pos = {0};

    res_t res = calculate_cursor_screen_position(ctx, text, 5, 5, 80, &pos);

    ck_assert(is_ok(&res));
    ck_assert_int_eq(pos.screen_row, 0);
    ck_assert_int_eq(pos.screen_col, 5);

    talloc_free(ctx);
}

END_TEST
// Test: cursor position with newline
START_TEST(test_cursor_position_with_newline) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *text = "hi\nworld";
    cursor_screen_pos_t pos = {0};

    // Cursor at byte 5: "hi\nwo|rld"
    res_t res = calculate_cursor_screen_position(ctx, text, 8, 5, 80, &pos);

    ck_assert(is_ok(&res));
    ck_assert_int_eq(pos.screen_row, 1);
    ck_assert_int_eq(pos.screen_col, 2);

    talloc_free(ctx);
}

END_TEST
// Test: cursor position with wrapping
START_TEST(test_cursor_position_wrapping) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    // 10 char line, terminal width 8 -> wraps at 8
    const char *text = "abcdefghij";
    cursor_screen_pos_t pos = {0};

    // Cursor at byte 9 -> should be on row 1, col 1
    res_t res = calculate_cursor_screen_position(ctx, text, 10, 9, 8, &pos);

    ck_assert(is_ok(&res));
    ck_assert_int_eq(pos.screen_row, 1);
    ck_assert_int_eq(pos.screen_col, 1);

    talloc_free(ctx);
}

END_TEST
// Test: cursor position with CJK wide characters
START_TEST(test_cursor_position_cjk_wide_chars) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    // "你好" - each char is 3 bytes UTF-8, 2 cells width
    const char *text = "\xe4\xbd\xa0\xe5\xa5\xbd"; // "你好"
    cursor_screen_pos_t pos = {0};

    // Cursor after first char (3 bytes) -> screen col should be 2
    res_t res = calculate_cursor_screen_position(ctx, text, 6, 3, 80, &pos);

    ck_assert(is_ok(&res));
    ck_assert_int_eq(pos.screen_row, 0);
    ck_assert_int_eq(pos.screen_col, 2);

    talloc_free(ctx);
}

END_TEST
// Test: cursor position with emoji
START_TEST(test_cursor_position_emoji) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    // "hello 😀 world" - emoji is 4 bytes, 2 cells width
    const char *text = "hello \xf0\x9f\x98\x80 world";
    cursor_screen_pos_t pos = {0};

    // Cursor after space after emoji (at byte 11: "hello 😀 |world")
    // "hello " = 6 bytes, emoji = 4 bytes, space = 1 byte -> byte 11 is before 'w'
    res_t res = calculate_cursor_screen_position(ctx, text, 17, 11, 80, &pos);

    ck_assert(is_ok(&res));
    ck_assert_int_eq(pos.screen_row, 0);
    ck_assert_int_eq(pos.screen_col, 9); // "hello " (6) + emoji (2) + " " (1) = 9

    talloc_free(ctx);
}

END_TEST
// Test: cursor position with combining characters
START_TEST(test_cursor_position_combining_chars) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    // "e" + combining acute accent (0-width)
    const char *text = "e\xcc\x81";  // é as e + combining acute
    cursor_screen_pos_t pos = {0};

    // Cursor after combining char
    res_t res = calculate_cursor_screen_position(ctx, text, 3, 3, 80, &pos);

    ck_assert(is_ok(&res));
    ck_assert_int_eq(pos.screen_row, 0);
    ck_assert_int_eq(pos.screen_col, 1); // e (1) + combining (0) = 1

    talloc_free(ctx);
}

END_TEST
// Test: cursor position with control character
START_TEST(test_cursor_position_control_char) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    // DELETE character (0x7F) - control character handling
    const char text[] = "hello\x7Fworld";
    cursor_screen_pos_t pos = {0};

    // Cursor after DELETE char (byte 6)
    res_t res = calculate_cursor_screen_position(ctx, text, 11, 6, 80, &pos);

    ck_assert(is_ok(&res));
    ck_assert_int_eq(pos.screen_row, 0);
    ck_assert_int_eq(pos.screen_col, 5); // "hello" (5) + DELETE (0 width) = 5

    talloc_free(ctx);
}

END_TEST
// Test: cursor position at wrap boundary
START_TEST(test_cursor_position_wrap_boundary) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    // Exactly 8 chars, terminal width 8
    const char *text = "abcdefgh";
    cursor_screen_pos_t pos = {0};

    // Cursor at byte 8 (end of line, exactly at boundary)
    res_t res = calculate_cursor_screen_position(ctx, text, 8, 8, 8, &pos);

    ck_assert(is_ok(&res));
    ck_assert_int_eq(pos.screen_row, 1);
    ck_assert_int_eq(pos.screen_col, 0);

    talloc_free(ctx);
}

END_TEST
// Test: cursor position with invalid UTF-8
START_TEST(test_cursor_position_invalid_utf8) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    // Invalid UTF-8 sequence
    const char *text = "hello\xff\xfe";
    cursor_screen_pos_t pos = {0};

    res_t res = calculate_cursor_screen_position(ctx, text, 7, 7, 80, &pos);

    // Should return error on invalid UTF-8
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_INVALID_ARG);

    talloc_free(ctx);
}

END_TEST
// Test: cursor position after SGR escape sequence
START_TEST(test_cursor_position_ansi_sgr_reset) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    // "\x1b[0mhello" - cursor at byte 4 (after SGR reset) should be at screen col 0
    const char *text = "\x1b[0mhello";
    cursor_screen_pos_t pos = {0};

    res_t res = calculate_cursor_screen_position(ctx, text, 9, 4, 80, &pos);

    ck_assert(is_ok(&res));
    ck_assert_int_eq(pos.screen_row, 0);
    ck_assert_int_eq(pos.screen_col, 0);

    talloc_free(ctx);
}

END_TEST
// Test: cursor position with SGR prefix (256-color)
START_TEST(test_cursor_position_ansi_sgr_256_color) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    // "\x1b[38;5;242mtext" - cursor at byte 11 (after 256-color SGR) should be at screen col 0
    const char *text = "\x1b[38;5;242mtext";
    cursor_screen_pos_t pos = {0};

    res_t res = calculate_cursor_screen_position(ctx, text, 15, 11, 80, &pos);

    ck_assert(is_ok(&res));
    ck_assert_int_eq(pos.screen_row, 0);
    ck_assert_int_eq(pos.screen_col, 0);

    talloc_free(ctx);
}

END_TEST
// Test: cursor in middle of colored text
START_TEST(test_cursor_position_ansi_middle_colored) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    // "\x1b[38;5;242mhello" - cursor at byte 13 (after "he") should be at screen col 2
    const char *text = "\x1b[38;5;242mhello";
    cursor_screen_pos_t pos = {0};

    res_t res = calculate_cursor_screen_position(ctx, text, 16, 13, 80, &pos);

    ck_assert(is_ok(&res));
    ck_assert_int_eq(pos.screen_row, 0);
    ck_assert_int_eq(pos.screen_col, 2);

    talloc_free(ctx);
}

END_TEST
// Test: cursor after multiple escape sequences
START_TEST(test_cursor_position_ansi_multiple) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    // "\x1b[0m\x1b[38;5;242mtext" - cursor at byte 15 (after both escapes) should be at screen col 0
    const char *text = "\x1b[0m\x1b[38;5;242mtext";
    cursor_screen_pos_t pos = {0};

    res_t res = calculate_cursor_screen_position(ctx, text, 19, 15, 80, &pos);

    ck_assert(is_ok(&res));
    ck_assert_int_eq(pos.screen_row, 0);
    ck_assert_int_eq(pos.screen_col, 0);

    talloc_free(ctx);
}

END_TEST

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
// Test: NULL ctx asserts
START_TEST(test_cursor_position_null_ctx_asserts) {
    const char *text = "hello";
    cursor_screen_pos_t pos = {0};
    calculate_cursor_screen_position(NULL, text, 5, 0, 80, &pos);
}

END_TEST
// Test: NULL text asserts
START_TEST(test_cursor_position_null_text_asserts) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    cursor_screen_pos_t pos = {0};
    calculate_cursor_screen_position(ctx, NULL, 0, 0, 80, &pos);
    talloc_free(ctx);
}

END_TEST
// Test: NULL pos_out asserts
START_TEST(test_cursor_position_null_pos_out_asserts) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *text = "hello";
    calculate_cursor_screen_position(ctx, text, 5, 0, 80, NULL);
    talloc_free(ctx);
}

END_TEST
#endif

// Test suite
static Suite *cursor_position_suite(void)
{
    Suite *s = suite_create("RenderDirect_CursorPosition");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    tcase_add_test(tc_core, test_cursor_position_simple_ascii);
    tcase_add_test(tc_core, test_cursor_position_at_start);
    tcase_add_test(tc_core, test_cursor_position_at_end);
    tcase_add_test(tc_core, test_cursor_position_with_newline);
    tcase_add_test(tc_core, test_cursor_position_wrapping);
    tcase_add_test(tc_core, test_cursor_position_cjk_wide_chars);
    tcase_add_test(tc_core, test_cursor_position_emoji);
    tcase_add_test(tc_core, test_cursor_position_combining_chars);
    tcase_add_test(tc_core, test_cursor_position_control_char);
    tcase_add_test(tc_core, test_cursor_position_wrap_boundary);
    tcase_add_test(tc_core, test_cursor_position_invalid_utf8);
    tcase_add_test(tc_core, test_cursor_position_ansi_sgr_reset);
    tcase_add_test(tc_core, test_cursor_position_ansi_sgr_256_color);
    tcase_add_test(tc_core, test_cursor_position_ansi_middle_colored);
    tcase_add_test(tc_core, test_cursor_position_ansi_multiple);

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
    tcase_add_test_raise_signal(tc_core, test_cursor_position_null_ctx_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_core, test_cursor_position_null_text_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_core, test_cursor_position_null_pos_out_asserts, SIGABRT);
#endif

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = cursor_position_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
