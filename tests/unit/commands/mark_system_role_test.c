/**
 * @file mark_system_role_test.c
 * @brief Tests for mark rewind with system role messages (covers marks.c line 172)
 */

#include <check.h>
#include <talloc.h>

#include "../../../src/marks.h"
#include "../../../src/openai/client.h"
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

    res_t res = ik_openai_conversation_create(parent);
    ck_assert(is_ok(&res));
    ik_openai_conversation_t *conv = res.ok;
    ck_assert_ptr_nonnull(conv);

    ik_repl_ctx_t *r = talloc_zero(parent, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(r);
    r->scrollback = scrollback;
    r->conversation = conv;
    r->marks = NULL;
    r->mark_count = 0;

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

// Test: Rewind with system role message (covers marks.c line 172 - else branch)
START_TEST(test_rewind_with_system_role) {
    // Create a system message
    res_t msg_res = ik_openai_msg_create(repl->conversation, "system", "You are a helpful assistant");
    ck_assert(is_ok(&msg_res));
    ik_openai_msg_t *sys_msg = msg_res.ok;

    // Add it to conversation
    msg_res = ik_openai_conversation_add_msg(repl->conversation, sys_msg);
    ck_assert(is_ok(&msg_res));
    ck_assert_uint_eq(repl->conversation->message_count, 1);

    // Create a user message
    msg_res = ik_openai_msg_create(repl->conversation, "user", "Hello");
    ck_assert(is_ok(&msg_res));
    msg_res = ik_openai_conversation_add_msg(repl->conversation, msg_res.ok);
    ck_assert(is_ok(&msg_res));
    ck_assert_uint_eq(repl->conversation->message_count, 2);

    // Create a mark after the user message
    res_t mark_res = ik_mark_create(repl, "checkpoint");
    ck_assert(is_ok(&mark_res));
    ck_assert_uint_eq(repl->mark_count, 1);

    // Add another message
    msg_res = ik_openai_msg_create(repl->conversation, "assistant", "Hi there!");
    ck_assert(is_ok(&msg_res));
    msg_res = ik_openai_conversation_add_msg(repl->conversation, msg_res.ok);
    ck_assert(is_ok(&msg_res));
    ck_assert_uint_eq(repl->conversation->message_count, 3);

    // Find and rewind to the mark - this should rebuild scrollback with system message
    ik_mark_t *target_mark = NULL;
    res_t find_res = ik_mark_find(repl, "checkpoint", &target_mark);
    ck_assert(is_ok(&find_res));

    // Rewind - this will trigger the else branch for system role (line 172)
    res_t rewind_res = ik_mark_rewind_to_mark(repl, target_mark);
    ck_assert(is_ok(&rewind_res));

    // Verify conversation was rewound
    ck_assert_uint_eq(repl->conversation->message_count, 2);
    ck_assert_str_eq(repl->conversation->messages[0]->role, "system");
    ck_assert_str_eq(repl->conversation->messages[1]->role, "user");
}
END_TEST
// Test: Rewind with multiple system messages
START_TEST(test_rewind_with_multiple_system_messages)
{
    // Add several system messages
    for (int i = 0; i < 3; i++) {
        char *content = talloc_asprintf(ctx, "System message %d", i);
        res_t msg_res = ik_openai_msg_create(repl->conversation, "system", content);
        ck_assert(is_ok(&msg_res));
        msg_res = ik_openai_conversation_add_msg(repl->conversation, msg_res.ok);
        ck_assert(is_ok(&msg_res));
        talloc_free(content);
    }

    // Create a mark
    res_t mark_res = ik_mark_create(repl, "test");
    ck_assert(is_ok(&mark_res));

    // Add more messages
    res_t msg_res = ik_openai_msg_create(repl->conversation, "user", "Test");
    ck_assert(is_ok(&msg_res));
    msg_res = ik_openai_conversation_add_msg(repl->conversation, msg_res.ok);
    ck_assert(is_ok(&msg_res));

    // Rewind
    ik_mark_t *target_mark = NULL;
    res_t find_res = ik_mark_find(repl, "test", &target_mark);
    ck_assert(is_ok(&find_res));

    res_t rewind_res = ik_mark_rewind_to_mark(repl, target_mark);
    ck_assert(is_ok(&rewind_res));

    // All 3 system messages should still be in conversation
    ck_assert_uint_eq(repl->conversation->message_count, 3);
}

END_TEST

static Suite *mark_system_role_suite(void)
{
    Suite *s = suite_create("Mark System Role");
    TCase *tc = tcase_create("system_messages");

    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_rewind_with_system_role);
    tcase_add_test(tc, test_rewind_with_multiple_system_messages);

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
