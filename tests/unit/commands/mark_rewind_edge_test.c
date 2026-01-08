/**
 * @file mark_rewind_edge_test.c
 * @brief Edge case tests for mark rewind to achieve 100% branch coverage
 *
 * This test file covers edge cases in ik_mark_rewind_to_mark():
 * - Line 185: Messages with invalid role values (default case in switch)
 * - Line 190: Messages with kind != NULL but content_count == 0
 * - Line 190: Messages with kind != NULL, content_count > 0 but non-TEXT content
 */

#include "../../../src/agent.h"
#include "../../../src/config.h"
#include "../../../src/marks.h"
#include "../../../src/message.h"
#include "../../../src/providers/provider.h"
#include "../../../src/repl.h"
#include "../../../src/scrollback.h"
#include "../../../src/shared.h"
#include "../../test_utils.h"

#include <check.h>
#include <talloc.h>

// Test fixture
static TALLOC_CTX *ctx;
static ik_repl_ctx_t *repl;

/**
 * Create a REPL context with conversation for testing
 */
static ik_repl_ctx_t *create_test_repl_with_conversation(void *parent)
{
    ik_scrollback_t *scrollback = ik_scrollback_create(parent, 80);
    ck_assert_ptr_nonnull(scrollback);

    // Create minimal config
    ik_config_t *cfg = talloc_zero(parent, ik_config_t);
    ck_assert_ptr_nonnull(cfg);

    // Create shared context
    ik_shared_ctx_t *shared = talloc_zero(parent, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    shared->cfg = cfg;

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
    r->current->messages = NULL;
    r->current->message_count = 0;
    r->current->message_capacity = 0;

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

// Test: Rewind with message that has invalid role value (line 185, branch 3)
START_TEST(test_rewind_with_invalid_role) {
    // Create a normal user message
    ik_message_t *msg_user = ik_message_create_text(ctx, IK_ROLE_USER, "Hello");
    ck_assert_ptr_nonnull(msg_user);
    res_t res = ik_agent_add_message(repl->current, msg_user);
    ck_assert(is_ok(&res));

    // Create a message and manually set an invalid role value
    ik_message_t *msg_invalid = talloc_zero(ctx, ik_message_t);
    ck_assert_ptr_nonnull(msg_invalid);
    msg_invalid->role = (ik_role_t)999;  // Invalid role value
    msg_invalid->content_count = 1;
    msg_invalid->content_blocks = talloc_zero(msg_invalid, ik_content_block_t);
    ck_assert_ptr_nonnull(msg_invalid->content_blocks);
    msg_invalid->content_blocks[0].type = IK_CONTENT_TEXT;
    msg_invalid->content_blocks[0].data.text.text = talloc_strdup(msg_invalid->content_blocks, "Test");
    ck_assert_ptr_nonnull(msg_invalid->content_blocks[0].data.text.text);

    res = ik_agent_add_message(repl->current, msg_invalid);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->current->message_count, 2);

    // Create a mark after the invalid role message
    res_t mark_res = ik_mark_create(repl, "after_invalid");
    ck_assert(is_ok(&mark_res));

    // Add another message
    ik_message_t *msg_user2 = ik_message_create_text(ctx, IK_ROLE_USER, "More");
    ck_assert_ptr_nonnull(msg_user2);
    res = ik_agent_add_message(repl->current, msg_user2);
    ck_assert(is_ok(&res));

    // Rewind to the mark (should skip rendering the invalid role message)
    ik_mark_t *target_mark = NULL;
    res_t find_res = ik_mark_find(repl, "after_invalid", &target_mark);
    ck_assert(is_ok(&find_res));

    res_t rewind_res = ik_mark_rewind_to_mark(repl, target_mark);
    ck_assert(is_ok(&rewind_res));

    // Should be back to 2 messages
    ck_assert_uint_eq(repl->current->message_count, 2);
}
END_TEST
// Test: Rewind with message that has content_count == 0 (line 190, branch 1)
START_TEST(test_rewind_with_empty_content) {
    // Create a normal user message
    ik_message_t *msg_user = ik_message_create_text(ctx, IK_ROLE_USER, "Hello");
    ck_assert_ptr_nonnull(msg_user);
    res_t res = ik_agent_add_message(repl->current, msg_user);
    ck_assert(is_ok(&res));

    // Create a message with content_count == 0
    ik_message_t *msg_empty = talloc_zero(ctx, ik_message_t);
    ck_assert_ptr_nonnull(msg_empty);
    msg_empty->role = IK_ROLE_ASSISTANT;
    msg_empty->content_count = 0;  // No content blocks
    msg_empty->content_blocks = NULL;

    res = ik_agent_add_message(repl->current, msg_empty);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->current->message_count, 2);

    // Create a mark after the empty content message
    res_t mark_res = ik_mark_create(repl, "after_empty");
    ck_assert(is_ok(&mark_res));

    // Add another message
    ik_message_t *msg_user2 = ik_message_create_text(ctx, IK_ROLE_USER, "More");
    ck_assert_ptr_nonnull(msg_user2);
    res = ik_agent_add_message(repl->current, msg_user2);
    ck_assert(is_ok(&res));

    // Rewind to the mark (should skip rendering the empty content message)
    ik_mark_t *target_mark = NULL;
    res_t find_res = ik_mark_find(repl, "after_empty", &target_mark);
    ck_assert(is_ok(&find_res));

    res_t rewind_res = ik_mark_rewind_to_mark(repl, target_mark);
    ck_assert(is_ok(&rewind_res));

    // Should be back to 2 messages
    ck_assert_uint_eq(repl->current->message_count, 2);
}

