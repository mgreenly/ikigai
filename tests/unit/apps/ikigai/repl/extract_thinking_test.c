#include "tests/test_constants.h"
/**
 * @file extract_thinking_test.c
 * @brief Unit tests for thinking block extraction from responses
 *
 * Tests that the completion callback correctly extracts thinking blocks
 * and stores them in agent context for later use in tool call messages.
 */

#include "apps/ikigai/agent.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/repl_callbacks.h"
#include "apps/ikigai/shared.h"
#include "apps/ikigai/scrollback.h"
#include "apps/ikigai/providers/provider.h"
#include "apps/ikigai/tool.h"
#include <check.h>
#include <talloc.h>

static void *ctx;
static ik_repl_ctx_t *repl;
static ik_agent_ctx_t *agent;

static void setup(void)
{
    ctx = talloc_new(NULL);

    // Create minimal shared context
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);

    // Create minimal REPL context
    repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->shared = shared;

    // Create agent
    agent = talloc_zero(repl, ik_agent_ctx_t);
    agent->scrollback = ik_scrollback_create(agent, 80);
    agent->streaming_line_buffer = NULL;
    agent->http_error_message = NULL;
    agent->response_model = NULL;
    agent->response_finish_reason = NULL;
    agent->response_input_tokens = 0;
    agent->response_output_tokens = 0;
    agent->response_thinking_tokens = 0;
    agent->assistant_response = NULL;
    agent->shared = shared;
    agent->pending_tool_call = NULL;
    agent->pending_thinking_text = NULL;
    agent->pending_thinking_signature = NULL;
    agent->pending_redacted_data = NULL;

    repl->current = agent;
}

static void teardown(void)
{
    talloc_free(ctx);
}

// Test: Extract thinking block with text and signature
START_TEST(test_extract_thinking_block) {
    // Create response with thinking block
    ik_response_t *response = talloc_zero(ctx, ik_response_t);
    response->model = talloc_strdup(response, "claude-sonnet-4-5");
    response->finish_reason = IK_FINISH_STOP;
    response->usage.output_tokens = 100;

    // Create thinking content block
    response->content_count = 1;
    response->content_blocks = talloc_array(response, ik_content_block_t, 1);
    response->content_blocks[0].type = IK_CONTENT_THINKING;
    response->content_blocks[0].data.thinking.text = talloc_strdup(response, "Let me solve this problem...");
    response->content_blocks[0].data.thinking.signature = talloc_strdup(response, "EqQBCgIYAhIM...");

    ik_provider_completion_t completion = {
        .success = true,
        .http_status = 200,
        .response = response,
        .error_category = IK_ERR_CAT_UNKNOWN,
        .error_message = NULL,
        .retry_after_ms = -1
    };

    // Call completion callback
    res_t result = ik_repl_completion_callback(&completion, agent);
    ck_assert(is_ok(&result));

    // Verify thinking text and signature were stored
    ck_assert_ptr_nonnull(agent->pending_thinking_text);
    ck_assert_str_eq(agent->pending_thinking_text, "Let me solve this problem...");
    ck_assert_ptr_nonnull(agent->pending_thinking_signature);
    ck_assert_str_eq(agent->pending_thinking_signature, "EqQBCgIYAhIM...");
    ck_assert_ptr_null(agent->pending_redacted_data);
}

END_TEST

// Test: Extract redacted thinking block
START_TEST(test_extract_redacted_thinking) {
    // Create response with redacted thinking block
    ik_response_t *response = talloc_zero(ctx, ik_response_t);
    response->model = talloc_strdup(response, "claude-sonnet-4-5");
    response->finish_reason = IK_FINISH_STOP;
    response->usage.output_tokens = 100;

    // Create redacted thinking content block
    response->content_count = 1;
    response->content_blocks = talloc_array(response, ik_content_block_t, 1);
    response->content_blocks[0].type = IK_CONTENT_REDACTED_THINKING;
    response->content_blocks[0].data.redacted_thinking.data = talloc_strdup(response, "EmwKAhgBEgy...");

    ik_provider_completion_t completion = {
        .success = true,
        .http_status = 200,
        .response = response,
        .error_category = IK_ERR_CAT_UNKNOWN,
        .error_message = NULL,
        .retry_after_ms = -1
    };

    // Call completion callback
    res_t result = ik_repl_completion_callback(&completion, agent);
    ck_assert(is_ok(&result));

    // Verify redacted data was stored
    ck_assert_ptr_null(agent->pending_thinking_text);
    ck_assert_ptr_null(agent->pending_thinking_signature);
    ck_assert_ptr_nonnull(agent->pending_redacted_data);
    ck_assert_str_eq(agent->pending_redacted_data, "EmwKAhgBEgy...");
}

