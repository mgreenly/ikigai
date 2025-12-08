/**
 * @file mark_test.c
 * @brief Unit tests for /mark and /rewind commands
 */

#include "../../../src/commands.h"
#include "../../../src/config.h"
#include "../../../src/shared.h"
#include "../../../src/error.h"
#include "../../../src/repl.h"
#include "../../../src/scrollback.h"
#include "../../../src/openai/client.h"
#include "../../../src/marks.h"
#include "../../test_utils.h"

#include <check.h>
#include <talloc.h>

// Forward declaration for suite function
static Suite *commands_mark_suite(void);

// Test fixture
static void *ctx;
static ik_repl_ctx_t *repl;

/**
 * Create a REPL context with scrollback and conversation for mark testing.
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


    // Create minimal config
    ik_cfg_t *cfg = talloc_zero(parent, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);

    // Create shared context
    ik_shared_ctx_t *shared = talloc_zero(parent, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    shared->cfg = cfg;

    // Create minimal REPL context
    ik_repl_ctx_t *r = talloc_zero(parent, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(r);
    r->scrollback = scrollback;
    r->conversation = conv;
    r->marks = NULL;
    r->mark_count = 0;
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

// Test: Create unlabeled mark
START_TEST(test_create_unlabeled_mark) {
    // Verify no marks initially
    ck_assert_uint_eq(repl->mark_count, 0);

    // Create an unlabeled mark
    res_t res = ik_mark_create(repl, NULL);
    ck_assert(is_ok(&res));

    // Verify mark was created
    ck_assert_uint_eq(repl->mark_count, 1);
    ck_assert_ptr_nonnull(repl->marks);
    ck_assert_ptr_nonnull(repl->marks[0]);
    ck_assert_ptr_null(repl->marks[0]->label);
    ck_assert_ptr_nonnull(repl->marks[0]->timestamp);
    ck_assert_uint_eq(repl->marks[0]->message_index, 0);
}
END_TEST
// Test: Create labeled mark
START_TEST(test_create_labeled_mark)
{
    // Create a labeled mark
    res_t res = ik_mark_create(repl, "checkpoint1");
    ck_assert(is_ok(&res));

    // Verify mark was created with label
    ck_assert_uint_eq(repl->mark_count, 1);
    ck_assert_ptr_nonnull(repl->marks[0]->label);
    ck_assert_str_eq(repl->marks[0]->label, "checkpoint1");
}

END_TEST
// Test: Create multiple marks
START_TEST(test_create_multiple_marks)
{
    // Add some messages to conversation
    res_t res = ik_openai_msg_create(repl->conversation, "user", "Hello");
    ck_assert(is_ok(&res));
    ik_msg_t *msg1 = res.ok;
    res = ik_openai_conversation_add_msg(repl->conversation, msg1);
    ck_assert(is_ok(&res));

    // Create first mark
    res = ik_mark_create(repl, "first");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->mark_count, 1);
    ck_assert_uint_eq(repl->marks[0]->message_index, 1);

    // Add another message
    res = ik_openai_msg_create(repl->conversation, "assistant", "Hi");
    ck_assert(is_ok(&res));
    ik_msg_t *msg2 = res.ok;
    res = ik_openai_conversation_add_msg(repl->conversation, msg2);
    ck_assert(is_ok(&res));

    // Create second mark
    res = ik_mark_create(repl, "second");
    ck_assert(is_ok(&res));
    ck_assert_uint_eq(repl->mark_count, 2);
    ck_assert_uint_eq(repl->marks[1]->message_index, 2);

    // Verify both marks exist
    ck_assert_str_eq(repl->marks[0]->label, "first");
    ck_assert_str_eq(repl->marks[1]->label, "second");
}

END_TEST
// Test: Find mark without label (most recent)
START_TEST(test_find_mark_most_recent)
{
    // Create two marks
    res_t res = ik_mark_create(repl, "first");
    ck_assert(is_ok(&res));
    res = ik_mark_create(repl, "second");
    ck_assert(is_ok(&res));

    // Find most recent mark (no label)
    ik_mark_t *found_mark;
    res = ik_mark_find(repl, NULL, &found_mark);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(found_mark);
    ck_assert_str_eq(found_mark->label, "second");
}

END_TEST
// Test: Find mark by label
START_TEST(test_find_mark_by_label)
{
    // Create two marks
    res_t res = ik_mark_create(repl, "first");
    ck_assert(is_ok(&res));
    res = ik_mark_create(repl, "second");
    ck_assert(is_ok(&res));

    // Find first mark by label
    ik_mark_t *found_mark;
    res = ik_mark_find(repl, "first", &found_mark);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(found_mark);
    ck_assert_str_eq(found_mark->label, "first");
}

END_TEST
// Test: Find mark - no marks error
START_TEST(test_find_mark_no_marks)
{
    // Try to find mark when none exist
    ik_mark_t *found_mark;
    res_t res = ik_mark_find(repl, NULL, &found_mark);
    ck_assert(is_err(&res));
}

END_TEST
// Test: Find mark - label not found
START_TEST(test_find_mark_label_not_found)
{
    // Create a mark with different label
    res_t res = ik_mark_create(repl, "exists");
    ck_assert(is_ok(&res));

    // Try to find non-existent label
    ik_mark_t *found_mark;
    res = ik_mark_find(repl, "notfound", &found_mark);
    ck_assert(is_err(&res));
}

END_TEST
// Test: Find mark by label with unlabeled marks in array
START_TEST(test_find_mark_with_unlabeled_marks)
{
    // Create mix of labeled and unlabeled marks
    res_t res = ik_mark_create(repl, NULL);  // unlabeled
    ck_assert(is_ok(&res));
    res = ik_mark_create(repl, "target");  // labeled
    ck_assert(is_ok(&res));
    res = ik_mark_create(repl, NULL);  // unlabeled
    ck_assert(is_ok(&res));

    // Find the labeled mark
    ik_mark_t *found_mark;
    res = ik_mark_find(repl, "target", &found_mark);
    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(found_mark);
    ck_assert_str_eq(found_mark->label, "target");
}

END_TEST
// Test: Rewind to mark
START_TEST(test_rewind_to_mark)
{
    // Build conversation with messages
    res_t res = ik_openai_msg_create(repl->conversation, "user", "Message 1");
    ck_assert(is_ok(&res));
    ik_msg_t *msg1 = res.ok;
    res = ik_openai_conversation_add_msg(repl->conversation, msg1);
    ck_assert(is_ok(&res));

    res = ik_openai_msg_create(repl->conversation, "assistant", "Response 1");
    ck_assert(is_ok(&res));
    ik_msg_t *msg2 = res.ok;
    res = ik_openai_conversation_add_msg(repl->conversation, msg2);
    ck_assert(is_ok(&res));

    // Create mark after 2 messages
    res = ik_mark_create(repl, "checkpoint");
    ck_assert(is_ok(&res));

    // Add more messages
    res = ik_openai_msg_create(repl->conversation, "user", "Message 2");
    ck_assert(is_ok(&res));
    ik_msg_t *msg3 = res.ok;
    res = ik_openai_conversation_add_msg(repl->conversation, msg3);
    ck_assert(is_ok(&res));

    res = ik_openai_msg_create(repl->conversation, "assistant", "Response 2");
    ck_assert(is_ok(&res));
    ik_msg_t *msg4 = res.ok;
    res = ik_openai_conversation_add_msg(repl->conversation, msg4);
    ck_assert(is_ok(&res));

    // Verify conversation has 4 messages
    ck_assert_uint_eq(repl->conversation->message_count, 4);

    // Rewind to checkpoint
    res = ik_mark_rewind_to(repl, "checkpoint");
    ck_assert(is_ok(&res));

    // Verify conversation was truncated to 2 messages
    ck_assert_uint_eq(repl->conversation->message_count, 2);

    // Verify mark was preserved (Bug 7 fix: marks are reusable)
    ck_assert_uint_eq(repl->mark_count, 1);
}

END_TEST
// Test: Rewind to most recent mark (no label)
START_TEST(test_rewind_to_most_recent)
{
    // Create conversation and marks
    res_t res = ik_openai_msg_create(repl->conversation, "user", "Message");
    ck_assert(is_ok(&res));
    ik_msg_t *msg = res.ok;
    res = ik_openai_conversation_add_msg(repl->conversation, msg);
    ck_assert(is_ok(&res));

    res = ik_mark_create(repl, "mark1");
    ck_assert(is_ok(&res));

    res = ik_openai_msg_create(repl->conversation, "assistant", "Response");
    ck_assert(is_ok(&res));
    ik_msg_t *msg2 = res.ok;
    res = ik_openai_conversation_add_msg(repl->conversation, msg2);
    ck_assert(is_ok(&res));

    // Rewind without label (to most recent)
    res = ik_mark_rewind_to(repl, NULL);
    ck_assert(is_ok(&res));

    // Verify conversation truncated
    ck_assert_uint_eq(repl->conversation->message_count, 1);
}

END_TEST
// Test: Rewind to middle mark (not first position)
START_TEST(test_rewind_to_middle_mark)
{
    // Create multiple marks
    res_t res = ik_openai_msg_create(repl->conversation, "user", "Message 1");
    ck_assert(is_ok(&res));
    res = ik_openai_conversation_add_msg(repl->conversation, res.ok);
    ck_assert(is_ok(&res));

    res = ik_mark_create(repl, "first");
    ck_assert(is_ok(&res));

    res = ik_openai_msg_create(repl->conversation, "assistant", "Response 1");
    ck_assert(is_ok(&res));
    res = ik_openai_conversation_add_msg(repl->conversation, res.ok);
    ck_assert(is_ok(&res));

    res = ik_mark_create(repl, "second");
    ck_assert(is_ok(&res));

    res = ik_openai_msg_create(repl->conversation, "user", "Message 2");
    ck_assert(is_ok(&res));
    res = ik_openai_conversation_add_msg(repl->conversation, res.ok);
    ck_assert(is_ok(&res));

    res = ik_mark_create(repl, "third");
    ck_assert(is_ok(&res));

    // Rewind to second mark (not first, not last)
    res = ik_mark_rewind_to(repl, "second");
    ck_assert(is_ok(&res));

    // Verify conversation truncated to position of second mark
    ck_assert_uint_eq(repl->conversation->message_count, 2);
    // Verify marks truncated (should have kept first and second, removed third)
    ck_assert_uint_eq(repl->mark_count, 2);
}

END_TEST
// Test: Rewind - no marks error
START_TEST(test_rewind_no_marks)
{
    // Try to rewind when no marks exist
    res_t res = ik_mark_rewind_to(repl, NULL);
    ck_assert(is_err(&res));
}

END_TEST
// Test: /mark command via dispatcher
START_TEST(test_mark_command_via_dispatcher)
{
    // Execute /mark command with label
    res_t res = ik_cmd_dispatch(ctx, repl, "/mark testlabel");
    ck_assert(is_ok(&res));

    // Verify mark was created
    ck_assert_uint_eq(repl->mark_count, 1);
    ck_assert_str_eq(repl->marks[0]->label, "testlabel");
}

END_TEST
// Test: /mark command without label
START_TEST(test_mark_command_without_label)
{
    // Execute /mark command without label
    res_t res = ik_cmd_dispatch(ctx, repl, "/mark");
    ck_assert(is_ok(&res));

    // Verify unlabeled mark was created
    ck_assert_uint_eq(repl->mark_count, 1);
    ck_assert_ptr_null(repl->marks[0]->label);
}

END_TEST
// Test: /rewind command via dispatcher
START_TEST(test_rewind_command_via_dispatcher)
{
    // Create conversation and mark
    res_t res = ik_openai_msg_create(repl->conversation, "user", "Test");
    ck_assert(is_ok(&res));
    ik_msg_t *msg = res.ok;
    res = ik_openai_conversation_add_msg(repl->conversation, msg);
    ck_assert(is_ok(&res));

    res = ik_cmd_dispatch(ctx, repl, "/mark point1");
    ck_assert(is_ok(&res));

    res = ik_openai_msg_create(repl->conversation, "assistant", "Response");
    ck_assert(is_ok(&res));
    ik_msg_t *msg2 = res.ok;
    res = ik_openai_conversation_add_msg(repl->conversation, msg2);
    ck_assert(is_ok(&res));

    // Rewind via command
    res = ik_cmd_dispatch(ctx, repl, "/rewind point1");
    ck_assert(is_ok(&res));

    // Verify rewound
    ck_assert_uint_eq(repl->conversation->message_count, 1);
}

END_TEST

// Suite definition
static Suite *commands_mark_suite(void)
{
    Suite *s = suite_create("Commands: Mark/Rewind");

    TCase *tc_create = tcase_create("Mark Creation");
    tcase_add_checked_fixture(tc_create, setup, teardown);
    tcase_add_test(tc_create, test_create_unlabeled_mark);
    tcase_add_test(tc_create, test_create_labeled_mark);
    tcase_add_test(tc_create, test_create_multiple_marks);
    suite_add_tcase(s, tc_create);

    TCase *tc_find = tcase_create("Mark Finding");
    tcase_add_checked_fixture(tc_find, setup, teardown);
    tcase_add_test(tc_find, test_find_mark_most_recent);
    tcase_add_test(tc_find, test_find_mark_by_label);
    tcase_add_test(tc_find, test_find_mark_no_marks);
    tcase_add_test(tc_find, test_find_mark_label_not_found);
    tcase_add_test(tc_find, test_find_mark_with_unlabeled_marks);
    suite_add_tcase(s, tc_find);

    TCase *tc_rewind = tcase_create("Mark Rewind");
    tcase_add_checked_fixture(tc_rewind, setup, teardown);
    tcase_add_test(tc_rewind, test_rewind_to_mark);
    tcase_add_test(tc_rewind, test_rewind_to_most_recent);
    tcase_add_test(tc_rewind, test_rewind_to_middle_mark);
    tcase_add_test(tc_rewind, test_rewind_no_marks);
    suite_add_tcase(s, tc_rewind);

    TCase *tc_commands = tcase_create("Command Dispatcher");
    tcase_add_checked_fixture(tc_commands, setup, teardown);
    tcase_add_test(tc_commands, test_mark_command_via_dispatcher);
    tcase_add_test(tc_commands, test_mark_command_without_label);
    tcase_add_test(tc_commands, test_rewind_command_via_dispatcher);
    suite_add_tcase(s, tc_commands);

    return s;
}

int main(void)
{
    int failed = 0;
    Suite *s = commands_mark_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (failed == 0) ? 0 : 1;
}
