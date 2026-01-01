#include "agent.h"
/**
 * @file completion_basic_test.c
 * @brief Unit tests for basic completion functionality
 */

#include <check.h>
#include <talloc.h>
#include "../../../src/agent.h"
#include "../../../src/shared.h"
#include "../../../src/repl.h"
#include "../../../src/shared.h"
#include "../../../src/repl_actions.h"
#include "../../../src/input.h"
#include "../../../src/completion.h"
#include "../../../src/input_buffer/core.h"
#include "../../test_utils.h"

/* Test: Typing "/" triggers completion automatically */
START_TEST(test_tab_triggers_completion) {
    void *ctx = talloc_new(NULL);

    // Create agent
    ik_agent_ctx_t *agent = NULL;
    res_t agent_res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&agent_res));

    // Create REPL context
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    ck_assert_ptr_nonnull(repl);
    repl->current = agent;
    repl->current->completion = NULL;
    repl->quit = false;

    // Create minimal shared context for test
    repl->shared = talloc_zero(repl, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(repl->shared);
    repl->shared->history = NULL;  /* No history for this test */

    // Type "/" (should trigger completion automatically)
    ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = '/'};
    res_t res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: Completion was created after typing "/"
    ck_assert_ptr_nonnull(repl->current->completion);
    ck_assert(repl->current->completion->count > 0);

    // Type "m" to filter
    action.codepoint = 'm';
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: Completion was updated with filtered matches
    ck_assert_ptr_nonnull(repl->current->completion);
    ck_assert(repl->current->completion->count > 0);

    // Verify: Prefix is stored
    ck_assert_str_eq(repl->current->completion->prefix, "/m");

    talloc_free(ctx);
}
END_TEST
/* Test: TAB accepts selection and dismisses completion */
START_TEST(test_tab_accepts_selection) {
    void *ctx = talloc_new(NULL);

    // Create agent
    ik_agent_ctx_t *agent = NULL;
    res_t agent_res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&agent_res));

    // Create REPL context
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

    // Type "/m" - completion is created automatically
    ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = '/'};
    ik_repl_process_action(repl, &action);
    action.codepoint = 'm';
    ik_repl_process_action(repl, &action);

    // Verify completion is active
    ck_assert_ptr_nonnull(repl->current->completion);

    // Press TAB to accept selection
    action.type = IK_INPUT_TAB;
    res_t res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: Completion is dismissed after accepting
    ck_assert_ptr_null(repl->current->completion);

    // Verify: Input buffer was updated with the selection
    size_t text_len = 0;
    const char *text = ik_input_buffer_get_text(repl->current->input_buffer, &text_len);
    ck_assert_uint_gt(text_len, 2);  // At least "/" + something
    ck_assert_int_eq(text[0], '/');

    talloc_free(ctx);
}

END_TEST
/* Test: Arrow up changes selection to previous candidate */
START_TEST(test_arrow_up_changes_selection) {
    void *ctx = talloc_new(NULL);

    // Create agent
    ik_agent_ctx_t *agent = NULL;
    res_t agent_res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&agent_res));

    // Create REPL context
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

    // Type "/m" - completion is created automatically
    ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = '/'};
    ik_repl_process_action(repl, &action);
    action.codepoint = 'm';
    ik_repl_process_action(repl, &action);
    // Completion should now be active (no need for TAB)
    ck_assert_ptr_nonnull(repl->current->completion);

    // Ensure we have multiple candidates
    ck_assert(repl->current->completion->count > 1);

    // Get initial selection (should be index 0)
    ck_assert_uint_eq(repl->current->completion->current, 0);
    const char *first_candidate = ik_completion_get_current(repl->current->completion);

    // Press Arrow Up (should wrap to last candidate)
    action.type = IK_INPUT_ARROW_UP;
    res_t res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: Selection changed to last candidate
    ck_assert_uint_eq(repl->current->completion->current, repl->current->completion->count - 1);
    const char *last_candidate = ik_completion_get_current(repl->current->completion);
    ck_assert_str_ne(last_candidate, first_candidate);

    // Verify: Completion still active
    ck_assert_ptr_nonnull(repl->current->completion);

    talloc_free(ctx);
}

