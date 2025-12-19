/**
 * @file clear_test.c
 * @brief Unit tests for /clear command core functionality
 */

#include "../../../src/agent.h"
#include "../../../src/commands.h"
#include "../../../src/config.h"
#include "../../../src/logger.h"
#include "../../../src/shared.h"
#include "../../../src/error.h"
#include "../../../src/marks.h"
#include "../../../src/openai/client.h"
#include "../../../src/repl.h"
#include "../../../src/scrollback.h"
#include "../../../src/wrapper.h"
#include "../../test_utils.h"

#include <check.h>
#include <string.h>
#include <talloc.h>

// Test fixture
static void *ctx;
static ik_repl_ctx_t *repl;

/**
 * Create a REPL context with scrollback and conversation for clear testing.
 */
static ik_repl_ctx_t *create_test_repl_with_conversation(void *parent)
{
    // Create scrollback buffer (80 columns is standard)
    ik_scrollback_t *scrollback = ik_scrollback_create(parent, 80);
    ck_assert_ptr_nonnull(scrollback);

    // Create conversation
    ik_openai_conversation_t *conv = ik_openai_conversation_create(parent);
    ck_assert_ptr_nonnull(conv);

    // Create minimal config
    ik_cfg_t *cfg = talloc_zero(parent, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);

    // Create shared context
    ik_shared_ctx_t *shared = talloc_zero(parent, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    shared->cfg = cfg;

    // Create logger (required by /clear command)
    shared->logger = ik_logger_create(parent, ".");

    // Create minimal REPL context
    ik_repl_ctx_t *r = talloc_zero(parent, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(r);
    
    // Create agent context
    ik_agent_ctx_t *agent = talloc_zero(r, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);
    agent->scrollback = scrollback;
    agent->conversation = conv;
    r->current = agent;

    r->shared = shared;

    return r;
}

static void setup(void)
{
    ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    repl = create_test_repl_with_conversation(ctx);
    ck_assert_ptr_nonnull(repl);
}

static void teardown(void)
{
    talloc_free(ctx);
}

// Test: Clear empty scrollback and conversation
START_TEST(test_clear_empty) {
    // Verify initially empty
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 0);
    ck_assert_uint_eq(repl->current->conversation->message_count, 0);

    // Execute /clear
    res_t res = ik_cmd_dispatch(ctx, repl, "/clear");
    ck_assert(is_ok(&res));

    // Verify still empty
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 0);
    ck_assert_uint_eq(repl->current->conversation->message_count, 0);
}
END_TEST
// Test: Clear scrollback with content
START_TEST(test_clear_scrollback_with_content)
{
    // Add some lines to scrollback
    res_t res = ik_scrollback_append_line(repl->current->scrollback, "Line 1", 6);
    ck_assert(is_ok(&res));
    res = ik_scrollback_append_line(repl->current->scrollback, "Line 2", 6);
    ck_assert(is_ok(&res));
    res = ik_scrollback_append_line(repl->current->scrollback, "Line 3", 6);
    ck_assert(is_ok(&res));

    // Verify content exists
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 3);

    // Execute /clear
    res = ik_cmd_dispatch(ctx, repl, "/clear");
    ck_assert(is_ok(&res));

    // Verify scrollback is empty
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 0);
}

END_TEST
// Test: Clear conversation with messages
START_TEST(test_clear_conversation_with_messages)
{
    // Add messages to conversation
    ik_msg_t *msg1 = ik_openai_msg_create(ctx, "user", "Hello");

    res_t res = ik_openai_conversation_add_msg(repl->current->conversation, msg1);
    ck_assert(is_ok(&res));

    ik_msg_t *msg2 = ik_openai_msg_create(ctx, "assistant", "Hi there!");

    res = ik_openai_conversation_add_msg(repl->current->conversation, msg2);
    ck_assert(is_ok(&res));

    // Verify messages exist
    ck_assert_uint_eq(repl->current->conversation->message_count, 2);

    // Execute /clear
    res = ik_cmd_dispatch(ctx, repl, "/clear");
    ck_assert(is_ok(&res));

    // Verify conversation is empty
    ck_assert_uint_eq(repl->current->conversation->message_count, 0);
    ck_assert_ptr_null(repl->current->conversation->messages);
}

