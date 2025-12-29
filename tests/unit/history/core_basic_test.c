/**
 * @file core_basic_test.c
 * @brief Unit tests for history core basic operations (create and add)
 */

#include "../../../src/history.h"
#include "../../../src/error.h"
#include "../../test_utils.h"

#include <check.h>
#include <talloc.h>
#include <string.h>

// Forward declaration for suite function
static Suite *history_core_basic_suite(void);

// Test fixture
static void *ctx;
static ik_history_t *hist;

static void setup(void)
{
    ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);
}

static void teardown(void)
{
    talloc_free(ctx);
}

// Test: Create history with capacity
START_TEST(test_create_history) {
    hist = ik_history_create(ctx, 10);
    ck_assert_ptr_nonnull(hist);
    ck_assert_uint_eq(hist->count, 0);
    ck_assert_uint_eq(hist->capacity, 10);
    ck_assert_uint_eq(hist->index, 0);
    ck_assert_ptr_null(hist->pending);
    ck_assert_ptr_nonnull(hist->entries);
}
END_TEST
// Test: Add entries within capacity
START_TEST(test_add_entries_within_capacity)
{
    hist = ik_history_create(ctx, 5);

    res_t res = ik_history_add(hist, "command1");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(hist->count, 1);
    ck_assert_str_eq(hist->entries[0], "command1");

    res = ik_history_add(hist, "command2");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(hist->count, 2);
    ck_assert_str_eq(hist->entries[1], "command2");

    res = ik_history_add(hist, "command3");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(hist->count, 3);
    ck_assert_str_eq(hist->entries[2], "command3");
}

END_TEST
// Test: Add entries exceeding capacity (oldest removed)
START_TEST(test_add_entries_exceeds_capacity)
{
    hist = ik_history_create(ctx, 3);

    // Fill to capacity
    ik_history_add(hist, "cmd1");
    ik_history_add(hist, "cmd2");
    ik_history_add(hist, "cmd3");
    ck_assert_uint_eq(hist->count, 3);

    // Add one more - should remove oldest (cmd1)
    res_t res = ik_history_add(hist, "cmd4");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(hist->count, 3);
    ck_assert_str_eq(hist->entries[0], "cmd2");
    ck_assert_str_eq(hist->entries[1], "cmd3");
    ck_assert_str_eq(hist->entries[2], "cmd4");
}

END_TEST
// Test: Empty string not added to history
START_TEST(test_empty_string_not_added)
{
    hist = ik_history_create(ctx, 5);

    res_t res = ik_history_add(hist, "");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(hist->count, 0);
}

END_TEST

// Suite setup
static Suite *history_core_basic_suite(void)
{
    Suite *s = suite_create("History Core Basic");

    TCase *tc_create = tcase_create("Create");
    tcase_set_timeout(tc_create, 30);
    tcase_add_checked_fixture(tc_create, setup, teardown);
    tcase_add_test(tc_create, test_create_history);
    suite_add_tcase(s, tc_create);

    TCase *tc_add = tcase_create("Add Entries");
    tcase_set_timeout(tc_add, 30);
    tcase_add_checked_fixture(tc_add, setup, teardown);
    tcase_add_test(tc_add, test_add_entries_within_capacity);
    tcase_add_test(tc_add, test_add_entries_exceeds_capacity);
    tcase_add_test(tc_add, test_empty_string_not_added);
    suite_add_tcase(s, tc_add);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = history_core_basic_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
