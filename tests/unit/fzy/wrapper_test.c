#include <check.h>
#include <talloc.h>
#include <inttypes.h>

#include "../../../src/fzy_wrapper.h"

TALLOC_CTX *ctx = NULL;

static void setup(void)
{
    ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(ctx);
}

START_TEST(test_fzy_filter_basic) {
    const char *candidates[] = {"mark", "model", "clear", "help"};
    size_t count = 0;

    ik_fzy_result_t *results = ik_fzy_filter(ctx, candidates, 4, "m", 10, &count);

    ck_assert_ptr_nonnull(results);
    ck_assert_uint_eq(count, 2);  // mark, model match "m"
    // Results should be sorted by score
    ck_assert(results[0].score >= results[1].score);
}
END_TEST

START_TEST(test_fzy_filter_no_match) {
    const char *candidates[] = {"mark", "model"};
    size_t count = 0;

    ik_fzy_result_t *results = ik_fzy_filter(ctx, candidates, 2, "xyz", 10, &count);

    ck_assert_ptr_null(results);
    ck_assert_uint_eq(count, 0);
}

END_TEST

START_TEST(test_fzy_filter_max_results) {
    const char *candidates[] = {"a", "ab", "abc", "abcd", "abcde"};
    size_t count = 0;

    ik_fzy_result_t *results = ik_fzy_filter(ctx, candidates, 5, "a", 3, &count);

    ck_assert_ptr_nonnull(results);
    ck_assert_uint_eq(count, 3);  // Limited to 3
}

END_TEST

START_TEST(test_fzy_filter_empty_search_string) {
    const char *candidates[] = {"mark", "model", "clear"};
    size_t count = 0;

    ik_fzy_result_t *results = ik_fzy_filter(ctx, candidates, 3, "", 10, &count);

    // Empty search should match all candidates
    ck_assert_ptr_nonnull(results);
    ck_assert_uint_eq(count, 3);
}

END_TEST

START_TEST(test_fzy_filter_single_candidate) {
    const char *candidates[] = {"hello"};
    size_t count = 0;

    ik_fzy_result_t *results = ik_fzy_filter(ctx, candidates, 1, "hel", 10, &count);

    ck_assert_ptr_nonnull(results);
    ck_assert_uint_eq(count, 1);
    ck_assert_str_eq(results[0].candidate, "hello");
}

END_TEST

START_TEST(test_fzy_filter_single_candidate_no_match) {
    const char *candidates[] = {"hello"};
    size_t count = 0;

    ik_fzy_result_t *results = ik_fzy_filter(ctx, candidates, 1, "xyz", 10, &count);

    ck_assert_ptr_null(results);
    ck_assert_uint_eq(count, 0);
}

END_TEST

START_TEST(test_fzy_filter_score_ordering) {
    const char *candidates[] = {"mark", "model", "m"};
    size_t count = 0;

    ik_fzy_result_t *results = ik_fzy_filter(ctx, candidates, 3, "m", 10, &count);

    ck_assert_ptr_nonnull(results);
    ck_assert_uint_eq(count, 3);
    // Exact match "m" should have highest score
    ck_assert(results[0].score >= results[1].score);
    ck_assert(results[1].score >= results[2].score);
}

END_TEST

static Suite *fzy_wrapper_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("FZY Wrapper");
    tc_core = tcase_create("Core");

    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_fzy_filter_basic);
    tcase_add_test(tc_core, test_fzy_filter_no_match);
    tcase_add_test(tc_core, test_fzy_filter_max_results);
    tcase_add_test(tc_core, test_fzy_filter_empty_search_string);
    tcase_add_test(tc_core, test_fzy_filter_single_candidate);
    tcase_add_test(tc_core, test_fzy_filter_single_candidate_no_match);
    tcase_add_test(tc_core, test_fzy_filter_score_ordering);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = fzy_wrapper_suite();
    sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/fzy/wrapper_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
