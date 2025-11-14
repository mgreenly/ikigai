#include <check.h>
#include <inttypes.h>
#include <signal.h>
#include <talloc.h>

#include "../../../src/scrollback.h"
#include "../../test_utils.h"

// Test: Create scrollback buffer successfully with given terminal width
START_TEST(test_scrollback_create_success)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *scrollback = NULL;
    res_t res = ik_scrollback_create(ctx, 80, &scrollback);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(scrollback);
    ck_assert_uint_eq(scrollback->count, 0);
    ck_assert_uint_gt(scrollback->capacity, 0);
    ck_assert_int_eq(scrollback->cached_width, 80);
    ck_assert_uint_eq(scrollback->total_physical_lines, 0);
    ck_assert_ptr_nonnull(scrollback->text_offsets);
    ck_assert_ptr_nonnull(scrollback->text_lengths);
    ck_assert_ptr_nonnull(scrollback->layouts);
    ck_assert_ptr_nonnull(scrollback->text_buffer);
    ck_assert_uint_eq(scrollback->buffer_used, 0);
    ck_assert_uint_gt(scrollback->buffer_capacity, 0);

    talloc_free(ctx);
}
END_TEST

// Test: Create scrollback with different terminal width
START_TEST(test_scrollback_create_different_width)
{
    TALLOC_CTX *ctx = talloc_new(NULL);

    ik_scrollback_t *scrollback = NULL;
    res_t res = ik_scrollback_create(ctx, 120, &scrollback);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(scrollback);
    ck_assert_int_eq(scrollback->cached_width, 120);

    talloc_free(ctx);
}
END_TEST

static Suite *scrollback_create_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Scrollback Create");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_scrollback_create_success);
    tcase_add_test(tc_core, test_scrollback_create_different_width);

    suite_add_tcase(s, tc_core);
    return s;
}

int32_t main(void)
{
    int32_t number_failed;
    Suite *s;
    SRunner *sr;

    s = scrollback_create_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
