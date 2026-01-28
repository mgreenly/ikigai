/**
 * @file clear_test.c
 * @brief Unit tests for /clear command core functionality
 */

#include "../../../src/agent.h"
#include "../../../src/commands.h"
#include "../../../src/config.h"
#include "../../../src/config_defaults.h"
#include "../../../src/logger.h"
#include "../../../src/shared.h"
#include "../../../src/error.h"
#include "../../../src/marks.h"
#include "../../../src/message.h"
#include "../../../src/repl.h"
#include "../../../src/scrollback.h"
#include "../../../src/wrapper.h"
#include "../../test_utils_helper.h"

#include <check.h>
#include <string.h>
#include <talloc.h>

// Test fixture
static void *ctx;
static ik_repl_ctx_t *repl;

// Suite-level setup: Set log directory
static void suite_setup(void)
{
    ik_test_set_log_dir(__FILE__);
}

/**
 * Create a REPL context with scrollback for clear testing.
 */
static ik_repl_ctx_t *create_test_repl_with_conversation(void *parent)
{
    // Create scrollback buffer (80 columns is standard)
    ik_scrollback_t *scrollback = ik_scrollback_create(parent, 80);
    ck_assert_ptr_nonnull(scrollback);

    // Create minimal config
    ik_config_t *cfg = talloc_zero(parent, ik_config_t);
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

    // Create agent context (messages array starts empty)
    ik_agent_ctx_t *agent = talloc_zero(r, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);
    agent->scrollback = scrollback;
    agent->shared = shared;  // Agent needs shared for system prompt fallback

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
// After clear, the default system message is displayed (fallback chain priority 4)
START_TEST(test_clear_empty) {
    // Verify initially empty
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 0);
    ck_assert_uint_eq(repl->current->message_count, 0);

    // Execute /clear
    res_t res = ik_cmd_dispatch(ctx, repl, "/clear");
    ck_assert(is_ok(&res));

    // After clear, default system message is shown (2 lines: message + blank)
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 2);
    ck_assert_uint_eq(repl->current->message_count, 0);

    // Verify the content is the default system message
    const char *line = NULL;
    size_t line_len = 0;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 0, &line, &line_len);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(strstr(line, "Ikigai"));
}
END_TEST
// Test: Clear scrollback with content
// After clear, previous content is replaced with default system message
START_TEST(test_clear_scrollback_with_content) {
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

    // Verify scrollback has default system message (2 lines: message + blank)
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 2);
}

END_TEST
// Test: Clear conversation with messages
START_TEST(test_clear_conversation_with_messages) {
    // Add messages using new API
    ik_message_t *msg1 = ik_message_create_text(ctx, IK_ROLE_USER, "Hello");
    res_t res = ik_agent_add_message(repl->current, msg1);
    ck_assert(is_ok(&res));

    ik_message_t *msg2 = ik_message_create_text(ctx, IK_ROLE_ASSISTANT, "Hi there!");
    res = ik_agent_add_message(repl->current, msg2);
    ck_assert(is_ok(&res));

    // Verify messages exist
    ck_assert_uint_eq(repl->current->message_count, 2);

    // Execute /clear
    res = ik_cmd_dispatch(ctx, repl, "/clear");
    ck_assert(is_ok(&res));

    // Verify conversation is empty
    ck_assert_uint_eq(repl->current->message_count, 0);
    ck_assert_ptr_null(repl->current->messages);
}

END_TEST
// Test: Clear both scrollback and conversation
START_TEST(test_clear_both_scrollback_and_conversation) {
    // Add scrollback content
    res_t res = ik_scrollback_append_line(repl->current->scrollback, "User message", 12);
    ck_assert(is_ok(&res));
    res = ik_scrollback_append_line(repl->current->scrollback, "Assistant response", 18);
    ck_assert(is_ok(&res));

    // Add conversation messages using new API
    ik_message_t *msg1 = ik_message_create_text(ctx, IK_ROLE_USER, "User message");
    res = ik_agent_add_message(repl->current, msg1);
    ck_assert(is_ok(&res));

    ik_message_t *msg2 = ik_message_create_text(ctx, IK_ROLE_ASSISTANT, "Assistant response");
    res = ik_agent_add_message(repl->current, msg2);
    ck_assert(is_ok(&res));

    // Verify both have content
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 2);
    ck_assert_uint_eq(repl->current->message_count, 2);

    // Execute /clear
    res = ik_cmd_dispatch(ctx, repl, "/clear");
    ck_assert(is_ok(&res));

    // Verify conversation is empty, scrollback has default system message
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 2);
    ck_assert_uint_eq(repl->current->message_count, 0);
}

END_TEST
// Test: Clear with empty messages (defensive check)
START_TEST(test_clear_with_null_conversation) {
    // Messages array starts NULL (empty)
    ck_assert_ptr_null(repl->current->messages);
    ck_assert_uint_eq(repl->current->message_count, 0);

    // Add scrollback content
    res_t res = ik_scrollback_append_line(repl->current->scrollback, "Line 1", 6);
    ck_assert(is_ok(&res));

    // Verify scrollback has content
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 1);

    // Execute /clear (should not crash)
    res = ik_cmd_dispatch(ctx, repl, "/clear");
    ck_assert(is_ok(&res));

    // Verify scrollback has default system message (2 lines)
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 2);
}

