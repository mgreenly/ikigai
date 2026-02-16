#include <check.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

#include "apps/ikigai/serialize.h"
#include "shared/error.h"

// NULL framebuffer returns error
START_TEST(test_null_framebuffer)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    res_t res = ik_serialize_framebuffer(ctx, NULL, 0, 2, 80, 0, 0, true);
    ck_assert(is_err(&res));
    talloc_free(ctx);
}
END_TEST

// Empty framebuffer produces valid JSON with empty rows
START_TEST(test_empty_framebuffer)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const uint8_t fb[] = "";
    res_t res = ik_serialize_framebuffer(ctx, fb, 0, 2, 80, 0, 0, true);
    ck_assert(is_ok(&res));
    char *json = (char *)res.ok;
    ck_assert_ptr_nonnull(json);
    ck_assert(strstr(json, "\"type\":\"framebuffer\"") != NULL);
    ck_assert(strstr(json, "\"rows\":2") != NULL);
    ck_assert(strstr(json, "\"cols\":80") != NULL);
    ck_assert(strstr(json, "\"visible\":true") != NULL);
    talloc_free(ctx);
}
END_TEST

// Plain text gets serialized into spans
START_TEST(test_plain_text)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *text = "Hello\r\nWorld\r\n";
    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text,
                                          strlen(text), 3, 80, 0, 0, false);
    ck_assert(is_ok(&res));
    char *json = (char *)res.ok;
    ck_assert(strstr(json, "Hello") != NULL);
    ck_assert(strstr(json, "World") != NULL);
    ck_assert(strstr(json, "\"visible\":false") != NULL);
    talloc_free(ctx);
}
END_TEST

// Bold escape sequence: ESC[1m
START_TEST(test_bold_escape)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *text = "\x1b[1mBold\r\n";
    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text,
                                          strlen(text), 2, 80, 0, 0, true);
    ck_assert(is_ok(&res));
    char *json = (char *)res.ok;
    ck_assert(strstr(json, "\"bold\":true") != NULL);
    ck_assert(strstr(json, "Bold") != NULL);
    talloc_free(ctx);
}
END_TEST

// Dim escape sequence: ESC[2m
START_TEST(test_dim_escape)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *text = "\x1b[2mDim\r\n";
    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text,
                                          strlen(text), 2, 80, 0, 0, true);
    ck_assert(is_ok(&res));
    char *json = (char *)res.ok;
    ck_assert(strstr(json, "\"dim\":true") != NULL);
    ck_assert(strstr(json, "Dim") != NULL);
    talloc_free(ctx);
}
END_TEST

// Reverse escape sequence: ESC[7m
START_TEST(test_reverse_escape)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *text = "\x1b[7mRev\r\n";
    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text,
                                          strlen(text), 2, 80, 0, 0, true);
    ck_assert(is_ok(&res));
    char *json = (char *)res.ok;
    ck_assert(strstr(json, "\"reverse\":true") != NULL);
    ck_assert(strstr(json, "Rev") != NULL);
    talloc_free(ctx);
}
END_TEST

// Reset escape sequence: ESC[0m
START_TEST(test_reset_escape)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *text = "\x1b[1mBold\x1b[0mPlain\r\n";
    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text,
                                          strlen(text), 2, 80, 0, 0, true);
    ck_assert(is_ok(&res));
    char *json = (char *)res.ok;
    ck_assert(strstr(json, "Bold") != NULL);
    ck_assert(strstr(json, "Plain") != NULL);
    talloc_free(ctx);
}
END_TEST

// 256-color foreground: ESC[38;5;123m
START_TEST(test_fg_color_256)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *text = "\x1b[38;5;123mColored\r\n";
    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text,
                                          strlen(text), 2, 80, 0, 0, true);
    ck_assert(is_ok(&res));
    char *json = (char *)res.ok;
    ck_assert(strstr(json, "\"fg\":123") != NULL);
    ck_assert(strstr(json, "Colored") != NULL);
    talloc_free(ctx);
}
END_TEST

