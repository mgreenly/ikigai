#include "agent.h"
/**
 * @file completion_trigger_test.c
 * @brief Unit tests for completion trigger on typing slash (not Tab)
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

/* Test: Typing "/" triggers completion display with all commands */
START_TEST(test_typing_slash_triggers_completion)
{
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

    // Type "/" - completion should activate with all commands
    ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = '/'};
    res_t res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify completion was created
    ck_assert_ptr_nonnull(repl->current->completion);
    // Should have all 12 commands: clear, debug, fork, help, kill, mark, model, rewind, send, check-mail, read-mail, system
    ck_assert_uint_eq(repl->current->completion->count, 12);

    talloc_free(ctx);
}
END_TEST

/* Test: Typing "/m" filters to matching commands */
START_TEST(test_typing_m_after_slash_filters)
{
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

    // Type "/" to trigger completion
    ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = '/'};
    res_t res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));
    size_t initial_count = repl->current->completion->count;

    // Type "m" to filter
    action.codepoint = 'm';
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify completion was updated
    ck_assert_ptr_nonnull(repl->current->completion);
    // Should have fewer matches than all commands
    ck_assert(repl->current->completion->count < initial_count);
    // Should have at least 1 match (mark and/or model)
    ck_assert(repl->current->completion->count > 0);

    talloc_free(ctx);
}
END_TEST

/* Test: Typing regular text without slash has no completion */
START_TEST(test_typing_regular_text_no_completion)
{
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

    // Type "hello" without slash - no completion
    ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = 'h'};
    res_t res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));
    action.codepoint = 'e';
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify no completion
    ck_assert_ptr_null(repl->current->completion);

    talloc_free(ctx);
}
END_TEST

/* Test: Backspace refilters completion */
START_TEST(test_backspace_refilters)
{
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

    // Type "/" to trigger completion
    ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = '/'};
    res_t res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));
    size_t slash_count = repl->current->completion->count;

    // Type "m" to narrow
    action.codepoint = 'm';
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));
    size_t m_count = repl->current->completion->count;
    ck_assert(m_count < slash_count);

    // Type "a" to narrow further
    action.codepoint = 'a';
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));
    size_t ma_count = repl->current->completion->count;
    ck_assert(ma_count <= m_count);

    // Backspace to return to "/m"
    action.type = IK_INPUT_BACKSPACE;
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify completion re-filtered back to "/m" state
    ck_assert_ptr_nonnull(repl->current->completion);
    ck_assert_uint_eq(repl->current->completion->count, m_count);

    talloc_free(ctx);
}
END_TEST

/* Test: Tab accepts completion and dismisses */
START_TEST(test_tab_cycles_without_triggering)
{
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

    // Type "/" to trigger completion
    ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = '/'};
    res_t res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(repl->current->completion);

    // Get initial selection
    ck_assert_uint_eq(repl->current->completion->current, 0);

    // Press TAB to accept and dismiss completion
    action.type = IK_INPUT_TAB;
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify completion is dismissed after Tab accept
    ck_assert_ptr_null(repl->current->completion);

    // Verify input contains "/" + something (cycled to next)
    size_t text_len = 0;
    const char *text = ik_input_buffer_get_text(repl->current->input_buffer, &text_len);
    ck_assert(text_len > 0);
    ck_assert_int_eq(text[0], '/');

    talloc_free(ctx);
}
END_TEST

/* Test: Empty slash followed by typing characters */
START_TEST(test_empty_slash_then_typing)
{
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

    // Type "/" alone
    ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = '/'};
    res_t res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(repl->current->completion);
    size_t initial_count = repl->current->completion->count;

    // Type "d" for "debug"
    action.codepoint = 'd';
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify completion was filtered
    ck_assert_ptr_nonnull(repl->current->completion);
    ck_assert(repl->current->completion->count < initial_count);

    talloc_free(ctx);
}
END_TEST

static Suite *completion_trigger_suite(void)
{
    Suite *s = suite_create("Completion_Trigger");
    TCase *tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_typing_slash_triggers_completion);
    tcase_add_test(tc_core, test_typing_m_after_slash_filters);
    tcase_add_test(tc_core, test_typing_regular_text_no_completion);
    tcase_add_test(tc_core, test_backspace_refilters);
    tcase_add_test(tc_core, test_tab_cycles_without_triggering);
    tcase_add_test(tc_core, test_empty_slash_then_typing);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = completion_trigger_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
