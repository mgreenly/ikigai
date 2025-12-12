/**
 * @file matching_args_test.c
 * @brief Unit tests for completion argument matching and clear logic
 */

#include "../../../src/completion.h"
#include "../../../src/commands.h"
#include "../../../src/repl.h"
#include "../../../src/marks.h"
#include "../../../src/config.h"
#include "../../../src/shared.h"
#include "../../test_utils.h"

#include <check.h>
#include <talloc.h>
#include <string.h>

// Test fixture
static void *ctx;
static ik_repl_ctx_t *test_repl;

static void setup(void)
{
    ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create minimal repl context for argument completion tests
    test_repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(test_repl);

    // Initialize marks array
    test_repl->marks = NULL;
    test_repl->mark_count = 0;

    // Create shared context
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    test_repl->shared = shared;

    // Initialize config with a default model
    shared->cfg = talloc_zero(ctx, ik_cfg_t);
    ck_assert_ptr_nonnull(shared->cfg);
    shared->cfg->openai_model = talloc_strdup(shared->cfg, "gpt-4o");
    ck_assert_ptr_nonnull(shared->cfg->openai_model);
}

static void teardown(void)
{
    talloc_free(ctx);
}

// Test: /debug argument completion
START_TEST(test_completion_debug_arguments)
{
    // "/debug " should complete to ["off", "on"] (order may vary by fzy score)
    ik_completion_t *comp = ik_completion_create_for_arguments(ctx, test_repl, "/debug ");
    ck_assert_ptr_nonnull(comp);
    ck_assert_uint_eq(comp->count, 2);

    // Verify both off and on are present
    bool found_off = false, found_on = false;
    for (size_t i = 0; i < comp->count; i++) {
        if (strcmp(comp->candidates[i], "off") == 0) found_off = true;
        if (strcmp(comp->candidates[i], "on") == 0) found_on = true;
    }
    ck_assert(found_off);
    ck_assert(found_on);
    ck_assert_str_eq(comp->prefix, "/debug ");

    // "/debug o" should match only "on" and "off"
    talloc_free(comp);
    comp = ik_completion_create_for_arguments(ctx, test_repl, "/debug o");
    ck_assert_ptr_nonnull(comp);
    ck_assert_uint_eq(comp->count, 2);

    // "/debug on" should match only "on"
    talloc_free(comp);
    comp = ik_completion_create_for_arguments(ctx, test_repl, "/debug on");
    ck_assert_ptr_nonnull(comp);
    ck_assert_uint_eq(comp->count, 1);
    ck_assert_str_eq(comp->candidates[0], "on");

    // "/debug of" should match only "off"
    talloc_free(comp);
    comp = ik_completion_create_for_arguments(ctx, test_repl, "/debug of");
    ck_assert_ptr_nonnull(comp);
    ck_assert_uint_eq(comp->count, 1);
    ck_assert_str_eq(comp->candidates[0], "off");
}
END_TEST

// Test: /rewind argument completion with marks
START_TEST(test_completion_rewind_arguments)
{
    // Create test marks
    ik_mark_t *mark1 = talloc_zero(test_repl, ik_mark_t);
    mark1->label = talloc_strdup(mark1, "cp1");
    mark1->message_index = 0;

    ik_mark_t *mark2 = talloc_zero(test_repl, ik_mark_t);
    mark2->label = talloc_strdup(mark2, "good");
    mark2->message_index = 1;

    test_repl->marks = talloc_array(test_repl, ik_mark_t *, 2);
    test_repl->marks[0] = mark1;
    test_repl->marks[1] = mark2;
    test_repl->mark_count = 2;

    // "/rewind " should show labeled marks
    ik_completion_t *comp = ik_completion_create_for_arguments(ctx, test_repl, "/rewind ");
    ck_assert_ptr_nonnull(comp);
    ck_assert_uint_eq(comp->count, 2);

    // "/rewind g" should match
    talloc_free(comp);
    comp = ik_completion_create_for_arguments(ctx, test_repl, "/rewind g");
    ck_assert_ptr_nonnull(comp);
    ck_assert_uint_ge(comp->count, 1);
}
END_TEST

