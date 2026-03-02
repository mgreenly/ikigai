/**
 * @file token_count_test.c
 * @brief Unit tests for token count byte estimator
 */

#include "apps/ikigai/token_count.h"
#include "tests/helpers/test_utils_helper.h"

#include <check.h>
#include <stdint.h>
#include <stdlib.h>

static Suite *token_count_suite(void);

START_TEST(test_zero_bytes) {
    ck_assert_int_eq(ik_token_count_from_bytes(0), 0);
}
END_TEST

START_TEST(test_one_byte) {
    ck_assert_int_eq(ik_token_count_from_bytes(1), 0);
}
END_TEST

START_TEST(test_three_bytes) {
    ck_assert_int_eq(ik_token_count_from_bytes(3), 0);
}
END_TEST

START_TEST(test_four_bytes) {
    ck_assert_int_eq(ik_token_count_from_bytes(4), 1);
}
END_TEST

START_TEST(test_eight_bytes) {
    ck_assert_int_eq(ik_token_count_from_bytes(8), 2);
}
END_TEST

START_TEST(test_one_hundred_bytes) {
    ck_assert_int_eq(ik_token_count_from_bytes(100), 25);
}
END_TEST

START_TEST(test_one_thousand_bytes) {
    ck_assert_int_eq(ik_token_count_from_bytes(1000), 250);
}
END_TEST

START_TEST(test_large_value) {
    /* 400000 bytes => 100000 tokens */
    ck_assert_int_eq(ik_token_count_from_bytes(400000), 100000);
}
END_TEST

START_TEST(test_not_divisible_by_four) {
    /* 13 bytes => 3 tokens (truncates) */
    ck_assert_int_eq(ik_token_count_from_bytes(13), 3);
}
END_TEST

static Suite *token_count_suite(void)
{
    Suite *s = suite_create("token_count");
    TCase *tc = tcase_create("core");

    tcase_add_test(tc, test_zero_bytes);
    tcase_add_test(tc, test_one_byte);
    tcase_add_test(tc, test_three_bytes);
    tcase_add_test(tc, test_four_bytes);
    tcase_add_test(tc, test_eight_bytes);
    tcase_add_test(tc, test_one_hundred_bytes);
    tcase_add_test(tc, test_one_thousand_bytes);
    tcase_add_test(tc, test_large_value);
    tcase_add_test(tc, test_not_divisible_by_four);

    suite_add_tcase(s, tc);
    return s;
}

int32_t main(void)
{
    Suite *s = token_count_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/token_count_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int32_t num_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (num_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
