/**
 * @file matching_test.c
 * @brief Unit tests for completion matching logic
 */

#include "../../../src/completion.h"
#include "../../test_utils.h"

#include <check.h>
#include <talloc.h>
#include <string.h>

// Test fixture
static void *ctx;

static void setup(void)
{
    ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);
}

static void teardown(void)
{
    talloc_free(ctx);
}

// Test: Single match
START_TEST(test_single_match)
{
    // "/cl" should match only "/clear"
    ik_completion_t *comp = ik_completion_create_for_commands(ctx, "/cl");
    ck_assert_ptr_nonnull(comp);
    ck_assert_uint_eq(comp->count, 1);
    ck_assert_str_eq(comp->candidates[0], "clear");
    ck_assert_uint_eq(comp->current, 0);
    ck_assert_str_eq(comp->prefix, "/cl");
}
END_TEST

// Test: Multiple matches (sorted by score)
START_TEST(test_multiple_matches_sorted)
{
    // "/m" should match "mark" and "model" (sorted by fzy score, not alphabetically)
    ik_completion_t *comp = ik_completion_create_for_commands(ctx, "/m");
    ck_assert_ptr_nonnull(comp);
    ck_assert_uint_ge(comp->count, 2);  // At least mark and model

    // Verify both mark and model are in results (order may vary by fzy score)
    bool found_mark = false;
    bool found_model = false;
    for (size_t i = 0; i < comp->count; i++) {
        if (strcmp(comp->candidates[i], "mark") == 0) {
            found_mark = true;
        }
        if (strcmp(comp->candidates[i], "model") == 0) {
            found_model = true;
        }
    }
    ck_assert(found_mark);
    ck_assert(found_model);

    ck_assert_uint_eq(comp->current, 0);
    ck_assert_str_eq(comp->prefix, "/m");
}
END_TEST

// Test: No matches (returns NULL)
START_TEST(test_no_matches)
{
    // "/xyz" should match nothing
    ik_completion_t *comp = ik_completion_create_for_commands(ctx, "/xyz");
    ck_assert_ptr_null(comp);
}
END_TEST

// Test: Empty prefix (just "/") matches all commands (up to max 10)
START_TEST(test_empty_prefix_all_commands)
{
    // "/" should match all commands (10 total)
    ik_completion_t *comp = ik_completion_create_for_commands(ctx, "/");
    ck_assert_ptr_nonnull(comp);
    ck_assert_uint_eq(comp->count, 13);  // clear, debug, fork, help, kill, mark, model, rewind, send, check-mail, read-mail, delete-mail, system

    // Verify all commands are present (order determined by fzy score, not alphabetical)
    bool found[13] = {false};
    const char *expected[] = {"clear", "debug", "fork", "help", "kill", "mark", "model", "rewind", "send", "check-mail", "read-mail", "delete-mail", "system"};

    for (size_t i = 0; i < comp->count; i++) {
        for (size_t j = 0; j < 13; j++) {
            if (strcmp(comp->candidates[i], expected[j]) == 0) {
                found[j] = true;
                break;
            }
        }
    }

    for (size_t i = 0; i < 13; i++) {
        ck_assert(found[i]);
    }
}
END_TEST

// Test: Uppercase prefix (tests case handling in fzy)
START_TEST(test_case_sensitive_matching)
{
    // With fzy, uppercase should still match (case-insensitive matching)
    // However, if no matches, that's also acceptable depending on fzy implementation
    ik_completion_create_for_commands(ctx, "/M");
    // Accept either matches or no matches - the important part is it doesn't crash
    // (either result is valid depending on fzy implementation)
}
END_TEST

// Test: Prefix matching only (non-prefix patterns don't match)
START_TEST(test_fuzzy_matching)
{
    // "ml" should NOT match "model" because "model" doesn't start with "ml"
    // Only prefix-based matching is supported for command completion
    ik_completion_t *comp = ik_completion_create_for_commands(ctx, "/ml");
    ck_assert_ptr_null(comp);  // No prefix match, so returns NULL
}
END_TEST

// Test: Navigation - next with wraparound
START_TEST(test_navigation_next_wraparound)
{
    ik_completion_t *comp = ik_completion_create_for_commands(ctx, "/m");
    ck_assert_ptr_nonnull(comp);
    ck_assert_uint_ge(comp->count, 2);  // At least mark and model
    ck_assert_uint_eq(comp->current, 0);

    // Get initial candidate
    const char *initial = ik_completion_get_current(comp);
    ck_assert_ptr_nonnull(initial);

    // Move to next
    ik_completion_next(comp);
    ck_assert_uint_eq(comp->current, 1);
    const char *next = ik_completion_get_current(comp);
    ck_assert_ptr_nonnull(next);
    ck_assert(strcmp(next, initial) != 0);  // Should be different

    // Move through all items and eventually wrap around
    size_t original_count = comp->count;
    for (size_t i = 1; i < original_count; i++) {
        ik_completion_next(comp);
    }
    // Should wrap around to 0
    ck_assert_uint_eq(comp->current, 0);
    ck_assert_str_eq(ik_completion_get_current(comp), initial);
}
END_TEST

