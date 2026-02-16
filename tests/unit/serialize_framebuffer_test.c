#include <check.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

#include "apps/ikigai/serialize.h"
#include "shared/error.h"

START_TEST(test_null_framebuffer)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    res_t res = ik_serialize_framebuffer(ctx, NULL, 0, 2, 80, 0, 0, true);
    ck_assert(is_err(&res));
    talloc_free(ctx);
}
END_TEST

START_TEST(test_empty_framebuffer)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const uint8_t fb[] = "";
    res_t res = ik_serialize_framebuffer(ctx, fb, 0, 2, 80, 0, 0, true);
    ck_assert(is_ok(&res));
    char *json = (char *)res.ok;
    ck_assert(strstr(json, "\"type\":\"framebuffer\"") != NULL);
    talloc_free(ctx);
}
END_TEST

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
    talloc_free(ctx);
}
END_TEST

START_TEST(test_bold_style)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *text = "\x1b[1mBold\r\n";
    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text,
                                          strlen(text), 2, 80, 0, 0, true);
    ck_assert(is_ok(&res));
    char *json = (char *)res.ok;
    ck_assert(strstr(json, "Bold") != NULL);
    ck_assert(strstr(json, "\"bold\":true") != NULL);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_dim_style)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *text = "\x1b[2mDim\r\n";
    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text,
                                          strlen(text), 2, 80, 0, 0, true);
    ck_assert(is_ok(&res));
    char *json = (char *)res.ok;
    ck_assert(strstr(json, "\"dim\":true") != NULL);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_reverse_style)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *text = "\x1b[7mReversed\r\n";
    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text,
                                          strlen(text), 2, 80, 0, 0, true);
    ck_assert(is_ok(&res));
    char *json = (char *)res.ok;
    ck_assert(strstr(json, "\"reverse\":true") != NULL);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_fg_color)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *text = "\x1b[38;5;42mColored\r\n";
    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text,
                                          strlen(text), 2, 80, 0, 0, true);
    ck_assert(is_ok(&res));
    char *json = (char *)res.ok;
    ck_assert(strstr(json, "\"fg\":42") != NULL);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_reset_style)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *text = "\x1b[1mBold\x1b[0mNormal\r\n";
    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text,
                                          strlen(text), 2, 80, 0, 0, true);
    ck_assert(is_ok(&res));
    char *json = (char *)res.ok;
    ck_assert(strstr(json, "Bold") != NULL);
    ck_assert(strstr(json, "Normal") != NULL);
    talloc_free(ctx);
}
END_TEST

// Trigger span capacity growth: initial cap is 4, so 5+ spans in one line
START_TEST(test_span_capacity_growth)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    // Each style change flushes a span: 5 style changes = 5+ spans
    const char *text =
        "A\x1b[1mB\x1b[0mC\x1b[2mD\x1b[0mE\x1b[7mF\r\n";
    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text,
                                          strlen(text), 2, 80, 0, 0, true);
    ck_assert(is_ok(&res));
    char *json = (char *)res.ok;
    ck_assert(strstr(json, "A") != NULL);
    ck_assert(strstr(json, "F") != NULL);
    talloc_free(ctx);
}
END_TEST

// Trigger text capacity growth: initial cap is 256, so 257+ chars in one span
START_TEST(test_text_capacity_growth)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    // Build a string of 300 'X' characters followed by CRLF
    char text[303];
    memset(text, 'X', 300);
    text[300] = '\r';
    text[301] = '\n';
    text[302] = '\0';
    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text,
                                          302, 2, 400, 0, 0, true);
    ck_assert(is_ok(&res));
    talloc_free(ctx);
}
END_TEST

// Text containing backslash and double-quote (escape_text path)
START_TEST(test_text_with_backslash_and_quote)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *text = "He said \"hello\"\\\r\n";
    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text,
                                          strlen(text), 2, 80, 0, 0, true);
    ck_assert(is_ok(&res));
    char *json = (char *)res.ok;
    // Escaped backslash and quote should be in JSON
    ck_assert(strstr(json, "\\\"") != NULL);
    ck_assert(strstr(json, "\\\\") != NULL);
    talloc_free(ctx);
}
END_TEST

