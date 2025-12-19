/**
 * @file help_test.c
 * @brief Unit tests for /help command
 */

#include "../../../src/agent.h"
#include "../../../src/commands.h"
#include "../../../src/config.h"
#include "../../../src/shared.h"
#include "../../../src/error.h"
#include "../../../src/repl.h"
#include "../../../src/scrollback.h"
#include "../../../src/openai/client.h"
#include "../../test_utils.h"

#include <check.h>
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
    ik_openai_conversation_t *conv = ik_openai_conversation_create(parent);
    ck_assert_ptr_nonnull(conv);


    // Create minimal config
    ik_cfg_t *cfg = talloc_zero(parent, ik_cfg_t);
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


    agent->conversation = conv;
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

    // First line should be header
    const char *line = NULL;
    size_t length = 0;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 0, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(line);
    ck_assert_str_eq(line, "Available commands:");
}
END_TEST
// Test: Help command includes all commands
START_TEST(test_help_includes_all_commands)
{
    res_t res = ik_cmd_dispatch(ctx, repl, "/help");
    ck_assert(is_ok(&res));

    // Get number of registered commands
    size_t cmd_count;
    ik_cmd_get_all(&cmd_count);

    // Should have header + one line per command
    size_t line_count = ik_scrollback_get_line_count(repl->current->scrollback);
    ck_assert_uint_eq(line_count, cmd_count + 1);
}

END_TEST
// Test: Help command lists clear
START_TEST(test_help_lists_clear)
{
    res_t res = ik_cmd_dispatch(ctx, repl, "/help");
    ck_assert(is_ok(&res));

    // Line 1 should be /clear (line 0 is header)
    const char *line = NULL;
    size_t length = 0;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 1, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(line);

    // Should start with "  /clear - "
    ck_assert(strncmp(line, "  /clear - ", 11) == 0);
}

END_TEST
// Test: Help command lists mark
START_TEST(test_help_lists_mark)
{
    res_t res = ik_cmd_dispatch(ctx, repl, "/help");
    ck_assert(is_ok(&res));

    // Line 2 should be /mark
    const char *line = NULL;
    size_t length = 0;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 2, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(line);

    // Should start with "  /mark - "
    ck_assert(strncmp(line, "  /mark - ", 10) == 0);
}

END_TEST
// Test: Help command lists rewind
START_TEST(test_help_lists_rewind)
{
    res_t res = ik_cmd_dispatch(ctx, repl, "/help");
    ck_assert(is_ok(&res));

    // Line 3 should be /rewind
    const char *line = NULL;
    size_t length = 0;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 3, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(line);

    // Should start with "  /rewind - "
    ck_assert(strncmp(line, "  /rewind - ", 12) == 0);
}

END_TEST
// Test: Help command lists help (self-reference)
START_TEST(test_help_lists_help)
{
    res_t res = ik_cmd_dispatch(ctx, repl, "/help");
    ck_assert(is_ok(&res));

    // Line 12 should be /help (shifted due to /fork, /kill, /send, /check-mail, /read-mail, /delete-mail, /filter-mail, /agents)
    const char *line = NULL;
    size_t length = 0;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 12, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(line);

    // Should start with "  /help - "
    ck_assert(strncmp(line, "  /help - ", 10) == 0);
}

END_TEST
// Test: Help command lists model
START_TEST(test_help_lists_model)
{
    res_t res = ik_cmd_dispatch(ctx, repl, "/help");
    ck_assert(is_ok(&res));

    // Line 13 should be /model (shifted due to /fork, /kill, /send, /check-mail, /read-mail, /delete-mail, /filter-mail, /agents)
    const char *line = NULL;
    size_t length = 0;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 13, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(line);

    // Should start with "  /model - "
    ck_assert(strncmp(line, "  /model - ", 11) == 0);
}

END_TEST
// Test: Help command lists system
START_TEST(test_help_lists_system)
{
    res_t res = ik_cmd_dispatch(ctx, repl, "/help");
    ck_assert(is_ok(&res));

    // Line 14 should be /system (shifted due to /fork, /kill, /send, /check-mail, /read-mail, /delete-mail, /filter-mail, /agents)
    const char *line = NULL;
    size_t length = 0;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 14, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(line);

    // Should start with "  /system - "
    ck_assert(strncmp(line, "  /system - ", 12) == 0);
}

END_TEST
// Test: Help command with arguments (args should be ignored)
START_TEST(test_help_with_arguments)
{
    res_t res = ik_cmd_dispatch(ctx, repl, "/help foo bar");
    ck_assert(is_ok(&res));

    // Should still show normal help output
    const char *line = NULL;
    size_t length = 0;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 0, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(line);
    ck_assert_str_eq(line, "Available commands:");
}

END_TEST

static Suite *commands_help_suite(void)
{
    Suite *s = suite_create("Commands/Help");
    TCase *tc = tcase_create("Core");

    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_help_shows_header);
    tcase_add_test(tc, test_help_includes_all_commands);
    tcase_add_test(tc, test_help_lists_clear);
    tcase_add_test(tc, test_help_lists_mark);
    tcase_add_test(tc, test_help_lists_rewind);
    tcase_add_test(tc, test_help_lists_help);
    tcase_add_test(tc, test_help_lists_model);
    tcase_add_test(tc, test_help_lists_system);
    tcase_add_test(tc, test_help_with_arguments);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = commands_help_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
