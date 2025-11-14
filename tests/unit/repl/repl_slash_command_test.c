/**
 * @file repl_slash_command_test.c
 * @brief Unit tests for REPL slash command handling
 */

#include <check.h>
#include <talloc.h>
#include "../../../src/repl.h"
#include "../../../src/input.h"
#include "../../../src/workspace.h"
#include "../../test_utils.h"

/* Test: /pp command clears workspace after execution */
START_TEST(test_pp_command_clears_workspace) {
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = NULL;

    /* Create REPL without initializing terminal */
    repl = ik_talloc_zero_wrapper(ctx, sizeof(ik_repl_ctx_t));
    ck_assert_ptr_nonnull(repl);

    /* Create workspace */
    res_t res = ik_workspace_create(repl, &repl->workspace);
    ck_assert(is_ok(&res));

    /* Insert "/pp" command */
    ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = '/'};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    action.codepoint = 'p';
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    res = ik_repl_process_action(repl, &action); // Second 'p'
    ck_assert(is_ok(&res));

    /* Verify workspace has "/pp" */
    size_t text_len = ik_byte_array_size(repl->workspace->text);
    ck_assert_uint_eq(text_len, 3);

    /* Send NEWLINE to execute command */
    action.type = IK_INPUT_NEWLINE;
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    /* Verify workspace was cleared */
    text_len = ik_byte_array_size(repl->workspace->text);
    ck_assert_uint_eq(text_len, 0);

    talloc_free(ctx);
}
END_TEST
/* Test: /pp with additional text (e.g., "/pp workspace") */
START_TEST(test_pp_command_with_args)
{
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = NULL;

    /* Create REPL without initializing terminal */
    repl = ik_talloc_zero_wrapper(ctx, sizeof(ik_repl_ctx_t));
    ck_assert_ptr_nonnull(repl);

    /* Create workspace */
    res_t res = ik_workspace_create(repl, &repl->workspace);
    ck_assert(is_ok(&res));

    /* Insert "/pp workspace" command */
    const char *cmd = "/pp workspace";
    for (size_t i = 0; cmd[i] != '\0'; i++) {
        ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = (uint32_t)cmd[i]};
        res = ik_repl_process_action(repl, &action);
        ck_assert(is_ok(&res));
    }

    /* Send NEWLINE to execute command */
    ik_input_action_t action = {.type = IK_INPUT_NEWLINE};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    /* Verify workspace was cleared */
    size_t text_len = ik_byte_array_size(repl->workspace->text);
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
    repl = ik_talloc_zero_wrapper(ctx, sizeof(ik_repl_ctx_t));
    ck_assert_ptr_nonnull(repl);

    /* Create workspace */
    res_t res = ik_workspace_create(repl, &repl->workspace);
    ck_assert(is_ok(&res));

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

    /* Unknown command should still clear the workspace */
    size_t text_len = ik_byte_array_size(repl->workspace->text);
    ck_assert_uint_eq(text_len, 0);

    talloc_free(ctx);
}

END_TEST
/* Test: Empty workspace on newline */
START_TEST(test_empty_workspace_newline)
{
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = NULL;

    /* Create REPL without initializing terminal */
    repl = ik_talloc_zero_wrapper(ctx, sizeof(ik_repl_ctx_t));
    ck_assert_ptr_nonnull(repl);

    /* Create workspace */
    res_t res = ik_workspace_create(repl, &repl->workspace);
    ck_assert(is_ok(&res));

    /* Workspace is empty, press NEWLINE */
    ik_input_action_t action = {.type = IK_INPUT_NEWLINE};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    /* Workspace should now have "\n" */
    size_t text_len = ik_byte_array_size(repl->workspace->text);
    ck_assert_uint_eq(text_len, 1);

    talloc_free(ctx);
}

