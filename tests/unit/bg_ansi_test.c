#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

#include "apps/ikigai/bg_ansi.h"

/* ================================================================
 * Helpers
 * ================================================================ */

static char *strip(TALLOC_CTX *ctx, const char *s)
{
    return bg_ansi_strip(ctx, s, strlen(s));
}

/* ================================================================
 * Bare text (no escapes)
 * ================================================================ */

START_TEST(test_empty_input)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    char *out = bg_ansi_strip(ctx, NULL, 0);
    ck_assert_ptr_nonnull(out);
    ck_assert_str_eq(out, "");
    talloc_free(ctx);
}
END_TEST

START_TEST(test_empty_string)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    char *out = bg_ansi_strip(ctx, "", 0);
    ck_assert_str_eq(out, "");
    talloc_free(ctx);
}
END_TEST

START_TEST(test_plain_text_passthrough)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    char *out = strip(ctx, "hello world");
    ck_assert_str_eq(out, "hello world");
    talloc_free(ctx);
}
END_TEST

START_TEST(test_plain_text_with_newlines)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    char *out = strip(ctx, "line1\nline2\nline3\n");
    ck_assert_str_eq(out, "line1\nline2\nline3\n");
    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * SGR — color and style codes
 * ================================================================ */

START_TEST(test_sgr_bold)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    /* bold: \x1b[1m */
    char *out = strip(ctx, "\x1b[1mhello\x1b[0m");
    ck_assert_str_eq(out, "hello");
    talloc_free(ctx);
}
END_TEST

START_TEST(test_sgr_colors)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    /* red foreground, reset */
    char *out = strip(ctx, "\x1b[31merror\x1b[0m");
    ck_assert_str_eq(out, "error");
    talloc_free(ctx);
}
END_TEST

START_TEST(test_sgr_256_color)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    /* 256-color: \x1b[38;5;196m (bright red) */
    char *out = strip(ctx, "\x1b[38;5;196mtext\x1b[0m");
    ck_assert_str_eq(out, "text");
    talloc_free(ctx);
}
END_TEST

START_TEST(test_sgr_truecolor)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    /* true color: \x1b[38;2;255;0;0m */
    char *out = strip(ctx, "\x1b[38;2;255;0;0mred text\x1b[0m");
    ck_assert_str_eq(out, "red text");
    talloc_free(ctx);
}
END_TEST

START_TEST(test_sgr_multiple_params)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    /* bold + underline + color */
    char *out = strip(ctx, "\x1b[1;4;32mgreen bold underline\x1b[0m");
    ck_assert_str_eq(out, "green bold underline");
    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * Cursor movement
 * ================================================================ */

START_TEST(test_cursor_up)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    /* cursor up 3 lines */
    char *out = strip(ctx, "before\x1b[3Aafter");
    ck_assert_str_eq(out, "beforeafter");
    talloc_free(ctx);
}
END_TEST

START_TEST(test_cursor_down)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    char *out = strip(ctx, "x\x1b[2By");
    ck_assert_str_eq(out, "xy");
    talloc_free(ctx);
}
END_TEST

START_TEST(test_cursor_forward_back)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    char *out = strip(ctx, "\x1b[5Chello\x1b[3D");
    ck_assert_str_eq(out, "hello");
    talloc_free(ctx);
}
END_TEST

START_TEST(test_cursor_position)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    /* \x1b[row;colH */
    char *out = strip(ctx, "\x1b[10;20Htext");
    ck_assert_str_eq(out, "text");
    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * Erase sequences
 * ================================================================ */

START_TEST(test_erase_display)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    /* \x1b[2J — erase entire display */
    char *out = strip(ctx, "before\x1b[2Jafter");
    ck_assert_str_eq(out, "beforeafter");
    talloc_free(ctx);
}
END_TEST

START_TEST(test_erase_line)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    /* \x1b[K — erase to end of line */
    char *out = strip(ctx, "keep\x1b[Kmore");
    ck_assert_str_eq(out, "keepmore");
    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * OSC sequences
 * ================================================================ */

START_TEST(test_osc_bel_terminator)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    /* OSC 0 — set window title, BEL terminated */
    char *out = strip(ctx, "\x1b]0;My Title\x07text");
    ck_assert_str_eq(out, "text");
    talloc_free(ctx);
}
END_TEST

