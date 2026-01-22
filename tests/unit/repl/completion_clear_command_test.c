#include "agent.h"
/**
 * @file completion_clear_command_test.c
 * @brief Unit test for /clear command clearing autocomplete state
 *
 * Verifies that executing /clear command properly clears autocomplete suggestions
 * so they don't persist after the command completes.
 */

#include <check.h>
#include "../../../src/agent.h"
#include <talloc.h>
#include "../../../src/shared.h"
#include "../../../src/repl.h"
#include "../../../src/repl_actions.h"
#include "../../../src/input.h"
#include "../../../src/completion.h"
#include "../../../src/input_buffer/core.h"
#include "../../../src/commands.h"
#include "../../../src/scrollback.h"
#include "../../test_utils_helper.h"

/* Test: /clear command clears autocomplete state */
START_TEST(test_clear_command_clears_autocomplete) {
    void *ctx = talloc_new(NULL);

    // Create agent
    ik_agent_ctx_t *agent = NULL;
    res_t agent_res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&agent_res));

    // Create minimal REPL context for testing
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    ck_assert_ptr_nonnull(repl);
    repl->current = agent;

    // Create input buffer
    repl->current->input_buffer = ik_input_buffer_create(ctx);
    ck_assert_ptr_nonnull(repl->current->input_buffer);

    // Override scrollback with test scrollback (80 columns for test)
    talloc_free(agent->scrollback);
    repl->current->scrollback = ik_scrollback_create(ctx, 80);
    ck_assert_ptr_nonnull(repl->current->scrollback);

    // Create minimal shared context
    repl->shared = talloc_zero(repl, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(repl->shared);
    repl->shared->cfg = ik_test_create_config(repl->shared);
    ck_assert_ptr_nonnull(repl->shared->cfg);
    repl->shared->history = NULL;  // No history needed for this test
    repl->shared->db_ctx = NULL;   // No database needed for this test
    repl->shared->session_id = 0;
    repl->shared->db_debug_pipe = NULL;

    repl->quit = false;
    repl->current->completion = NULL;  // Initialize completion to NULL

    agent->messages = NULL; agent->message_count = 0;  // Initialize conversation to NULL

    // Type "/clear" to trigger autocomplete and have a valid command
    ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = '/'};
    ik_repl_process_action(repl, &action);
    action.codepoint = 'c';
    ik_repl_process_action(repl, &action);
    action.codepoint = 'l';
    ik_repl_process_action(repl, &action);
    action.codepoint = 'e';
    ik_repl_process_action(repl, &action);
    action.codepoint = 'a';
    ik_repl_process_action(repl, &action);
    action.codepoint = 'r';
    ik_repl_process_action(repl, &action);

    // Verify autocomplete is active with suggestions
    ck_assert_ptr_nonnull(repl->current->completion);
    ck_assert_uint_gt(repl->current->completion->count, 0);

    // Execute /clear command by simulating Enter key (NEWLINE)
    action.type = IK_INPUT_NEWLINE;
    res_t res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // CRITICAL: Verify autocomplete state is cleared after /clear
    // This is the main assertion - autocomplete should be NULL or have no matches
    if (repl->current->completion != NULL) {
        // If completion still exists, it should have no matches
        ck_assert_uint_eq(repl->current->completion->count, 0);
    }
    // Better yet, completion should be completely cleared (NULL)
    ck_assert_ptr_null(repl->current->completion);

    // Verify the clear command actually executed by checking scrollback was cleared
    // and system message was added
    size_t line_count = ik_scrollback_get_line_count(repl->current->scrollback);
    // After clear, scrollback should only have the system message (if configured)
    // Since we have a cfg with system_message, expect 1 line
    if (repl->shared->cfg->openai_system_message != NULL) {
        ck_assert_uint_ge(line_count, 1);
    }

    talloc_free(ctx);
}
END_TEST

/* Test Suite */
static Suite *completion_clear_command_suite(void)
{
    Suite *s = suite_create("Completion_Clear_Command");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    tcase_add_test(tc_core, test_clear_command_clears_autocomplete);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = completion_clear_command_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/repl/completion_clear_command_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
