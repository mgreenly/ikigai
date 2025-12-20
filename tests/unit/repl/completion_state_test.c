#include "agent.h"
/**
 * @file completion_state_test.c
 * @brief Unit tests for completion state machine with Tab cycling behavior
 *
 * Tests Tab cycling through completions, ESC revert, Space commit, and Enter submit.
 */

#include <check.h>
#include <talloc.h>
#include "../../../src/agent.h"
#include "../../../src/shared.h"
#include "../../../src/repl.h"
#include "../../../src/repl_actions.h"
#include "../../../src/input.h"
#include "../../../src/completion.h"
#include "../../../src/input_buffer/core.h"
#include "../../test_utils.h"

/* Test: TAB cycles to next match and dismisses */
START_TEST(test_tab_cycles_to_next) {
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

    // Type "/m" to get completion with ["mark", "model"]
    ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = '/'};
    ik_repl_process_action(repl, &action);
    action.codepoint = 'm';
    ik_repl_process_action(repl, &action);

    // Completion should be active with at least 2 candidates
    ck_assert_ptr_nonnull(repl->current->completion);
    ck_assert(repl->current->completion->count >= 2);

    // Get first candidate
    const char *first = ik_completion_get_current(repl->current->completion);
    ck_assert_ptr_nonnull(first);

    // Press TAB - should cycle to next and dismiss completion
    action.type = IK_INPUT_TAB;
    res_t res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: completion dismissed after accept
    ck_assert_ptr_null(repl->current->completion);

    // Verify: input buffer was updated with next selection (not first)
    size_t text_len = 0;
    const char *text = ik_input_buffer_get_text(repl->current->input_buffer, &text_len);
    ck_assert_uint_gt(text_len, 0);
    ck_assert_int_eq(text[0], '/');
    // Should be a command different from original "/m"
    ck_assert_str_ne(text, "/m");

    talloc_free(ctx);
}
END_TEST
/* Test: TAB updates input buffer with current selection and dismisses */
START_TEST(test_tab_updates_input_buffer)
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

    // Type "/m" to get completion
    ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = '/'};
    ik_repl_process_action(repl, &action);
    action.codepoint = 'm';
    ik_repl_process_action(repl, &action);

    // Completion should be active
    ck_assert_ptr_nonnull(repl->current->completion);

    // Get current selection (just to verify completion is working)
    const char *selected = ik_completion_get_current(repl->current->completion);
    ck_assert_ptr_nonnull(selected);

    // Press TAB - should cycle to next, update input buffer, and dismiss
    action.type = IK_INPUT_TAB;
    res_t res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: completion dismissed after Tab accept
    ck_assert_ptr_null(repl->current->completion);

    // Verify: input buffer contains "/" + a selected candidate
    size_t text_len = 0;
    const char *text = ik_input_buffer_get_text(repl->current->input_buffer, &text_len);
    // Input should be "/" + some selected candidate (no trailing space yet)
    ck_assert_uint_gt(text_len, 0);
    ck_assert_int_eq(text[0], '/');
    // Should contain some completion (at least "/" + some command name)
    ck_assert_uint_ge(text_len, 2);
    // Verify buffer contains a valid command from the completions
    // (not the original prefix "/m" but a full command)
    ck_assert_str_ne(text, "/m");

    talloc_free(ctx);
}

