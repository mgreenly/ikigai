#include <check.h>
#include <talloc.h>
#include <string.h>
#include "repl.h"
#include "openai/client.h"
#include "tool.h"
#include "scrollback.h"
#include "config.h"

/* Test fixtures */
static TALLOC_CTX *ctx = NULL;
static ik_repl_ctx_t *repl = NULL;

static void setup(void)
{
    ctx = talloc_new(NULL);

    /* Create minimal repl context for testing */
    repl = talloc_zero(ctx, ik_repl_ctx_t);

    /* Create conversation */
    res_t conv_res = ik_openai_conversation_create(repl);
    ck_assert(!conv_res.is_err);
    repl->conversation = conv_res.ok;

    /* Create scrollback */
    repl->scrollback = ik_scrollback_create(repl, 10);
    ck_assert_ptr_nonnull(repl->scrollback);

    /* Create pending_tool_call with a simple glob call */
    repl->pending_tool_call = ik_tool_call_create(repl,
                                                   "call_test123",
                                                   "glob",
                                                   "{\"pattern\": \"*.c\"}");
    ck_assert_ptr_nonnull(repl->pending_tool_call);
}

static void teardown(void)
{
    talloc_free(ctx);
    ctx = NULL;
    repl = NULL;
}

/*
 * Tests for ik_repl_execute_pending_tool
 */

START_TEST(test_execute_pending_tool_basic) {
    /* Execute pending tool call */
    ik_repl_execute_pending_tool(repl);

    /* Verify pending_tool_call is cleared */
    ck_assert_ptr_null(repl->pending_tool_call);

    /* Verify messages were added to conversation */
    /* Should have 2 messages: tool_call and tool_result */
    ck_assert_uint_eq(repl->conversation->message_count, 2);

    /* First message should be tool_call */
    ik_openai_msg_t *tc_msg = repl->conversation->messages[0];
    ck_assert_str_eq(tc_msg->role, "tool_call");

    /* Second message should be tool_result */
    ik_openai_msg_t *result_msg = repl->conversation->messages[1];
    ck_assert_str_eq(result_msg->role, "tool_result");
}
END_TEST START_TEST(test_execute_pending_tool_clears_pending)
{
    /* Execute pending tool call */
    ik_repl_execute_pending_tool(repl);

    /* Verify pending_tool_call is NULL after execution */
    ck_assert_ptr_null(repl->pending_tool_call);
}

END_TEST START_TEST(test_execute_pending_tool_conversation_messages)
{
    /* Execute pending tool call */
    ik_repl_execute_pending_tool(repl);

    /* First message should be tool_call with correct ID */
    ik_openai_msg_t *tc_msg = repl->conversation->messages[0];
    ck_assert_str_eq(tc_msg->role, "tool_call");
    ck_assert_ptr_nonnull(tc_msg->data_json);

    /* Second message should be tool_result with correct ID */
    ik_openai_msg_t *result_msg = repl->conversation->messages[1];
    ck_assert_str_eq(result_msg->role, "tool_result");
    ck_assert_ptr_nonnull(result_msg->data_json);
}

END_TEST START_TEST(test_execute_pending_tool_file_read)
{
    /* Change to file_read tool */
    talloc_free(repl->pending_tool_call);
    repl->pending_tool_call = ik_tool_call_create(repl,
                                                   "call_read123",
                                                   "file_read",
                                                   "{\"file_path\": \"/etc/hostname\"}");

    /* Execute pending tool call */
    ik_repl_execute_pending_tool(repl);

    /* Verify pending_tool_call is cleared */
    ck_assert_ptr_null(repl->pending_tool_call);

    /* Verify messages were added */
    ck_assert_uint_eq(repl->conversation->message_count, 2);
}

END_TEST

/*
 * Test suite
 */

static Suite *repl_tool_suite(void)
{
    Suite *s = suite_create("REPL Tool Execution");
    TCase *tc_core = tcase_create("Core");

    tcase_add_checked_fixture(tc_core, setup, teardown);

    tcase_add_test(tc_core, test_execute_pending_tool_basic);
    tcase_add_test(tc_core, test_execute_pending_tool_clears_pending);
    tcase_add_test(tc_core, test_execute_pending_tool_conversation_messages);
    tcase_add_test(tc_core, test_execute_pending_tool_file_read);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = repl_tool_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
