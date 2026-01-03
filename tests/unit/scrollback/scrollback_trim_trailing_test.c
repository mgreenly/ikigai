/**
 * @file scrollback_trim_trailing_test.c
 * @brief Unit tests for scrollback trim trailing whitespace utility
 */

#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "../../../src/scrollback_utils.h"
#include "../../test_utils.h"

/* Test: NULL input returns empty string */
START_TEST(test_trim_trailing_null_returns_empty) {
    void *ctx = talloc_new(NULL);
    char *result = ik_scrollback_trim_trailing(ctx, NULL, 0);
    ck_assert_str_eq(result, "");
    talloc_free(ctx);
}
END_TEST
/* Test: Empty string returns empty string */
START_TEST(test_trim_trailing_empty_returns_empty) {
    void *ctx = talloc_new(NULL);
    char *result = ik_scrollback_trim_trailing(ctx, "", 0);
    ck_assert_str_eq(result, "");
    talloc_free(ctx);
}

END_TEST
/* Test: String with no trailing whitespace */
START_TEST(test_trim_trailing_no_whitespace) {
    void *ctx = talloc_new(NULL);
    char *result = ik_scrollback_trim_trailing(ctx, "hello", 5);
    ck_assert_str_eq(result, "hello");
    talloc_free(ctx);
}

END_TEST
/* Test: String with single trailing newline */
START_TEST(test_trim_trailing_single_newline) {
    void *ctx = talloc_new(NULL);
    char *result = ik_scrollback_trim_trailing(ctx, "hello\n", 6);
    ck_assert_str_eq(result, "hello");
    talloc_free(ctx);
}

END_TEST
/* Test: String with multiple trailing newlines */
START_TEST(test_trim_trailing_multiple_newlines) {
    void *ctx = talloc_new(NULL);
    char *result = ik_scrollback_trim_trailing(ctx, "hello\n\n\n", 8);
    ck_assert_str_eq(result, "hello");
    talloc_free(ctx);
}

END_TEST
/* Test: String with mixed trailing whitespace */
START_TEST(test_trim_trailing_mixed_whitespace) {
    void *ctx = talloc_new(NULL);
    char *result = ik_scrollback_trim_trailing(ctx, "hello \t\n\r\n", 10);
    ck_assert_str_eq(result, "hello");
    talloc_free(ctx);
}

END_TEST
/* Test: String with internal whitespace is preserved */
START_TEST(test_trim_trailing_preserves_internal_whitespace) {
    void *ctx = talloc_new(NULL);
    char *result = ik_scrollback_trim_trailing(ctx, "hello\nworld\n", 12);
    ck_assert_str_eq(result, "hello\nworld");
    talloc_free(ctx);
}

END_TEST
/* Test: String with only whitespace returns empty */
START_TEST(test_trim_trailing_all_whitespace) {
    void *ctx = talloc_new(NULL);
    char *result = ik_scrollback_trim_trailing(ctx, "\n\n\n", 3);
    ck_assert_str_eq(result, "");
    talloc_free(ctx);
}

END_TEST

static Suite *scrollback_trim_trailing_suite(void)
{
    Suite *s = suite_create("Scrollback Trim Trailing");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);
    tcase_set_timeout(tc_core, 30);
    tcase_set_timeout(tc_core, 30);
    tcase_set_timeout(tc_core, 30);
    tcase_set_timeout(tc_core, 30);

    /* Add all tests */
    tcase_add_test(tc_core, test_trim_trailing_null_returns_empty);
    tcase_add_test(tc_core, test_trim_trailing_empty_returns_empty);
    tcase_add_test(tc_core, test_trim_trailing_no_whitespace);
    tcase_add_test(tc_core, test_trim_trailing_single_newline);
    tcase_add_test(tc_core, test_trim_trailing_multiple_newlines);
    tcase_add_test(tc_core, test_trim_trailing_mixed_whitespace);
    tcase_add_test(tc_core, test_trim_trailing_preserves_internal_whitespace);
    tcase_add_test(tc_core, test_trim_trailing_all_whitespace);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = scrollback_trim_trailing_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
