#include "agent.h"
/**
 * @file repl_tool_loop_integration_test.c
 * @brief Integration tests for complete tool loop
 *
 * Tests the end-to-end tool loop behavior:
 * 1. Response A arrives with finish_reason: "tool_calls"
 * 2. Tool execution is triggered and conversation state is updated
 * 3. Request B is sent automatically with updated conversation
 * 4. Response B arrives with finish_reason: "stop"
 * 5. Loop completes and transitions to IDLE state
 */

#include "repl.h"
#include "../../../src/agent.h"
#include "repl_event_handlers.h"
#include "repl_callbacks.h"
#include "openai/client.h"
#include "openai/client_multi.h"
#include "providers/provider.h"
#include "scrollback.h"
#include "config.h"
#include "../../../src/shared.h"
#include <check.h>
#include <talloc.h>
#include <string.h>
#include <stdlib.h>

static void *ctx;
static ik_repl_ctx_t *repl;

static void setup(void)
{
    ctx = talloc_new(NULL);
    setenv("OPENAI_API_KEY", "test-key", 1);

    /* Create minimal REPL context for testing */
    repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);

    /* Create agent context */
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(agent);

    /* Create conversation */
    agent->conversation = ik_openai_conversation_create(ctx);
    repl->current = agent;
    agent->repl = repl;  // Set backpointer

    /* Create multi-handle wrapped in mock provider */
    res_t res = ik_openai_multi_create(ctx);
    ck_assert(is_ok(&res));

    typedef struct { ik_openai_multi_t *multi; } mock_pctx_t;
    mock_pctx_t *mock_ctx = talloc_zero(repl->current, mock_pctx_t);
    mock_ctx->multi = res.ok;

    static const ik_provider_vtable_t mock_vt = {
        .fdset = NULL, .perform = NULL, .timeout = NULL, .info_read = NULL,
        .start_request = NULL, .start_stream = NULL, .cleanup = NULL, .cancel = NULL,
    };

    ik_provider_t *provider = talloc_zero(repl->current, ik_provider_t);
    provider->name = "mock";
    provider->vt = &mock_vt;
    provider->ctx = mock_ctx;
    repl->current->provider_instance = provider;

    /* Create config */
    ik_config_t *cfg = talloc_zero(ctx, ik_config_t);
    cfg->openai_model = talloc_strdup(cfg, "gpt-4");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 1000;
    cfg->max_tool_turns = 50;  // Default limit for tool loop iterations
    // Create shared context
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    shared->cfg = cfg;
    repl->shared = shared;

    repl->current->assistant_response = NULL;
    repl->current->streaming_line_buffer = NULL;
    repl->current->http_error_message = NULL;
    repl->current->response_model = NULL;
    repl->current->response_finish_reason = NULL;
    repl->current->response_completion_tokens = 0;
    repl->current->state = IK_AGENT_STATE_WAITING_FOR_LLM;
    repl->current->curl_still_running = 0;
    repl->current->tool_iteration_count = 0;  // Initialize tool loop counter

    /* Use the agent created above */
    agent->scrollback = ik_scrollback_create(repl, 80);
}

static void teardown(void)
{
    unsetenv("OPENAI_API_KEY");
    talloc_free(ctx);
}

/*
 * Test: When finish_reason is "tool_calls", handle_request_success should:
 * 1. Add assistant response to conversation (if any)
 * 2. NOT transition to IDLE state
 * 3. Submit a follow-up request
 */
START_TEST(test_handle_request_success_with_tool_calls_continues_loop) {
    /* Simulate Response A: finish_reason = "tool_calls" */
    repl->current->response_finish_reason = talloc_strdup(repl, "tool_calls");
    repl->current->response_model = talloc_strdup(repl, "gpt-4");
    repl->current->response_completion_tokens = 42;

    /* Simulate accumulated assistant response (could be empty for tool calls) */
    repl->current->assistant_response = talloc_strdup(repl, "");

    /* Add initial user message to conversation */
    ik_msg_t *user_msg = ik_openai_msg_create(repl->current->conversation, "user", "Find all C files");
    ik_openai_conversation_add_msg(repl->current->conversation, user_msg);

    /* Call handle_request_success */
    ik_repl_handle_agent_request_success(repl, repl->current);

    /* Verify:
     * 1. State should still be WAITING_FOR_LLM (not transitioned to IDLE)
     * 2. curl_still_running should be set to 1 (new request initiated)
     * 3. Assistant response should be cleared
     */
    ck_assert_int_eq(repl->current->state, IK_AGENT_STATE_WAITING_FOR_LLM);
    ck_assert_int_eq(repl->current->curl_still_running, 1);
    ck_assert_ptr_null(repl->current->assistant_response);
}
END_TEST
/*
 * Test: When finish_reason is "stop", handle_request_success should:
 * 1. Add assistant response to conversation
 * 2. NOT submit a follow-up request
 */