END_TEST
// Test: Rewind with message that has non-TEXT content type (line 190, branch 3)
START_TEST(test_rewind_with_non_text_content) {
    // Create a normal user message
    ik_message_t *msg_user = ik_message_create_text(ctx, IK_ROLE_USER, "Hello");
    ck_assert_ptr_nonnull(msg_user);
    res_t res = ik_agent_add_message(repl->current, msg_user);
    ck_assert(is_ok(&res));

    // Create a message with IK_CONTENT_THINKING instead of IK_CONTENT_TEXT
    ik_message_t *msg_thinking = talloc_zero(ctx, ik_message_t);
    ck_assert_ptr_nonnull(msg_thinking);
    msg_thinking->role = IK_ROLE_ASSISTANT;
    msg_thinking->content_count = 1;
    msg_thinking->content_blocks = talloc_zero(msg_thinking, ik_content_block_t);
    ck_assert_ptr_nonnull(msg_thinking->content_blocks);
    msg_thinking->content_blocks[0].type = IK_CONTENT_THINKING;  // Non-TEXT type
    msg_thinking->content_blocks[0].data.thinking.text = talloc_strdup(msg_thinking->content_blocks, "Thinking...");
    ck_assert_ptr_nonnull(msg_thinking->content_blocks[0].data.thinking.text);

    res = ik_agent_add_message(repl->current, msg_thinking);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->current->message_count, 2);

    // Create a mark after the thinking content message
    res_t mark_res = ik_mark_create(repl, "after_thinking");
    ck_assert(is_ok(&mark_res));

    // Add another message
    ik_message_t *msg_user2 = ik_message_create_text(ctx, IK_ROLE_USER, "More");
    ck_assert_ptr_nonnull(msg_user2);
    res = ik_agent_add_message(repl->current, msg_user2);
    ck_assert(is_ok(&res));

    // Rewind to the mark (should skip rendering the thinking content message)
    ik_mark_t *target_mark = NULL;
    res_t find_res = ik_mark_find(repl, "after_thinking", &target_mark);
    ck_assert(is_ok(&find_res));

    res_t rewind_res = ik_mark_rewind_to_mark(repl, target_mark);
    ck_assert(is_ok(&rewind_res));

    // Should be back to 2 messages
    ck_assert_uint_eq(repl->current->message_count, 2);
}

END_TEST

static Suite *mark_rewind_edge_suite(void)
{
    Suite *s = suite_create("Mark Rewind Edge Cases");
    TCase *tc = tcase_create("edge_cases");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);

    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_rewind_with_invalid_role);
    tcase_add_test(tc, test_rewind_with_empty_content);
    tcase_add_test(tc, test_rewind_with_non_text_content);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = mark_rewind_edge_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
