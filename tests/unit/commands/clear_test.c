/**
 * @file clear_test.c
 * @brief Unit tests for /clear command
 */

#include "../../../src/commands.h"
#include "../../../src/config.h"
#include "../../../src/error.h"
#include "../../../src/repl.h"
#include "../../../src/scrollback.h"
#include "../../../src/openai/client.h"
#include "../../../src/marks.h"
#include "../../test_utils.h"

#include <check.h>
#include <talloc.h>

// Forward declaration for suite function
static Suite *commands_clear_suite(void);

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
    res_t res = ik_openai_conversation_create(parent);
    ck_assert(is_ok(&res));
    ik_openai_conversation_t *conv = res.ok;
    ck_assert_ptr_nonnull(conv);

    // Create minimal REPL context
    ik_repl_ctx_t *r = talloc_zero(parent, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(r);
    r->scrollback = scrollback;
    r->conversation = conv;

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
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->scrollback), 0);
    ck_assert_uint_eq(repl->conversation->message_count, 0);

    // Execute /clear
    res_t res = ik_cmd_dispatch(ctx, repl, "/clear");
    ck_assert(is_ok(&res));

    // Verify still empty
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->scrollback), 0);
    ck_assert_uint_eq(repl->conversation->message_count, 0);
}
END_TEST
// Test: Clear scrollback with content
START_TEST(test_clear_scrollback_with_content)
{
    // Add some lines to scrollback
    res_t res = ik_scrollback_append_line(repl->scrollback, "Line 1", 6);
    ck_assert(is_ok(&res));
    res = ik_scrollback_append_line(repl->scrollback, "Line 2", 6);
    ck_assert(is_ok(&res));
    res = ik_scrollback_append_line(repl->scrollback, "Line 3", 6);
    ck_assert(is_ok(&res));

    // Verify content exists
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->scrollback), 3);

    // Execute /clear
    res = ik_cmd_dispatch(ctx, repl, "/clear");
    ck_assert(is_ok(&res));

    // Verify scrollback is empty
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->scrollback), 0);
}

END_TEST
// Test: Clear conversation with messages
START_TEST(test_clear_conversation_with_messages)
{
    // Add messages to conversation
    res_t res = ik_openai_msg_create(ctx, "user", "Hello");
    ck_assert(is_ok(&res));
    ik_openai_msg_t *msg1 = res.ok;

    res = ik_openai_conversation_add_msg(repl->conversation, msg1);
    ck_assert(is_ok(&res));

    res = ik_openai_msg_create(ctx, "assistant", "Hi there!");
    ck_assert(is_ok(&res));
    ik_openai_msg_t *msg2 = res.ok;

    res = ik_openai_conversation_add_msg(repl->conversation, msg2);
    ck_assert(is_ok(&res));

    // Verify messages exist
    ck_assert_uint_eq(repl->conversation->message_count, 2);

    // Execute /clear
    res = ik_cmd_dispatch(ctx, repl, "/clear");
    ck_assert(is_ok(&res));

    // Verify conversation is empty
    ck_assert_uint_eq(repl->conversation->message_count, 0);
    ck_assert_ptr_null(repl->conversation->messages);
}

END_TEST
// Test: Clear both scrollback and conversation
START_TEST(test_clear_both_scrollback_and_conversation)
{
    // Add scrollback content
    res_t res = ik_scrollback_append_line(repl->scrollback, "User message", 12);
    ck_assert(is_ok(&res));
    res = ik_scrollback_append_line(repl->scrollback, "Assistant response", 18);
    ck_assert(is_ok(&res));

    // Add conversation messages
    res = ik_openai_msg_create(ctx, "user", "User message");
    ck_assert(is_ok(&res));
    ik_openai_msg_t *msg1 = res.ok;
    res = ik_openai_conversation_add_msg(repl->conversation, msg1);
    ck_assert(is_ok(&res));

    res = ik_openai_msg_create(ctx, "assistant", "Assistant response");
    ck_assert(is_ok(&res));
    ik_openai_msg_t *msg2 = res.ok;
    res = ik_openai_conversation_add_msg(repl->conversation, msg2);
    ck_assert(is_ok(&res));

    // Verify both have content
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->scrollback), 2);
    ck_assert_uint_eq(repl->conversation->message_count, 2);

    // Execute /clear
    res = ik_cmd_dispatch(ctx, repl, "/clear");
    ck_assert(is_ok(&res));

    // Verify both are empty
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->scrollback), 0);
    ck_assert_uint_eq(repl->conversation->message_count, 0);
}

END_TEST
// Test: Clear with NULL conversation (defensive check)
START_TEST(test_clear_with_null_conversation)
{
    // Set conversation to NULL
    repl->conversation = NULL;

    // Add scrollback content
    res_t res = ik_scrollback_append_line(repl->scrollback, "Line 1", 6);
    ck_assert(is_ok(&res));

    // Verify scrollback has content
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->scrollback), 1);

    // Execute /clear (should not crash)
    res = ik_cmd_dispatch(ctx, repl, "/clear");
    ck_assert(is_ok(&res));

    // Verify scrollback is cleared
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->scrollback), 0);
}

END_TEST
// Test: Clear command with arguments (should be ignored)
START_TEST(test_clear_with_ignored_arguments)
{
    // Add content
    res_t res = ik_scrollback_append_line(repl->scrollback, "Line 1", 6);
    ck_assert(is_ok(&res));

    // Execute /clear with extra arguments (should be ignored)
    res = ik_cmd_dispatch(ctx, repl, "/clear extra args");
    ck_assert(is_ok(&res));

    // Verify still cleared
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->scrollback), 0);
}

END_TEST
// Test: Clear with marks
START_TEST(test_clear_with_marks)
{
    // Add some content and marks
    res_t res = ik_scrollback_append_line(repl->scrollback, "Line 1", 6);
    ck_assert(is_ok(&res));

    res = ik_openai_msg_create(ctx, "user", "Message");
    ck_assert(is_ok(&res));
    ik_openai_msg_t *msg = res.ok;
    res = ik_openai_conversation_add_msg(repl->conversation, msg);
    ck_assert(is_ok(&res));

    // Create marks
    res = ik_mark_create(repl, "mark1");
    ck_assert(is_ok(&res));
    res = ik_mark_create(repl, "mark2");
    ck_assert(is_ok(&res));

    // Verify marks exist
    ck_assert_uint_eq(repl->mark_count, 2);
    ck_assert_ptr_nonnull(repl->marks);

    // Execute /clear
    res = ik_cmd_dispatch(ctx, repl, "/clear");
    ck_assert(is_ok(&res));

    // Verify marks are cleared
    ck_assert_uint_eq(repl->mark_count, 0);
    ck_assert_ptr_null(repl->marks);

    // Verify scrollback and conversation also cleared
    ck_assert_uint_eq(ik_scrollback_get_line_count(repl->scrollback), 0);
    ck_assert_uint_eq(repl->conversation->message_count, 0);
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