// Hide cursor sequence: ESC[?25l
START_TEST(test_hide_cursor_sequence)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *text = "\x1b[?25lHello\r\n";
    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text,
                                          strlen(text), 2, 80, 0, 0, true);
    ck_assert(is_ok(&res));
    char *json = (char *)res.ok;
    ck_assert(strstr(json, "Hello") != NULL);
    talloc_free(ctx);
}
END_TEST

// Home sequence: ESC[H
START_TEST(test_home_sequence)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *text = "\x1b[HHello\r\n";
    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text,
                                          strlen(text), 2, 80, 0, 0, true);
    ck_assert(is_ok(&res));
    char *json = (char *)res.ok;
    ck_assert(strstr(json, "Hello") != NULL);
    talloc_free(ctx);
}
END_TEST

// Unknown escape sequences are skipped
START_TEST(test_unknown_escape_sequence)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    // ESC[99J is an unknown sequence (erase in display with unknown param)
    const char *text = "\x1b[99JHello\r\n";
    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text,
                                          strlen(text), 2, 80, 0, 0, true);
    ck_assert(is_ok(&res));
    char *json = (char *)res.ok;
    ck_assert(strstr(json, "Hello") != NULL);
    talloc_free(ctx);
}
END_TEST

// Text with quotes and backslashes is escaped in JSON
START_TEST(test_escape_text_special_chars)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *text = "say \"hi\"\\\r\n";
    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text,
                                          strlen(text), 2, 80, 0, 0, true);
    ck_assert(is_ok(&res));
    char *json = (char *)res.ok;
    ck_assert(strstr(json, "\\\"hi\\\"") != NULL);
    ck_assert(strstr(json, "\\\\") != NULL);
    talloc_free(ctx);
}
END_TEST

// Multiple style attributes combined: bold + fg + dim + reverse
START_TEST(test_combined_styles)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    // Bold then fg then dim then reverse — all on same span
    const char *text = "\x1b[1m\x1b[38;5;42m\x1b[2m\x1b[7mStyled\r\n";
    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text,
                                          strlen(text), 2, 80, 0, 0, true);
    ck_assert(is_ok(&res));
    char *json = (char *)res.ok;
    ck_assert(strstr(json, "\"bold\":true") != NULL);
    ck_assert(strstr(json, "\"fg\":42") != NULL);
    ck_assert(strstr(json, "\"dim\":true") != NULL);
    ck_assert(strstr(json, "\"reverse\":true") != NULL);
    talloc_free(ctx);
}
END_TEST

// Multiple rows with content
START_TEST(test_multiple_rows)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *text = "Row0\r\nRow1\r\nRow2\r\n";
    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text,
                                          strlen(text), 4, 80, 0, 0, true);
    ck_assert(is_ok(&res));
    char *json = (char *)res.ok;
    ck_assert(strstr(json, "Row0") != NULL);
    ck_assert(strstr(json, "Row1") != NULL);
    ck_assert(strstr(json, "Row2") != NULL);
    ck_assert(strstr(json, "\"row\":0") != NULL);
    ck_assert(strstr(json, "\"row\":1") != NULL);
    ck_assert(strstr(json, "\"row\":2") != NULL);
    ck_assert(strstr(json, "\"row\":3") != NULL);
    talloc_free(ctx);
}
END_TEST

// Span capacity growth: more than 4 spans on a single line
START_TEST(test_span_capacity_growth)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    // 5 style changes on one line = 5 spans (initial cap is 4, so growth needed)
    const char *text = "\x1b[1mA\x1b[0mB\x1b[2mC\x1b[0mD\x1b[7mE\r\n";
    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text,
                                          strlen(text), 2, 80, 0, 0, true);
    ck_assert(is_ok(&res));
    char *json = (char *)res.ok;
    ck_assert_ptr_nonnull(json);
    talloc_free(ctx);
}
END_TEST