START_TEST(test_osc_st_terminator)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    /* OSC terminated by ST: \x1b\ */
    char *out = strip(ctx, "\x1b]0;title\x1b\\after");
    ck_assert_str_eq(out, "after");
    talloc_free(ctx);
}
END_TEST

START_TEST(test_osc_hyperlink)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    /* OSC 8 hyperlink */
    char *out = strip(ctx,
        "\x1b]8;;https://example.com\x07link text\x1b]8;;\x07");
    ck_assert_str_eq(out, "link text");
    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * Two-character escape sequences
 * ================================================================ */

START_TEST(test_two_char_reverse_index)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    /* \x1bM — reverse index (scroll up) */
    char *out = strip(ctx, "a\x1bMb");
    ck_assert_str_eq(out, "ab");
    talloc_free(ctx);
}
END_TEST

START_TEST(test_two_char_keypad_mode)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    /* \x1b= alternate keypad, \x1b> numeric keypad */
    char *out = strip(ctx, "\x1b=text\x1b>");
    ck_assert_str_eq(out, "text");
    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * Mixed text and escapes
 * ================================================================ */

START_TEST(test_mixed_text_and_escapes)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    char *out = strip(ctx,
        "\x1b[32mOK\x1b[0m: \x1b[1mtest passed\x1b[0m\n");
    ck_assert_str_eq(out, "OK: test passed\n");
    talloc_free(ctx);
}
END_TEST

START_TEST(test_text_before_after_escape)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    char *out = strip(ctx, "prefix\x1b[1mbold\x1b[0msuffix");
    ck_assert_str_eq(out, "prefixboldsuffix");
    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * Consecutive escapes with no text between
 * ================================================================ */

START_TEST(test_consecutive_escapes)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    /* Multiple escape sequences back to back, no text between */
    char *out = strip(ctx, "\x1b[1m\x1b[32m\x1b[4mtext\x1b[0m\x1b[0m");
    ck_assert_str_eq(out, "text");
    talloc_free(ctx);
}
END_TEST

START_TEST(test_only_escapes_no_text)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    char *out = strip(ctx, "\x1b[0m\x1b[31m\x1b[1m");
    ck_assert_str_eq(out, "");
    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * Partial / malformed sequences
 * ================================================================ */

START_TEST(test_bare_esc_at_end)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    /* bare ESC with nothing following — dropped */
    char *out = strip(ctx, "text\x1b");
    ck_assert_str_eq(out, "text");
    talloc_free(ctx);
}
END_TEST

START_TEST(test_unterminated_csi)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    /* CSI with parameters but no final byte */
    char *out = strip(ctx, "a\x1b[1;2");
    ck_assert_str_eq(out, "a");
    talloc_free(ctx);
}
END_TEST

START_TEST(test_unterminated_osc)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    /* OSC with no terminator — everything to end of input dropped */
    char *out = strip(ctx, "before\x1b]0;title");
    ck_assert_str_eq(out, "before");
    talloc_free(ctx);
}
END_TEST

START_TEST(test_csi_no_params)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    /* \x1b[m — SGR with no params (same as reset) */
    char *out = strip(ctx, "\x1b[mtext");
    ck_assert_str_eq(out, "text");
    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * Real-world patterns
 * ================================================================ */

START_TEST(test_compiler_error_colors)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    /*
     * Simulates gcc/clang colored output:
     * filename:line:col: error: message
     */
    const char *input =
        "\x1b[01;31mfoo.c\x1b[m:\x1b[01;31m10\x1b[m:"
        "\x1b[01;31m5\x1b[m: \x1b[01;31merror\x1b[m: "
        "undeclared identifier 'x'\n";
    char *out = strip(ctx, input);
    ck_assert_str_eq(out, "foo.c:10:5: error: undeclared identifier 'x'\n");
    talloc_free(ctx);
}
END_TEST

START_TEST(test_progress_bar)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    /*
     * Simulates a progress bar that overwrites itself using \r and
     * cursor codes. We just strip escapes and preserve the text.
     */
    const char *input =
        "\r\x1b[KBuilding [====    ] 50%\r\x1b[KBuilding [========] 100%\n";
    char *out = strip(ctx, input);
    ck_assert_str_eq(out, "\rBuilding [====    ] 50%\rBuilding [========] 100%\n");
    talloc_free(ctx);
}
END_TEST

