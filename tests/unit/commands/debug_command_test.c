/**
 * @file debug_command_test.c
 * @brief Unit tests for /debug slash command
 */

#include "../../../src/agent.h"
#include <check.h>
#include <talloc.h>
#include <string.h>
#include "../../../src/repl.h"
#include "../../../src/commands.h"
#include "../../../src/scrollback.h"
#include "../../../src/config.h"
#include "../../../src/shared.h"
#include "../../../src/debug_pipe.h"
#include "../../test_utils.h"

/**
 * Create a minimal REPL context for debug command testing.
 */
static ik_repl_ctx_t *create_test_repl(void *parent)
{
    // Create scrollback buffer
    ik_scrollback_t *scrollback = ik_scrollback_create(parent, 80);
    ck_assert_ptr_nonnull(scrollback);

    // Create debug pipe manager
    res_t res = ik_debug_manager_create(parent);
    ck_assert(is_ok(&res));
    ik_debug_pipe_manager_t *debug_mgr = res.ok;
    ck_assert_ptr_nonnull(debug_mgr);

    // Create minimal config
    ik_config_t *cfg = talloc_zero(parent, ik_config_t);
    ck_assert_ptr_nonnull(cfg);

    // Create shared context
    ik_shared_ctx_t *shared = talloc_zero(parent, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    shared->cfg = cfg;
    shared->debug_mgr = debug_mgr;
    shared->debug_enabled = false;  // Default: disabled

    // Create minimal REPL context
    ik_repl_ctx_t *r = talloc_zero(parent, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(r);

    // Create agent context
    ik_agent_ctx_t *agent = talloc_zero(r, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);
    agent->scrollback = scrollback;
    r->current = agent;

    r->shared = shared;

    return r;
}

/**
 * Test: /debug on enables debug output
 */
START_TEST(test_debug_on) {
    void *ctx = talloc_new(NULL);

    // Create minimal REPL
    ik_repl_ctx_t *repl = create_test_repl(ctx);

    // Debug should be disabled by default
    ck_assert(!repl->shared->debug_enabled);

    // Dispatch "/debug on"
    res_t res = ik_cmd_dispatch(ctx, repl, "/debug on");
    ck_assert(is_ok(&res));

    // Verify debug is now enabled
    ck_assert(repl->shared->debug_enabled);

    // Verify confirmation message in scrollback
    size_t line_count = ik_scrollback_get_line_count(repl->current->scrollback);
    ck_assert_uint_ge(line_count, 1);

    const char *last_line = NULL;
    size_t last_line_len = 0;
    res = ik_scrollback_get_line_text(repl->current->scrollback, line_count - 1, &last_line, &last_line_len);
    ck_assert(is_ok(&res));
    ck_assert_ptr_ne(strstr(last_line, "Debug"), NULL);

    talloc_free(ctx);
}
END_TEST
/**
 * Test: /debug off disables debug output
 */
START_TEST(test_debug_off)
{
    void *ctx = talloc_new(NULL);

    // Create minimal REPL
    ik_repl_ctx_t *repl = create_test_repl(ctx);

    // Enable debug first
    repl->shared->debug_enabled = true;

    // Dispatch "/debug off"
    res_t res = ik_cmd_dispatch(ctx, repl, "/debug off");
    ck_assert(is_ok(&res));

    // Verify debug is now disabled
    ck_assert(!repl->shared->debug_enabled);

    // Verify confirmation message in scrollback
    size_t line_count = ik_scrollback_get_line_count(repl->current->scrollback);
    ck_assert_uint_ge(line_count, 1);

    const char *last_line = NULL;
    size_t last_line_len = 0;
    res = ik_scrollback_get_line_text(repl->current->scrollback, line_count - 1, &last_line, &last_line_len);
    ck_assert(is_ok(&res));
    ck_assert_ptr_ne(strstr(last_line, "Debug"), NULL);

    talloc_free(ctx);
}

END_TEST
/**
 * Test: /debug (no args) shows current status
 */
START_TEST(test_debug_status)
{
    void *ctx = talloc_new(NULL);

    // Create minimal REPL
    ik_repl_ctx_t *repl = create_test_repl(ctx);

    // Dispatch "/debug" (no arguments)
    res_t res = ik_cmd_dispatch(ctx, repl, "/debug");
    ck_assert(is_ok(&res));

    // Verify status message in scrollback
    size_t line_count = ik_scrollback_get_line_count(repl->current->scrollback);
    ck_assert_uint_ge(line_count, 1);

    const char *last_line = NULL;
    size_t last_line_len = 0;
    res = ik_scrollback_get_line_text(repl->current->scrollback, line_count - 1, &last_line, &last_line_len);
    ck_assert(is_ok(&res));
    ck_assert_ptr_ne(strstr(last_line, "OFF"), NULL);

    talloc_free(ctx);
}

END_TEST
/**
 * Test: /debug (no args) shows ON when enabled
 */
START_TEST(test_debug_status_on)
{
    void *ctx = talloc_new(NULL);

    // Create minimal REPL and enable debug
    ik_repl_ctx_t *repl = create_test_repl(ctx);
    repl->shared->debug_enabled = true;

    // Dispatch "/debug" (no arguments)
    res_t res = ik_cmd_dispatch(ctx, repl, "/debug");
    ck_assert(is_ok(&res));

    // Verify status message shows ON
    size_t line_count = ik_scrollback_get_line_count(repl->current->scrollback);
    ck_assert_uint_ge(line_count, 1);

    const char *last_line = NULL;
    size_t last_line_len = 0;
    res = ik_scrollback_get_line_text(repl->current->scrollback, line_count - 1, &last_line, &last_line_len);
    ck_assert(is_ok(&res));
    ck_assert_ptr_ne(strstr(last_line, "ON"), NULL);

    talloc_free(ctx);
}

END_TEST
/**
 * Test: /debug with invalid argument shows error
 */
START_TEST(test_debug_invalid_arg)
{
    void *ctx = talloc_new(NULL);

    // Create minimal REPL
    ik_repl_ctx_t *repl = create_test_repl(ctx);

    // Dispatch "/debug invalid"
    res_t res = ik_cmd_dispatch(ctx, repl, "/debug invalid");
    ck_assert(is_err(&res));

    // Verify error message in scrollback
    size_t line_count = ik_scrollback_get_line_count(repl->current->scrollback);
    ck_assert_uint_ge(line_count, 1);

    const char *last_line = NULL;
    size_t last_line_len = 0;
    res = ik_scrollback_get_line_text(repl->current->scrollback, line_count - 1, &last_line, &last_line_len);
    ck_assert(is_ok(&res));
    ck_assert_ptr_ne(strstr(last_line, "Error"), NULL);
    ck_assert_ptr_ne(strstr(last_line, "invalid"), NULL);

    talloc_free(ctx);
}

END_TEST

static Suite *debug_command_suite(void)
{
    Suite *s = suite_create("Debug Command");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);
    tcase_add_test(tc_core, test_debug_on);
    tcase_add_test(tc_core, test_debug_off);
    tcase_add_test(tc_core, test_debug_status);
    tcase_add_test(tc_core, test_debug_status_on);
    tcase_add_test(tc_core, test_debug_invalid_arg);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = debug_command_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
