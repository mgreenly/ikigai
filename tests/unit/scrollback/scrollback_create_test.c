/**
 * @file scrollback_create_test.c
 * @brief Unit tests for scrollback buffer creation
 */

#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "../../../src/scrollback.h"
#include "../../test_utils.h"

/* Test: Create scrollback buffer */
START_TEST(test_scrollback_create) {
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = NULL;
    int32_t terminal_width = 80;

    /* Create scrollback */
    res_t res = ik_scrollback_create(ctx, terminal_width, &scrollback);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(scrollback);

    /* Verify initial state */
    ck_assert_uint_eq(scrollback->count, 0);
    ck_assert_uint_gt(scrollback->capacity, 0);
    ck_assert_int_eq(scrollback->cached_width, terminal_width);
    ck_assert_uint_eq(scrollback->total_physical_lines, 0);
    ck_assert_uint_eq(scrollback->buffer_used, 0);
    ck_assert_uint_gt(scrollback->buffer_capacity, 0);

    talloc_free(ctx);
}
END_TEST
/* Test: NULL parameter assertions */
START_TEST(test_scrollback_create_null_scrollback_out_asserts)
{
    void *ctx = talloc_new(NULL);
    int32_t terminal_width = 80;

    /* scrollback_out cannot be NULL - should abort */
    ik_scrollback_create(ctx, terminal_width, NULL);

    talloc_free(ctx);
}

END_TEST START_TEST(test_scrollback_create_invalid_width_asserts)
{
    void *ctx = talloc_new(NULL);
    ik_scrollback_t *scrollback = NULL;

    /* terminal_width must be > 0 - should abort */
    ik_scrollback_create(ctx, 0, &scrollback);

    talloc_free(ctx);
}

END_TEST

static Suite *scrollback_create_suite(void)
{
    Suite *s = suite_create("Scrollback Create");
    TCase *tc_core = tcase_create("Core");
    TCase *tc_assertions = tcase_create("Assertions");
    tcase_set_timeout(tc_assertions, 30); // Longer timeout for valgrind

    /* Normal tests */
    tcase_add_test(tc_core, test_scrollback_create);

    /* Assertion tests */
    tcase_add_test_raise_signal(tc_assertions, test_scrollback_create_null_scrollback_out_asserts, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_scrollback_create_invalid_width_asserts, SIGABRT);

    suite_add_tcase(s, tc_core);
    suite_add_tcase(s, tc_assertions);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = scrollback_create_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