START_TEST(test_test_runner_output)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    /*
     * Simulates test runner output with green/red results.
     */
    const char *input =
        "\x1b[32m.\x1b[0m\x1b[32m.\x1b[0m\x1b[31mF\x1b[0m\n"
        "\x1b[31mFAILED\x1b[0m: test_foo\n";
    char *out = strip(ctx, input);
    ck_assert_str_eq(out, "..F\nFAILED: test_foo\n");
    talloc_free(ctx);
}
END_TEST

START_TEST(test_window_title_osc)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    /* Terminal sets window title at start, then outputs normal text */
    char *out = strip(ctx, "\x1b]0;bash\x07$ ls -la\n");
    ck_assert_str_eq(out, "$ ls -la\n");
    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * Suite
 * ================================================================ */

static Suite *bg_ansi_suite(void)
{
    Suite *s = suite_create("bg_ansi");

    TCase *tc_plain = tcase_create("PlainText");
    tcase_add_test(tc_plain, test_empty_input);
    tcase_add_test(tc_plain, test_empty_string);
    tcase_add_test(tc_plain, test_plain_text_passthrough);
    tcase_add_test(tc_plain, test_plain_text_with_newlines);
    suite_add_tcase(s, tc_plain);

    TCase *tc_sgr = tcase_create("SGR");
    tcase_add_test(tc_sgr, test_sgr_bold);
    tcase_add_test(tc_sgr, test_sgr_colors);
    tcase_add_test(tc_sgr, test_sgr_256_color);
    tcase_add_test(tc_sgr, test_sgr_truecolor);
    tcase_add_test(tc_sgr, test_sgr_multiple_params);
    suite_add_tcase(s, tc_sgr);

    TCase *tc_cursor = tcase_create("CursorMovement");
    tcase_add_test(tc_cursor, test_cursor_up);
    tcase_add_test(tc_cursor, test_cursor_down);
    tcase_add_test(tc_cursor, test_cursor_forward_back);
    tcase_add_test(tc_cursor, test_cursor_position);
    suite_add_tcase(s, tc_cursor);

    TCase *tc_erase = tcase_create("EraseSequences");
    tcase_add_test(tc_erase, test_erase_display);
    tcase_add_test(tc_erase, test_erase_line);
    suite_add_tcase(s, tc_erase);

    TCase *tc_osc = tcase_create("OSC");
    tcase_add_test(tc_osc, test_osc_bel_terminator);
    tcase_add_test(tc_osc, test_osc_st_terminator);
    tcase_add_test(tc_osc, test_osc_hyperlink);
    suite_add_tcase(s, tc_osc);

    TCase *tc_two = tcase_create("TwoCharEscapes");
    tcase_add_test(tc_two, test_two_char_reverse_index);
    tcase_add_test(tc_two, test_two_char_keypad_mode);
    suite_add_tcase(s, tc_two);

    TCase *tc_mixed = tcase_create("Mixed");
    tcase_add_test(tc_mixed, test_mixed_text_and_escapes);
    tcase_add_test(tc_mixed, test_text_before_after_escape);
    tcase_add_test(tc_mixed, test_consecutive_escapes);
    tcase_add_test(tc_mixed, test_only_escapes_no_text);
    suite_add_tcase(s, tc_mixed);

    TCase *tc_malformed = tcase_create("Malformed");
    tcase_add_test(tc_malformed, test_bare_esc_at_end);
    tcase_add_test(tc_malformed, test_unterminated_csi);
    tcase_add_test(tc_malformed, test_unterminated_osc);
    tcase_add_test(tc_malformed, test_csi_no_params);
    suite_add_tcase(s, tc_malformed);

    TCase *tc_realworld = tcase_create("RealWorld");
    tcase_add_test(tc_realworld, test_compiler_error_colors);
    tcase_add_test(tc_realworld, test_progress_bar);
    tcase_add_test(tc_realworld, test_test_runner_output);
    tcase_add_test(tc_realworld, test_window_title_osc);
    suite_add_tcase(s, tc_realworld);

    return s;
}

int32_t main(void)
{
    Suite *s = bg_ansi_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/bg_ansi_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