END_TEST

// Test: Extract thinking block with tool call
START_TEST(test_extract_thinking_with_tool_call) {
    // Create response with thinking block followed by tool call
    ik_response_t *response = talloc_zero(ctx, ik_response_t);
    response->model = talloc_strdup(response, "claude-sonnet-4-5");
    response->finish_reason = IK_FINISH_TOOL_USE;
    response->usage.output_tokens = 150;

    // Create two content blocks: thinking + tool_call
    response->content_count = 2;
    response->content_blocks = talloc_array(response, ik_content_block_t, 2);

    // Thinking block first (as in real Anthropic responses)
    response->content_blocks[0].type = IK_CONTENT_THINKING;
    response->content_blocks[0].data.thinking.text = talloc_strdup(response, "I need to search...");
    response->content_blocks[0].data.thinking.signature = talloc_strdup(response, "sig123");

    // Tool call second
    response->content_blocks[1].type = IK_CONTENT_TOOL_CALL;
    response->content_blocks[1].data.tool_call.id = talloc_strdup(response, "toolu_01abc");
    response->content_blocks[1].data.tool_call.name = talloc_strdup(response, "glob");
    response->content_blocks[1].data.tool_call.arguments = talloc_strdup(response, "{\"pattern\": \"*.c\"}");
    response->content_blocks[1].data.tool_call.thought_signature = NULL;

    ik_provider_completion_t completion = {
        .success = true,
        .http_status = 200,
        .response = response,
        .error_category = IK_ERR_CAT_UNKNOWN,
        .error_message = NULL,
        .retry_after_ms = -1
    };

    // Call completion callback
    res_t result = ik_repl_completion_callback(&completion, agent);
    ck_assert(is_ok(&result));

    // Verify both thinking and tool call were stored
    ck_assert_ptr_nonnull(agent->pending_thinking_text);
    ck_assert_str_eq(agent->pending_thinking_text, "I need to search...");
    ck_assert_ptr_nonnull(agent->pending_thinking_signature);
    ck_assert_str_eq(agent->pending_thinking_signature, "sig123");
    ck_assert_ptr_nonnull(agent->pending_tool_call);
    ck_assert_str_eq(agent->pending_tool_call->id, "toolu_01abc");
    ck_assert_str_eq(agent->pending_tool_call->name, "glob");
}

END_TEST

// Test: Previous pending values are cleared before extraction
START_TEST(test_extract_clears_previous) {
    // Set up existing pending values
    agent->pending_thinking_text = talloc_strdup(agent, "old thinking");
    agent->pending_thinking_signature = talloc_strdup(agent, "old signature");
    agent->pending_redacted_data = talloc_strdup(agent, "old redacted");
    agent->pending_tool_call = ik_tool_call_create(agent, "old_id", "old_name", "{}");

    // Create response with only a text block (no thinking, no tool call)
    ik_response_t *response = talloc_zero(ctx, ik_response_t);
    response->model = talloc_strdup(response, "claude-sonnet-4-5");
    response->finish_reason = IK_FINISH_STOP;
    response->usage.output_tokens = 50;

    // Create text content block only
    response->content_count = 1;
    response->content_blocks = talloc_array(response, ik_content_block_t, 1);
    response->content_blocks[0].type = IK_CONTENT_TEXT;
    response->content_blocks[0].data.text.text = talloc_strdup(response, "Hello");

    ik_provider_completion_t completion = {
        .success = true,
        .http_status = 200,
        .response = response,
        .error_category = IK_ERR_CAT_UNKNOWN,
        .error_message = NULL,
        .retry_after_ms = -1
    };

    // Call completion callback
    res_t result = ik_repl_completion_callback(&completion, agent);
    ck_assert(is_ok(&result));

    // Verify all pending values were cleared
    ck_assert_ptr_null(agent->pending_thinking_text);
    ck_assert_ptr_null(agent->pending_thinking_signature);
    ck_assert_ptr_null(agent->pending_redacted_data);
    ck_assert_ptr_null(agent->pending_tool_call);
}

END_TEST