// Text capacity growth: line longer than 256 chars
START_TEST(test_text_capacity_growth)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    // Build a 300-char line to trigger text_cap growth (initial 256)
    char text[310];
    memset(text, 'X', 300);
    text[300] = '\r';
    text[301] = '\n';
    text[302] = '\0';
    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text,
                                          302, 2, 80, 0, 0, true);
    ck_assert(is_ok(&res));
    talloc_free(ctx);
}
END_TEST

// Text on last row without trailing CRLF gets flushed
START_TEST(test_trailing_text_without_crlf)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *text = "Hello";
    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text,
                                          strlen(text), 2, 80, 0, 0, true);
    ck_assert(is_ok(&res));
    char *json = (char *)res.ok;
    ck_assert(strstr(json, "Hello") != NULL);
    talloc_free(ctx);
}
END_TEST

// Empty row followed by CRLF creates empty line span
START_TEST(test_empty_line_crlf)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *text = "\r\nText\r\n";
    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text,
                                          strlen(text), 3, 80, 0, 0, true);
    ck_assert(is_ok(&res));
    char *json = (char *)res.ok;
    ck_assert(strstr(json, "Text") != NULL);
    talloc_free(ctx);
}
END_TEST

// Truncated escape: ESC[ at end of buffer
START_TEST(test_truncated_escape_at_end)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    // ESC[ with nothing after it
    const char *text = "Hi\x1b[";
    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text,
                                          strlen(text), 2, 80, 0, 0, true);
    ck_assert(is_ok(&res));
    char *json = (char *)res.ok;
    ck_assert(strstr(json, "Hi") != NULL);
    talloc_free(ctx);
}
END_TEST

// Lone ESC at end of buffer (no '[' follows)
START_TEST(test_lone_esc_at_end)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *text = "Hi\x1b";
    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text,
                                          strlen(text), 2, 80, 0, 0, true);
    ck_assert(is_ok(&res));
    talloc_free(ctx);
}
END_TEST

// Truncated \r at end of buffer (no \n follows)
START_TEST(test_truncated_cr_at_end)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *text = "Hi\r";
    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text,
                                          strlen(text), 2, 80, 0, 0, true);
    ck_assert(is_ok(&res));
    talloc_free(ctx);
}
END_TEST

// Partial fg color prefix: ESC[3 followed by non-8 char
START_TEST(test_partial_fg_color_wrong_prefix)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    // ESC[3 followed by 'J' instead of '8;5;...'
    const char *text = "\x1b[3JHello\r\n";
    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text,
                                          strlen(text), 2, 80, 0, 0, true);
    ck_assert(is_ok(&res));
    char *json = (char *)res.ok;
    ck_assert(strstr(json, "Hello") != NULL);
    talloc_free(ctx);
}
END_TEST

// fg color with truncated data after 38;5;
START_TEST(test_fg_color_truncated)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    // ESC[38;5; with no digits or 'm' after
    const char *text = "\x1b[38;5;";
    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text,
                                          strlen(text), 2, 80, 0, 0, true);
    ck_assert(is_ok(&res));
    talloc_free(ctx);
}
END_TEST

// fg color with digits but no trailing 'm'
START_TEST(test_fg_color_no_m)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    // ESC[38;5;42 followed by non-'m' terminator
    const char *text = "\x1b[38;5;42X\r\n";
    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text,
                                          strlen(text), 2, 80, 0, 0, true);
    ck_assert(is_ok(&res));
    talloc_free(ctx);
}
END_TEST

// Escape sequence with long intermediate bytes (skip loop)
START_TEST(test_escape_skip_long_intermediate)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    // ESC[ followed by several intermediate bytes then terminator
    const char *text = "\x1b[?1049hHello\r\n";
    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text,
                                          strlen(text), 2, 80, 0, 0, true);
    ck_assert(is_ok(&res));
    char *json = (char *)res.ok;
    ck_assert(strstr(json, "Hello") != NULL);
    talloc_free(ctx);
}
END_TEST

