#include "tools/web_search/domain_utils.h"

#include <check.h>
#include <inttypes.h>
#include <stdlib.h>

START_TEST(test_exact_match) {
    ck_assert_int_eq(url_matches_domain("http://example.com", "example.com"), 1);
    ck_assert_int_eq(url_matches_domain("https://example.com", "example.com"), 1);
    ck_assert_int_eq(url_matches_domain("example.com", "example.com"), 1);
}
END_TEST

START_TEST(test_subdomain_match) {
    ck_assert_int_eq(url_matches_domain("http://www.example.com", "example.com"), 1);
    ck_assert_int_eq(url_matches_domain("https://api.example.com", "example.com"), 1);
    ck_assert_int_eq(url_matches_domain("http://subdomain.example.com", "example.com"), 1);
}
END_TEST

START_TEST(test_with_path) {
    ck_assert_int_eq(url_matches_domain("http://example.com/path", "example.com"), 1);
    ck_assert_int_eq(url_matches_domain("https://www.example.com/path/to/page", "example.com"), 1);
    ck_assert_int_eq(url_matches_domain("example.com/path", "example.com"), 1);
}
END_TEST

START_TEST(test_no_match) {
    ck_assert_int_eq(url_matches_domain("http://other.com", "example.com"), 0);
    ck_assert_int_eq(url_matches_domain("https://notexample.com", "example.com"), 0);
    ck_assert_int_eq(url_matches_domain("http://example.org", "example.com"), 0);
}
END_TEST

START_TEST(test_partial_string_no_match) {
    ck_assert_int_eq(url_matches_domain("http://fakeexample.com", "example.com"), 0);
    ck_assert_int_eq(url_matches_domain("http://example.com.fake", "example.com"), 0);
}
END_TEST

START_TEST(test_null_inputs) {
    ck_assert_int_eq(url_matches_domain(NULL, "example.com"), 0);
    ck_assert_int_eq(url_matches_domain("http://example.com", NULL), 0);
    ck_assert_int_eq(url_matches_domain(NULL, NULL), 0);
}
END_TEST

START_TEST(test_no_protocol) {
    ck_assert_int_eq(url_matches_domain("example.com", "example.com"), 1);
    ck_assert_int_eq(url_matches_domain("www.example.com", "example.com"), 1);
    ck_assert_int_eq(url_matches_domain("example.com/path", "example.com"), 1);
}
END_TEST

START_TEST(test_case_insensitive) {
    ck_assert_int_eq(url_matches_domain("http://EXAMPLE.COM", "example.com"), 1);
    ck_assert_int_eq(url_matches_domain("http://Example.Com", "EXAMPLE.COM"), 1);
    ck_assert_int_eq(url_matches_domain("HTTP://www.EXAMPLE.com", "Example.COM"), 1);
}
END_TEST

static Suite *domain_utils_suite(void)
{
    Suite *s = suite_create("DomainUtils");

    TCase *tc = tcase_create("Core");
    tcase_add_test(tc, test_exact_match);
    tcase_add_test(tc, test_subdomain_match);
    tcase_add_test(tc, test_with_path);
    tcase_add_test(tc, test_no_match);
    tcase_add_test(tc, test_partial_string_no_match);
    tcase_add_test(tc, test_null_inputs);
    tcase_add_test(tc, test_no_protocol);
    tcase_add_test(tc, test_case_insensitive);

    suite_add_tcase(s, tc);

    return s;
}

int32_t main(void)
{
    Suite *s = domain_utils_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/tools/web_search/domain_utils_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int32_t number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