END_TEST
// Test: Clear both scrollback and conversation
START_TEST(test_clear_both_scrollback_and_conversation)
{
    // Add scrollback content
    res_t res = ik_scrollback_append_line(repl->current->scrollback, "User message", 12);
    ck_assert(is_ok(&res));
    res = ik_scrollback_append_line(repl->current->scrollback, "Assistant response", 18);
    ck_assert(is_ok(&res));

    // Add conversation messages
    ik_msg_t *msg1 = ik_openai_msg_create(ctx, "user", "User message");
    res = ik_openai_conversation_add_msg(repl->current->conversation, msg1);
    ck_assert(is_ok(&res));

    ik_msg_t *msg2 = ik_openai_msg_create(ctx, "assistant", "Assistant response");
    res = ik_openai_conversation_add_msg(repl->current->conversation, msg2);
    ck_assert(is_ok(&res));

    // Verify both have content
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 2);
    ck_assert_uint_eq(repl->current->conversation->message_count, 2);

    // Execute /clear
    res = ik_cmd_dispatch(ctx, repl, "/clear");
    ck_assert(is_ok(&res));

    // Verify both are empty
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 0);
    ck_assert_uint_eq(repl->current->conversation->message_count, 0);
}

END_TEST
// Test: Clear with NULL conversation (defensive check)
START_TEST(test_clear_with_null_conversation)
{
    // Set conversation to NULL
    repl->current->conversation = NULL;

    // Add scrollback content
    res_t res = ik_scrollback_append_line(repl->current->scrollback, "Line 1", 6);
    ck_assert(is_ok(&res));

    // Verify scrollback has content
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 1);

    // Execute /clear (should not crash)
    res = ik_cmd_dispatch(ctx, repl, "/clear");
    ck_assert(is_ok(&res));

    // Verify scrollback is cleared
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 0);
}

END_TEST
// Test: Clear command with arguments (should be ignored)
START_TEST(test_clear_with_ignored_arguments)
{
    // Add content
    res_t res = ik_scrollback_append_line(repl->current->scrollback, "Line 1", 6);
    ck_assert(is_ok(&res));

    // Execute /clear with extra arguments (should be ignored)
    res = ik_cmd_dispatch(ctx, repl, "/clear extra args");
    ck_assert(is_ok(&res));

    // Verify still cleared
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 0);
}

END_TEST
// Test: Clear with marks
START_TEST(test_clear_with_marks)
{
    // Add some content and marks
    res_t res = ik_scrollback_append_line(repl->current->scrollback, "Line 1", 6);
    ck_assert(is_ok(&res));

    ik_msg_t *msg = ik_openai_msg_create(ctx, "user", "Message");
    res = ik_openai_conversation_add_msg(repl->current->conversation, msg);
    ck_assert(is_ok(&res));

    // Create marks
    res = ik_mark_create(repl, "mark1");
    ck_assert(is_ok(&res));
    res = ik_mark_create(repl, "mark2");
    ck_assert(is_ok(&res));

    // Verify marks exist
    ck_assert_uint_eq(repl->current->mark_count, 2);
    ck_assert_ptr_nonnull(repl->current->marks);

    // Execute /clear
    res = ik_cmd_dispatch(ctx, repl, "/clear");
    ck_assert(is_ok(&res));

    // Verify marks are cleared
    ck_assert_uint_eq(repl->current->mark_count, 0);
    ck_assert_ptr_null(repl->current->marks);

    // Verify scrollback and conversation also cleared
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 0);
    ck_assert_uint_eq(repl->current->conversation->message_count, 0);
}

