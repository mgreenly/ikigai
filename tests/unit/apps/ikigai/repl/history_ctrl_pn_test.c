#include "apps/ikigai/agent.h"
/**
 * @file history_ctrl_pn_test.c
 * @brief Tests for Ctrl+P/Ctrl+N (currently disabled)
 *
 * Ctrl+P and Ctrl+N are currently no-ops. History navigation will be
 * re-enabled via Ctrl+R reverse search in a future release.
 */

#include <check.h>
#include "apps/ikigai/agent.h"
#include <string.h>
#include <talloc.h>
#include "apps/ikigai/shared.h"
#include "apps/ikigai/history.h"
#include "apps/ikigai/input.h"
#include "apps/ikigai/input_buffer/core.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/repl_actions.h"
#include "apps/ikigai/scrollback.h"
#include "tests/helpers/test_utils_helper.h"

/* Test: Ctrl+P is a no-op (disabled) */
START_TEST(test_ctrl_p_disabled) {
    void *ctx = talloc_new(NULL);
    res_t res;

    // Create REPL context
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    term->screen_rows = 10;
    term->screen_cols = 80;

    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    ik_history_t *history = ik_history_create(ctx, 10);
    res = ik_history_add(history, "first entry");
    ck_assert(is_ok(&res));
    res = ik_history_add(history, "second entry");
    ck_assert(is_ok(&res));

    // Create agent
    ik_agent_ctx_t *agent = NULL;
    res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    repl->current = agent;
    shared->term = term;
    ik_input_buffer_t *input_buf = repl->current->input_buffer;

    // Override agent scrollback
    talloc_free(agent->scrollback);
    repl->current->scrollback = scrollback;
    repl->shared->history = history;
    repl->current->viewport_offset = 0;

    // Press Ctrl+P - should do nothing (disabled)
    ik_input_action_t action = {.type = IK_INPUT_CTRL_P};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: Input buffer still empty
    size_t text_len = 0;
    ik_input_buffer_get_text(input_buf, &text_len);
    ck_assert_uint_eq(text_len, 0);

    // Verify: Not browsing history
    ck_assert(!ik_history_is_browsing(history));

    talloc_free(ctx);
}
END_TEST
/* Test: Ctrl+N is a no-op (disabled) */
START_TEST(test_ctrl_n_disabled) {
    void *ctx = talloc_new(NULL);
    res_t res;

    // Create REPL context
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    term->screen_rows = 10;
    term->screen_cols = 80;

    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    ik_history_t *history = ik_history_create(ctx, 10);
    res = ik_history_add(history, "entry");
    ck_assert(is_ok(&res));

    // Create agent
    ik_agent_ctx_t *agent = NULL;
    res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    repl->current = agent;
    shared->term = term;
    ik_input_buffer_t *input_buf = repl->current->input_buffer;
    res = ik_input_buffer_insert_codepoint(input_buf, 'h');
    ck_assert(is_ok(&res));

    // Override agent scrollback
    talloc_free(agent->scrollback);
    repl->current->scrollback = scrollback;
    repl->shared->history = history;
    repl->current->viewport_offset = 0;

    // Press Ctrl+N - should do nothing (disabled)
    ik_input_action_t action = {.type = IK_INPUT_CTRL_N};
    res = ik_repl_process_action(repl, &action);
    ck_assert(is_ok(&res));

    // Verify: Input buffer unchanged
    size_t text_len = 0;
    const char *text = ik_input_buffer_get_text(input_buf, &text_len);
    ck_assert_uint_eq(text_len, 1);
    ck_assert_mem_eq(text, "h", 1);

    // Verify: Not browsing
    ck_assert(!ik_history_is_browsing(history));

    talloc_free(ctx);
}

END_TEST

static Suite *history_ctrl_pn_suite(void)
{
    Suite *s = suite_create("History_Ctrl_PN");
    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    tcase_add_test(tc_core, test_ctrl_p_disabled);
    tcase_add_test(tc_core, test_ctrl_n_disabled);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = history_ctrl_pn_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/repl/history_ctrl_pn_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
