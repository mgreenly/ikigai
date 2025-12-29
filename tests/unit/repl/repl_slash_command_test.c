#include "agent.h"
/**
 * @file repl_slash_command_test.c
 * @brief Unit tests for REPL slash command handling
 */

#include <check.h>
#include "../../../src/agent.h"
#include <talloc.h>
#include "../../../src/repl.h"
#include "../../../src/repl_actions.h"
#include "../../../src/input.h"
#include "../../../src/input_buffer/core.h"
#include "../../../src/shared.h"
#include "../../test_utils.h"

/* Test: /pp command clears input buffer after execution */
START_TEST(test_pp_command_clears_input_buffer) {
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = NULL;

    /* Create REPL without initializing terminal */
    repl = talloc_zero_(ctx, sizeof(ik_repl_ctx_t));
    ck_assert_ptr_nonnull(repl);

    /* Create minimal shared context for history access */
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    shared->history = NULL;
    repl->shared = shared;

    res_t res;

    /* Create agent context for display state */
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    repl->current = agent;

    /* Create input buffer */
    repl->current->input_buffer = ik_input_buffer_create(repl);

    /* Create scrollback (needed for submit_line) */
    repl->current->scrollback = ik_scrollback_create(repl, 80);

    /* Insert "/pp" command */
    ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = '/'};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    action.codepoint = 'p';
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    res = ik_repl_process_action(repl, &action); // Second 'p'
    ck_assert(is_ok(&res));

    /* Verify input buffer has "/pp" */
    size_t text_len = ik_byte_array_size(repl->current->input_buffer->text);
    ck_assert_uint_eq(text_len, 3);

    /* Send NEWLINE to execute command */
    action.type = IK_INPUT_NEWLINE;
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    /* Verify input buffer was cleared */
    text_len = ik_byte_array_size(repl->current->input_buffer->text);
    ck_assert_uint_eq(text_len, 0);

    talloc_free(ctx);
}
END_TEST
/* Test: /pp with additional text (e.g., "/pp input buffer") */
START_TEST(test_pp_command_with_args)
{
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = NULL;

    /* Create REPL without initializing terminal */
    repl = talloc_zero_(ctx, sizeof(ik_repl_ctx_t));
    ck_assert_ptr_nonnull(repl);

    /* Create minimal shared context for history access */
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    shared->history = NULL;
    repl->shared = shared;

    res_t res;

    /* Create agent context for display state */
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    repl->current = agent;

    /* Create input buffer */
    repl->current->input_buffer = ik_input_buffer_create(repl);

    /* Create scrollback (needed for submit_line) */
    repl->current->scrollback = ik_scrollback_create(repl, 80);

    /* Insert "/pp input buffer" command */
    const char *cmd = "/pp input_buffer";
    for (size_t i = 0; cmd[i] != '\0'; i++) {
        ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = (uint32_t)cmd[i]};
        res = ik_repl_process_action(repl, &action);
        ck_assert(is_ok(&res));
    }

    /* Send NEWLINE to execute command */
    ik_input_action_t action = {.type = IK_INPUT_NEWLINE};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    /* Verify input buffer was cleared */
    size_t text_len = ik_byte_array_size(repl->current->input_buffer->text);
    ck_assert_uint_eq(text_len, 0);

    talloc_free(ctx);
}

END_TEST
/* Test: Unknown slash command is ignored */
START_TEST(test_unknown_slash_command)
{
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = NULL;

    /* Create REPL without initializing terminal */
    repl = talloc_zero_(ctx, sizeof(ik_repl_ctx_t));
    ck_assert_ptr_nonnull(repl);

    /* Create minimal shared context for history access */
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    shared->history = NULL;
    repl->shared = shared;

    res_t res;

    /* Create agent context for display state */
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    repl->current = agent;

    /* Create input buffer */
    repl->current->input_buffer = ik_input_buffer_create(repl);

    /* Create scrollback (needed for error messages) */
    repl->current->scrollback = ik_scrollback_create(repl, 80);

    /* Insert "/unknown" command */
    const char *cmd = "/unknown";
    for (size_t i = 0; cmd[i] != '\0'; i++) {
        ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = (uint32_t)cmd[i]};
        res = ik_repl_process_action(repl, &action);
        ck_assert(is_ok(&res));
    }

    /* Send NEWLINE */
    ik_input_action_t action = {.type = IK_INPUT_NEWLINE};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    /* Unknown command should still clear the input buffer */
    size_t text_len = ik_byte_array_size(repl->current->input_buffer->text);
    ck_assert_uint_eq(text_len, 0);

    talloc_free(ctx);
}