END_TEST
// Test: Clear with system message should display system message in scrollback
START_TEST(test_clear_with_system_message_displays_in_scrollback)
{
    // Create a config with system message
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->openai_system_message = talloc_strdup(cfg, "You are a helpful assistant.");
    ck_assert_ptr_nonnull(cfg->openai_system_message);

    // Attach config to REPL
    // Create shared context
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    shared->cfg = cfg;
    shared->logger = ik_logger_create(ctx, ".");
    repl->shared = shared;

    // Add some content to scrollback first
    res_t res = ik_scrollback_append_line(repl->current->scrollback, "User message", 12);
    ck_assert(is_ok(&res));
    res = ik_scrollback_append_line(repl->current->scrollback, "Assistant response", 18);
    ck_assert(is_ok(&res));

    // Verify scrollback has content
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 2);

    // Execute /clear
    res = ik_cmd_dispatch(ctx, repl, "/clear");
    ck_assert(is_ok(&res));

    // After /clear with system message configured,
    // scrollback should have 2 lines (the system message + blank line), not 0
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 2);

    // Verify the content is the system message (with color styling)
    const char *line = NULL;
    size_t line_len = 0;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 0, &line, &line_len);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(line);
    // System messages are colored with gray 242
    ck_assert_ptr_nonnull(strstr(line, "You are a helpful assistant."));

    // Verify the second line is blank
    res = ik_scrollback_get_line_text(repl->current->scrollback, 1, &line, &line_len);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(line_len, 0);
}

END_TEST
// Test: Clear without system message should have empty scrollback
START_TEST(test_clear_without_system_message_empty_scrollback)
{
    // Create a config WITHOUT system message
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->openai_system_message = NULL;

    // Attach config to REPL
    // Create shared context
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    shared->cfg = cfg;
    shared->logger = ik_logger_create(ctx, ".");
    repl->shared = shared;

    // Add some content to scrollback
    res_t res = ik_scrollback_append_line(repl->current->scrollback, "User message", 12);
    ck_assert(is_ok(&res));

    // Verify scrollback has content
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 1);

    // Execute /clear
    res = ik_cmd_dispatch(ctx, repl, "/clear");
    ck_assert(is_ok(&res));

    // Without system message, scrollback should be empty
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 0);
}

END_TEST
// Test: Clear with system message when append fails (OOM during scrollback append)
START_TEST(test_clear_with_system_message_append_failure)
{
    // Reset mock state (uses global mocking variables from test_utils)
    ik_test_talloc_realloc_fail_on_call = -1;
    ik_test_talloc_realloc_call_count = 0;

    // Create a very long system message that will exceed buffer capacity
    // Initial buffer capacity is 1024 bytes
    char long_message[2000];
    memset(long_message, 'A', sizeof(long_message) - 1);
    long_message[sizeof(long_message) - 1] = '\0';

    // Create a config with long system message
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->openai_system_message = talloc_strdup(cfg, long_message);
    ck_assert_ptr_nonnull(cfg->openai_system_message);

    // Attach config to REPL
    // Create shared context
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    shared->cfg = cfg;
    shared->logger = ik_logger_create(ctx, ".");
    repl->shared = shared;

    // Add some content to scrollback first
    res_t res = ik_scrollback_append_line(repl->current->scrollback, "Initial content", 15);
    ck_assert(is_ok(&res));

    // Reset counter after setup, so we can count reallocs during /clear
    ik_test_talloc_realloc_call_count = 0;

    // Set mock to fail on the first realloc during /clear
    // This will be the buffer reallocation when appending the long system message
    ik_test_talloc_realloc_fail_on_call = 0;

    // Execute /clear - should fail when trying to append system message
    res = ik_cmd_dispatch(ctx, repl, "/clear");

    // Should return error (not crash)
    ck_assert(is_err(&res));

    // Cleanup error
    talloc_free(res.err);

    // Disable mock
    ik_test_talloc_realloc_fail_on_call = -1;
}

END_TEST

static Suite *commands_clear_suite(void)
{
    Suite *s = suite_create("Commands/Clear");
    TCase *tc = tcase_create("Core");

    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_clear_empty);
    tcase_add_test(tc, test_clear_scrollback_with_content);
    tcase_add_test(tc, test_clear_conversation_with_messages);
    tcase_add_test(tc, test_clear_both_scrollback_and_conversation);
    tcase_add_test(tc, test_clear_with_null_conversation);
    tcase_add_test(tc, test_clear_with_ignored_arguments);
    tcase_add_test(tc, test_clear_with_marks);
    tcase_add_test(tc, test_clear_with_system_message_displays_in_scrollback);
    tcase_add_test(tc, test_clear_without_system_message_empty_scrollback);
    tcase_add_test(tc, test_clear_with_system_message_append_failure);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = commands_clear_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
