/**
 * @file core_browse_test.c
 * @brief Unit tests for history browsing operations
 */

#include "apps/ikigai/history.h"
#include "shared/error.h"
#include "tests/helpers/test_utils_helper.h"

#include <check.h>
#include <talloc.h>
#include <string.h>

// Forward declaration for suite function
static Suite *history_core_browse_suite(void);

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

// Test: Browsing workflow (start -> prev -> prev -> next -> stop)
START_TEST(test_browsing_workflow) {
    hist = ik_history_create(ctx, 10);

    // Add some entries
    ik_history_add(hist, "cmd1");
    ik_history_add(hist, "cmd2");
    ik_history_add(hist, "cmd3");

    // Start browsing with pending input
    res_t res = ik_history_start_browsing(hist, "pending");
    ck_assert(is_ok(&res));
    ck_assert(ik_history_is_browsing(hist));
    ck_assert_uint_eq(hist->index, 2); // Should be at last entry
    ck_assert_str_eq(hist->pending, "pending");

    // Get current (should be cmd3)
    const char *current = ik_history_get_current(hist);
    ck_assert_ptr_nonnull(current);
    ck_assert_str_eq(current, "cmd3");

    // Navigate to previous (cmd2)
    const char *entry = ik_history_prev(hist);
    ck_assert_ptr_nonnull(entry);
    ck_assert_str_eq(entry, "cmd2");
    ck_assert_uint_eq(hist->index, 1);

    // Navigate to previous again (cmd1)
    entry = ik_history_prev(hist);
    ck_assert_ptr_nonnull(entry);
    ck_assert_str_eq(entry, "cmd1");
    ck_assert_uint_eq(hist->index, 0);

    // Try to go before first (should return NULL)
    entry = ik_history_prev(hist);
    ck_assert_ptr_null(entry);
    ck_assert_uint_eq(hist->index, 0);

    // Navigate forward (cmd2)
    entry = ik_history_next(hist);
    ck_assert_ptr_nonnull(entry);
    ck_assert_str_eq(entry, "cmd2");
    ck_assert_uint_eq(hist->index, 1);

    // Navigate forward (cmd3)
    entry = ik_history_next(hist);
    ck_assert_ptr_nonnull(entry);
    ck_assert_str_eq(entry, "cmd3");
    ck_assert_uint_eq(hist->index, 2);

    // Navigate forward to pending
    entry = ik_history_next(hist);
    ck_assert_ptr_nonnull(entry);
    ck_assert_str_eq(entry, "pending");
    ck_assert_uint_eq(hist->index, 3);
    ck_assert(!ik_history_is_browsing(hist));

    // Try to go past pending (should return NULL)
    entry = ik_history_next(hist);
    ck_assert_ptr_null(entry);
}
END_TEST
// Test: Pending input preservation
START_TEST(test_pending_input_preservation) {
    hist = ik_history_create(ctx, 5);
    ik_history_add(hist, "cmd1");
    ik_history_add(hist, "cmd2");

    // Start browsing
    res_t res = ik_history_start_browsing(hist, "my incomplete command");
    ck_assert(is_ok(&res));
    ck_assert_str_eq(hist->pending, "my incomplete command");

    // Navigate around
    ik_history_prev(hist);
    ik_history_prev(hist);

    // Pending should still be preserved
    ck_assert_str_eq(hist->pending, "my incomplete command");

    // Navigate to pending
    ik_history_next(hist);  // Move from cmd1 to cmd2
    const char *entry = ik_history_next(hist);  // Move from cmd2 to pending
    ck_assert_ptr_nonnull(entry);
    ck_assert_str_eq(entry, "my incomplete command");
}

END_TEST
// Test: Stop browsing
START_TEST(test_stop_browsing) {
    hist = ik_history_create(ctx, 5);
    ik_history_add(hist, "cmd1");

    // Start browsing
    ik_history_start_browsing(hist, "pending");
    ck_assert(ik_history_is_browsing(hist));

    // Stop browsing
    ik_history_stop_browsing(hist);
    ck_assert(!ik_history_is_browsing(hist));
    ck_assert_uint_eq(hist->index, 1); // index = count
    ck_assert_ptr_null(hist->pending);
}

END_TEST
// Test: Stop browsing when not browsing (no pending)
START_TEST(test_stop_browsing_no_pending) {
    hist = ik_history_create(ctx, 5);
    ik_history_add(hist, "cmd1");

    // Not browsing (no pending input)
    ck_assert(!ik_history_is_browsing(hist));
    ck_assert_ptr_null(hist->pending);

    // Stop browsing when not browsing (should be no-op)
    ik_history_stop_browsing(hist);
    ck_assert(!ik_history_is_browsing(hist));
    ck_assert_uint_eq(hist->index, 1); // index = count
    ck_assert_ptr_null(hist->pending);
}

END_TEST
// Test: Empty history browsing (no-op)
START_TEST(test_empty_history_browsing) {
    hist = ik_history_create(ctx, 5);

    // Start browsing with empty history
    res_t res = ik_history_start_browsing(hist, "pending");
    ck_assert(is_ok(&res));

    // Should not be browsing (no history to browse)
    ck_assert(!ik_history_is_browsing(hist));

    // Get current should return pending
    const char *current = ik_history_get_current(hist);
    ck_assert_ptr_nonnull(current);
    ck_assert_str_eq(current, "pending");

    // Prev should return NULL
    const char *entry = ik_history_prev(hist);
    ck_assert_ptr_null(entry);

    // Next should return NULL
    entry = ik_history_next(hist);
    ck_assert_ptr_null(entry);
}