START_TEST(test_handle_request_success_with_stop_ends_loop)
{
    /* Simulate Response B: finish_reason = "stop" */
    repl->current->response_finish_reason = talloc_strdup(repl, "stop");
    repl->current->response_model = talloc_strdup(repl, "gpt-4");
    repl->current->response_completion_tokens = 24;

    /* Simulate accumulated assistant response */
    repl->current->assistant_response = talloc_strdup(repl, "I found 3 C files.");

    /* Add initial user message to conversation */
    ik_msg_t *user_msg = ik_openai_msg_create(repl->current->conversation, "user", "Find all C files");
    ik_openai_conversation_add_msg(repl->current->conversation, user_msg);

    /* Record initial conversation size */
    size_t initial_count = repl->current->conversation->message_count;

    /* Call handle_request_success */
    ik_repl_handle_agent_request_success(repl, repl->current);

    /* Verify:
     * 1. Assistant message was added to conversation
     * 2. curl_still_running should still be 0 (no new request)
     * 3. Assistant response should be cleared
     */
    ck_assert_uint_eq(repl->current->conversation->message_count, initial_count + 1);
    ck_assert_str_eq(repl->current->conversation->messages[initial_count]->kind, "assistant");
    ck_assert_str_eq(repl->current->conversation->messages[initial_count]->content, "I found 3 C files.");
    ck_assert_int_eq(repl->current->curl_still_running, 0);
    ck_assert_ptr_null(repl->current->assistant_response);
}

END_TEST
/*
 * Test: When finish_reason is NULL, should not continue loop
 */
START_TEST(test_handle_request_success_with_null_finish_reason)
{
    /* Simulate response with NULL finish_reason */
    repl->current->response_finish_reason = NULL;
    repl->current->assistant_response = talloc_strdup(repl, "Response text");

    /* Add initial user message to conversation */
    ik_msg_t *user_msg = ik_openai_msg_create(repl->current->conversation, "user", "Test");
    ik_openai_conversation_add_msg(repl->current->conversation, user_msg);

    /* Call handle_request_success */
    ik_repl_handle_agent_request_success(repl, repl->current);

    /* Verify: curl_still_running should still be 0 (no new request) */
    ck_assert_int_eq(repl->current->curl_still_running, 0);
}

END_TEST
/*
 * Test: Multiple tool loop iterations
 */
START_TEST(test_multiple_tool_loop_iterations)
{
    /* Add initial user message */
    ik_msg_t *user_msg = ik_openai_msg_create(repl->current->conversation, "user", "Find files");
    ik_openai_conversation_add_msg(repl->current->conversation, user_msg);

    /* First iteration: finish_reason = "tool_calls" */
    repl->current->response_finish_reason = talloc_strdup(repl, "tool_calls");
    repl->current->assistant_response = talloc_strdup(repl, "");

    ik_repl_handle_agent_request_success(repl, repl->current);

    ck_assert_int_eq(repl->current->state, IK_AGENT_STATE_WAITING_FOR_LLM);
    ck_assert_int_eq(repl->current->curl_still_running, 1);

    /* Reset for second iteration */
    talloc_free(repl->current->response_finish_reason);
    repl->current->curl_still_running = 0;

    /* Second iteration: finish_reason = "stop" */
    repl->current->response_finish_reason = talloc_strdup(repl, "stop");
    repl->current->assistant_response = talloc_strdup(repl, "Done!");

    ik_repl_handle_agent_request_success(repl, repl->current);

    /* Verify loop stops */
    ck_assert_int_eq(repl->current->curl_still_running, 0);
}

END_TEST
/*
 * Test: Tool loop with empty assistant response content
 */
START_TEST(test_tool_loop_with_empty_content)
{
    /* Add initial user message */
    ik_msg_t *user_msg = ik_openai_msg_create(repl->current->conversation, "user", "Test");
    ik_openai_conversation_add_msg(repl->current->conversation, user_msg);

    /* Set finish_reason to "tool_calls" with empty response */
    repl->current->response_finish_reason = talloc_strdup(repl, "tool_calls");
    repl->current->assistant_response = NULL;  /* NULL response */

    size_t initial_count = repl->current->conversation->message_count;

    ik_repl_handle_agent_request_success(repl, repl->current);

    /* Verify: No assistant message added (response was NULL) */
    ck_assert_uint_eq(repl->current->conversation->message_count, initial_count);

    /* Verify: Follow-up request was initiated */
    ck_assert_int_eq(repl->current->curl_still_running, 1);
}

END_TEST
/*
 * Test: Multi-tool scenario (glob → file_read → response)
 *
 * This test simulates the complete Story 04 flow:
 * 1. User message: "Find config file and show contents"
 * 2. Response A: assistant calls glob tool (finish_reason: "tool_calls")
 * 3. Tool result added to conversation
 * 4. Response B: assistant calls file_read tool (finish_reason: "tool_calls")
 * 5. Tool result added to conversation
 * 6. Response C: assistant provides final text (finish_reason: "stop")
 *
 * Expected conversation after all iterations:
 * - user: "Find config file and show contents"
 * - assistant: "" (tool call content, may be empty)
 * - tool: glob result
 * - assistant: "" (tool call content, may be empty)
 * - tool: file_read result
 * - assistant: "I found config.json..."
 *
 * Total: 6 messages (1 user + 5 assistant/tool messages)
 */