// Combined bold + dim (triggers comma between style attrs)
START_TEST(test_bold_plus_dim)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *text = "\x1b[1m\x1b[2mBothStyles\r\n";
    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text,
                                          strlen(text), 2, 80, 0, 0, true);
    ck_assert(is_ok(&res));
    char *json = (char *)res.ok;
    ck_assert(strstr(json, "\"bold\":true") != NULL);
    ck_assert(strstr(json, "\"dim\":true") != NULL);
    talloc_free(ctx);
}
END_TEST

// Combined bold + reverse (triggers comma between style attrs)
START_TEST(test_bold_plus_reverse)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *text = "\x1b[1m\x1b[7mBoldRev\r\n";
    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text,
                                          strlen(text), 2, 80, 0, 0, true);
    ck_assert(is_ok(&res));
    char *json = (char *)res.ok;
    ck_assert(strstr(json, "\"bold\":true") != NULL);
    ck_assert(strstr(json, "\"reverse\":true") != NULL);
    talloc_free(ctx);
}
END_TEST

// Combined fg + bold + dim + reverse (all style comma paths)
START_TEST(test_all_styles_combined)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *text = "\x1b[38;5;10m\x1b[1m\x1b[2m\x1b[7mAll\r\n";
    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text,
                                          strlen(text), 2, 80, 0, 0, true);
    ck_assert(is_ok(&res));
    char *json = (char *)res.ok;
    ck_assert(strstr(json, "\"fg\":10") != NULL);
    ck_assert(strstr(json, "\"bold\":true") != NULL);
    ck_assert(strstr(json, "\"dim\":true") != NULL);
    ck_assert(strstr(json, "\"reverse\":true") != NULL);
    talloc_free(ctx);
}
END_TEST

// Hide cursor sequence
START_TEST(test_hide_cursor)
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

// Home sequence
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

// Multiple lines with spans
START_TEST(test_multiple_lines)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *text = "Line1\r\nLine2\r\n";
    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text,
                                          strlen(text), 3, 80, 0, 0, true);
    ck_assert(is_ok(&res));
    char *json = (char *)res.ok;
    ck_assert(strstr(json, "Line1") != NULL);
    ck_assert(strstr(json, "Line2") != NULL);
    talloc_free(ctx);
}
END_TEST

// Unknown escape sequence (skip loop)
START_TEST(test_unknown_escape)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    const char *text = "\x1b[?1049hHi\r\n";
    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text,
                                          strlen(text), 2, 80, 0, 0, true);
    ck_assert(is_ok(&res));
    char *json = (char *)res.ok;
    ck_assert(strstr(json, "Hi") != NULL);
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
    tcase_add_test(tc_basic, test_multiple_lines);
    suite_add_tcase(s, tc_basic);

    TCase *tc_style = tcase_create("Styles");
    tcase_add_test(tc_style, test_bold_style);
    tcase_add_test(tc_style, test_dim_style);
    tcase_add_test(tc_style, test_reverse_style);
    tcase_add_test(tc_style, test_fg_color);
    tcase_add_test(tc_style, test_reset_style);
    tcase_add_test(tc_style, test_bold_plus_dim);
    tcase_add_test(tc_style, test_bold_plus_reverse);
    tcase_add_test(tc_style, test_all_styles_combined);
    suite_add_tcase(s, tc_style);

    TCase *tc_escape = tcase_create("Escapes");
    tcase_add_test(tc_escape, test_hide_cursor);
    tcase_add_test(tc_escape, test_home_sequence);
    tcase_add_test(tc_escape, test_unknown_escape);
    suite_add_tcase(s, tc_escape);

    TCase *tc_growth = tcase_create("Growth");
    tcase_add_test(tc_growth, test_span_capacity_growth);
    tcase_add_test(tc_growth, test_text_capacity_growth);
    tcase_add_test(tc_growth, test_text_with_backslash_and_quote);
    suite_add_tcase(s, tc_growth);

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