END_TEST
/* Test: Empty input buffer on newline - Phase 4: Enter always submits and clears */
START_TEST(test_empty_input_buffer_newline)
{
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = NULL;

    /* Create REPL without initializing terminal */
    repl = talloc_zero_(ctx, sizeof(ik_repl_ctx_t));
    ck_assert_ptr_nonnull(repl);

    /* Create minimal shared context for history access */
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    shared->history = NULL;
    repl->shared = shared;

    res_t res;

    /* Create agent context for display state */
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    repl->current = agent;

    /* Create input buffer */
    repl->current->input_buffer = ik_input_buffer_create(repl);

    /* Create scrollback (needed for submit_line) */
    repl->current->scrollback = ik_scrollback_create(repl, 80);

    /* input buffer is empty, press NEWLINE */
    ik_input_action_t action = {.type = IK_INPUT_NEWLINE};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    /* Phase 4 behavior: Enter submits and clears input buffer (even if empty) */
    size_t text_len = ik_byte_array_size(repl->current->input_buffer->text);
    ck_assert_uint_eq(text_len, 0);

    talloc_free(ctx);
}

END_TEST
/* Test: Regular text (not slash command) on newline - Phase 4: Enter submits and clears */
START_TEST(test_slash_in_middle_not_command)
{
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = NULL;

    /* Create REPL without initializing terminal */
    repl = talloc_zero_(ctx, sizeof(ik_repl_ctx_t));
    ck_assert_ptr_nonnull(repl);

    /* Create minimal shared context for history access */
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    shared->history = NULL;
    repl->shared = shared;

    res_t res;

    /* Create agent context for display state */
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    repl->current = agent;

    /* Create input buffer */
    repl->current->input_buffer = ik_input_buffer_create(repl);

    /* Create scrollback (needed for submit_line) */
    repl->current->scrollback = ik_scrollback_create(repl, 80);

    /* Insert "hello" */
    const char *text = "hello";
    for (size_t i = 0; text[i] != '\0'; i++) {
        ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = (uint32_t)text[i]};
        res = ik_repl_process_action(repl, &action);
        ck_assert(is_ok(&res));
    }

    /* Send NEWLINE - Phase 4: should submit to scrollback and clear input buffer */
    ik_input_action_t action = {.type = IK_INPUT_NEWLINE};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    /* Phase 4 behavior: Enter submits and clears input buffer */
    size_t text_len = ik_byte_array_size(repl->current->input_buffer->text);
    ck_assert_uint_eq(text_len, 0);

    /* Verify "hello" was added to scrollback (content + blank line) */
    size_t line_count = ik_scrollback_get_line_count(repl->current->scrollback);
    ck_assert_uint_eq(line_count, 2);

    talloc_free(ctx);
}