START_TEST(test_multi_tool_scenario_glob_then_file_read)
{
    /* Initial state: User asks to find and read config file */
    ik_msg_t *user_msg = ik_openai_msg_create(repl->current->conversation, "user",
                                              "Find config file and show contents");
    ik_openai_conversation_add_msg(repl->current->conversation, user_msg);

    /* Verify initial state: 1 message (user) */
    ck_assert_uint_eq(repl->current->conversation->message_count, 1);

    /* ===== First iteration: glob tool call ===== */
    /* Response A: finish_reason = "tool_calls", empty content (tool call only) */
    repl->current->response_finish_reason = talloc_strdup(repl, "tool_calls");
    repl->current->assistant_response = talloc_strdup(repl, "");
    repl->current->response_model = talloc_strdup(repl, "gpt-4");
    repl->current->response_completion_tokens = 10;

    ik_repl_handle_agent_request_success(repl, repl->current);

    /* Verify: Still in WAITING_FOR_LLM state (loop continues) */
    ck_assert_int_eq(repl->current->state, IK_AGENT_STATE_WAITING_FOR_LLM);
    ck_assert_int_eq(repl->current->curl_still_running, 1);

    /* Simulate tool execution: Add tool result to conversation */
    /* In real scenario, tool dispatcher would add this */
    ik_msg_t *tool_result_1 = ik_openai_msg_create(repl->current->conversation, "tool",
                                                   "{\"output\":\"config.json\"}");
    ik_openai_conversation_add_msg(repl->current->conversation, tool_result_1);

    /* Verify: 2 messages now (user + tool result) */
    /* Note: Empty assistant response was not added (strlen == 0) */
    ck_assert_uint_eq(repl->current->conversation->message_count, 2);

    /* Reset for next iteration */
    talloc_free(repl->current->response_finish_reason);
    repl->current->curl_still_running = 0;

    /* ===== Second iteration: file_read tool call ===== */
    /* Response B: finish_reason = "tool_calls", empty content (tool call only) */
    repl->current->response_finish_reason = talloc_strdup(repl, "tool_calls");
    repl->current->assistant_response = talloc_strdup(repl, "");
    repl->current->response_model = talloc_strdup(repl, "gpt-4");
    repl->current->response_completion_tokens = 15;

    ik_repl_handle_agent_request_success(repl, repl->current);

    /* Verify: Still in WAITING_FOR_LLM state (loop continues) */
    ck_assert_int_eq(repl->current->state, IK_AGENT_STATE_WAITING_FOR_LLM);
    ck_assert_int_eq(repl->current->curl_still_running, 1);

    /* Simulate tool execution: Add second tool result to conversation */
    ik_msg_t *tool_result_2 = ik_openai_msg_create(repl->current->conversation, "tool",
                                                   "{\"output\":\"{\\\"debug\\\":true}\"}");
    ik_openai_conversation_add_msg(repl->current->conversation, tool_result_2);

    /* Verify: 3 messages now (user + 2 tool results) */
    ck_assert_uint_eq(repl->current->conversation->message_count, 3);

    /* Reset for final iteration */
    talloc_free(repl->current->response_finish_reason);
    repl->current->curl_still_running = 0;

    /* ===== Final iteration: text response ===== */
    /* Response C: finish_reason = "stop", final text content */
    repl->current->response_finish_reason = talloc_strdup(repl, "stop");
    repl->current->assistant_response = talloc_strdup(repl, "I found config.json with debug:true");
    repl->current->response_model = talloc_strdup(repl, "gpt-4");
    repl->current->response_completion_tokens = 20;

    ik_repl_handle_agent_request_success(repl, repl->current);

    /* Verify: Loop stops (no new request initiated) */
    ck_assert_int_eq(repl->current->curl_still_running, 0);

    /* Verify final conversation state: 4 messages total */
    /* - user: "Find config file and show contents"
     * - tool: glob result
     * - tool: file_read result
     * - assistant: "I found config.json with debug:true"
     */
    ck_assert_uint_eq(repl->current->conversation->message_count, 4);

    /* Verify last message is assistant with final content */
    ck_assert_str_eq(repl->current->conversation->messages[3]->kind, "assistant");
    ck_assert_str_eq(repl->current->conversation->messages[3]->content,
                     "I found config.json with debug:true");
}

END_TEST

/*
 * Test suite
 */
static Suite *repl_tool_loop_integration_suite(void)
{
    Suite *s = suite_create("REPL Tool Loop Integration");

    TCase *tc_core = tcase_create("Core");
    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_handle_request_success_with_tool_calls_continues_loop);
    tcase_add_test(tc_core, test_handle_request_success_with_stop_ends_loop);
    tcase_add_test(tc_core, test_handle_request_success_with_null_finish_reason);
    tcase_add_test(tc_core, test_multiple_tool_loop_iterations);
    tcase_add_test(tc_core, test_tool_loop_with_empty_content);
    tcase_add_test(tc_core, test_multi_tool_scenario_glob_then_file_read);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = repl_tool_loop_integration_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
