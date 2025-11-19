/**
 * @file dispatch_test.c
 * @brief Unit tests for command dispatcher
 */

#include "../../../src/commands.h"
#include "../../../src/config.h"
#include "../../../src/error.h"
#include "../../../src/repl.h"
#include "../../../src/scrollback.h"
#include "../../test_utils.h"

#include <check.h>
#include <talloc.h>

// Forward declaration for suite function
static Suite *commands_dispatch_suite(void);

// Test fixture
static void *ctx;
static ik_repl_ctx_t *repl;

/**
 * Create a minimal REPL context for command testing.
 * Only creates the scrollback buffer needed by command handlers.
 */
static ik_repl_ctx_t *create_test_repl_for_commands(void *parent)
{
    // Create scrollback buffer (80 columns is standard)
    ik_scrollback_t *scrollback = NULL;
    res_t res = ik_scrollback_create(parent, 80, &scrollback);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(scrollback);

    // Create minimal REPL context
    ik_repl_ctx_t *r = talloc_zero(parent, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(r);
    r->scrollback = scrollback;

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
    ck_assert_uint_eq(count, 6);     // clear, mark, rewind, help, model, system

    // Verify command names
    ck_assert_str_eq(cmds[0].name, "clear");
    ck_assert_str_eq(cmds[1].name, "mark");
    ck_assert_str_eq(cmds[2].name, "rewind");
    ck_assert_str_eq(cmds[3].name, "help");
    ck_assert_str_eq(cmds[4].name, "model");
    ck_assert_str_eq(cmds[5].name, "system");

    // Verify descriptions exist
    ck_assert_ptr_nonnull(cmds[0].description);
    ck_assert_ptr_nonnull(cmds[1].description);
    ck_assert_ptr_nonnull(cmds[2].description);

    // Verify handlers exist
    ck_assert_ptr_nonnull(cmds[0].handler);
    ck_assert_ptr_nonnull(cmds[1].handler);
    ck_assert_ptr_nonnull(cmds[2].handler);
}
END_TEST
// Test: Dispatch valid command (clear)
START_TEST(test_dispatch_clear_command)
{
    res_t res = ik_cmd_dispatch(ctx, repl, "/clear");
    ck_assert(is_ok(&res));

    // Verify scrollback received the TODO message
    const char *line = NULL;
    size_t length = 0;
    res = ik_scrollback_get_line_text(repl->scrollback, 0, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(line);
    ck_assert_str_eq(line, "TODO: /clear not yet implemented");
}

END_TEST
// Test: Dispatch valid command (help)
START_TEST(test_dispatch_help_command)
{
    res_t res = ik_cmd_dispatch(ctx, repl, "/help");
    ck_assert(is_ok(&res));

    // Verify scrollback received the TODO message
    const char *line = NULL;
    size_t length = 0;
    res = ik_scrollback_get_line_text(repl->scrollback, 0, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(line);
    ck_assert_str_eq(line, "TODO: /help not yet implemented");
}

END_TEST
// Test: Dispatch command with arguments (mark)
START_TEST(test_dispatch_mark_with_args)
{
    res_t res = ik_cmd_dispatch(ctx, repl, "/mark checkpoint1");
    ck_assert(is_ok(&res));

    // Verify scrollback received the TODO message
    const char *line = NULL;
    size_t length = 0;
    res = ik_scrollback_get_line_text(repl->scrollback, 0, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(line);
    ck_assert_str_eq(line, "TODO: /mark not yet implemented");
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
    res = ik_scrollback_get_line_text(repl->scrollback, 0, &line, &length);
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
    res = ik_scrollback_get_line_text(repl->scrollback, 0, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(line);
    ck_assert_str_eq(line, "Error: Empty command");
}

END_TEST
// Test: Dispatch command with leading/trailing whitespace
START_TEST(test_dispatch_command_with_whitespace)
{
    res_t res = ik_cmd_dispatch(ctx, repl, "/  clear  ");
    ck_assert(is_ok(&res));

    // Verify scrollback received the TODO message
    const char *line = NULL;
    size_t length = 0;
    res = ik_scrollback_get_line_text(repl->scrollback, 0, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(line);
    ck_assert_str_eq(line, "TODO: /clear not yet implemented");
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
    res = ik_scrollback_get_line_text(repl->scrollback, 0, &line, &length);
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

    // Verify scrollback received the TODO message
    const char *line = NULL;
    size_t length = 0;
    res = ik_scrollback_get_line_text(repl->scrollback, 0, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(line);
    ck_assert_str_eq(line, "TODO: /model not yet implemented");
}

END_TEST
// Test: Dispatch rewind command with argument
START_TEST(test_dispatch_rewind_with_arg)
{
    res_t res = ik_cmd_dispatch(ctx, repl, "/rewind checkpoint1");
    ck_assert(is_ok(&res));

    // Verify scrollback received the TODO message
    const char *line = NULL;
    size_t length = 0;
    res = ik_scrollback_get_line_text(repl->scrollback, 0, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(line);
    ck_assert_str_eq(line, "TODO: /rewind not yet implemented");
}

END_TEST
// Test: Dispatch system command with multiword argument
START_TEST(test_dispatch_system_with_multiword_arg)
{
    res_t res =
        ik_cmd_dispatch(ctx, repl, "/system You are a helpful assistant");
    ck_assert(is_ok(&res));

    // Verify scrollback received the TODO message
    const char *line = NULL;
    size_t length = 0;
    res = ik_scrollback_get_line_text(repl->scrollback, 0, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(line);
    ck_assert_str_eq(line, "TODO: /system not yet implemented");
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
