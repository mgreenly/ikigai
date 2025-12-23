/**
 * @file mark_rewind_twice_test.c
 * @brief Test that marks can be reused after rewind (Bug 7)
 */

#include "../../../src/agent.h"
#include "../../../src/commands_mark.h"
#include "../../../src/config.h"
#include "../../../src/shared.h"
#include "../../../src/db/connection.h"
#include "../../../src/db/session.h"
#include "../../../src/marks.h"
#include "../../../src/openai/client.h"
#include "../../../src/repl.h"
#include "../../../src/scrollback.h"

#include <check.h>
#include <talloc.h>

START_TEST(test_rewind_to_same_mark_twice) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create REPL context
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);

    // Create agent context
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);
    repl->current = agent;

    // Create minimal config
    ik_config_t *cfg = talloc_zero(ctx, ik_config_t);
    ck_assert_ptr_nonnull(cfg);

    // Create shared context
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    shared->cfg = cfg;
    repl->shared = shared;

    // Initialize conversation
    repl->current->conversation = talloc_zero(repl, ik_openai_conversation_t);
    ck_assert_ptr_nonnull(repl->current->conversation);
    repl->current->conversation->messages = NULL;
    repl->current->conversation->message_count = 0;

    // Initialize marks array
    repl->current->marks = NULL;
    repl->current->mark_count = 0;

    // Initialize scrollback
    agent->scrollback = ik_scrollback_create(repl, 80);

    // Step 1: Add initial message
    ik_msg_t *msg1 = talloc_zero(repl->current->conversation, ik_msg_t);
    msg1->kind = talloc_strdup(msg1, "user");
    msg1->content = talloc_strdup(msg1, "Message 1");
    repl->current->conversation->messages = talloc_array(repl->current->conversation, ik_msg_t *, 1);
    repl->current->conversation->messages[0] = msg1;
    repl->current->conversation->message_count = 1;

    // Step 2: Create mark
    res_t mark_res = ik_mark_create(repl, "test-mark");
    ck_assert(is_ok(&mark_res));
    ck_assert_uint_eq(repl->current->mark_count, 1);

    // Step 3: Add more messages
    ik_msg_t *msg2 = talloc_zero(repl->current->conversation, ik_msg_t);
    msg2->kind = talloc_strdup(msg2, "assistant");
    msg2->content = talloc_strdup(msg2, "Response 1");
    repl->current->conversation->messages = talloc_realloc(repl->current->conversation,
                                                           repl->current->conversation->messages,
                                                           ik_msg_t *,
                                                           2);
    repl->current->conversation->messages[1] = msg2;
    repl->current->conversation->message_count = 2;

    // Step 4: Rewind to mark (first time)
    res_t rewind1_res = ik_mark_rewind_to(repl, "test-mark");
    ck_assert(is_ok(&rewind1_res));
    ck_assert_uint_eq(repl->current->conversation->message_count, 1);

    // Bug 7: Mark should still be in stack after rewind
    ck_assert_uint_eq(repl->current->mark_count, 1);
    ck_assert_ptr_nonnull(repl->current->marks);
    ck_assert_ptr_nonnull(repl->current->marks[0]);
    ck_assert_str_eq(repl->current->marks[0]->label, "test-mark");

    // Step 5: Add different messages
    ik_msg_t *msg3 = talloc_zero(repl->current->conversation, ik_msg_t);
    msg3->kind = talloc_strdup(msg3, "assistant");
    msg3->content = talloc_strdup(msg3, "Response 2");
    repl->current->conversation->messages = talloc_realloc(repl->current->conversation,
                                                           repl->current->conversation->messages,
                                                           ik_msg_t *,
                                                           2);
    repl->current->conversation->messages[1] = msg3;
    repl->current->conversation->message_count = 2;

    // Step 6: Rewind to same mark again (second time)
    res_t rewind2_res = ik_mark_rewind_to(repl, "test-mark");
    ck_assert(is_ok(&rewind2_res));
    ck_assert_uint_eq(repl->current->conversation->message_count, 1);

    // Mark should STILL be in stack
    ck_assert_uint_eq(repl->current->mark_count, 1);

    talloc_free(ctx);
}
END_TEST START_TEST(test_rewind_to_unlabeled_mark_twice)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Create REPL context
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);

    // Create agent context
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);
    repl->current = agent;

    // Create minimal config
    ik_config_t *cfg = talloc_zero(ctx, ik_config_t);
    ck_assert_ptr_nonnull(cfg);

    // Create shared context
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    shared->cfg = cfg;
    repl->shared = shared;

    // Initialize conversation
    repl->current->conversation = talloc_zero(repl, ik_openai_conversation_t);
    ck_assert_ptr_nonnull(repl->current->conversation);
    repl->current->conversation->messages = NULL;
    repl->current->conversation->message_count = 0;

    // Initialize marks array
    repl->current->marks = NULL;
    repl->current->mark_count = 0;

    // Initialize scrollback
    agent->scrollback = ik_scrollback_create(repl, 80);

    // Add initial message
    ik_msg_t *msg1 = talloc_zero(repl->current->conversation, ik_msg_t);
    msg1->kind = talloc_strdup(msg1, "user");
    msg1->content = talloc_strdup(msg1, "Message 1");
    repl->current->conversation->messages = talloc_array(repl->current->conversation, ik_msg_t *, 1);
    repl->current->conversation->messages[0] = msg1;
    repl->current->conversation->message_count = 1;

    // Create unlabeled mark
    res_t mark_res = ik_mark_create(repl, NULL);
    ck_assert(is_ok(&mark_res));
    ck_assert_uint_eq(repl->current->mark_count, 1);

    // Add message
    ik_msg_t *msg2 = talloc_zero(repl->current->conversation, ik_msg_t);
    msg2->kind = talloc_strdup(msg2, "assistant");
    msg2->content = talloc_strdup(msg2, "Response 1");
    repl->current->conversation->messages = talloc_realloc(repl->current->conversation,
                                                           repl->current->conversation->messages,
                                                           ik_msg_t *,
                                                           2);
    repl->current->conversation->messages[1] = msg2;
    repl->current->conversation->message_count = 2;

    // Rewind to unlabeled mark (first time)
    res_t rewind1_res = ik_mark_rewind_to(repl, NULL);
    ck_assert(is_ok(&rewind1_res));
    ck_assert_uint_eq(repl->current->mark_count, 1);

    // Add another message
    ik_msg_t *msg3 = talloc_zero(repl->current->conversation, ik_msg_t);
    msg3->kind = talloc_strdup(msg3, "assistant");
    msg3->content = talloc_strdup(msg3, "Response 2");
    repl->current->conversation->messages = talloc_realloc(repl->current->conversation,
                                                           repl->current->conversation->messages,
                                                           ik_msg_t *,
                                                           2);
    repl->current->conversation->messages[1] = msg3;
    repl->current->conversation->message_count = 2;

    // Rewind to unlabeled mark again (second time)
    res_t rewind2_res = ik_mark_rewind_to(repl, NULL);
    ck_assert(is_ok(&rewind2_res));
    ck_assert_uint_eq(repl->current->mark_count, 1);

    talloc_free(ctx);
}

END_TEST

static Suite *mark_rewind_twice_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Mark Rewind Twice");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_rewind_to_same_mark_twice);
    tcase_add_test(tc_core, test_rewind_to_unlabeled_mark_twice);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = mark_rewind_twice_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