// CRLF after span with existing spans on same line (branch: text_len > 0 || span_count == 0)
START_TEST(test_crlf_after_style_no_text)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    // Bold style change, then CRLF without text in current span
    const char *text = "A\x1b[1m\r\nB\r\n";
    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text,
                                          strlen(text), 3, 80, 0, 0, true);
    ck_assert(is_ok(&res));
    char *json = (char *)res.ok;
    ck_assert(strstr(json, "A") != NULL);
    ck_assert(strstr(json, "B") != NULL);
    talloc_free(ctx);
}
END_TEST

// fg color prefix partially matching: "38;" but not "38;5;"
START_TEST(test_fg_color_partial_match)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    // ESC[38;2;... (not 38;5;) - will fail fg check, fall through to skip
    const char *text = "\x1b[38;2;255mHi\r\n";
    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text,
                                          strlen(text), 2, 80, 0, 0, true);
    ck_assert(is_ok(&res));
    talloc_free(ctx);
}
END_TEST

// Truncated fg color: ESC[38;5 (missing ;)
START_TEST(test_fg_color_truncated_early)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *text = "\x1b[38;5";
    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text,
                                          strlen(text), 2, 80, 0, 0, true);
    ck_assert(is_ok(&res));
    talloc_free(ctx);
}
END_TEST

// ESC[0 at end of buffer (truncated reset)
START_TEST(test_truncated_reset)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *text = "Hi\x1b[0";
    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text,
                                          strlen(text), 2, 80, 0, 0, true);
    ck_assert(is_ok(&res));
    talloc_free(ctx);
}
END_TEST

// ESC[0X (partial reset, wrong terminator)
START_TEST(test_partial_reset_wrong_term)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *text = "\x1b[0XHi\r\n";
    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text,
                                          strlen(text), 2, 80, 0, 0, true);
    ck_assert(is_ok(&res));
    talloc_free(ctx);
}
END_TEST

// ESC[1 at end of buffer (truncated bold)
START_TEST(test_truncated_bold)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *text = "Hi\x1b[1";
    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text,
                                          strlen(text), 2, 80, 0, 0, true);
    ck_assert(is_ok(&res));
    talloc_free(ctx);
}
END_TEST

// ESC[2 at end of buffer (truncated dim)
START_TEST(test_truncated_dim)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *text = "Hi\x1b[2";
    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text,
                                          strlen(text), 2, 80, 0, 0, true);
    ck_assert(is_ok(&res));
    talloc_free(ctx);
}
END_TEST

// ESC[7 at end of buffer (truncated reverse)
START_TEST(test_truncated_reverse)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *text = "Hi\x1b[7";
    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text,
                                          strlen(text), 2, 80, 0, 0, true);
    ck_assert(is_ok(&res));
    talloc_free(ctx);
}
END_TEST

// Skip loop hits end of buffer without finding terminator
START_TEST(test_escape_skip_truncated)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    // ESC[ followed by intermediate chars but no final byte (0x40-0x7E)
    const char text[] = "\x1b[?25";  // intermediate bytes, no terminator in range
    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text,
                                          5, 2, 80, 0, 0, true);
    ck_assert(is_ok(&res));
    talloc_free(ctx);
}
END_TEST

// Text past all rows - trailing text on row >= rows is not flushed
START_TEST(test_text_past_all_rows)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    // 2 rows allocated, but text spans 3 CRLF lines plus trailing text
    const char *text = "R0\r\nR1\r\nOverflow";
    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text,
                                          strlen(text), 2, 80, 0, 0, true);
    ck_assert(is_ok(&res));
    talloc_free(ctx);
}
END_TEST

