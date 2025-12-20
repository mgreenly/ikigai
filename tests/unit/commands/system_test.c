/**
 * @file system_test.c
 * @brief Unit tests for /system command
 */

#include "../../../src/agent.h"
#include "../../../src/commands.h"
#include "../../../src/config.h"
#include "../../../src/shared.h"
#include "../../../src/error.h"
#include "../../../src/repl.h"
#include "../../../src/scrollback.h"
#include "../../test_utils.h"

#include <check.h>
#include <talloc.h>

// Forward declaration for suite function
static Suite *commands_system_suite(void);

// Test fixture
static void *ctx;
static ik_repl_ctx_t *repl;

/**
 * Create a REPL context with config for system message testing.
 */
static ik_repl_ctx_t *create_test_repl_with_config(void *parent)
{
    // Create scrollback buffer (80 columns is standard)
    ik_scrollback_t *scrollback = ik_scrollback_create(parent, 80);
    ck_assert_ptr_nonnull(scrollback);

    // Create config
    ik_cfg_t *cfg = talloc_zero(parent, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->openai_system_message = NULL;  // Start with no system message

    // Create minimal REPL context
    ik_repl_ctx_t *r = talloc_zero(parent, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(r);

    // Create agent context
    ik_agent_ctx_t *agent = talloc_zero(r, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);
    agent->scrollback = scrollback;
    r->current = agent;

    // Create shared context
    ik_shared_ctx_t *shared = talloc_zero(parent, ik_shared_ctx_t);
    shared->cfg = cfg;
    r->shared = shared;

    return r;
}

static void setup(void)
{
    ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    repl = create_test_repl_with_config(ctx);
    ck_assert_ptr_nonnull(repl);
}

static void teardown(void)
{
    talloc_free(ctx);
}

// Test: Set system message
START_TEST(test_system_set_message) {
    // Verify initial state (no system message)
    ck_assert_ptr_null(repl->shared->cfg->openai_system_message);

    // Execute /system with message
    res_t res = ik_cmd_dispatch(ctx, repl, "/system You are a helpful assistant");
    ck_assert(is_ok(&res));

    // Verify system message was set
    ck_assert_ptr_nonnull(repl->shared->cfg->openai_system_message);
    ck_assert_str_eq(repl->shared->cfg->openai_system_message, "You are a helpful assistant");

    // Verify confirmation message in scrollback
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 1);
    const char *line;
    size_t length;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 0, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(line);
    ck_assert_str_eq(line, "System message set to: You are a helpful assistant");
}
END_TEST
// Test: Clear system message (no args)
START_TEST(test_system_clear_message)
{
    // Set initial system message
    repl->shared->cfg->openai_system_message = talloc_strdup(repl->shared->cfg, "Initial message");
    ck_assert_ptr_nonnull(repl->shared->cfg->openai_system_message);

    // Execute /system without args
    res_t res = ik_cmd_dispatch(ctx, repl, "/system");
    ck_assert(is_ok(&res));

    // Verify system message was cleared
    ck_assert_ptr_null(repl->shared->cfg->openai_system_message);

    // Verify confirmation message in scrollback
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 1);
    const char *line;
    size_t length;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 0, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(line);
    ck_assert_str_eq(line, "System message cleared");
}

END_TEST
// Test: Replace existing system message
START_TEST(test_system_replace_message)
{
    // Set initial system message
    repl->shared->cfg->openai_system_message = talloc_strdup(repl->shared->cfg, "Old message");
    ck_assert_ptr_nonnull(repl->shared->cfg->openai_system_message);

    // Execute /system with new message
    res_t res = ik_cmd_dispatch(ctx, repl, "/system New message");
    ck_assert(is_ok(&res));

    // Verify system message was replaced
    ck_assert_ptr_nonnull(repl->shared->cfg->openai_system_message);
    ck_assert_str_eq(repl->shared->cfg->openai_system_message, "New message");

    // Verify confirmation message in scrollback
    const char *line;
    size_t length;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 0, &line, &length);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(line);
    ck_assert_str_eq(line, "System message set to: New message");
}

END_TEST
// Test: Set system message with special characters
START_TEST(test_system_with_special_chars)
{
    res_t res = ik_cmd_dispatch(ctx, repl, "/system You are a \"pirate\" assistant!");
    ck_assert(is_ok(&res));

    ck_assert_ptr_nonnull(repl->shared->cfg->openai_system_message);
    ck_assert_str_eq(repl->shared->cfg->openai_system_message, "You are a \"pirate\" assistant!");
}

END_TEST
// Test: Set long system message
START_TEST(test_system_long_message)
{
    const char *long_msg = "/system You are a helpful assistant that provides detailed "
                           "explanations and considers multiple perspectives when answering questions";
    res_t res = ik_cmd_dispatch(ctx, repl, long_msg);
    ck_assert(is_ok(&res));

    ck_assert_ptr_nonnull(repl->shared->cfg->openai_system_message);
    ck_assert_str_eq(repl->shared->cfg->openai_system_message,
                     "You are a helpful assistant that provides detailed "
                     "explanations and considers multiple perspectives when answering questions");
}

END_TEST
// Test: Multiple set/clear cycles
START_TEST(test_system_multiple_cycles)
{
    // Set message
    res_t res = ik_cmd_dispatch(ctx, repl, "/system First");
    ck_assert(is_ok(&res));
    ck_assert_str_eq(repl->shared->cfg->openai_system_message, "First");

    // Clear message
    res = ik_cmd_dispatch(ctx, repl, "/system");
    ck_assert(is_ok(&res));
    ck_assert_ptr_null(repl->shared->cfg->openai_system_message);

    // Set again
    res = ik_cmd_dispatch(ctx, repl, "/system Second");
    ck_assert(is_ok(&res));
    ck_assert_str_eq(repl->shared->cfg->openai_system_message, "Second");

    // Verify scrollback has all 3 messages
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 3);
}

END_TEST

// Test suite setup
static Suite *commands_system_suite(void)
{
    Suite *s = suite_create("Commands - System");

    TCase *tc_system = tcase_create("System");
    tcase_add_checked_fixture(tc_system, setup, teardown);
    tcase_add_test(tc_system, test_system_set_message);
    tcase_add_test(tc_system, test_system_clear_message);
    tcase_add_test(tc_system, test_system_replace_message);
    tcase_add_test(tc_system, test_system_with_special_chars);
    tcase_add_test(tc_system, test_system_long_message);
    tcase_add_test(tc_system, test_system_multiple_cycles);
    suite_add_tcase(s, tc_system);

    return s;
}

int main(void)
{
    Suite *s = commands_system_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