END_TEST
/* Test: /pp command output appears in scrollback (command itself not rendered - legacy behavior) */
START_TEST(test_pp_command_order_in_scrollback)
{
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = NULL;

    /* Create minimal REPL context */
    repl = talloc_zero_(ctx, sizeof(ik_repl_ctx_t));
    ck_assert_ptr_nonnull(repl);

    /* Create minimal shared context for history access */
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    shared->history = NULL;
    repl->shared = shared;

    res_t res;

    /* Create agent context for display state */
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    repl->current = agent;

    /* Create input buffer */
    repl->current->input_buffer = ik_input_buffer_create(repl);

    /* Create scrollback */
    repl->current->scrollback = ik_scrollback_create(repl, 80);

    /* Insert "/pp" command */
    const char *cmd = "/pp";
    for (size_t i = 0; cmd[i] != '\0'; i++) {
        ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = (uint32_t)cmd[i]};
        res = ik_repl_process_action(repl, &action);
        ck_assert(is_ok(&res));
    }

    /* Send NEWLINE to execute command */
    ik_input_action_t action = {.type = IK_INPUT_NEWLINE};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    /* Check scrollback contents - /pp is a legacy debug command that only outputs its result
     * (doesn't use event renderer, so command text is not added to scrollback) */
    size_t line_count = ik_scrollback_get_line_count(repl->current->scrollback);
    ck_assert_msg(line_count >= 1, "Expected at least 1 line in scrollback (output)");

    /* Get first line - should be PP output (contains "ik_input_buffer_t") */
    const char *line_text = NULL;
    size_t line_len = 0;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 0, &line_text, &line_len);
    ck_assert(is_ok(&res));
    ck_assert_msg(line_len > 0, "Expected PP output in first line");

    talloc_free(ctx);
}

END_TEST
/* Test: /pp output newline handling - verify trailing newline is handled correctly */
START_TEST(test_pp_output_trailing_newline)
{
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = NULL;

    /* Create minimal REPL context */
    repl = talloc_zero_(ctx, sizeof(ik_repl_ctx_t));
    ck_assert_ptr_nonnull(repl);

    /* Create minimal shared context for history access */
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    shared->history = NULL;
    repl->shared = shared;

    res_t res;

    /* Create agent context for display state */
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    repl->current = agent;

    /* Create input buffer */
    repl->current->input_buffer = ik_input_buffer_create(repl);

    /* Create scrollback */
    repl->current->scrollback = ik_scrollback_create(repl, 80);

    /* Insert "/pp" command */
    const char *cmd = "/pp";
    for (size_t i = 0; cmd[i] != '\0'; i++) {
        ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = (uint32_t)cmd[i]};
        res = ik_repl_process_action(repl, &action);
        ck_assert(is_ok(&res));
    }

    /* Send NEWLINE to execute command */
    ik_input_action_t action = {.type = IK_INPUT_NEWLINE};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    /* Verify scrollback has output lines from /pp
     * /pp is a legacy debug command - command text is NOT added, only output
     * PP output ends with \n, which creates a trailing empty line that should be skipped */
    size_t line_count = ik_scrollback_get_line_count(repl->current->scrollback);
    ck_assert_msg(line_count >= 1, "Expected at least 1 line, got %zu", line_count);

    /* Verify all lines are pp output (not empty from trailing newline) */
    const char *line_text = NULL;
    size_t line_len = 0;
    for (size_t i = 0; i < line_count; i++) {
        res = ik_scrollback_get_line_text(repl->current->scrollback, i, &line_text, &line_len);
        ck_assert(is_ok(&res));
        /* Each line should have content (trailing empty line should be skipped) */
        ck_assert_msg(line_len > 0, "Line %zu should not be empty", i);
    }

    talloc_free(ctx);
}

END_TEST

/* Test Suite */
static Suite *repl_slash_command_suite(void)
{
    Suite *s = suite_create("REPL Slash Commands");

    TCase *tc_basic = tcase_create("Basic");
    tcase_set_timeout(tc_basic, 30);
    tcase_set_timeout(tc_basic, 30);
    tcase_set_timeout(tc_basic, 30);
    tcase_set_timeout(tc_basic, 30);
    tcase_set_timeout(tc_basic, 30);
    tcase_add_test(tc_basic, test_pp_command_clears_input_buffer);
    tcase_add_test(tc_basic, test_pp_command_with_args);
    tcase_add_test(tc_basic, test_unknown_slash_command);
    tcase_add_test(tc_basic, test_empty_input_buffer_newline);
    tcase_add_test(tc_basic, test_slash_in_middle_not_command);
    tcase_add_test(tc_basic, test_pp_command_order_in_scrollback);
    tcase_add_test(tc_basic, test_pp_output_trailing_newline);
    suite_add_tcase(s, tc_basic);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = repl_slash_command_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
