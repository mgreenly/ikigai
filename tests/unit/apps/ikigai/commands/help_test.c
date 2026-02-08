/**
 * @file help_test.c
 * @brief Unit tests for /help command
 */

#include "apps/ikigai/agent.h"
#include "apps/ikigai/commands.h"
#include "apps/ikigai/config.h"
#include "apps/ikigai/shared.h"
#include "shared/error.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/scrollback.h"
#include "tests/helpers/test_utils_helper.h"

#include <check.h>
#include <stdio.h>
#include <talloc.h>

// Forward declaration for suite function
static Suite *commands_help_suite(void);

// Test fixture
static void *ctx;
static ik_repl_ctx_t *repl;

/**
 * Create a minimal REPL context for command testing.
 */
static ik_repl_ctx_t *create_test_repl_for_commands(void *parent)
{
    // Create scrollback buffer (80 columns is standard)
    ik_scrollback_t *scrollback = ik_scrollback_create(parent, 80);
    ck_assert_ptr_nonnull(scrollback);

    // Create conversation (needed for mark/rewind commands)

    // Create minimal config
    ik_config_t *cfg = talloc_zero(parent, ik_config_t);
    ck_assert_ptr_nonnull(cfg);

    // Create shared context
    ik_shared_ctx_t *shared = talloc_zero(parent, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    shared->cfg = cfg;

    // Create minimal REPL context
    ik_repl_ctx_t *r = talloc_zero(parent, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(r);

    // Create agent context
    ik_agent_ctx_t *agent = talloc_zero(r, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);
    agent->scrollback = scrollback;

    r->current = agent;

    r->current->marks = NULL;
    r->current->mark_count = 0;
    r->shared = shared;

    return r;
}

static void setup(void)
{
    ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    repl = create_test_repl_for_commands(ctx);
    ck_assert_ptr_nonnull(repl);
}

static void teardown(void)
{
    talloc_free(ctx);
}

// Test: Help command shows header
START_TEST(test_help_shows_header) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/help");
    ck_assert(is_ok(&res));

    // Line 0: command echo, Line 1: blank, Line 2: header
    const char *line = NULL;
    size_t length = 0;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 2, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(line);
    ck_assert_str_eq(line, "Available commands:");
}
END_TEST
// Test: Help command includes all commands
START_TEST(test_help_includes_all_commands) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/help");
    ck_assert(is_ok(&res));

    // Get number of registered commands
    size_t cmd_count;
    ik_cmd_get_all(&cmd_count);

    // Should have: echo + blank + header + commands + trailing blank
    // = 2 + 1 + cmd_count + 1 = cmd_count + 4
    size_t line_count = ik_scrollback_get_line_count(repl->current->scrollback);
    ck_assert_uint_eq(line_count, cmd_count + 4);
}

END_TEST
// Test: Help command lists clear
START_TEST(test_help_lists_clear) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/help");
    ck_assert(is_ok(&res));

    // Line 4 should be /clear (alphabetically: agents, clear, ...)
    const char *line = NULL;
    size_t length = 0;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 4, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(line);

    // Should start with "  /clear - "
    ck_assert(strncmp(line, "  /clear - ", 11) == 0);
}

END_TEST
// Test: Help command lists mark
START_TEST(test_help_lists_mark) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/help");
    ck_assert(is_ok(&res));

    // Line 9 should be /mark (alphabetically: ..., kill, mark, ...)
    const char *line = NULL;
    size_t length = 0;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 9, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(line);

    // Should start with "  /mark - "
    ck_assert(strncmp(line, "  /mark - ", 10) == 0);
}

END_TEST
// Test: Help command lists rewind
START_TEST(test_help_lists_rewind) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/help");
    ck_assert(is_ok(&res));

    // Line 14 should be /rewind (alphabetically: ..., reap, refresh, rewind, ...)
    const char *line = NULL;
    size_t length = 0;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 14, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(line);

    // Should start with "  /rewind - "
    ck_assert(strncmp(line, "  /rewind - ", 12) == 0);
}

END_TEST
// Test: Help command lists help (self-reference)
START_TEST(test_help_lists_help) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/help");
    ck_assert(is_ok(&res));

    // Line 7 should be /help (alphabetically: ..., fork, help, kill, ...)
    const char *line = NULL;
    size_t length = 0;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 7, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(line);

    // Should start with "  /help - "
    ck_assert(strncmp(line, "  /help - ", 10) == 0);
}

END_TEST
// Test: Help command lists model
START_TEST(test_help_lists_model) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/help");
    ck_assert(is_ok(&res));

    // Line 10 should be /model (alphabetically: ..., mark, model, pin, ...)
    const char *line = NULL;
    size_t length = 0;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 10, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(line);

    // Should start with "  /model - "
    ck_assert(strncmp(line, "  /model - ", 11) == 0);
}

END_TEST
// Test: Help command lists system
START_TEST(test_help_lists_system) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/help");
    ck_assert(is_ok(&res));

    // Line 16 should be /system (alphabetically: ..., rewind, send, system, tool, ...)
    const char *line = NULL;
    size_t length = 0;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 16, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(line);

    // Should start with "  /system - "
    ck_assert(strncmp(line, "  /system - ", 12) == 0);
}

END_TEST

// Test: Help command lists exit
START_TEST(test_help_lists_exit) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/help");
    ck_assert(is_ok(&res));

    // Line 5 should be /exit (alphabetically: ..., clear, exit, fork, ...)
    const char *line = NULL;
    size_t length = 0;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 5, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(line);

    // Should start with "  /exit - "
    ck_assert(strncmp(line, "  /exit - ", 10) == 0);
}

END_TEST
// Test: Help command with arguments (args should be ignored)
START_TEST(test_help_with_arguments) {
    res_t res = ik_cmd_dispatch(ctx, repl, "/help foo bar");
    ck_assert(is_ok(&res));

    // Should still show normal help output at line 2 (after echo + blank)
    const char *line = NULL;
    size_t length = 0;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 2, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(line);
    ck_assert_str_eq(line, "Available commands:");
}

END_TEST

static Suite *commands_help_suite(void)
{
    Suite *s = suite_create("Commands/Help");
    TCase *tc = tcase_create("Core");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);

    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_help_shows_header);
    tcase_add_test(tc, test_help_includes_all_commands);
    tcase_add_test(tc, test_help_lists_clear);
    tcase_add_test(tc, test_help_lists_mark);
    tcase_add_test(tc, test_help_lists_rewind);
    tcase_add_test(tc, test_help_lists_help);
    tcase_add_test(tc, test_help_lists_model);
    tcase_add_test(tc, test_help_lists_system);
    tcase_add_test(tc, test_help_lists_exit);
    tcase_add_test(tc, test_help_with_arguments);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = commands_help_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/commands/help_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