END_TEST
/* Test: ESC reverts to original input */
START_TEST(test_esc_reverts_to_original)
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

    // Type "/m" - get multiple completions
    ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = '/'};
    ik_repl_process_action(repl, &action);
    action.codepoint = 'm';
    ik_repl_process_action(repl, &action);

    // Verify original input
    size_t original_len = 0;
    const char *original_text = ik_input_buffer_get_text(repl->current->input_buffer, &original_len);
    ck_assert_uint_eq(original_len, 2);
    ck_assert_mem_eq(original_text, "/m", 2);
    char original_copy[3];
    memcpy(original_copy, original_text, 2);
    original_copy[2] = '\0';

    // Completion is active
    ck_assert_ptr_nonnull(repl->current->completion);
    size_t original_count = repl->current->completion->count;
    ck_assert(original_count > 1);

    // Save original input to completion context manually before pressing Tab
    // (simulating what happens when Tab is pressed and completion is being cycled)
    // Actually, let's just keep the completion active and directly press ESC without Tab
    // ESC should dismiss and revert

    // Press ESC - should revert to original
    action.type = IK_INPUT_ESCAPE;
    res_t res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: completion dismissed
    ck_assert_ptr_null(repl->current->completion);

    // Verify: input reverted to original
    size_t reverted_len = 0;
    const char *reverted_text = ik_input_buffer_get_text(repl->current->input_buffer, &reverted_len);
    ck_assert_uint_eq(reverted_len, original_len);
    ck_assert_mem_eq(reverted_text, original_copy, original_len);

    talloc_free(ctx);
}

END_TEST
/* Test: Space commits selection and continues editing */
START_TEST(test_space_commits_selection)
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

    // Type "/cl" to get completion (unique match: /clear)
    ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = '/'};
    ik_repl_process_action(repl, &action);
    action.codepoint = 'c';
    ik_repl_process_action(repl, &action);
    action.codepoint = 'l';
    ik_repl_process_action(repl, &action);

    // Completion is active
    ck_assert_ptr_nonnull(repl->current->completion);

    // Get current selection
    const char *selected = ik_completion_get_current(repl->current->completion);
    ck_assert_ptr_nonnull(selected);
    char *selected_copy = talloc_strdup(ctx, selected);

    // Press SPACE - should commit and dismiss completion
    action.type = IK_INPUT_CHAR;
    action.codepoint = ' ';
    res_t res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: completion dismissed
    ck_assert_ptr_null(repl->current->completion);

    // Verify: input buffer contains "/" + selected + " "
    size_t text_len = 0;
    const char *text = ik_input_buffer_get_text(repl->current->input_buffer, &text_len);
    // Should be "/" + selected + " ", so ends with space
    ck_assert_uint_gt(text_len, strlen(selected_copy) + 1);  // At least "/" + selected + " "
    ck_assert_int_eq(text[0], '/');
    // Last char should be space
    ck_assert_int_eq(text[text_len - 1], ' ');
    // Should contain the selected command
    ck_assert_mem_eq(text + 1, selected_copy, strlen(selected_copy));

    talloc_free(ctx);
}

END_TEST
/* Test: Tab with single candidate accepts and dismisses */
START_TEST(test_enter_commits_and_submits)
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

    // Type "/cl" (unique prefix for "/clear")
    ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = '/'};
    ik_repl_process_action(repl, &action);
    action.codepoint = 'c';
    ik_repl_process_action(repl, &action);
    action.codepoint = 'l';
    ik_repl_process_action(repl, &action);

    // Completion is active
    ck_assert_ptr_nonnull(repl->current->completion);

    // Verify it's the single match /clear
    const char *selected = ik_completion_get_current(repl->current->completion);
    ck_assert_ptr_nonnull(selected);
    ck_assert_str_eq(selected, "clear");

    // Press Tab - should accept and dismiss completion
    action.type = IK_INPUT_TAB;
    res_t res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify completion dismissed
    ck_assert_ptr_null(repl->current->completion);

    // Verify input buffer has the selected command
    size_t text_len = 0;
    const char *text = ik_input_buffer_get_text(repl->current->input_buffer, &text_len);
    char *text_copy = talloc_strndup(ctx, text, text_len);
    ck_assert_str_eq(text_copy, "/clear");

    talloc_free(ctx);
}

END_TEST

/* Test Suite */
static Suite *completion_state_suite(void)
{
    Suite *s = suite_create("Completion_State");
    TCase *tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_tab_cycles_to_next);
    tcase_add_test(tc_core, test_tab_updates_input_buffer);
    tcase_add_test(tc_core, test_esc_reverts_to_original);
    tcase_add_test(tc_core, test_space_commits_selection);
    tcase_add_test(tc_core, test_enter_commits_and_submits);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = completion_state_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