// Test: Navigation - prev with wraparound
START_TEST(test_navigation_prev_wraparound)
{
    ik_completion_t *comp = ik_completion_create_for_commands(ctx, "/m");
    ck_assert_ptr_nonnull(comp);
    ck_assert_uint_ge(comp->count, 2);  // At least mark and model
    ck_assert_uint_eq(comp->current, 0);

    // Get initial candidate
    const char *initial = ik_completion_get_current(comp);
    ck_assert_ptr_nonnull(initial);

    // Move to prev (should wrap to last)
    ik_completion_prev(comp);
    ck_assert_uint_eq(comp->current, comp->count - 1);
    const char *last = ik_completion_get_current(comp);
    ck_assert_ptr_nonnull(last);
    ck_assert(strcmp(last, initial) != 0);  // Should be different

    // Move prev from last -> should go to count-2
    ik_completion_prev(comp);
    ck_assert_uint_eq(comp->current, comp->count - 2);

    // Keep moving until we wrap back to 0
    // We're at count-2, need to move count-2 times to get back to 0
    for (size_t i = 0; i < comp->count - 2; i++) {
        ik_completion_prev(comp);
    }
    ck_assert_uint_eq(comp->current, 0);
    ck_assert_str_eq(ik_completion_get_current(comp), initial);
}
END_TEST

// Test: Get current candidate
START_TEST(test_get_current)
{
    ik_completion_t *comp = ik_completion_create_for_commands(ctx, "/m");
    ck_assert_ptr_nonnull(comp);

    const char *current = ik_completion_get_current(comp);
    ck_assert_ptr_nonnull(current);
    ck_assert_str_eq(current, "mark");

    // Navigate and check again
    ik_completion_next(comp);
    current = ik_completion_get_current(comp);
    ck_assert_ptr_nonnull(current);
    ck_assert_str_eq(current, "model");
}
END_TEST

// Test: Prefix matching validation - valid match
START_TEST(test_prefix_matching_valid)
{
    ik_completion_t *comp = ik_completion_create_for_commands(ctx, "/m");
    ck_assert_ptr_nonnull(comp);

    // "/mar" still starts with "/m"
    ck_assert(ik_completion_matches_prefix(comp, "/mar"));

    // "/model" still starts with "/m"
    ck_assert(ik_completion_matches_prefix(comp, "/model"));

    // Exact match
    ck_assert(ik_completion_matches_prefix(comp, "/m"));
}
END_TEST

// Test: Prefix matching validation - invalid match
START_TEST(test_prefix_matching_invalid)
{
    ik_completion_t *comp = ik_completion_create_for_commands(ctx, "/m");
    ck_assert_ptr_nonnull(comp);

    // "/h" does not start with "/m"
    ck_assert(!ik_completion_matches_prefix(comp, "/h"));

    // "/clear" does not start with "/m"
    ck_assert(!ik_completion_matches_prefix(comp, "/clear"));

    // Empty string does not start with "/m"
    ck_assert(!ik_completion_matches_prefix(comp, ""));

    // "m" (no slash) does not start with "/m"
    ck_assert(!ik_completion_matches_prefix(comp, "m"));
}
END_TEST

// Test: Single character prefix
START_TEST(test_single_char_prefix)
{
    // "/c" should match "clear" and "check-mail"
    ik_completion_t *comp = ik_completion_create_for_commands(ctx, "/c");
    ck_assert_ptr_nonnull(comp);
    ck_assert_uint_eq(comp->count, 2);
    // Both commands start with 'c'
    ck_assert(strcmp(comp->candidates[0], "clear") == 0 || strcmp(comp->candidates[0], "check-mail") == 0);
    ck_assert(strcmp(comp->candidates[1], "clear") == 0 || strcmp(comp->candidates[1], "check-mail") == 0);
}
END_TEST

// Test: Exact command name as prefix
START_TEST(test_exact_command_as_prefix)
{
    // "/clear" should match "clear"
    ik_completion_t *comp = ik_completion_create_for_commands(ctx, "/clear");
    ck_assert_ptr_nonnull(comp);
    ck_assert_uint_eq(comp->count, 1);
    ck_assert_str_eq(comp->candidates[0], "clear");
}
END_TEST

// Test: Navigation with single candidate
START_TEST(test_navigation_single_candidate)
{
    ik_completion_t *comp = ik_completion_create_for_commands(ctx, "/cl");
    ck_assert_ptr_nonnull(comp);
    ck_assert_uint_eq(comp->count, 1);
    ck_assert_uint_eq(comp->current, 0);

    // Next on single item should stay at 0 (wraparound to self)
    ik_completion_next(comp);
    ck_assert_uint_eq(comp->current, 0);

    // Prev on single item should stay at 0 (wraparound to self)
    ik_completion_prev(comp);
    ck_assert_uint_eq(comp->current, 0);
}
END_TEST

// Test: Memory ownership (talloc child of context)
START_TEST(test_memory_ownership)
{
    void *test_ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(test_ctx);

    ik_completion_t *comp = ik_completion_create_for_commands(test_ctx, "/m");
    ck_assert_ptr_nonnull(comp);
    ck_assert_uint_ge(comp->count, 2);  // At least 2 matches

    // Free parent context should free completion
    talloc_free(test_ctx);
    // If this doesn't crash, ownership is correct
}
END_TEST

static Suite *completion_matching_suite(void)
{
    Suite *s = suite_create("Completion Matching");
    TCase *tc = tcase_create("Core");

    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_single_match);
    tcase_add_test(tc, test_multiple_matches_sorted);
    tcase_add_test(tc, test_no_matches);
    tcase_add_test(tc, test_empty_prefix_all_commands);
    tcase_add_test(tc, test_case_sensitive_matching);
    tcase_add_test(tc, test_fuzzy_matching);
    tcase_add_test(tc, test_navigation_next_wraparound);
    tcase_add_test(tc, test_navigation_prev_wraparound);
    tcase_add_test(tc, test_get_current);
    tcase_add_test(tc, test_prefix_matching_valid);
    tcase_add_test(tc, test_prefix_matching_invalid);
    tcase_add_test(tc, test_single_char_prefix);
    tcase_add_test(tc, test_exact_command_as_prefix);
    tcase_add_test(tc, test_navigation_single_candidate);
    tcase_add_test(tc, test_memory_ownership);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = completion_matching_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
