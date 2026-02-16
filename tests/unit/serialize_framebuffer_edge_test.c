#include <check.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

#include "apps/ikigai/serialize.h"
#include "shared/error.h"

START_TEST(test_truncated_escape_at_end) { TALLOC_CTX *ctx = talloc_new(NULL); const char *text = "Hi\x1b["; res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text, strlen(text), 2, 80, 0, 0, true); ck_assert(is_ok(&res)); ck_assert(strstr((char *)res.ok, "Hi") != NULL); talloc_free(ctx); } END_TEST
START_TEST(test_lone_esc_at_end) { TALLOC_CTX *ctx = talloc_new(NULL); const char *text = "Hi\x1b"; res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text, strlen(text), 2, 80, 0, 0, true); ck_assert(is_ok(&res)); talloc_free(ctx); } END_TEST
START_TEST(test_truncated_cr_at_end) { TALLOC_CTX *ctx = talloc_new(NULL); const char *text = "Hi\r"; res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text, strlen(text), 2, 80, 0, 0, true); ck_assert(is_ok(&res)); talloc_free(ctx); } END_TEST
START_TEST(test_partial_fg_color_wrong_prefix) { TALLOC_CTX *ctx = talloc_new(NULL); const char *text = "\x1b[3JHello\r\n"; res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text, strlen(text), 2, 80, 0, 0, true); ck_assert(is_ok(&res)); ck_assert(strstr((char *)res.ok, "Hello") != NULL); talloc_free(ctx); } END_TEST
START_TEST(test_fg_color_truncated) { TALLOC_CTX *ctx = talloc_new(NULL); const char *text = "\x1b[38;5;"; res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text, strlen(text), 2, 80, 0, 0, true); ck_assert(is_ok(&res)); talloc_free(ctx); } END_TEST
START_TEST(test_fg_color_no_m) { TALLOC_CTX *ctx = talloc_new(NULL); const char *text = "\x1b[38;5;42X\r\n"; res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text, strlen(text), 2, 80, 0, 0, true); ck_assert(is_ok(&res)); talloc_free(ctx); } END_TEST
START_TEST(test_escape_skip_long_intermediate) { TALLOC_CTX *ctx = talloc_new(NULL); const char *text = "\x1b[?1049hHello\r\n"; res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text, strlen(text), 2, 80, 0, 0, true); ck_assert(is_ok(&res)); ck_assert(strstr((char *)res.ok, "Hello") != NULL); talloc_free(ctx); } END_TEST
START_TEST(test_crlf_after_style_no_text) { TALLOC_CTX *ctx = talloc_new(NULL); const char *text = "A\x1b[1m\r\nB\r\n"; res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text, strlen(text), 3, 80, 0, 0, true); ck_assert(is_ok(&res)); char *json = (char *)res.ok; ck_assert(strstr(json, "A") != NULL); ck_assert(strstr(json, "B") != NULL); talloc_free(ctx); } END_TEST
START_TEST(test_fg_color_partial_match) { TALLOC_CTX *ctx = talloc_new(NULL); const char *text = "\x1b[38;2;255mHi\r\n"; res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text, strlen(text), 2, 80, 0, 0, true); ck_assert(is_ok(&res)); talloc_free(ctx); } END_TEST
START_TEST(test_fg_color_truncated_early) { TALLOC_CTX *ctx = talloc_new(NULL); const char *text = "\x1b[38;5"; res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text, strlen(text), 2, 80, 0, 0, true); ck_assert(is_ok(&res)); talloc_free(ctx); } END_TEST
START_TEST(test_truncated_reset) { TALLOC_CTX *ctx = talloc_new(NULL); const char *text = "Hi\x1b[0"; res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text, strlen(text), 2, 80, 0, 0, true); ck_assert(is_ok(&res)); talloc_free(ctx); } END_TEST
START_TEST(test_partial_reset_wrong_term) { TALLOC_CTX *ctx = talloc_new(NULL); const char *text = "\x1b[0XHi\r\n"; res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text, strlen(text), 2, 80, 0, 0, true); ck_assert(is_ok(&res)); talloc_free(ctx); } END_TEST
START_TEST(test_truncated_bold) { TALLOC_CTX *ctx = talloc_new(NULL); const char *text = "Hi\x1b[1"; res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text, strlen(text), 2, 80, 0, 0, true); ck_assert(is_ok(&res)); talloc_free(ctx); } END_TEST
START_TEST(test_truncated_dim) { TALLOC_CTX *ctx = talloc_new(NULL); const char *text = "Hi\x1b[2"; res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text, strlen(text), 2, 80, 0, 0, true); ck_assert(is_ok(&res)); talloc_free(ctx); } END_TEST
START_TEST(test_truncated_reverse) { TALLOC_CTX *ctx = talloc_new(NULL); const char *text = "Hi\x1b[7"; res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text, strlen(text), 2, 80, 0, 0, true); ck_assert(is_ok(&res)); talloc_free(ctx); } END_TEST
START_TEST(test_escape_skip_truncated) { TALLOC_CTX *ctx = talloc_new(NULL); const char text[] = "\x1b[?25"; res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text, 5, 2, 80, 0, 0, true); ck_assert(is_ok(&res)); talloc_free(ctx); } END_TEST
START_TEST(test_text_past_all_rows) { TALLOC_CTX *ctx = talloc_new(NULL); const char *text = "R0\r\nR1\r\nOverflow"; res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text, strlen(text), 2, 80, 0, 0, true); ck_assert(is_ok(&res)); talloc_free(ctx); } END_TEST
START_TEST(test_esc_non_bracket) { TALLOC_CTX *ctx = talloc_new(NULL); const char *text = "Hi\x1bOA\r\n"; res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text, strlen(text), 2, 80, 0, 0, true); ck_assert(is_ok(&res)); talloc_free(ctx); } END_TEST
START_TEST(test_cr_without_lf) { TALLOC_CTX *ctx = talloc_new(NULL); const char *text = "Hi\rX\r\n"; res_t res = ik_serialize_framebuffer(ctx, (const uint8_t *)text, strlen(text), 2, 80, 0, 0, true); ck_assert(is_ok(&res)); talloc_free(ctx); } END_TEST

static Suite *suite(void)
{
    Suite *s = suite_create("serialize_framebuffer_edge");
    TCase *tc = tcase_create("Edge Cases");
    tcase_add_test(tc, test_truncated_escape_at_end);
    tcase_add_test(tc, test_lone_esc_at_end);
    tcase_add_test(tc, test_truncated_cr_at_end);
    tcase_add_test(tc, test_partial_fg_color_wrong_prefix);
    tcase_add_test(tc, test_fg_color_truncated);
    tcase_add_test(tc, test_fg_color_no_m);
    tcase_add_test(tc, test_escape_skip_long_intermediate);
    tcase_add_test(tc, test_crlf_after_style_no_text);
    tcase_add_test(tc, test_fg_color_partial_match);
    tcase_add_test(tc, test_fg_color_truncated_early);
    tcase_add_test(tc, test_truncated_reset);
    tcase_add_test(tc, test_partial_reset_wrong_term);
    tcase_add_test(tc, test_truncated_bold);
    tcase_add_test(tc, test_truncated_dim);
    tcase_add_test(tc, test_truncated_reverse);
    tcase_add_test(tc, test_escape_skip_truncated);
    tcase_add_test(tc, test_text_past_all_rows);
    tcase_add_test(tc, test_esc_non_bracket);
    tcase_add_test(tc, test_cr_without_lf);
    suite_add_tcase(s, tc);
    return s;
}

int32_t main(void)
{
    Suite *s = suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/serialize_framebuffer_edge_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
