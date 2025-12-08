/**
 * @file fzy_wrapper_test.c
 * @brief Unit tests for fzy_wrapper.c assertion violations
 */

#include <check.h>
#include <talloc.h>
#include <signal.h>
#include "../../../src/fzy_wrapper.h"

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)

/* Test: NULL ctx assertion */
START_TEST(test_fzy_filter_null_ctx)
{
    const char *candidates[] = {"test"};
    size_t count = 0;
    ik_fzy_filter(NULL, candidates, 1, "t", 10, &count);
}
END_TEST

/* Test: NULL candidates assertion */
START_TEST(test_fzy_filter_null_candidates)
{
    void *ctx = talloc_new(NULL);
    size_t count = 0;
    ik_fzy_filter(ctx, NULL, 1, "t", 10, &count);
    talloc_free(ctx);
}
END_TEST

/* Test: NULL search assertion */
START_TEST(test_fzy_filter_null_search)
{
    void *ctx = talloc_new(NULL);
    const char *candidates[] = {"test"};
    size_t count = 0;
    ik_fzy_filter(ctx, candidates, 1, NULL, 10, &count);
    talloc_free(ctx);
}
END_TEST

/* Test: NULL count_out assertion */
START_TEST(test_fzy_filter_null_count_out)
{
    void *ctx = talloc_new(NULL);
    const char *candidates[] = {"test"};
    ik_fzy_filter(ctx, candidates, 1, "t", 10, NULL);
    talloc_free(ctx);
}
END_TEST

#endif // !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)

/* Test: Normal operation */
START_TEST(test_fzy_filter_normal)
{
    void *ctx = talloc_new(NULL);
    const char *candidates[] = {"mark", "model", "help"};
    size_t count = 0;

    ik_fzy_result_t *results = ik_fzy_filter(ctx, candidates, 3, "m", 10, &count);

    ck_assert_ptr_nonnull(results);
    ck_assert_uint_eq(count, 2); // "mark" and "model" match "m"

    talloc_free(ctx);
}
END_TEST

/* Test: No matches */
START_TEST(test_fzy_filter_no_matches)
{
    void *ctx = talloc_new(NULL);
    const char *candidates[] = {"mark", "model", "help"};
    size_t count = 0;

    ik_fzy_result_t *results = ik_fzy_filter(ctx, candidates, 3, "xyz", 10, &count);

    ck_assert_ptr_null(results);
    ck_assert_uint_eq(count, 0);

    talloc_free(ctx);
}
END_TEST

/* Test: Zero candidates */
START_TEST(test_fzy_filter_zero_candidates)
{
    void *ctx = talloc_new(NULL);
    const char *candidates[] = {};
    size_t count = 0;

    ik_fzy_result_t *results = ik_fzy_filter(ctx, candidates, 0, "m", 10, &count);

    ck_assert_ptr_null(results);
    ck_assert_uint_eq(count, 0);

    talloc_free(ctx);
}
END_TEST

/* Test: Prefix matching only - "m" matches "mark", "model" but NOT "system" */
START_TEST(test_fzy_filter_prefix_only)
{
    void *ctx = talloc_new(NULL);
    const char *candidates[] = {"mark", "model", "system"};
    size_t count = 0;

    ik_fzy_result_t *results = ik_fzy_filter(ctx, candidates, 3, "m", 10, &count);

    ck_assert_ptr_nonnull(results);
    ck_assert_uint_eq(count, 2); // Only "mark" and "model" should match

    // Verify the returned candidates are mark and model
    bool found_mark = false;
    bool found_model = false;
    for (size_t i = 0; i < count; i++) {
        if (strcmp(results[i].candidate, "mark") == 0) {
            found_mark = true;
        } else if (strcmp(results[i].candidate, "model") == 0) {
            found_model = true;
        }
    }
    ck_assert(found_mark);
    ck_assert(found_model);

    talloc_free(ctx);
}
END_TEST

/* Test: Case-insensitive prefix matching - "m" matches "Mark", "MODEL" */
START_TEST(test_fzy_filter_prefix_case_insensitive)
{
    void *ctx = talloc_new(NULL);
    const char *candidates[] = {"Mark", "MODEL", "system"};
    size_t count = 0;

    ik_fzy_result_t *results = ik_fzy_filter(ctx, candidates, 3, "m", 10, &count);

    ck_assert_ptr_nonnull(results);
    ck_assert_uint_eq(count, 2); // "Mark" and "MODEL" should match

    // Verify the returned candidates
    bool found_mark = false;
    bool found_model = false;
    for (size_t i = 0; i < count; i++) {
        if (strcmp(results[i].candidate, "Mark") == 0) {
            found_mark = true;
        } else if (strcmp(results[i].candidate, "MODEL") == 0) {
            found_model = true;
        }
    }
    ck_assert(found_mark);
    ck_assert(found_model);

    talloc_free(ctx);
}
END_TEST

/* Test: No prefix match - "m" with candidates that don't start with "m" returns NULL */
START_TEST(test_fzy_filter_no_prefix_match)
{
    void *ctx = talloc_new(NULL);
    const char *candidates[] = {"system", "clear", "help"};
    size_t count = 0;

    ik_fzy_result_t *results = ik_fzy_filter(ctx, candidates, 3, "m", 10, &count);

    ck_assert_ptr_null(results);
    ck_assert_uint_eq(count, 0);

    talloc_free(ctx);
}
END_TEST

static Suite *fzy_wrapper_suite(void)
{
    Suite *s = suite_create("FZY_Wrapper");
    TCase *tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_fzy_filter_normal);
    tcase_add_test(tc_core, test_fzy_filter_no_matches);
    tcase_add_test(tc_core, test_fzy_filter_zero_candidates);
    tcase_add_test(tc_core, test_fzy_filter_prefix_only);
    tcase_add_test(tc_core, test_fzy_filter_prefix_case_insensitive);
    tcase_add_test(tc_core, test_fzy_filter_no_prefix_match);

    suite_add_tcase(s, tc_core);

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
    TCase *tc_assertions = tcase_create("Assertions");
    tcase_add_test_raise_signal(tc_assertions, test_fzy_filter_null_ctx, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_fzy_filter_null_candidates, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_fzy_filter_null_search, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_fzy_filter_null_count_out, SIGABRT);
    suite_add_tcase(s, tc_assertions);
#endif // !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = fzy_wrapper_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
