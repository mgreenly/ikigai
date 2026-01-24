// Tests for render text utilities
#include "render_text.h"

#include <check.h>
#include <string.h>

START_TEST(test_count_newlines_empty)
{
    size_t count = ik_render_count_newlines("", 0);
    ck_assert_uint_eq(count, 0);
}
END_TEST

START_TEST(test_count_newlines_no_newlines)
{
    const char *text = "hello world";
    size_t count = ik_render_count_newlines(text, strlen(text));
    ck_assert_uint_eq(count, 0);
}
END_TEST

START_TEST(test_count_newlines_one_newline)
{
    const char *text = "hello\nworld";
    size_t count = ik_render_count_newlines(text, strlen(text));
    ck_assert_uint_eq(count, 1);
}
END_TEST

START_TEST(test_count_newlines_multiple)
{
    const char *text = "line1\nline2\nline3\n";
    size_t count = ik_render_count_newlines(text, strlen(text));
    ck_assert_uint_eq(count, 3);
}
END_TEST

START_TEST(test_copy_text_empty)
{
    char dest[100];
    size_t written = ik_render_copy_text_with_crlf(dest, "", 0);
    ck_assert_uint_eq(written, 0);
}
END_TEST

START_TEST(test_copy_text_no_newlines)
{
    char dest[100];
    const char *src = "hello";
    size_t written = ik_render_copy_text_with_crlf(dest, src, strlen(src));
    ck_assert_uint_eq(written, 5);
    ck_assert_mem_eq(dest, "hello", 5);
}
END_TEST

START_TEST(test_copy_text_with_newline)
{
    char dest[100];
    const char *src = "hello\nworld";
    size_t written = ik_render_copy_text_with_crlf(dest, src, strlen(src));
    ck_assert_uint_eq(written, 12);  // 5 + 2 + 5 = 12
    ck_assert_mem_eq(dest, "hello\r\nworld", 12);
}
END_TEST

START_TEST(test_copy_text_multiple_newlines)
{
    char dest[100];
    const char *src = "a\nb\nc";
    size_t written = ik_render_copy_text_with_crlf(dest, src, strlen(src));
    ck_assert_uint_eq(written, 7);  // a \r\n b \r\n c = 1+2+1+2+1 = 7
    ck_assert_mem_eq(dest, "a\r\nb\r\nc", 7);
}
END_TEST

static Suite *render_text_suite(void)
{
    Suite *s = suite_create("Render Text");

    TCase *tc = tcase_create("Count Newlines");
    tcase_add_test(tc, test_count_newlines_empty);
    tcase_add_test(tc, test_count_newlines_no_newlines);
    tcase_add_test(tc, test_count_newlines_one_newline);
    tcase_add_test(tc, test_count_newlines_multiple);
    suite_add_tcase(s, tc);

    tc = tcase_create("Copy Text");
    tcase_add_test(tc, test_copy_text_empty);
    tcase_add_test(tc, test_copy_text_no_newlines);
    tcase_add_test(tc, test_copy_text_with_newline);
    tcase_add_test(tc, test_copy_text_multiple_newlines);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = render_text_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
