/**
 * @file exit_autocomplete_test.c
 * @brief Unit test for /exit command autocomplete
 *
 * Verifies that typing "/ex" triggers autocomplete that suggests "/exit".
 * Requirement: exit-003
 */

#include <check.h>
#include <talloc.h>
#include "../../../src/agent.h"
#include "../../../src/shared.h"
#include "../../../src/repl.h"
#include "../../../src/repl_actions.h"
#include "../../../src/input.h"
#include "../../../src/completion.h"
#include "../../test_utils_helper.h"
#include <string.h>

/* Test: Typing /ex triggers autocomplete suggesting /exit */
START_TEST(test_exit_autocomplete) {
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
    repl->shared->history = NULL;

    // Type "/ex" - completion should be created automatically
    ik_input_action_t action = {.type = IK_INPUT_CHAR, .codepoint = '/'};
    res_t res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    action.codepoint = 'e';
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    action.codepoint = 'x';
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: Completion was created
    ck_assert_ptr_nonnull(repl->current->completion);
    ck_assert_uint_gt(repl->current->completion->count, 0);

    // Verify: Prefix is stored correctly
    ck_assert_str_eq(repl->current->completion->prefix, "/ex");

    // Verify: /exit is in the completion candidates
    bool found_exit = false;
    for (size_t i = 0; i < repl->current->completion->count; i++) {
        if (strcmp(repl->current->completion->candidates[i], "exit") == 0) {
            found_exit = true;
            break;
        }
    }
    ck_assert(found_exit);

    talloc_free(ctx);
}
END_TEST

/* Test Suite */
static Suite *exit_autocomplete_suite(void)
{
    Suite *s = suite_create("Exit_Autocomplete");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    tcase_add_test(tc_core, test_exit_autocomplete);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = exit_autocomplete_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