END_TEST
/* Test: Arrow down changes selection to next candidate */
START_TEST(test_arrow_down_changes_selection) {
    void *ctx = talloc_new(NULL);

    // Create agent
    ik_agent_ctx_t *agent = NULL;
    res_t agent_res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&agent_res));

    // Create REPL context
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

    // Type "/m" - completion is created automatically
    ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = '/'};
    ik_repl_process_action(repl, &action);
    action.codepoint = 'm';
    ik_repl_process_action(repl, &action);
    // Completion should now be active (no need for TAB)
    ck_assert_ptr_nonnull(repl->current->completion);

    // Ensure we have multiple candidates
    ck_assert(repl->current->completion->count > 1);

    // Get initial selection (index 0)
    ck_assert_uint_eq(repl->current->completion->current, 0);
    const char *first_candidate = ik_completion_get_current(repl->current->completion);

    // Press Arrow Down
    action.type = IK_INPUT_ARROW_DOWN;
    res_t res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: Selection moved to next (index 1)
    ck_assert_uint_eq(repl->current->completion->current, 1);
    const char *second_candidate = ik_completion_get_current(repl->current->completion);
    ck_assert_str_ne(second_candidate, first_candidate);

    // Verify: Completion still active
    ck_assert_ptr_nonnull(repl->current->completion);

    talloc_free(ctx);
}

END_TEST
/* Test: Escape dismisses completion without accepting */
START_TEST(test_escape_dismisses_completion) {
    void *ctx = talloc_new(NULL);

    // Create agent
    ik_agent_ctx_t *agent = NULL;
    res_t agent_res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&agent_res));

    // Create REPL context
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

    // Type "/m" - completion is created automatically
    ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = '/'};
    ik_repl_process_action(repl, &action);
    action.codepoint = 'm';
    ik_repl_process_action(repl, &action);
    // Completion should now be active (no need for TAB)
    ck_assert_ptr_nonnull(repl->current->completion);

    // Verify original input before Escape
    size_t original_len = 0;
    ik_input_buffer_get_text(repl->current->input_buffer, &original_len);
    ck_assert_uint_eq(original_len, 2); // "/m"

    // Press Escape
    action.type = IK_INPUT_ESCAPE;
    res_t res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: Completion dismissed
    ck_assert_ptr_null(repl->current->completion);

    // Verify: Input buffer unchanged
    size_t text_len = 0;
    const char *text = ik_input_buffer_get_text(repl->current->input_buffer, &text_len);
    ck_assert_uint_eq(text_len, 2);
    ck_assert_mem_eq(text, "/m", 2);

    talloc_free(ctx);
}

END_TEST
/* Test: Typing updates completion dynamically */
START_TEST(test_typing_updates_completion) {
    void *ctx = talloc_new(NULL);

    // Create agent
    ik_agent_ctx_t *agent = NULL;
    res_t agent_res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&agent_res));

    // Create REPL context
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

    // Type "/m" - completion is created automatically
    ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = '/'};
    ik_repl_process_action(repl, &action);
    action.codepoint = 'm';
    ik_repl_process_action(repl, &action);
    // Completion should now be active (no need for TAB)
    ck_assert_ptr_nonnull(repl->current->completion);

    size_t initial_count = repl->current->completion->count;

    // Type 'o' to narrow to "/mo" (should match "model" only)
    action.type = IK_INPUT_CHAR;
    action.codepoint = 'o';
    res_t res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: Completion was updated (new prefix)
    ck_assert_ptr_nonnull(repl->current->completion);
    ck_assert_str_eq(repl->current->completion->prefix, "/mo");

    // Verify: Candidate count changed (narrower match)
    ck_assert(repl->current->completion->count < initial_count);

    talloc_free(ctx);
}

END_TEST

static Suite *completion_navigation_suite(void)
{
    Suite *s = suite_create("Completion Basic");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);
    tcase_set_timeout(tc_core, 30);
    tcase_set_timeout(tc_core, 30);
    tcase_set_timeout(tc_core, 30);
    tcase_set_timeout(tc_core, 30);

    tcase_add_test(tc_core, test_tab_triggers_completion);
    tcase_add_test(tc_core, test_tab_accepts_selection);
    tcase_add_test(tc_core, test_arrow_up_changes_selection);
    tcase_add_test(tc_core, test_arrow_down_changes_selection);
    tcase_add_test(tc_core, test_escape_dismisses_completion);
    tcase_add_test(tc_core, test_typing_updates_completion);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = completion_navigation_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