END_TEST
/* Test: Regular text starting with / but not a command on newline */
START_TEST(test_slash_in_middle_not_command)
{
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = NULL;

    /* Create REPL without initializing terminal */
    repl = ik_talloc_zero_wrapper(ctx, sizeof(ik_repl_ctx_t));
    ck_assert_ptr_nonnull(repl);

    /* Create workspace */
    res_t res = ik_workspace_create(repl, &repl->workspace);
    ck_assert(is_ok(&res));

    /* Insert "hello" */
    const char *text = "hello";
    for (size_t i = 0; text[i] != '\0'; i++) {
        ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = (uint32_t)text[i]};
        res = ik_repl_process_action(repl, &action);
        ck_assert(is_ok(&res));
    }

    /* Send NEWLINE - should insert newline, not execute command */
    ik_input_action_t action = {.type = IK_INPUT_NEWLINE};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    /* Workspace should now have "hello\n" */
    size_t text_len = ik_byte_array_size(repl->workspace->text);
    ck_assert_uint_eq(text_len, 6);

    talloc_free(ctx);
}

END_TEST
/* Test: OOM when creating format buffer in slash command handler */
START_TEST(test_pp_command_oom_format_buffer)
{
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = NULL;

    /* Create REPL without initializing terminal */
    repl = ik_talloc_zero_wrapper(ctx, sizeof(ik_repl_ctx_t));
    ck_assert_ptr_nonnull(repl);

    /* Create workspace */
    res_t res = ik_workspace_create(repl, &repl->workspace);
    ck_assert(is_ok(&res));

    /* Insert "/pp" command */
    const char *cmd = "/pp";
    for (size_t i = 0; cmd[i] != '\0'; i++) {
        ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = (uint32_t)cmd[i]};
        res = ik_repl_process_action(repl, &action);
        ck_assert(is_ok(&res));
    }

    /* Inject OOM to fail after command string allocation (fail inside handler) */
    // Fail on 2nd allocation: 1st is command string, 2nd is format buffer
    oom_test_fail_after_n_calls(2);

    /* Send NEWLINE - should fail with OOM from format buffer creation */
    ik_input_action_t action = {.type = IK_INPUT_NEWLINE};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_OOM);

    oom_test_reset();
    talloc_free(ctx);
}

END_TEST
/* Test: OOM when allocating command string */
START_TEST(test_pp_command_oom_command_string)
{
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = NULL;

    /* Create REPL without initializing terminal */
    repl = ik_talloc_zero_wrapper(ctx, sizeof(ik_repl_ctx_t));
    ck_assert_ptr_nonnull(repl);

    /* Create workspace */
    res_t res = ik_workspace_create(repl, &repl->workspace);
    ck_assert(is_ok(&res));

    /* Insert "/pp" command */
    const char *cmd = "/pp";
    for (size_t i = 0; cmd[i] != '\0'; i++) {
        ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = (uint32_t)cmd[i]};
        res = ik_repl_process_action(repl, &action);
        ck_assert(is_ok(&res));
    }

    /* Inject OOM to fail talloc_strndup (command string allocation) */
    // Note: talloc_strndup uses talloc internally, which will be intercepted
    oom_test_fail_next_alloc();

    /* Send NEWLINE - should fail with OOM */
    ik_input_action_t action = {.type = IK_INPUT_NEWLINE};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_OOM);

    oom_test_reset();
    talloc_free(ctx);
}

END_TEST

/* Test Suite */
static Suite *repl_slash_command_suite(void)
{
    Suite *s = suite_create("REPL Slash Commands");

    TCase *tc_basic = tcase_create("Basic");
    tcase_add_test(tc_basic, test_pp_command_clears_workspace);
    tcase_add_test(tc_basic, test_pp_command_with_args);
    tcase_add_test(tc_basic, test_unknown_slash_command);
    tcase_add_test(tc_basic, test_empty_workspace_newline);
    tcase_add_test(tc_basic, test_slash_in_middle_not_command);
    suite_add_tcase(s, tc_basic);

    TCase *tc_oom = tcase_create("OOM");
    tcase_add_test(tc_oom, test_pp_command_oom_format_buffer);
    tcase_add_test(tc_oom, test_pp_command_oom_command_string);
    suite_add_tcase(s, tc_oom);

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