END_TEST
// Test: Navigation boundaries
START_TEST(test_navigation_boundaries) {
    hist = ik_history_create(ctx, 5);
    ik_history_add(hist, "only_one");

    // Start browsing
    ik_history_start_browsing(hist, "pending");
    ck_assert_uint_eq(hist->index, 0);

    // Already at first, prev should return NULL
    const char *entry = ik_history_prev(hist);
    ck_assert_ptr_null(entry);
    ck_assert_uint_eq(hist->index, 0);

    // Move to pending
    entry = ik_history_next(hist);
    ck_assert_ptr_nonnull(entry);
    ck_assert_str_eq(entry, "pending");

    // Already past last, next should return NULL
    entry = ik_history_next(hist);
    ck_assert_ptr_null(entry);
}

END_TEST
// Test: Get current when not browsing
START_TEST(test_get_current_not_browsing) {
    hist = ik_history_create(ctx, 5);
    ik_history_add(hist, "cmd1");

    // Not browsing yet
    ck_assert(!ik_history_is_browsing(hist));

    // Get current should return NULL (no pending input saved)
    const char *current = ik_history_get_current(hist);
    ck_assert_ptr_null(current);
}

END_TEST
// Test: Restart browsing updates pending input
START_TEST(test_restart_browsing_updates_pending) {
    hist = ik_history_create(ctx, 5);
    ik_history_add(hist, "cmd1");
    ik_history_add(hist, "cmd2");

    // Start browsing
    ik_history_start_browsing(hist, "first pending");
    ck_assert_str_eq(hist->pending, "first pending");

    // Navigate around
    ik_history_prev(hist);

    // Start browsing again with different pending
    ik_history_start_browsing(hist, "second pending");
    ck_assert_str_eq(hist->pending, "second pending");
    ck_assert_uint_eq(hist->index, 1); // Should be at last entry again
}

END_TEST
// Test: Add entry while browsing frees pending
START_TEST(test_add_entry_while_browsing) {
    hist = ik_history_create(ctx, 5);
    ik_history_add(hist, "cmd1");
    ik_history_add(hist, "cmd2");

    // Start browsing with pending input
    ik_history_start_browsing(hist, "pending input");
    ck_assert(ik_history_is_browsing(hist));
    ck_assert_ptr_nonnull(hist->pending);

    // Add a new entry while browsing
    res_t res = ik_history_add(hist, "cmd3");
    ck_assert(is_ok(&res));

    // Should no longer be browsing, pending should be freed
    ck_assert(!ik_history_is_browsing(hist));
    ck_assert_ptr_null(hist->pending);
    ck_assert_uint_eq(hist->index, 3); // index should be at count
    ck_assert_uint_eq(hist->count, 3);
}

END_TEST
// Test: Start browsing twice on empty history
START_TEST(test_start_browsing_twice_empty_history) {
    hist = ik_history_create(ctx, 5);

    // Start browsing on empty history
    res_t res = ik_history_start_browsing(hist, "first pending");
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(hist->pending);
    ck_assert_str_eq(hist->pending, "first pending");
    ck_assert(!ik_history_is_browsing(hist));

    // Start browsing again with different pending (should free old)
    res = ik_history_start_browsing(hist, "second pending");
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(hist->pending);
    ck_assert_str_eq(hist->pending, "second pending");
    ck_assert(!ik_history_is_browsing(hist));
}

END_TEST
// Test: Call next when index > count
START_TEST(test_next_past_pending) {
    hist = ik_history_create(ctx, 5);
    ik_history_add(hist, "cmd1");

    // Start browsing
    ik_history_start_browsing(hist, "pending");

    // Navigate to pending (index becomes count)
    const char *entry = ik_history_next(hist);
    ck_assert_ptr_nonnull(entry);
    ck_assert_str_eq(entry, "pending");
    ck_assert_uint_eq(hist->index, 1); // index == count

    // Try to go past pending (index becomes count+1, return NULL)
    entry = ik_history_next(hist);
    ck_assert_ptr_null(entry);
    ck_assert_uint_eq(hist->index, 2); // index > count

    // Try again when index > count (should still return NULL)
    entry = ik_history_next(hist);
    ck_assert_ptr_null(entry);
    ck_assert_uint_eq(hist->index, 2); // index stays at count+1
}

END_TEST

// Suite setup
static Suite *history_core_browse_suite(void)
{
    Suite *s = suite_create("History Core Browsing");

    TCase *tc_browse = tcase_create("Browsing");
    tcase_set_timeout(tc_browse, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_browse, setup, teardown);
    tcase_add_test(tc_browse, test_browsing_workflow);
    tcase_add_test(tc_browse, test_pending_input_preservation);
    tcase_add_test(tc_browse, test_stop_browsing);
    tcase_add_test(tc_browse, test_stop_browsing_no_pending);
    tcase_add_test(tc_browse, test_empty_history_browsing);
    tcase_add_test(tc_browse, test_navigation_boundaries);
    tcase_add_test(tc_browse, test_get_current_not_browsing);
    tcase_add_test(tc_browse, test_restart_browsing_updates_pending);
    tcase_add_test(tc_browse, test_add_entry_while_browsing);
    tcase_add_test(tc_browse, test_start_browsing_twice_empty_history);
    tcase_add_test(tc_browse, test_next_past_pending);
    suite_add_tcase(s, tc_browse);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = history_core_browse_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/history/core_browse_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
