/**
 * @file mark_system_role_test.c
 * @brief Tests for mark rewind with various role messages (covers marks.c line 172)
 *
 * NOTE: In the new message API, system messages are handled separately via
 * request->system_prompt and are not stored in the messages array. These tests
 * have been updated to test mark/rewind behavior with user/assistant messages.
 */

#include "../../../src/agent.h"
#include "../../../src/message.h"
#include "../../../src/providers/provider.h"
#include <check.h>
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

// Test: Rewind preserves message order
START_TEST(test_rewind_preserves_message_order) {
    // Create a user message
    ik_message_t *msg_user1 = ik_message_create_text(ctx, IK_ROLE_USER, "Hello");
    ck_assert_ptr_nonnull(msg_user1);
    res_t res = ik_agent_add_message(repl->current, msg_user1);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->current->message_count, 1);

    // Create an assistant message
    ik_message_t *msg_asst1 = ik_message_create_text(ctx, IK_ROLE_ASSISTANT, "Hi there");
    ck_assert_ptr_nonnull(msg_asst1);
    res = ik_agent_add_message(repl->current, msg_asst1);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->current->message_count, 2);

    // Create a mark
    res_t mark_res = ik_mark_create(repl, "checkpoint");
    ck_assert(is_ok(&mark_res));
    ck_assert_uint_eq(repl->current->mark_count, 1);

    // Add more messages
    ik_message_t *msg_user2 = ik_message_create_text(ctx, IK_ROLE_USER, "How are you?");
    ck_assert_ptr_nonnull(msg_user2);
    res = ik_agent_add_message(repl->current, msg_user2);
    ck_assert(is_ok(&res));

    ik_message_t *msg_asst2 = ik_message_create_text(ctx, IK_ROLE_ASSISTANT, "I'm fine!");
    ck_assert_ptr_nonnull(msg_asst2);
    res = ik_agent_add_message(repl->current, msg_asst2);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->current->message_count, 4);

    // Find and rewind to the mark
    ik_mark_t *target_mark = NULL;
    res_t find_res = ik_mark_find(repl, "checkpoint", &target_mark);
    ck_assert(is_ok(&find_res));

    res_t rewind_res = ik_mark_rewind_to_mark(repl, target_mark);
    ck_assert(is_ok(&rewind_res));

    // Verify conversation was rewound to 2 messages
    ck_assert_uint_eq(repl->current->message_count, 2);
    ck_assert(repl->current->messages[0]->role == IK_ROLE_USER);
    ck_assert(repl->current->messages[1]->role == IK_ROLE_ASSISTANT);
}
END_TEST
// Test: Rewind with multiple user/assistant pairs
START_TEST(test_rewind_with_multiple_message_pairs) {
    // Add several user/assistant pairs
    for (int i = 0; i < 3; i++) {
        char *user_content = talloc_asprintf(ctx, "User message %d", i);
        ik_message_t *msg_user = ik_message_create_text(ctx, IK_ROLE_USER, user_content);
        ck_assert_ptr_nonnull(msg_user);
        res_t res = ik_agent_add_message(repl->current, msg_user);
        ck_assert(is_ok(&res));

        char *asst_content = talloc_asprintf(ctx, "Assistant response %d", i);
        ik_message_t *msg_asst = ik_message_create_text(ctx, IK_ROLE_ASSISTANT, asst_content);
        ck_assert_ptr_nonnull(msg_asst);
        res = ik_agent_add_message(repl->current, msg_asst);
        ck_assert(is_ok(&res));

        talloc_free(user_content);
        talloc_free(asst_content);
    }
    ck_assert_uint_eq(repl->current->message_count, 6);

    // Create a mark at this point
    res_t mark_res = ik_mark_create(repl, "test");
    ck_assert(is_ok(&mark_res));

    // Add one more message
    ik_message_t *msg = ik_message_create_text(ctx, IK_ROLE_USER, "Extra message");
    ck_assert_ptr_nonnull(msg);
    res_t res = ik_agent_add_message(repl->current, msg);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->current->message_count, 7);

    // Rewind
    ik_mark_t *target_mark = NULL;
    res_t find_res = ik_mark_find(repl, "test", &target_mark);
    ck_assert(is_ok(&find_res));

    res_t rewind_res = ik_mark_rewind_to_mark(repl, target_mark);
    ck_assert(is_ok(&rewind_res));

    // Should be back to 6 messages
    ck_assert_uint_eq(repl->current->message_count, 6);
}

END_TEST
// Test: Rewind with tool result message (covers marks.c line 188)
START_TEST(test_rewind_with_tool_result_message) {
    // Create a user message
    ik_message_t *msg_user = ik_message_create_text(ctx, IK_ROLE_USER, "Use a tool");
    ck_assert_ptr_nonnull(msg_user);
    res_t res = ik_agent_add_message(repl->current, msg_user);
    ck_assert(is_ok(&res));

    // Create a tool result message (IK_ROLE_TOOL)
    ik_message_t *msg_tool = ik_message_create_tool_result(ctx, "call_123", "Tool output", false);
    ck_assert_ptr_nonnull(msg_tool);
    ck_assert(msg_tool->role == IK_ROLE_TOOL);
    res = ik_agent_add_message(repl->current, msg_tool);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->current->message_count, 2);

    // Create a mark
    res_t mark_res = ik_mark_create(repl, "with_tool");
    ck_assert(is_ok(&mark_res));

    // Add another message
    ik_message_t *msg_user2 = ik_message_create_text(ctx, IK_ROLE_USER, "More messages");
    ck_assert_ptr_nonnull(msg_user2);
    res = ik_agent_add_message(repl->current, msg_user2);
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->current->message_count, 3);

    // Rewind to mark
    ik_mark_t *target_mark = NULL;
    res_t find_res = ik_mark_find(repl, "with_tool", &target_mark);
    ck_assert(is_ok(&find_res));

    res_t rewind_res = ik_mark_rewind_to_mark(repl, target_mark);
    ck_assert(is_ok(&rewind_res));

    // Should be back to 2 messages, including the tool result
    ck_assert_uint_eq(repl->current->message_count, 2);
    ck_assert(repl->current->messages[0]->role == IK_ROLE_USER);
    ck_assert(repl->current->messages[1]->role == IK_ROLE_TOOL);
}

END_TEST

static Suite *mark_system_role_suite(void)
{
    Suite *s = suite_create("Mark System Role");
    TCase *tc = tcase_create("messages");
    tcase_set_timeout(tc, 30);

    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_rewind_preserves_message_order);
    tcase_add_test(tc, test_rewind_with_multiple_message_pairs);
    tcase_add_test(tc, test_rewind_with_tool_result_message);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = mark_system_role_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
