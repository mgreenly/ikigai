/**
 * @file mark_scrollback_format_test.c
 * @brief Tests for rewind scrollback formatting (no role prefixes, system message included)
 *
 * Bug fix verification:
 * - Messages should NOT have "You:" or "Assistant:" prefixes after rewind
 * - System message from config should be rendered first
 */

#include "../../../src/agent.h"
#include "../../../src/message.h"
#include "../../../src/providers/provider.h"
#include <check.h>
#include <string.h>
#include <talloc.h>

#include "../../../src/config.h"
#include "../../../src/shared.h"
#include "../../../src/marks.h"
#include "../../../src/repl.h"
#include "../../../src/scrollback.h"
#include "../../test_utils.h"

// Test fixture
static TALLOC_CTX *ctx;
static ik_repl_ctx_t *repl;
static ik_config_t *cfg;

/**
 * Create a REPL context with conversation and config for testing
 */
static ik_repl_ctx_t *create_test_repl_with_config(void *parent)
{
    ik_scrollback_t *scrollback = ik_scrollback_create(parent, 80);
    ck_assert_ptr_nonnull(scrollback);

    // Create minimal config (will be replaced by setup)
    ik_config_t *test_cfg = talloc_zero(parent, ik_config_t);
    ck_assert_ptr_nonnull(test_cfg);

    // Create shared context
    ik_shared_ctx_t *shared = talloc_zero(parent, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    shared->cfg = test_cfg;

    ik_repl_ctx_t *r = talloc_zero(parent, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(r);

    // Create agent context
    ik_agent_ctx_t *agent = talloc_zero(r, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);
    agent->scrollback = scrollback;

    r->current = agent;

    r->shared = shared;
    r->current->marks = NULL;
    r->current->mark_count = 0;

    return r;
}

// Helper to get scrollback line text as null-terminated string
static const char *get_line_text(ik_scrollback_t *scrollback, size_t index)
{
    const char *text;
    size_t len;
    res_t res = ik_scrollback_get_line_text(scrollback, index, &text, &len);
    ck_assert(is_ok(&res));
    return text;
}

static void setup(void)
{
    ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create config with system message
    cfg = talloc_zero(ctx, ik_config_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->openai_system_message = talloc_strdup(cfg, "You are a helpful assistant for testing.");

    repl = create_test_repl_with_config(ctx);
    ck_assert_ptr_nonnull(repl);
    // Create shared context
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    shared->cfg = cfg;
    repl->shared = shared;
}

static void teardown(void)
{
    talloc_free(ctx);
}

// Test: Rewind should render messages without "You:" and "Assistant:" prefixes
START_TEST(test_rewind_no_role_prefixes) {
    // Add a user message
    ik_message_t *msg_user1 = ik_message_create_text(ctx, IK_ROLE_USER, "what is 2 + 2");
    // removed assertion
    ik_agent_add_message(repl->current, msg_user1);
    // removed assertion

    // Add an assistant response
    ik_message_t *msg_asst1 = ik_message_create_text(ctx, IK_ROLE_ASSISTANT, "2 + 2 = 4");
    // removed assertion
    ik_agent_add_message(repl->current, msg_asst1);
    // removed assertion

    // Create a mark
    res_t mark_res = ik_mark_create(repl, "qux");
    ck_assert(is_ok(&mark_res));

    // Add another exchange
    ik_message_t *msg_user2 = ik_message_create_text(ctx, IK_ROLE_USER, "what is 3 + 3");
    // removed assertion
    ik_agent_add_message(repl->current, msg_user2);
    // removed assertion

    ik_message_t *msg_asst2 = ik_message_create_text(ctx, IK_ROLE_ASSISTANT, "3 + 3 = 6");
    // removed assertion
    ik_agent_add_message(repl->current, msg_asst2);
    // removed assertion

    // Rewind to mark
    ik_mark_t *target_mark = NULL;
    res_t find_res = ik_mark_find(repl, "qux", &target_mark);
    ck_assert(is_ok(&find_res));

    res_t rewind_res = ik_mark_rewind_to_mark(repl, target_mark);
    ck_assert(is_ok(&rewind_res));

    // Check scrollback content - should NOT have "You:" or "Assistant:" prefixes
    // Line 0: system message
    // Line 1: blank line
    // Line 2: user message (no prefix)
    // Line 3: blank line
    // Line 4: assistant message (no prefix)
    // Line 5: blank line
    // Line 6: /mark qux
    // Line 7: blank line

    // Get scrollback line count (each event + blank line = 8 lines total)
    size_t line_count = ik_scrollback_get_line_count(repl->current->scrollback);
    ck_assert_uint_eq(line_count, 8);

    // Get lines and verify content
    const char *line0 = get_line_text(repl->current->scrollback, 0);
    const char *line2 = get_line_text(repl->current->scrollback, 2);
    const char *line4 = get_line_text(repl->current->scrollback, 4);
    const char *line6 = get_line_text(repl->current->scrollback, 6);

    // Verify system message is first (with color styling)
    ck_assert_ptr_nonnull(strstr(line0, "You are a helpful assistant for testing."));

    // Verify user message has no "You:" prefix
    ck_assert_str_eq(line2, "what is 2 + 2");

    // Verify assistant message has no "Assistant:" prefix (but has color styling)
    ck_assert_ptr_nonnull(strstr(line4, "2 + 2 = 4"));

    // Verify mark indicator
    ck_assert_str_eq(line6, "/mark qux");
}
END_TEST
// Test: Rewind should include system message from config
START_TEST(test_rewind_includes_system_message)
{
    // Add a user message
    ik_message_t *msg_user = ik_message_create_text(ctx, IK_ROLE_USER, "Hello");
    // removed assertion
    ik_agent_add_message(repl->current, msg_user);
    // removed assertion

    // Create a mark
    res_t mark_res = ik_mark_create(repl, "test");
    ck_assert(is_ok(&mark_res));

    // Add more content
    ik_message_t *msg_asst = ik_message_create_text(ctx, IK_ROLE_ASSISTANT, "World");
    // removed assertion
    ik_agent_add_message(repl->current, msg_asst);
    // removed assertion

    // Rewind
    ik_mark_t *target_mark = NULL;
    res_t find_res = ik_mark_find(repl, "test", &target_mark);
    ck_assert(is_ok(&find_res));

    res_t rewind_res = ik_mark_rewind_to_mark(repl, target_mark);
    ck_assert(is_ok(&rewind_res));

    // Verify system message is first line (with color styling)
    const char *line0 = get_line_text(repl->current->scrollback, 0);
    ck_assert_ptr_nonnull(strstr(line0, "You are a helpful assistant for testing."));
}

END_TEST
// Test: Rewind without system message configured
START_TEST(test_rewind_without_system_message)
{
    // Remove system message from config
    talloc_free(cfg->openai_system_message);
    cfg->openai_system_message = NULL;

    // Add a user message
    ik_message_t *msg_user = ik_message_create_text(ctx, IK_ROLE_USER, "Hello");
    // removed assertion
    ik_agent_add_message(repl->current, msg_user);
    // removed assertion

    // Create a mark
    res_t mark_res = ik_mark_create(repl, "test");
    ck_assert(is_ok(&mark_res));

    // Add more content
    ik_message_t *msg_asst = ik_message_create_text(ctx, IK_ROLE_ASSISTANT, "World");
    // removed assertion
    ik_agent_add_message(repl->current, msg_asst);
    // removed assertion

    // Rewind
    ik_mark_t *target_mark = NULL;
    res_t find_res = ik_mark_find(repl, "test", &target_mark);
    ck_assert(is_ok(&find_res));

    res_t rewind_res = ik_mark_rewind_to_mark(repl, target_mark);
    ck_assert(is_ok(&rewind_res));

    // First line should be user message (no system message)
    const char *line0 = get_line_text(repl->current->scrollback, 0);
    ck_assert_str_eq(line0, "Hello");
}

END_TEST
// Test: Rewind with NULL config
START_TEST(test_rewind_with_null_config)
{
    // Set config to NULL
    repl->shared->cfg = NULL;

    // Add a user message
    ik_message_t *msg_user = ik_message_create_text(ctx, IK_ROLE_USER, "Test message");
    // removed assertion
    ik_agent_add_message(repl->current, msg_user);
    // removed assertion

    // Create a mark
    res_t mark_res = ik_mark_create(repl, "test");
    ck_assert(is_ok(&mark_res));

    // Add more content
    ik_message_t *msg_asst = ik_message_create_text(ctx, IK_ROLE_ASSISTANT, "Response");
    // removed assertion
    ik_agent_add_message(repl->current, msg_asst);
    // removed assertion

    // Rewind should succeed even without config
    ik_mark_t *target_mark = NULL;
    res_t find_res = ik_mark_find(repl, "test", &target_mark);
    ck_assert(is_ok(&find_res));

    res_t rewind_res = ik_mark_rewind_to_mark(repl, target_mark);
    ck_assert(is_ok(&rewind_res));

    // First line should be user message (no system message since no config)
    const char *line0 = get_line_text(repl->current->scrollback, 0);
    ck_assert_str_eq(line0, "Test message");
}

END_TEST

static Suite *mark_scrollback_format_suite(void)
{
    Suite *s = suite_create("Mark Scrollback Format");
    TCase *tc = tcase_create("scrollback_format");
    tcase_set_timeout(tc, 30);

    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_rewind_no_role_prefixes);
    tcase_add_test(tc, test_rewind_includes_system_message);
    tcase_add_test(tc, test_rewind_without_system_message);
    tcase_add_test(tc, test_rewind_with_null_config);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = mark_scrollback_format_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