// ESC followed by non-'[' character
START_TEST(test_esc_non_bracket)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *text = "Hi\x1bOA\r\n";  // ESC O A (cursor key in alternate mode)
    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text,
                                          strlen(text), 2, 80, 0, 0, true);
    ck_assert(is_ok(&res));
    talloc_free(ctx);
}
END_TEST

// \r followed by non-\n character
START_TEST(test_cr_without_lf)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *text = "Hi\rX\r\n";
    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text,
                                          strlen(text), 2, 80, 0, 0, true);
    ck_assert(is_ok(&res));
    talloc_free(ctx);
}
END_TEST

static Suite *serialize_framebuffer_suite(void)
{
    Suite *s = suite_create("serialize_framebuffer");

    TCase *tc_basic = tcase_create("Basic");
    tcase_add_test(tc_basic, test_null_framebuffer);
    tcase_add_test(tc_basic, test_empty_framebuffer);
    tcase_add_test(tc_basic, test_plain_text);
    tcase_add_test(tc_basic, test_multiple_rows);
    tcase_add_test(tc_basic, test_trailing_text_without_crlf);
    tcase_add_test(tc_basic, test_empty_line_crlf);
    suite_add_tcase(s, tc_basic);

    TCase *tc_escape = tcase_create("Escape Sequences");
    tcase_add_test(tc_escape, test_bold_escape);
    tcase_add_test(tc_escape, test_dim_escape);
    tcase_add_test(tc_escape, test_reverse_escape);
    tcase_add_test(tc_escape, test_reset_escape);
    tcase_add_test(tc_escape, test_fg_color_256);
    tcase_add_test(tc_escape, test_hide_cursor_sequence);
    tcase_add_test(tc_escape, test_home_sequence);
    tcase_add_test(tc_escape, test_unknown_escape_sequence);
    suite_add_tcase(s, tc_escape);

    TCase *tc_style = tcase_create("Style");
    tcase_add_test(tc_style, test_escape_text_special_chars);
    tcase_add_test(tc_style, test_combined_styles);
    suite_add_tcase(s, tc_style);

    TCase *tc_growth = tcase_create("Growth");
    tcase_add_test(tc_growth, test_span_capacity_growth);
    tcase_add_test(tc_growth, test_text_capacity_growth);
    suite_add_tcase(s, tc_growth);

    TCase *tc_edge = tcase_create("Edge Cases");
    tcase_add_test(tc_edge, test_truncated_escape_at_end);
    tcase_add_test(tc_edge, test_lone_esc_at_end);
    tcase_add_test(tc_edge, test_truncated_cr_at_end);
    tcase_add_test(tc_edge, test_partial_fg_color_wrong_prefix);
    tcase_add_test(tc_edge, test_fg_color_truncated);
    tcase_add_test(tc_edge, test_fg_color_no_m);
    tcase_add_test(tc_edge, test_escape_skip_long_intermediate);
    tcase_add_test(tc_edge, test_crlf_after_style_no_text);
    tcase_add_test(tc_edge, test_fg_color_partial_match);
    tcase_add_test(tc_edge, test_fg_color_truncated_early);
    tcase_add_test(tc_edge, test_truncated_reset);
    tcase_add_test(tc_edge, test_partial_reset_wrong_term);
    tcase_add_test(tc_edge, test_truncated_bold);
    tcase_add_test(tc_edge, test_truncated_dim);
    tcase_add_test(tc_edge, test_truncated_reverse);
    tcase_add_test(tc_edge, test_escape_skip_truncated);
    tcase_add_test(tc_edge, test_text_past_all_rows);
    tcase_add_test(tc_edge, test_esc_non_bracket);
    tcase_add_test(tc_edge, test_cr_without_lf);
    suite_add_tcase(s, tc_edge);

    return s;
}

int32_t main(void)
{
    Suite *s = serialize_framebuffer_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/serialize_framebuffer_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
