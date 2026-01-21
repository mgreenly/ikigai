/**
 * @file scrollback_create_test.c
 * @brief Unit tests for scrollback buffer creation
 */

#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "../../../src/scrollback.h"
#include "../../test_utils_helper.h"

/* Test: Create scrollback buffer */
START_TEST(test_scrollback_create) {
    void *ctx = talloc_new(NULL);
    int32_t terminal_width = 80;

    /* Create scrollback */
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, terminal_width);
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

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
/* Test: Invalid width assertion */
START_TEST(test_scrollback_create_invalid_width_asserts) {
    void *ctx = talloc_new(NULL);

    /* terminal_width must be > 0 - should abort */
    ik_scrollback_create(ctx, 0);

    talloc_free(ctx);
}

END_TEST
#endif

static Suite *scrollback_create_suite(void)
{
    Suite *s = suite_create("Scrollback Create");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    /* Normal tests */
    tcase_add_test(tc_core, test_scrollback_create);

    suite_add_tcase(s, tc_core);

#if !defined(NDEBUG) && !defined(SKIP_SIGNAL_TESTS)
    /* Assertion tests */
    TCase *tc_assertions = tcase_create("Assertions");
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_assertions, IK_TEST_TIMEOUT); // Longer timeout for valgrind
    tcase_add_test_raise_signal(tc_assertions, test_scrollback_create_invalid_width_asserts, SIGABRT);
    suite_add_tcase(s, tc_assertions);
#endif

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = scrollback_create_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/scrollback/scrollback_create_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