// Test: Extract thinking block with NULL text field
START_TEST(test_extract_thinking_null_text) {
    // Create response with thinking block that has NULL text
    ik_response_t *response = talloc_zero(ctx, ik_response_t);
    response->model = talloc_strdup(response, "claude-sonnet-4-5");
    response->finish_reason = IK_FINISH_STOP;
    response->usage.output_tokens = 100;

    // Create thinking content block with NULL text but valid signature
    response->content_count = 1;
    response->content_blocks = talloc_array(response, ik_content_block_t, 1);
    response->content_blocks[0].type = IK_CONTENT_THINKING;
    response->content_blocks[0].data.thinking.text = NULL;
    response->content_blocks[0].data.thinking.signature = talloc_strdup(response, "EqQBCgIYAhIM...");

    ik_provider_completion_t completion = {
        .success = true,
        .http_status = 200,
        .response = response,
        .error_category = IK_ERR_CAT_UNKNOWN,
        .error_message = NULL,
        .retry_after_ms = -1
    };

    // Call completion callback
    res_t result = ik_repl_completion_callback(&completion, agent);
    ck_assert(is_ok(&result));

    // Verify thinking text is NULL but signature was stored
    ck_assert_ptr_null(agent->pending_thinking_text);
    ck_assert_ptr_nonnull(agent->pending_thinking_signature);
    ck_assert_str_eq(agent->pending_thinking_signature, "EqQBCgIYAhIM...");
}

END_TEST

// Test: Extract thinking block with NULL signature field
START_TEST(test_extract_thinking_null_signature) {
    // Create response with thinking block that has NULL signature
    ik_response_t *response = talloc_zero(ctx, ik_response_t);
    response->model = talloc_strdup(response, "claude-sonnet-4-5");
    response->finish_reason = IK_FINISH_STOP;
    response->usage.output_tokens = 100;

    // Create thinking content block with valid text but NULL signature
    response->content_count = 1;
    response->content_blocks = talloc_array(response, ik_content_block_t, 1);
    response->content_blocks[0].type = IK_CONTENT_THINKING;
    response->content_blocks[0].data.thinking.text = talloc_strdup(response, "Let me solve this...");
    response->content_blocks[0].data.thinking.signature = NULL;

    ik_provider_completion_t completion = {
        .success = true,
        .http_status = 200,
        .response = response,
        .error_category = IK_ERR_CAT_UNKNOWN,
        .error_message = NULL,
        .retry_after_ms = -1
    };

    // Call completion callback
    res_t result = ik_repl_completion_callback(&completion, agent);
    ck_assert(is_ok(&result));

    // Verify thinking text was stored but signature is NULL
    ck_assert_ptr_nonnull(agent->pending_thinking_text);
    ck_assert_str_eq(agent->pending_thinking_text, "Let me solve this...");
    ck_assert_ptr_null(agent->pending_thinking_signature);
}

END_TEST

// Test: Extract redacted thinking with NULL data field
START_TEST(test_extract_redacted_thinking_null_data) {
    // Create response with redacted thinking block that has NULL data
    ik_response_t *response = talloc_zero(ctx, ik_response_t);
    response->model = talloc_strdup(response, "claude-sonnet-4-5");
    response->finish_reason = IK_FINISH_STOP;
    response->usage.output_tokens = 100;

    // Create redacted thinking content block with NULL data
    response->content_count = 1;
    response->content_blocks = talloc_array(response, ik_content_block_t, 1);
    response->content_blocks[0].type = IK_CONTENT_REDACTED_THINKING;
    response->content_blocks[0].data.redacted_thinking.data = NULL;

    ik_provider_completion_t completion = {
        .success = true,
        .http_status = 200,
        .response = response,
        .error_category = IK_ERR_CAT_UNKNOWN,
        .error_message = NULL,
        .retry_after_ms = -1
    };

    // Call completion callback
    res_t result = ik_repl_completion_callback(&completion, agent);
    ck_assert(is_ok(&result));

    // Verify redacted data is NULL (not stored)
    ck_assert_ptr_null(agent->pending_thinking_text);
    ck_assert_ptr_null(agent->pending_thinking_signature);
    ck_assert_ptr_null(agent->pending_redacted_data);
}

END_TEST

/*
 * Test suite
 */

static Suite *extract_thinking_suite(void)
{
    Suite *s = suite_create("extract_thinking");

    TCase *tc_core = tcase_create("thinking_extraction");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_extract_thinking_block);
    tcase_add_test(tc_core, test_extract_redacted_thinking);
    tcase_add_test(tc_core, test_extract_thinking_with_tool_call);
    tcase_add_test(tc_core, test_extract_clears_previous);
    tcase_add_test(tc_core, test_extract_thinking_null_text);
    tcase_add_test(tc_core, test_extract_thinking_null_signature);
    tcase_add_test(tc_core, test_extract_redacted_thinking_null_data);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = extract_thinking_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/repl/extract_thinking_test.xml");
    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
