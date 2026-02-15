#include "tests/test_constants.h"
#include "apps/ikigai/serialize.h"
#include "shared/error.h"

#include <check.h>
#include <inttypes.h>
#include <string.h>
#include <talloc.h>

START_TEST(test_serialize_empty_framebuffer) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    // Empty framebuffer (just the control sequences, no content)
    const char *fb = "\x1b[?25l\x1b[H\x1b[J\x1b[?25h\x1b[1;1H";
    size_t fb_len = strlen(fb);

    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)fb, fb_len, 24, 80, 0, 0, true);
    ck_assert(is_ok(&res));

    const char *json = (const char *)res.ok;
    ck_assert(json != NULL);
    ck_assert(strstr(json, "\"rows\":24") != NULL);
    ck_assert(strstr(json, "\"cols\":80") != NULL);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_serialize_plain_text) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    // Simple plain text
    const char *fb = "\x1b[?25l\x1b[H" "Hello\r\nWorld" "\x1b[J\x1b[?25h\x1b[1;1H";
    size_t fb_len = strlen(fb);

    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)fb, fb_len, 2, 10, 0, 0, true);
    ck_assert(is_ok(&res));

    const char *json = (const char *)res.ok;
    ck_assert(json != NULL);
    ck_assert(strstr(json, "Hello") != NULL);
    ck_assert(strstr(json, "World") != NULL);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_serialize_with_fg_color) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    // Text with foreground color
    const char *fb = "\x1b[?25l\x1b[H" "\x1b[38;5;153mHello" "\x1b[J\x1b[?25h\x1b[1;1H";
    size_t fb_len = strlen(fb);

    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)fb, fb_len, 1, 10, 0, 0, true);
    ck_assert(is_ok(&res));

    const char *json = (const char *)res.ok;
    ck_assert(json != NULL);
    ck_assert(strstr(json, "\"fg\":153") != NULL);
    ck_assert(strstr(json, "Hello") != NULL);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_serialize_with_reset) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    // Text with color and reset
    const char *fb = "\x1b[?25l\x1b[H" "\x1b[38;5;153mColored\x1b[0mPlain" "\x1b[J\x1b[?25h\x1b[1;1H";
    size_t fb_len = strlen(fb);

    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)fb, fb_len, 1, 20, 0, 0, true);
    ck_assert(is_ok(&res));

    const char *json = (const char *)res.ok;
    ck_assert(json != NULL);
    ck_assert(strstr(json, "Colored") != NULL);
    ck_assert(strstr(json, "Plain") != NULL);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_serialize_with_bold) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    // Text with bold
    const char *fb = "\x1b[?25l\x1b[H" "\x1b[1mBold" "\x1b[J\x1b[?25h\x1b[1;1H";
    size_t fb_len = strlen(fb);

    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)fb, fb_len, 1, 10, 0, 0, true);
    ck_assert(is_ok(&res));

    const char *json = (const char *)res.ok;
    ck_assert(json != NULL);
    ck_assert(strstr(json, "\"bold\":true") != NULL);
    ck_assert(strstr(json, "Bold") != NULL);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_serialize_with_dim) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    // Text with dim
    const char *fb = "\x1b[?25l\x1b[H" "\x1b[2mDim" "\x1b[J\x1b[?25h\x1b[1;1H";
    size_t fb_len = strlen(fb);

    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)fb, fb_len, 1, 10, 0, 0, true);
    ck_assert(is_ok(&res));

    const char *json = (const char *)res.ok;
    ck_assert(json != NULL);
    ck_assert(strstr(json, "\"dim\":true") != NULL);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_serialize_with_reverse) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    // Text with reverse video
    const char *fb = "\x1b[?25l\x1b[H" "\x1b[7mReverse" "\x1b[J\x1b[?25h\x1b[1;1H";
    size_t fb_len = strlen(fb);

    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)fb, fb_len, 1, 10, 0, 0, true);
    ck_assert(is_ok(&res));

    const char *json = (const char *)res.ok;
    ck_assert(json != NULL);
    ck_assert(strstr(json, "\"reverse\":true") != NULL);

    talloc_free(ctx);
}
END_TEST

START_TEST(test_serialize_cursor_position) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    // Check cursor position in output
    const char *fb = "\x1b[?25l\x1b[H" "Test" "\x1b[J\x1b[?25h\x1b[1;1H";
    size_t fb_len = strlen(fb);

    res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)fb, fb_len, 10, 20, 5, 7, false);
    ck_assert(is_ok(&res));

    const char *json = (const char *)res.ok;
    ck_assert(json != NULL);
    ck_assert(strstr(json, "\"cursor\":{\"row\":5,\"col\":7,\"visible\":false}") != NULL);

    talloc_free(ctx);
}
END_TEST

static Suite *serialize_suite(void)
{
    Suite *s = suite_create("Serialize");

    TCase *tc_framebuffer = tcase_create("Framebuffer");
    tcase_set_timeout(tc_framebuffer, IK_TEST_TIMEOUT);
    tcase_add_test(tc_framebuffer, test_serialize_empty_framebuffer);
    tcase_add_test(tc_framebuffer, test_serialize_plain_text);
    tcase_add_test(tc_framebuffer, test_serialize_with_fg_color);
    tcase_add_test(tc_framebuffer, test_serialize_with_reset);
    tcase_add_test(tc_framebuffer, test_serialize_with_bold);
    tcase_add_test(tc_framebuffer, test_serialize_with_dim);
    tcase_add_test(tc_framebuffer, test_serialize_with_reverse);
    tcase_add_test(tc_framebuffer, test_serialize_cursor_position);
    suite_add_tcase(s, tc_framebuffer);

    return s;
}

int main(void)
{
    Suite *s = serialize_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/serialize_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    int32_t number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
