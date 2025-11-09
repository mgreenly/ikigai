/**
 * @file cursor_test.c
 * @brief Unit tests for cursor module
 */

#include <check.h>
#include <signal.h>
#include <talloc.h>
#include "../../../src/cursor.h"
#include "../../test_utils.h"

// Test cursor creation
START_TEST(test_cursor_create) {
    void *ctx = talloc_new(NULL);
    ik_cursor_t *cursor = NULL;

    res_t result = ik_cursor_create(ctx, &cursor);

    ck_assert(is_ok(&result));
    ck_assert_ptr_nonnull(cursor);
    ck_assert_uint_eq(cursor->byte_offset, 0);
    ck_assert_uint_eq(cursor->grapheme_offset, 0);

    talloc_free(ctx);
}
END_TEST
// Test cursor creation OOM
START_TEST(test_cursor_create_oom)
{
    void *ctx = talloc_new(NULL);
    ik_cursor_t *cursor = NULL;

    /* Test OOM during cursor allocation */
    oom_test_fail_next_alloc();
    res_t result = ik_cursor_create(ctx, &cursor);
    ck_assert(is_err(&result));
    ck_assert_ptr_null(cursor);
    oom_test_reset();

    talloc_free(ctx);
}

END_TEST
// Test NULL parent parameter assertion
START_TEST(test_cursor_create_null_parent)
{
    ik_cursor_t *cursor = NULL;

    /* parent cannot be NULL - should abort */
    ik_cursor_create(NULL, &cursor);
}

END_TEST
// Test NULL out parameter assertion
START_TEST(test_cursor_create_null_out)
{
    void *ctx = talloc_new(NULL);

    /* cursor_out cannot be NULL - should abort */
    ik_cursor_create(ctx, NULL);

    talloc_free(ctx);
}

END_TEST

// Test suite
static Suite *cursor_suite(void)
{
    Suite *s = suite_create("Cursor");

    TCase *tc_create = tcase_create("Create");
    tcase_add_test(tc_create, test_cursor_create);
    tcase_add_test(tc_create, test_cursor_create_oom);
    suite_add_tcase(s, tc_create);

    TCase *tc_assertions = tcase_create("Assertions");
    tcase_add_test_raise_signal(tc_assertions, test_cursor_create_null_parent, SIGABRT);
    tcase_add_test_raise_signal(tc_assertions, test_cursor_create_null_out, SIGABRT);
    suite_add_tcase(s, tc_assertions);

    return s;
}

int main(void)
{
    Suite *s = cursor_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
