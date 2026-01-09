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
#include "../../../src/agent.h"
#include "../../../src/message.h"
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

    /* Create agent context */
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    repl->current = agent;

    /* Initialize messages array (new API) */
    repl->current->messages = NULL;
    repl->current->message_count = 0;
    repl->current->message_capacity = 0;

    repl->current->scrollback = ik_scrollback_create(repl, 80);
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
    ik_message_t *user_msg = ik_message_create_text(ctx, IK_ROLE_USER, "Find all C files");
    res_t res = ik_agent_add_message(repl->current, user_msg);
    ck_assert(is_ok(&res));

    /* Now simulate receiving a tool_call from the API */
    /* Create a canonical tool_call message using new API */
    ik_message_t *tool_call_msg = ik_message_create_tool_call(
        ctx,
        "call_abc123",              /* id */
        "glob",                     /* name */
        "{\"pattern\":\"*.c\"}"     /* arguments */
        );

    /* Add tool_call message to conversation */
    res = ik_agent_add_message(repl->current, tool_call_msg);
    ck_assert(is_ok(&res));

    /* Verify conversation has 2 messages */
    ck_assert_uint_eq(repl->current->message_count, 2);

    /* Verify first message is user */
    ck_assert(repl->current->messages[0]->role == IK_ROLE_USER);

    /* Verify second message is assistant (tool calls come from assistant) */
    ck_assert(repl->current->messages[1]->role == IK_ROLE_ASSISTANT);
    ck_assert_uint_ge(repl->current->messages[1]->content_count, 1);
}
END_TEST
/*
 * Test: Execute glob tool and add tool_result message
 */
START_TEST(test_execute_tool_and_add_result_message) {
    /* Start with a user message and tool_call message */
    ik_message_t *user_msg = ik_message_create_text(ctx, IK_ROLE_USER, "Find all C files");
    ik_agent_add_message(repl->current, user_msg);

    ik_message_t *tool_call_msg = ik_message_create_tool_call(
        ctx,
        "call_abc123",
        "glob",
        "{\"pattern\":\"*.c\"}"
        );
    ik_agent_add_message(repl->current, tool_call_msg);

    /* Execute the tool dispatcher */
    res_t tool_res = ik_tool_dispatch(ctx, "glob", "{\"pattern\":\"*.c\"}");
    ck_assert(is_ok(&tool_res));
    char *tool_output = tool_res.ok;
    ck_assert_ptr_nonnull(tool_output);

    /* Create a canonical tool_result message using new API */
    ik_message_t *tool_result_msg = ik_message_create_tool_result(
        ctx,
        "call_abc123",          /* tool_call_id */
        tool_output,            /* content */
        false                   /* is_error */
        );

    /* Add tool_result message to conversation */
    res_t res = ik_agent_add_message(repl->current, tool_result_msg);
    ck_assert(is_ok(&res));

    /* Verify conversation has 3 messages in correct order */
    ck_assert_uint_eq(repl->current->message_count, 3);

    /* Verify message ordering: user -> assistant (tool_call) -> tool (result) */
    ck_assert(repl->current->messages[0]->role == IK_ROLE_USER);
    ck_assert(repl->current->messages[1]->role == IK_ROLE_ASSISTANT);
    ck_assert(repl->current->messages[2]->role == IK_ROLE_TOOL);
}

END_TEST
/*
 * Test: Verify message ordering is preserved
 */
START_TEST(test_message_ordering_preserved) {
    /* Build complete conversation: user -> tool_call -> tool_result */

    /* 1. User message */
    ik_message_t *user_msg = ik_message_create_text(ctx, IK_ROLE_USER, "List files");
    ik_agent_add_message(repl->current, user_msg);

    /* 2. Tool call message */
    ik_message_t *tool_call_msg = ik_message_create_tool_call(
        ctx,
        "call_123",
        "glob",
        "{\"pattern\":\"*\"}"
        );
    ik_agent_add_message(repl->current, tool_call_msg);

    /* 3. Tool result message */
    ik_message_t *tool_result_msg = ik_message_create_tool_result(
        ctx,
        "call_123",
        "{\"files\":[\"a.c\",\"b.c\",\"c.c\"]}",
        false
        );
    ik_agent_add_message(repl->current, tool_result_msg);

    /* Verify ordering is preserved */
    ck_assert_uint_eq(repl->current->message_count, 3);
    ck_assert(repl->current->messages[0]->role == IK_ROLE_USER);
    ck_assert(repl->current->messages[1]->role == IK_ROLE_ASSISTANT);
    ck_assert(repl->current->messages[2]->role == IK_ROLE_TOOL);
}

END_TEST

/*
 * Test suite
 */
static Suite *repl_tool_call_state_mutation_suite(void)
{
    Suite *s = suite_create("REPL Tool Call State Mutation");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);
    tcase_set_timeout(tc_core, 30);
    tcase_set_timeout(tc_core, 30);
    tcase_set_timeout(tc_core, 30);
    tcase_set_timeout(tc_core, 30);
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
