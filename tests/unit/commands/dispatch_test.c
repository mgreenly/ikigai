/**
 * @file dispatch_test.c
 * @brief Unit tests for command dispatcher
 */

#include "../../../src/agent.h"
#include "../../../src/commands.h"
#include "../../../src/config.h"
#include "../../../src/error.h"
#include "../../../src/repl.h"
#include "../../../src/scrollback.h"
#include "../../../src/openai/client.h"
#include "../../../src/shared.h"
#include "../../../src/wrapper.h"
#include "../../test_utils.h"

#include <check.h>
#include <talloc.h>

// Mock posix_rename_ (logger rotation) to prevent PANIC during tests
int posix_rename_(const char *oldpath, const char *newpath)
{
    (void)oldpath;
    (void)newpath;
    return 0;  // Success
}

// Forward declaration for suite function
static Suite *commands_dispatch_suite(void);

// Test fixture
static void *ctx;
static ik_repl_ctx_t *repl;

/**
 * Create a minimal REPL context for command testing.
 * Creates scrollback, conversation, and config needed by command handlers.
 */
static ik_repl_ctx_t *create_test_repl_for_commands(void *parent)
{
    // Create scrollback buffer (80 columns is standard)
    ik_scrollback_t *scrollback = ik_scrollback_create(parent, 80);
    ck_assert_ptr_nonnull(scrollback);

    // Create conversation (needed for mark/rewind commands)
    res_t res = ik_openai_conversation_create(parent);
    ck_assert(is_ok(&res));
    ik_openai_conversation_t *conv = res.ok;
    ck_assert_ptr_nonnull(conv);

    // Create config (needed for model/system commands)
    ik_cfg_t *cfg = talloc_zero(parent, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->openai_model = talloc_strdup(cfg, "gpt-5-mini");
    ck_assert_ptr_nonnull(cfg->openai_model);

    // Create minimal REPL context
    ik_repl_ctx_t *r = talloc_zero(parent, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(r);
    
    // Create agent context
    ik_agent_ctx_t *agent = talloc_zero(r, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);
    agent->scrollback = scrollback;


    agent->conversation = conv;
    r->current = agent;


    // Create shared context
    ik_shared_ctx_t *shared = talloc_zero(parent, ik_shared_ctx_t);
    shared->cfg = cfg;
    r->shared = shared;

    r->current->marks = NULL;
    r->current->mark_count = 0;

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

// Test: Get all commands
START_TEST(test_cmd_get_all) {
    size_t count = 0;
    const ik_command_t *cmds = ik_cmd_get_all(&count);

    ck_assert_ptr_nonnull(cmds);
    ck_assert_uint_eq(count, 14);     // clear, mark, rewind, fork, kill, send, check-mail, read-mail, delete-mail, filter-mail, help, model, system, debug

    // Verify command names
    ck_assert_str_eq(cmds[0].name, "clear");
    ck_assert_str_eq(cmds[1].name, "mark");
    ck_assert_str_eq(cmds[2].name, "rewind");
    ck_assert_str_eq(cmds[3].name, "fork");
    ck_assert_str_eq(cmds[4].name, "kill");
    ck_assert_str_eq(cmds[5].name, "send");
    ck_assert_str_eq(cmds[6].name, "check-mail");
    ck_assert_str_eq(cmds[7].name, "read-mail");
    ck_assert_str_eq(cmds[8].name, "delete-mail");
    ck_assert_str_eq(cmds[9].name, "filter-mail");
    ck_assert_str_eq(cmds[10].name, "help");
    ck_assert_str_eq(cmds[11].name, "model");
    ck_assert_str_eq(cmds[12].name, "system");
    ck_assert_str_eq(cmds[13].name, "debug");

    // Verify descriptions exist
    ck_assert_ptr_nonnull(cmds[0].description);
    ck_assert_ptr_nonnull(cmds[1].description);
    ck_assert_ptr_nonnull(cmds[2].description);
    ck_assert_ptr_nonnull(cmds[3].description);

    // Verify handlers exist
    ck_assert_ptr_nonnull(cmds[0].handler);
    ck_assert_ptr_nonnull(cmds[1].handler);
    ck_assert_ptr_nonnull(cmds[2].handler);
    ck_assert_ptr_nonnull(cmds[3].handler);
}
END_TEST
// Test: Dispatch valid command (clear)
START_TEST(test_dispatch_clear_command)
{
    // Add some content to scrollback
    res_t res = ik_scrollback_append_line(repl->current->scrollback, "Line 1", 6);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 1);

    // Dispatch /clear command
    res = ik_cmd_dispatch(ctx, repl, "/clear");
    ck_assert(is_ok(&res));

    // Verify scrollback is now empty (clear was executed)
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 0);
}

END_TEST
// Test: Dispatch valid command (help)
START_TEST(test_dispatch_help_command)
{
    res_t res = ik_cmd_dispatch(ctx, repl, "/help");
    ck_assert(is_ok(&res));

    // Verify scrollback received help header
    const char *line = NULL;
    size_t length = 0;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 0, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(line);
    ck_assert_str_eq(line, "Available commands:");

    // Verify at least one command is listed
    size_t line_count = ik_scrollback_get_line_count(repl->current->scrollback);
    ck_assert_uint_gt(line_count, 1);
}

END_TEST
// Test: Dispatch command with arguments (mark)
START_TEST(test_dispatch_mark_with_args)
{
    res_t res = ik_cmd_dispatch(ctx, repl, "/mark checkpoint1");
    ck_assert(is_ok(&res));

    // Verify mark was created
    ck_assert_uint_eq(repl->current->mark_count, 1);
    ck_assert_ptr_nonnull(repl->current->marks);
    ck_assert_str_eq(repl->current->marks[0]->label, "checkpoint1");

    // Verify scrollback received the mark indicator
    const char *line = NULL;
    size_t length = 0;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 0, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(line);
    ck_assert_str_eq(line, "/mark checkpoint1");
}

END_TEST
// Test: Dispatch unknown command
START_TEST(test_dispatch_unknown_command)
{
    res_t res = ik_cmd_dispatch(ctx, repl, "/unknown");
    ck_assert(is_err(&res));

    // Verify error message in scrollback
    const char *line = NULL;
    size_t length = 0;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 0, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(line);
    ck_assert_str_eq(line, "Error: Unknown command 'unknown'");
}

END_TEST
// Test: Dispatch empty command (just "/")
START_TEST(test_dispatch_empty_command)
{
    res_t res = ik_cmd_dispatch(ctx, repl, "/");
    ck_assert(is_err(&res));

    // Verify error message in scrollback
    const char *line = NULL;
    size_t length = 0;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 0, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(line);
    ck_assert_str_eq(line, "Error: Empty command");
}

END_TEST
// Test: Dispatch command with leading/trailing whitespace
START_TEST(test_dispatch_command_with_whitespace)
{
    // Add content to scrollback
    res_t res = ik_scrollback_append_line(repl->current->scrollback, "Test line", 9);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 1);

    // Dispatch /clear with whitespace
    res = ik_cmd_dispatch(ctx, repl, "/  clear  ");
    ck_assert(is_ok(&res));

    // Verify scrollback is cleared (whitespace was handled correctly)
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 0);
}

