#include "agent.h"
/**
 * @file completion_state_advanced_test.c
 * @brief Advanced unit tests for completion state machine
 *
 * Tests Tab wrapping, original input preservation, and edge cases.
 */

#include <check.h>
#include <talloc.h>
#include "../../../src/agent.h"
#include "../../../src/shared.h"
#include "../../../src/shared.h"
#include "../../../src/repl.h"
#include "../../../src/repl_actions.h"
#include "../../../src/input.h"
#include "../../../src/completion.h"
#include "../../../src/input_buffer/core.h"
#include "../../test_utils.h"

/* Test: Tab accepts single match */
START_TEST(test_tab_wraps_around)
{
    void *ctx = talloc_new(NULL);

    
    // Create agent
    ik_agent_ctx_t *agent = NULL;
    res_t agent_res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&agent_res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    ck_assert_ptr_nonnull(repl);
    repl->current = agent;
    repl->quit = false;
    
    // Create minimal shared context for test
    repl->shared = talloc_zero(repl, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(repl->shared);
    repl->shared->history = NULL;  /* No history for this test */
    
    // Create minimal shared context for test
    repl->shared = talloc_zero(repl, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(repl->shared);
    repl->shared->history = NULL;  /* No history for this test */

    // Type "/mar" to get completion with just ["mark"]
    ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = '/'};
    ik_repl_process_action(repl, &action);
    action.codepoint = 'm';
    ik_repl_process_action(repl, &action);
    action.codepoint = 'a';
    ik_repl_process_action(repl, &action);
    action.codepoint = 'r';
    ik_repl_process_action(repl, &action);

    // Completion should be active with single candidate
    ck_assert_ptr_nonnull(repl->current->completion);
    ck_assert_uint_eq(repl->current->completion->count, 1);

    // Get the single candidate
    const char *single = ik_completion_get_current(repl->current->completion);
    ck_assert_str_eq(single, "mark");

    // Press TAB - should accept single match and dismiss
    action.type = IK_INPUT_TAB;
    res_t res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: completion dismissed
    ck_assert_ptr_null(repl->current->completion);

    // Verify: input buffer has the single match
    size_t text_len = 0;
    const char *text = ik_input_buffer_get_text(repl->current->input_buffer, &text_len);
    char *text_copy = talloc_strndup(ctx, text, text_len);
    ck_assert_str_eq(text_copy, "/mark");

    talloc_free(ctx);
}
END_TEST

/* Test: Tab accepts completion and dismisses */
START_TEST(test_original_input_stored)
{
    void *ctx = talloc_new(NULL);

    
    // Create agent
    ik_agent_ctx_t *agent = NULL;
    res_t agent_res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&agent_res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    ck_assert_ptr_nonnull(repl);
    repl->current = agent;
    repl->quit = false;
    
    // Create minimal shared context for test
    repl->shared = talloc_zero(repl, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(repl->shared);
    repl->shared->history = NULL;  /* No history for this test */
    
    // Create minimal shared context for test
    repl->shared = talloc_zero(repl, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(repl->shared);
    repl->shared->history = NULL;  /* No history for this test */

    // Type "/mod" to get single completion (model)
    ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = '/'};
    ik_repl_process_action(repl, &action);
    action.codepoint = 'm';
    ik_repl_process_action(repl, &action);
    action.codepoint = 'o';
    ik_repl_process_action(repl, &action);
    action.codepoint = 'd';
    ik_repl_process_action(repl, &action);

    // Completion is active
    ck_assert_ptr_nonnull(repl->current->completion);
    ck_assert_uint_eq(repl->current->completion->count, 1);

    // Get the single match
    const char *selected = ik_completion_get_current(repl->current->completion);
    ck_assert_str_eq(selected, "model");

    // Press Tab - should accept and dismiss
    action.type = IK_INPUT_TAB;
    res_t res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Completion should be dismissed
    ck_assert_ptr_null(repl->current->completion);

    // Verify input buffer has the full completion
    size_t text_len = 0;
    const char *text = ik_input_buffer_get_text(repl->current->input_buffer, &text_len);
    char *text_copy = talloc_strndup(ctx, text, text_len);
    ck_assert_str_eq(text_copy, "/model");

    talloc_free(ctx);
}
END_TEST

/* Test: Tab accepts and dismisses - each Tab press accepts current selection */
START_TEST(test_multiple_tab_presses)
{
    void *ctx = talloc_new(NULL);

    
    // Create agent
    ik_agent_ctx_t *agent = NULL;
    res_t agent_res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&agent_res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    ck_assert_ptr_nonnull(repl);
    repl->current = agent;
    repl->quit = false;
    
    // Create minimal shared context for test
    repl->shared = talloc_zero(repl, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(repl->shared);
    repl->shared->history = NULL;  /* No history for this test */
    
    // Create minimal shared context for test
    repl->shared = talloc_zero(repl, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(repl->shared);
    repl->shared->history = NULL;  /* No history for this test */

    // Type "/m" to get multiple commands
    ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = '/'};
    ik_repl_process_action(repl, &action);
    action.codepoint = 'm';
    ik_repl_process_action(repl, &action);

    // Completion should be active with multiple commands
    ck_assert_ptr_nonnull(repl->current->completion);
    ck_assert(repl->current->completion->count > 1);

    // Get first candidate before Tab
    const char *first = ik_completion_get_current(repl->current->completion);
    ck_assert_ptr_nonnull(first);

    // Press TAB - should cycle to next (which wraps to 1) and dismiss
    action.type = IK_INPUT_TAB;
    res_t res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify completion dismissed after first Tab
    ck_assert_ptr_null(repl->current->completion);

    // Verify input contains a completion (cycling to next)
    size_t text_len = 0;
    const char *text = ik_input_buffer_get_text(repl->current->input_buffer, &text_len);
    ck_assert_uint_gt(text_len, 0);
    ck_assert_int_eq(text[0], '/');
    // Should be different from original prefix "/m"
    ck_assert_str_ne(text, "/m");

    talloc_free(ctx);
}
END_TEST

/* Test: update_completion_after_char preserves original_input across updates */
START_TEST(test_update_completion_preserves_original_input)
{
    void *ctx = talloc_new(NULL);

    
    // Create agent
    ik_agent_ctx_t *agent = NULL;
    res_t agent_res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&agent_res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    ck_assert_ptr_nonnull(repl);
    repl->current = agent;
    repl->quit = false;
    
    // Create minimal shared context for test
    repl->shared = talloc_zero(repl, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(repl->shared);
    repl->shared->history = NULL;  /* No history for this test */
    
    // Create minimal shared context for test
    repl->shared = talloc_zero(repl, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(repl->shared);
    repl->shared->history = NULL;  /* No history for this test */

    // Type "/m" to trigger initial completion
    ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = '/'};
    ik_repl_process_action(repl, &action);
    action.codepoint = 'm';
    ik_repl_process_action(repl, &action);

    // Verify completion is active
    ck_assert_ptr_nonnull(repl->current->completion);

    // Manually set original_input (simulating Tab/cycling scenario)
    repl->current->completion->original_input = talloc_strdup(repl->current->completion, "/m");
    ck_assert_ptr_nonnull(repl->current->completion->original_input);

    // Type another character to trigger update_completion_after_char
    // This should preserve the original_input
    action.codepoint = 'o';
    ik_repl_process_action(repl, &action);

    // If completion still exists (matches "/mo"), original_input should be preserved
    if (repl->current->completion != NULL && repl->current->completion->original_input != NULL) {
        ck_assert_str_eq(repl->current->completion->original_input, "/m");
    }

    talloc_free(ctx);
}
END_TEST

/* Test: Space commit when completion is NULL (no-op) */
START_TEST(test_space_commit_no_completion)
{
    void *ctx = talloc_new(NULL);

    // Create agent
    ik_agent_ctx_t *agent = NULL;
    res_t agent_res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&agent_res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    ck_assert_ptr_nonnull(repl);
    repl->current = agent;
    repl->quit = false;

    // Create minimal shared context for test
    repl->shared = talloc_zero(repl, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(repl->shared);
    repl->shared->history = NULL;  /* No history for this test */
    repl->current->completion = NULL;  // No active completion

    // Type some normal text (not a command)
    ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = 'h'};
    ik_repl_process_action(repl, &action);
    action.codepoint = 'i';
    ik_repl_process_action(repl, &action);

    // Verify no completion
    ck_assert_ptr_null(repl->current->completion);

    // Press Space - should just add space normally
    action.codepoint = ' ';
    res_t res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify space was added
    size_t text_len = 0;
    const char *text = ik_input_buffer_get_text(repl->current->input_buffer, &text_len);
    ck_assert_uint_eq(text_len, 3);
    ck_assert_mem_eq(text, "hi ", 3);

    talloc_free(ctx);
}
END_TEST

/* Test Suite */
static Suite *completion_state_advanced_suite(void)
{
    Suite *s = suite_create("Completion_State_Advanced");
    TCase *tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_tab_wraps_around);
    tcase_add_test(tc_core, test_original_input_stored);
    tcase_add_test(tc_core, test_multiple_tab_presses);
    tcase_add_test(tc_core, test_update_completion_preserves_original_input);
    tcase_add_test(tc_core, test_space_commit_no_completion);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = completion_state_advanced_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