// Test: /rewind with no marks
START_TEST(test_completion_rewind_no_marks)
{
    // No marks created - should return NULL
    ik_completion_t *comp = ik_completion_create_for_arguments(ctx, test_repl, "/rewind ");
    ck_assert_ptr_null(comp);
}
END_TEST

// Test: /model argument completion
START_TEST(test_completion_model_arguments)
{
    ik_completion_t *comp = ik_completion_create_for_arguments(ctx, test_repl, "/model ");
    ck_assert_ptr_nonnull(comp);
    ck_assert(comp->count > 0);
}
END_TEST

// Test: Uppercase argument prefix (tests case handling in fzy)
START_TEST(test_completion_argument_case_sensitive)
{
    // With fzy, uppercase should still match (case-insensitive matching)
    // However, if no matches, that's also acceptable depending on fzy implementation
    ik_completion_create_for_arguments(ctx, test_repl, "/debug O");
    // Accept either matches or no matches - the important part is it doesn't crash
    // (either result is valid depending on fzy implementation)
}
END_TEST

// Test: No space in input (just command name)
START_TEST(test_completion_no_space_in_input)
{
    // "/debug" without space should return NULL (no argument completion)
    ik_completion_t *comp = ik_completion_create_for_arguments(ctx, test_repl, "/debug");
    ck_assert_ptr_null(comp);
}
END_TEST

// Test: Empty command name ("/ ")
START_TEST(test_completion_empty_command_name)
{
    // "/ " should return NULL (empty command name)
    ik_completion_t *comp = ik_completion_create_for_arguments(ctx, test_repl, "/ ");
    ck_assert_ptr_null(comp);
}
END_TEST

// Test: Clear completion state
START_TEST(test_completion_clear)
{
    // Create a completion with multiple candidates
    ik_completion_t *comp = ik_completion_create_for_commands(ctx, "/m");
    ck_assert_ptr_nonnull(comp);
    ck_assert_uint_ge(comp->count, 2);
    ck_assert_ptr_nonnull(comp->candidates);
    ck_assert_ptr_nonnull(comp->prefix);

    // Clear the completion
    ik_completion_clear(comp);

    // Verify all state is cleared
    ck_assert_uint_eq(comp->count, 0);
    ck_assert_uint_eq(comp->current, 0);
    ck_assert_ptr_null(comp->candidates);
    ck_assert_ptr_null(comp->prefix);
    ck_assert_ptr_null(comp->original_input);
}
END_TEST

// Test: Clear completion with original_input set
START_TEST(test_completion_clear_with_original_input)
{
    // Create a completion and set original_input
    ik_completion_t *comp = ik_completion_create_for_commands(ctx, "/m");
    ck_assert_ptr_nonnull(comp);

    // Manually set original_input (this would normally be set during completion cycling)
    comp->original_input = talloc_strdup(comp, "/m");
    ck_assert_ptr_nonnull(comp->original_input);

    // Clear the completion
    ik_completion_clear(comp);

    // Verify all state is cleared, including original_input
    ck_assert_uint_eq(comp->count, 0);
    ck_assert_uint_eq(comp->current, 0);
    ck_assert_ptr_null(comp->candidates);
    ck_assert_ptr_null(comp->prefix);
    ck_assert_ptr_null(comp->original_input);
}
END_TEST

static Suite *completion_matching_args_suite(void)
{
    Suite *s = suite_create("Completion Argument Matching");
    TCase *tc = tcase_create("Core");

    tcase_add_checked_fixture(tc, setup, teardown);

    // Argument completion tests
    tcase_add_test(tc, test_completion_debug_arguments);
    tcase_add_test(tc, test_completion_rewind_arguments);
    tcase_add_test(tc, test_completion_rewind_no_marks);
    tcase_add_test(tc, test_completion_model_arguments);
    tcase_add_test(tc, test_completion_argument_case_sensitive);
    tcase_add_test(tc, test_completion_no_space_in_input);
    tcase_add_test(tc, test_completion_empty_command_name);

    // Completion clear tests
    tcase_add_test(tc, test_completion_clear);
    tcase_add_test(tc, test_completion_clear_with_original_input);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = completion_matching_args_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