END_TEST
// Test: Dispatch command with slash and whitespace
START_TEST(test_dispatch_slash_whitespace)
{
    res_t res = ik_cmd_dispatch(ctx, repl, "/   ");
    ck_assert(is_err(&res));

    // Verify error message in scrollback
    const char *line = NULL;
    size_t length = 0;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 0, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(line);
    ck_assert_str_eq(line, "Error: Empty command");
}

END_TEST
// Test: Dispatch model command with argument
START_TEST(test_dispatch_model_with_arg)
{
    res_t res = ik_cmd_dispatch(ctx, repl, "/model gpt-4-turbo");
    ck_assert(is_ok(&res));

    // Verify model changed
    ck_assert_str_eq(repl->shared->cfg->openai_model, "gpt-4-turbo");

    // Verify scrollback received confirmation message
    const char *line = NULL;
    size_t length = 0;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 0, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(line);
    ck_assert_str_eq(line, "Switched to model: gpt-4-turbo");
}

END_TEST
// Test: Dispatch rewind command with argument
START_TEST(test_dispatch_rewind_with_arg)
{
    res_t res = ik_cmd_dispatch(ctx, repl, "/rewind checkpoint1");
    ck_assert(is_ok(&res));

    // Verify scrollback received error message (no marks exist)
    const char *line = NULL;
    size_t length = 0;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 0, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(line);
    ck_assert_str_eq(line, "Error: No marks found");
}

END_TEST
// Test: Dispatch system command with multiword argument
START_TEST(test_dispatch_system_with_multiword_arg)
{
    res_t res =
        ik_cmd_dispatch(ctx, repl, "/system You are a helpful assistant");
    ck_assert(is_ok(&res));

    // Verify scrollback received the confirmation message
    const char *line = NULL;
    size_t length = 0;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 0, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(line);
    ck_assert_str_eq(line, "System message set to: You are a helpful assistant");
}

END_TEST

static Suite *commands_dispatch_suite(void)
{
    Suite *s = suite_create("Commands/Dispatch");
    TCase *tc = tcase_create("Core");

    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_cmd_get_all);
    tcase_add_test(tc, test_dispatch_clear_command);
    tcase_add_test(tc, test_dispatch_help_command);
    tcase_add_test(tc, test_dispatch_mark_with_args);
    tcase_add_test(tc, test_dispatch_rewind_with_arg);
    tcase_add_test(tc, test_dispatch_unknown_command);
    tcase_add_test(tc, test_dispatch_empty_command);
    tcase_add_test(tc, test_dispatch_command_with_whitespace);
    tcase_add_test(tc, test_dispatch_slash_whitespace);
    tcase_add_test(tc, test_dispatch_model_with_arg);
    tcase_add_test(tc, test_dispatch_system_with_multiword_arg);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = commands_dispatch_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
