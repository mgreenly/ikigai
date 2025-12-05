/**
 * @file repl_tool_call_state_mutation_test.c
 * @brief Unit tests for REPL tool call conversation state mutation
 *
 * Tests the mutation of conversation state when tool calls are received:
 * 1. Adding assistant message with tool_calls to conversation as canonical message
 * 2. Executing tool dispatcher to get tool result
 * 3. Adding tool result message to conversation as canonical message
 * 4. Verifying correct message ordering (user -> tool_call -> tool_result)
 */

#include "repl.h"
#include "openai/client.h"
#include "tool.h"
#include "scrollback.h"
#include <check.h>
#include <talloc.h>
#include <string.h>

static void *ctx;
static ik_repl_ctx_t *repl;

static void setup(void)
{
    ctx = talloc_new(NULL);

    /* Create minimal REPL context for testing */
    repl = talloc_zero(ctx, ik_repl_ctx_t);

    /* Create conversation */
    res_t res = ik_openai_conversation_create(ctx);
    ck_assert(is_ok(&res));
    repl->conversation = res.ok;
    ck_assert_ptr_nonnull(repl->conversation);

    repl->scrollback = ik_scrollback_create(repl, 80);
}

static void teardown(void)
{
    talloc_free(ctx);
}

/*
 * Test: Add user message, then tool_call message to conversation
 */
START_TEST(test_add_tool_call_message_to_conversation) {
    /* Add a user message first */
    ik_msg_t *user_msg = ik_openai_msg_create(repl->conversation, "user", "Find all C files").ok;
    res_t res = ik_openai_conversation_add_msg(repl->conversation, user_msg);
    ck_assert(is_ok(&res));

    /* Now simulate receiving a tool_call from the API */
    /* Create a canonical tool_call message with:
     *   role: "tool_call"
     *   content: human-readable summary
     *   data_json: structured tool call data
     */
    ik_msg_t *tool_call_msg = ik_openai_msg_create_tool_call(
        repl->conversation,
        "call_abc123",                      /* id */
        "function",                         /* type */
        "glob",                             /* name */
        "{\"pattern\":\"*.c\"}",            /* arguments */
        "glob(pattern=\"*.c\")"             /* content (human-readable) */
        );

    /* Add tool_call message to conversation */
    res = ik_openai_conversation_add_msg(repl->conversation, tool_call_msg);
    ck_assert(is_ok(&res));

    /* Verify conversation has 2 messages */
    ck_assert_uint_eq(repl->conversation->message_count, 2);

    /* Verify first message is user */
    ck_assert_str_eq(repl->conversation->messages[0]->kind, "user");
    ck_assert_str_eq(repl->conversation->messages[0]->content, "Find all C files");

    /* Verify second message is tool_call */
    ck_assert_str_eq(repl->conversation->messages[1]->kind, "tool_call");
    ck_assert_str_eq(repl->conversation->messages[1]->content, "glob(pattern=\"*.c\")");
    ck_assert_ptr_nonnull(repl->conversation->messages[1]->data_json);
}
END_TEST
/*
 * Test: Execute glob tool and add tool_result message
 */
START_TEST(test_execute_tool_and_add_result_message)
{
    /* Start with a user message and tool_call message */
    ik_msg_t *user_msg = ik_openai_msg_create(repl->conversation, "user", "Find all C files").ok;
    ik_openai_conversation_add_msg(repl->conversation, user_msg);

    ik_msg_t *tool_call_msg = ik_openai_msg_create_tool_call(
        repl->conversation,
        "call_abc123",
        "function",
        "glob",
        "{\"pattern\":\"*.c\"}",
        "glob(pattern=\"*.c\")"
        );
    ik_openai_conversation_add_msg(repl->conversation, tool_call_msg);

    /* Execute the tool dispatcher */
    res_t tool_res = ik_tool_dispatch(ctx, "glob", "{\"pattern\":\"*.c\"}");
    ck_assert(is_ok(&tool_res));
    char *tool_output = tool_res.ok;
    ck_assert_ptr_nonnull(tool_output);

    /* Create a canonical tool_result message with:
     *   role: "tool_result"
     *   content: human-readable summary
     *   data_json: structured result data
     */
    char *data_json = talloc_asprintf(ctx,
                                      "{\"tool_call_id\":\"call_abc123\",\"name\":\"glob\",\"output\":%s,\"success\":true}",
                                      tool_output
                                      );

    ik_msg_t *tool_result_msg = ik_openai_msg_create(
        repl->conversation,
        "tool_result",
        "Files found: src/main.c, src/config.c"  /* human-readable summary */
        ).ok;
    tool_result_msg->data_json = talloc_steal(tool_result_msg, data_json);

    /* Add tool_result message to conversation */
    res_t res = ik_openai_conversation_add_msg(repl->conversation, tool_result_msg);
    ck_assert(is_ok(&res));

    /* Verify conversation has 3 messages in correct order */
    ck_assert_uint_eq(repl->conversation->message_count, 3);

    /* Verify message ordering: user -> tool_call -> tool_result */
    ck_assert_str_eq(repl->conversation->messages[0]->kind, "user");
    ck_assert_str_eq(repl->conversation->messages[1]->kind, "tool_call");
    ck_assert_str_eq(repl->conversation->messages[2]->kind, "tool_result");

    /* Verify tool_result message has correct data */
    ck_assert_str_eq(repl->conversation->messages[2]->content, "Files found: src/main.c, src/config.c");
    ck_assert_ptr_nonnull(repl->conversation->messages[2]->data_json);
}

END_TEST
/*
 * Test: Verify message ordering is preserved
 */
START_TEST(test_message_ordering_preserved)
{
    /* Build complete conversation: user -> tool_call -> tool_result */

    /* 1. User message */
    ik_msg_t *user_msg = ik_openai_msg_create(repl->conversation, "user", "List files").ok;
    ik_openai_conversation_add_msg(repl->conversation, user_msg);

    /* 2. Tool call message */
    ik_msg_t *tool_call_msg = ik_openai_msg_create_tool_call(
        repl->conversation,
        "call_123",
        "function",
        "glob",
        "{\"pattern\":\"*\"}",
        "glob(pattern=\"*\")"
        );
    ik_openai_conversation_add_msg(repl->conversation, tool_call_msg);

    /* 3. Tool result message */
    ik_msg_t *tool_result_msg = ik_openai_msg_create(repl->conversation, "tool_result", "Found 3 files").ok;
    tool_result_msg->data_json = talloc_strdup(tool_result_msg,
                                               "{\"tool_call_id\":\"call_123\",\"name\":\"glob\",\"output\":\"{}\",\"success\":true}"
                                               );
    ik_openai_conversation_add_msg(repl->conversation, tool_result_msg);

    /* Verify ordering is preserved */
    ck_assert_uint_eq(repl->conversation->message_count, 3);
    ck_assert_str_eq(repl->conversation->messages[0]->kind, "user");
    ck_assert_str_eq(repl->conversation->messages[1]->kind, "tool_call");
    ck_assert_str_eq(repl->conversation->messages[2]->kind, "tool_result");
}

END_TEST

/*
 * Test suite
 */
static Suite *repl_tool_call_state_mutation_suite(void)
{
    Suite *s = suite_create("REPL Tool Call State Mutation");

    TCase *tc_core = tcase_create("Core");
    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_add_tool_call_message_to_conversation);
    tcase_add_test(tc_core, test_execute_tool_and_add_result_message);
    tcase_add_test(tc_core, test_message_ordering_preserved);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = repl_tool_call_state_mutation_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
