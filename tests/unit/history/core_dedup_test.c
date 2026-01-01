/**
 * @file core_dedup_test.c
 * @brief Unit tests for history deduplication operations
 */

#include "../../../src/history.h"
#include "../../../src/error.h"
#include "../../test_utils.h"

#include <check.h>
#include <talloc.h>
#include <string.h>

// Forward declaration for suite function
static Suite *history_core_dedup_suite(void);

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

START_TEST(test_history_dedup_consecutive_identical) {
    hist = ik_history_create(ctx, 10);
    res_t res = ik_history_add(hist, "mycommand");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(hist->count, 1);
    res = ik_history_add(hist, "mycommand");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(hist->count, 1);
    res = ik_history_add(hist, "othercommand");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(hist->count, 2);
    res = ik_history_add(hist, "othercommand");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(hist->count, 2);
}
END_TEST

START_TEST(test_history_dedup_reuse_moves_to_end) {
    hist = ik_history_create(ctx, 10);
    ik_history_add(hist, "cmd1");
    ik_history_add(hist, "cmd2");
    ik_history_add(hist, "cmd3");
    ck_assert_uint_eq(hist->count, 3);
    res_t res = ik_history_add(hist, "cmd1");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(hist->count, 3);
    ck_assert_str_eq(hist->entries[0], "cmd2");
    ck_assert_str_eq(hist->entries[1], "cmd3");
    ck_assert_str_eq(hist->entries[2], "cmd1");
}

END_TEST

START_TEST(test_history_dedup_reuse_middle_entry) {
    hist = ik_history_create(ctx, 10);
    ik_history_add(hist, "cmd1");
    ik_history_add(hist, "cmd2");
    ik_history_add(hist, "cmd3");
    ik_history_add(hist, "cmd4");
    ck_assert_uint_eq(hist->count, 4);
    res_t res = ik_history_add(hist, "cmd2");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(hist->count, 4);
    ck_assert_str_eq(hist->entries[0], "cmd1");
    ck_assert_str_eq(hist->entries[1], "cmd3");
    ck_assert_str_eq(hist->entries[2], "cmd4");
    ck_assert_str_eq(hist->entries[3], "cmd2");
}

END_TEST

START_TEST(test_history_dedup_case_sensitive) {
    hist = ik_history_create(ctx, 10);
    res_t res = ik_history_add(hist, "mycommand");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(hist->count, 1);
    res = ik_history_add(hist, "MYCOMMAND");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(hist->count, 2);
    res = ik_history_add(hist, "mycommand");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(hist->count, 2);
}

END_TEST

START_TEST(test_history_dedup_whitespace_significant) {
    hist = ik_history_create(ctx, 10);
    res_t res = ik_history_add(hist, "my command");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(hist->count, 1);
    res = ik_history_add(hist, "my  command");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(hist->count, 2);
    res = ik_history_add(hist, "my  command");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(hist->count, 2);
}

END_TEST

START_TEST(test_history_dedup_respects_capacity) {
    hist = ik_history_create(ctx, 3);
    ik_history_add(hist, "cmd1");
    ik_history_add(hist, "cmd2");
    ik_history_add(hist, "cmd3");
    ck_assert_uint_eq(hist->count, 3);
    res_t res = ik_history_add(hist, "cmd1");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(hist->count, 3);
    ck_assert_str_eq(hist->entries[0], "cmd2");
    ck_assert_str_eq(hist->entries[1], "cmd3");
    ck_assert_str_eq(hist->entries[2], "cmd1");
}

END_TEST

START_TEST(test_history_dedup_identical_with_pending) {
    hist = ik_history_create(ctx, 10);
    ik_history_add(hist, "cmd1");
    ik_history_add(hist, "cmd2");

    // Start browsing to create pending
    res_t res = ik_history_start_browsing(hist, "pending input");
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(hist->pending);
    ck_assert(ik_history_is_browsing(hist));

    // Add identical to most recent entry - should free pending
    res = ik_history_add(hist, "cmd2");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(hist->count, 2);  // Count stays the same
    ck_assert_ptr_null(hist->pending);  // Pending should be freed
    ck_assert(!ik_history_is_browsing(hist));  // No longer browsing
    ck_assert_uint_eq(hist->index, hist->count);  // Index should equal count
}

END_TEST

// Suite setup
static Suite *history_core_dedup_suite(void)
{
    Suite *s = suite_create("History Core Deduplication");

    TCase *tc_dedup = tcase_create("Deduplication");
    tcase_set_timeout(tc_dedup, 30);
    tcase_add_checked_fixture(tc_dedup, setup, teardown);
    tcase_add_test(tc_dedup, test_history_dedup_consecutive_identical);
    tcase_add_test(tc_dedup, test_history_dedup_reuse_moves_to_end);
    tcase_add_test(tc_dedup, test_history_dedup_reuse_middle_entry);
    tcase_add_test(tc_dedup, test_history_dedup_case_sensitive);
    tcase_add_test(tc_dedup, test_history_dedup_whitespace_significant);
    tcase_add_test(tc_dedup, test_history_dedup_respects_capacity);
    tcase_add_test(tc_dedup, test_history_dedup_identical_with_pending);
    suite_add_tcase(s, tc_dedup);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = history_core_dedup_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
