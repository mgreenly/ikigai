// Unit tests for REPL scrollback integration

#include <check.h>
#include "../../../src/agent.h"
#include <talloc.h>
#include <string.h>
#include "../../../src/repl.h"
#include "../../../src/shared.h"
#include "../../../src/repl_actions.h"
#include "../../../src/scrollback.h"
#include "../../test_utils_helper.h"
#include "../../../src/logger.h"
#include "../terminal/terminal_test_mocks.h"

static void suite_setup(void)
{
    ik_test_set_log_dir(__FILE__);
}

/* Test: REPL context can hold scrollback buffer */
START_TEST(test_repl_context_with_scrollback) {
    void *ctx = talloc_new(NULL);

    // Manually construct REPL context (like other tests do)
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(repl);

    // Create agent context for display state
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    repl->current = agent;

    // Create scrollback with terminal width of 80
    ik_scrollback_t *scrollback = ik_scrollback_create(repl, 80);
    ck_assert_ptr_nonnull(scrollback);

    // Assign to REPL context
    repl->current->scrollback = scrollback;
    repl->current->viewport_offset = 0;

    // Verify scrollback is accessible through REPL
    ck_assert_ptr_nonnull(repl->current->scrollback);
    ck_assert_uint_eq(repl->current->viewport_offset, 0);

    // Verify scrollback is empty initially
    size_t line_count = ik_scrollback_get_line_count(repl->current->scrollback);
    ck_assert_uint_eq(line_count, 0);

    // Cleanup
    talloc_free(ctx);
}
END_TEST
/* Test: REPL scrollback integration with terminal width */
START_TEST(test_repl_scrollback_terminal_width) {
    void *ctx = talloc_new(NULL);

    // Create REPL context with mocked terminal
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    ck_assert_ptr_nonnull(term);
    term->screen_rows = 24;
    term->screen_cols = 120;

    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(repl);
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    repl->shared = shared;
    shared->term = term;

    // Create agent context for display state
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    repl->current = agent;

    // Create scrollback with terminal width
    ik_scrollback_t *scrollback = ik_scrollback_create(repl, term->screen_cols);
    ck_assert_ptr_nonnull(scrollback);

    repl->current->scrollback = scrollback;
    repl->current->viewport_offset = 0;

    // Verify scrollback uses correct terminal width
    ck_assert_int_eq(repl->current->scrollback->cached_width, 120);

    // Cleanup
    talloc_free(ctx);
}

END_TEST

/* Create test suite */
static Suite *repl_scrollback_suite(void)
{
    Suite *s = suite_create("REPL Scrollback Integration");

    TCase *tc_init = tcase_create("Initialization");
    tcase_set_timeout(tc_init, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_init, suite_setup, NULL);
    tcase_add_test(tc_init, test_repl_context_with_scrollback);
    tcase_add_test(tc_init, test_repl_scrollback_terminal_width);
    suite_add_tcase(s, tc_init);

    return s;
}

int main(void)
{
    Suite *s = repl_scrollback_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/repl/repl_scrollback_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    ik_test_reset_terminal();

    return (number_failed == 0) ? 0 : 1;
}