END_TEST
// Test: Clear command with arguments (should be ignored)
START_TEST(test_clear_with_ignored_arguments) {
    // Add content
    res_t res = ik_scrollback_append_line(repl->current->scrollback, "Line 1", 6);
    ck_assert(is_ok(&res));

    // Execute /clear with extra arguments (should be ignored)
    res = ik_cmd_dispatch(ctx, repl, "/clear extra args");
    ck_assert(is_ok(&res));

    // Verify old content cleared, default system message shown
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 2);
}

END_TEST
// Test: Clear with marks
START_TEST(test_clear_with_marks) {
    // Add some content and marks
    res_t res = ik_scrollback_append_line(repl->current->scrollback, "Line 1", 6);
    ck_assert(is_ok(&res));

    ik_message_t *msg = ik_message_create_text(ctx, IK_ROLE_USER, "Message");
    res = ik_agent_add_message(repl->current, msg);
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

    // Verify conversation cleared, scrollback has default system message
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 2);
    ck_assert_uint_eq(repl->current->message_count, 0);
}

END_TEST
// Test: Clear with system message should display system message in scrollback
START_TEST(test_clear_with_system_message_displays_in_scrollback) {
    // Create a config with system message
    ik_config_t *cfg = talloc_zero(ctx, ik_config_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->openai_system_message = talloc_strdup(cfg, "You are a helpful assistant.");
    ck_assert_ptr_nonnull(cfg->openai_system_message);

    // Attach config to REPL and agent
    // Create shared context
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    shared->cfg = cfg;
    shared->logger = ik_logger_create(ctx, ".");
    repl->shared = shared;
    repl->current->shared = shared;

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
// Test: Clear without config system message shows default message
// When no config message is set, the hardcoded default is shown (priority 4 in fallback)
START_TEST(test_clear_without_config_shows_default_message) {
    // Create a config WITHOUT system message
    ik_config_t *cfg = talloc_zero(ctx, ik_config_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->openai_system_message = NULL;

    // Attach config to REPL and agent
    // Create shared context
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    shared->cfg = cfg;
    shared->logger = ik_logger_create(ctx, ".");
    repl->shared = shared;
    repl->current->shared = shared;

    // Add some content to scrollback
    res_t res = ik_scrollback_append_line(repl->current->scrollback, "User message", 12);
    ck_assert(is_ok(&res));

    // Verify scrollback has content
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 1);

    // Execute /clear
    res = ik_cmd_dispatch(ctx, repl, "/clear");
    ck_assert(is_ok(&res));

    // With fallback chain, default system message is always shown (2 lines)
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->current->scrollback), 2);

    // Verify the content is the default system message
    const char *line = NULL;
    size_t line_len = 0;
    res = ik_scrollback_get_line_text(repl->current->scrollback, 0, &line, &line_len);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(line);
    ck_assert_ptr_nonnull(strstr(line, "Ikigai"));
}

END_TEST
// Test: Clear with system message completes successfully even with long config message
// Note: System messages are truncated to 256 chars for display, so the original
// long message content doesn't affect scrollback capacity requirements
START_TEST(test_clear_with_long_system_message_truncates) {
    // Create a very long system message
    char long_message[2000];
    memset(long_message, 'A', sizeof(long_message) - 1);
    long_message[sizeof(long_message) - 1] = '\0';

    // Create a config with long system message
    ik_config_t *cfg = talloc_zero(ctx, ik_config_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->openai_system_message = talloc_strdup(cfg, long_message);
    ck_assert_ptr_nonnull(cfg->openai_system_message);

    // Attach config to REPL and agent
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    shared->cfg = cfg;
    shared->logger = ik_logger_create(ctx, ".");
    repl->shared = shared;
    repl->current->shared = shared;

    // Execute /clear
    res_t res = ik_cmd_dispatch(ctx, repl, "/clear");
    ck_assert(is_ok(&res));

    // Verify scrollback has content (system message was rendered)
    ck_assert_uint_gt(ik_scrollback_get_line_count(repl->current->scrollback), 0);

    // Get the rendered content and verify it's truncated (ends with "...")
    const char *text;
    size_t length;
    ik_scrollback_get_line_text(repl->current->scrollback, 0, &text, &length);
    ck_assert_ptr_nonnull(strstr(text, "..."));
}

END_TEST

static Suite *commands_clear_suite(void)
{
    Suite *s = suite_create("Commands/Clear");
    TCase *tc = tcase_create("Core");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);

    tcase_add_unchecked_fixture(tc, suite_setup, NULL);
    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_clear_empty);
    tcase_add_test(tc, test_clear_scrollback_with_content);
    tcase_add_test(tc, test_clear_conversation_with_messages);
    tcase_add_test(tc, test_clear_both_scrollback_and_conversation);
    tcase_add_test(tc, test_clear_with_null_conversation);
    tcase_add_test(tc, test_clear_with_ignored_arguments);
    tcase_add_test(tc, test_clear_with_marks);
    tcase_add_test(tc, test_clear_with_system_message_displays_in_scrollback);
    tcase_add_test(tc, test_clear_without_config_shows_default_message);
    tcase_add_test(tc, test_clear_with_long_system_message_truncates);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = commands_clear_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/commands/clear_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
