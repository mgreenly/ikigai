#include "../../test_constants.h"
/**
 * @file repl_http_completion_callback_advanced_test.c
 * @brief Unit tests for REPL provider completion callback (advanced)
 *
 * Tests advanced completion callback behaviors: tool call handling
 * and finish reason mapping.
 */

#include "repl.h"
#include "../../../src/agent.h"
#include "repl_callbacks.h"
#include "shared.h"
#include "scrollback.h"
#include "config.h"
#include "providers/provider.h"
#include "tool.h"
#include <check.h>
#include <talloc.h>
#include <curl/curl.h>
#include <unistd.h>

static void *ctx;
static ik_repl_ctx_t *repl;

static void setup(void)
{
    ctx = talloc_new(NULL);

    /* Create minimal shared context */
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);

    /* Create minimal REPL context for testing callback */
    repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    repl->shared = shared;

    /* Create agent context for display state */
    ik_agent_ctx_t *agent = talloc_zero(repl, ik_agent_ctx_t);
    repl->current = agent;
    repl->current->shared = shared;
    repl->current->scrollback = ik_scrollback_create(repl, 80);
    repl->current->streaming_line_buffer = NULL;
    repl->current->http_error_message = NULL;
    repl->current->response_model = NULL;
    repl->current->response_finish_reason = NULL;
    repl->current->response_input_tokens = 0;
    repl->current->response_output_tokens = 0;
    repl->current->response_thinking_tokens = 0;
    repl->current->pending_tool_call = NULL;
}

/* Helper to create a simple successful completion */
static ik_provider_completion_t make_success_completion(void)
{
    ik_provider_completion_t completion = {
        .success = true,
        .http_status = 200,
        .response = NULL,
        .error_category = 0,
        .error_message = NULL,
        .retry_after_ms = -1
    };
    return completion;
}

static void teardown(void)
{
    talloc_free(ctx);
}

/* Test: Completion stores tool_call in pending_tool_call */
START_TEST(test_completion_stores_tool_call) {
    /* Create response with tool call content block */
    ik_response_t *response = talloc_zero(ctx, ik_response_t);
    response->model = NULL;
    response->finish_reason = IK_FINISH_TOOL_USE;
    response->usage.output_tokens = 50;
    response->content_count = 1;
    response->content_blocks = talloc_array(response, ik_content_block_t, 1);
    response->content_blocks[0].type = IK_CONTENT_TOOL_CALL;
    response->content_blocks[0].data.tool_call.id = talloc_strdup(response, "call_test123");
    response->content_blocks[0].data.tool_call.name = talloc_strdup(response, "glob");
    response->content_blocks[0].data.tool_call.arguments = talloc_strdup(response, "{\"pattern\": \"*.c\"}");

    /* Create successful completion */
    ik_provider_completion_t completion = make_success_completion();
    completion.response = response;

    /* Call callback */
    res_t result = ik_repl_completion_callback(&completion, repl->current);
    ck_assert(is_ok(&result));

    /* Verify tool_call was stored */
    ck_assert_ptr_nonnull(repl->current->pending_tool_call);
    ck_assert_str_eq(repl->current->pending_tool_call->id, "call_test123");
    ck_assert_str_eq(repl->current->pending_tool_call->name, "glob");
    ck_assert_str_eq(repl->current->pending_tool_call->arguments, "{\"pattern\": \"*.c\"}");
}

END_TEST
/* Test: Completion clears previous pending_tool_call before storing new one */
START_TEST(test_completion_clears_previous_tool_call) {
    /* Set up previous pending_tool_call */
    repl->current->pending_tool_call = ik_tool_call_create(repl, "old_call", "old_tool", "{}");

    /* Create response with new tool call */
    ik_response_t *response = talloc_zero(ctx, ik_response_t);
    response->model = NULL;
    response->finish_reason = IK_FINISH_TOOL_USE;
    response->usage.output_tokens = 25;
    response->content_count = 1;
    response->content_blocks = talloc_array(response, ik_content_block_t, 1);
    response->content_blocks[0].type = IK_CONTENT_TOOL_CALL;
    response->content_blocks[0].data.tool_call.id = talloc_strdup(response, "new_call");
    response->content_blocks[0].data.tool_call.name = talloc_strdup(response, "new_tool");
    response->content_blocks[0].data.tool_call.arguments = talloc_strdup(response, "{\"key\": \"value\"}");

    /* Create successful completion */
    ik_provider_completion_t completion = make_success_completion();
    completion.response = response;

    /* Call callback */
    res_t result = ik_repl_completion_callback(&completion, repl->current);
    ck_assert(is_ok(&result));

    /* Verify new tool_call replaced old one */
    ck_assert_ptr_nonnull(repl->current->pending_tool_call);
    ck_assert_str_eq(repl->current->pending_tool_call->id, "new_call");
    ck_assert_str_eq(repl->current->pending_tool_call->name, "new_tool");
}

END_TEST
/* Test: Completion with NULL tool_call clears pending_tool_call */
START_TEST(test_completion_null_tool_call_clears_pending) {
    /* Set up previous pending_tool_call */
    repl->current->pending_tool_call = ik_tool_call_create(repl, "old_call", "old_tool", "{}");

    /* Create response without tool call */
    ik_response_t *response = talloc_zero(ctx, ik_response_t);
    response->model = NULL;
    response->finish_reason = IK_FINISH_STOP;
    response->usage.output_tokens = 10;
    response->content_blocks = NULL;
    response->content_count = 0;

    /* Create successful completion */
    ik_provider_completion_t completion = make_success_completion();
    completion.response = response;

    /* Call callback */
    res_t result = ik_repl_completion_callback(&completion, repl->current);
    ck_assert(is_ok(&result));

    /* Verify pending_tool_call was cleared */
    ck_assert_ptr_null(repl->current->pending_tool_call);
}

END_TEST

/* Test: Completion with IK_FINISH_STOP maps to "stop" */
START_TEST(test_completion_finish_reason_stop) {
    /* Create response with STOP finish reason */
    ik_response_t *response = talloc_zero(ctx, ik_response_t);
    response->model = NULL;
    response->finish_reason = IK_FINISH_STOP;
    response->usage.output_tokens = 0;
    response->content_blocks = NULL;
    response->content_count = 0;

    /* Create successful completion */
    ik_provider_completion_t completion = make_success_completion();
    completion.response = response;

    /* Call callback */
    res_t result = ik_repl_completion_callback(&completion, repl->current);
    ck_assert(is_ok(&result));

    /* Verify finish reason was mapped */
    ck_assert_ptr_nonnull(repl->current->response_finish_reason);
    ck_assert_str_eq(repl->current->response_finish_reason, "stop");
}

END_TEST

/* Test: Completion with IK_FINISH_LENGTH maps to "length" */
START_TEST(test_completion_finish_reason_length) {
    /* Create response with LENGTH finish reason */
    ik_response_t *response = talloc_zero(ctx, ik_response_t);
    response->model = NULL;
    response->finish_reason = IK_FINISH_LENGTH;
    response->usage.output_tokens = 0;
    response->content_blocks = NULL;
    response->content_count = 0;

    /* Create successful completion */
    ik_provider_completion_t completion = make_success_completion();
    completion.response = response;

    /* Call callback */
    res_t result = ik_repl_completion_callback(&completion, repl->current);
    ck_assert(is_ok(&result));

    /* Verify finish reason was mapped */
    ck_assert_ptr_nonnull(repl->current->response_finish_reason);
    ck_assert_str_eq(repl->current->response_finish_reason, "length");
}

END_TEST
/* Test: Completion with IK_FINISH_TOOL_USE maps to "tool_use" */
START_TEST(test_completion_finish_reason_tool_use) {
    /* Create response with TOOL_USE finish reason */
    ik_response_t *response = talloc_zero(ctx, ik_response_t);
    response->model = NULL;
    response->finish_reason = IK_FINISH_TOOL_USE;
    response->usage.output_tokens = 0;
    response->content_blocks = NULL;
    response->content_count = 0;

    /* Create successful completion */
    ik_provider_completion_t completion = make_success_completion();
    completion.response = response;

    /* Call callback */
    res_t result = ik_repl_completion_callback(&completion, repl->current);
    ck_assert(is_ok(&result));

    /* Verify finish reason was mapped */
    ck_assert_ptr_nonnull(repl->current->response_finish_reason);
    ck_assert_str_eq(repl->current->response_finish_reason, "tool_use");
}

END_TEST
/* Test: Completion with IK_FINISH_CONTENT_FILTER maps to "content_filter" */
START_TEST(test_completion_finish_reason_content_filter) {
    /* Create response with CONTENT_FILTER finish reason */
    ik_response_t *response = talloc_zero(ctx, ik_response_t);
    response->model = NULL;
    response->finish_reason = IK_FINISH_CONTENT_FILTER;
    response->usage.output_tokens = 0;
    response->content_blocks = NULL;
    response->content_count = 0;

    /* Create successful completion */
    ik_provider_completion_t completion = make_success_completion();
    completion.response = response;

    /* Call callback */
    res_t result = ik_repl_completion_callback(&completion, repl->current);
    ck_assert(is_ok(&result));

    /* Verify finish reason was mapped */
    ck_assert_ptr_nonnull(repl->current->response_finish_reason);
    ck_assert_str_eq(repl->current->response_finish_reason, "content_filter");
}

END_TEST
/* Test: Completion with IK_FINISH_ERROR maps to "error" */
START_TEST(test_completion_finish_reason_error) {
    /* Create response with ERROR finish reason */
    ik_response_t *response = talloc_zero(ctx, ik_response_t);
    response->model = NULL;
    response->finish_reason = IK_FINISH_ERROR;
    response->usage.output_tokens = 0;
    response->content_blocks = NULL;
    response->content_count = 0;

    /* Create successful completion */
    ik_provider_completion_t completion = make_success_completion();
    completion.response = response;

    /* Call callback */
    res_t result = ik_repl_completion_callback(&completion, repl->current);
    ck_assert(is_ok(&result));

    /* Verify finish reason was mapped */
    ck_assert_ptr_nonnull(repl->current->response_finish_reason);
    ck_assert_str_eq(repl->current->response_finish_reason, "error");
}

END_TEST
/* Test: Completion with IK_FINISH_UNKNOWN maps to "unknown" */
START_TEST(test_completion_finish_reason_unknown) {
    /* Create response with UNKNOWN finish reason */
    ik_response_t *response = talloc_zero(ctx, ik_response_t);
    response->model = NULL;
    response->finish_reason = IK_FINISH_UNKNOWN;
    response->usage.output_tokens = 0;
    response->content_blocks = NULL;
    response->content_count = 0;

    /* Create successful completion */
    ik_provider_completion_t completion = make_success_completion();
    completion.response = response;

    /* Call callback */
    res_t result = ik_repl_completion_callback(&completion, repl->current);
    ck_assert(is_ok(&result));

    /* Verify finish reason was mapped */
    ck_assert_ptr_nonnull(repl->current->response_finish_reason);
    ck_assert_str_eq(repl->current->response_finish_reason, "unknown");
}

END_TEST

/*
 * Test suite
 */

static Suite *repl_http_completion_callback_advanced_suite(void)
{
    Suite *s = suite_create("repl_http_completion_callback_advanced");

    TCase *tc_core = tcase_create("callback_behavior");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_completion_stores_tool_call);
    tcase_add_test(tc_core, test_completion_clears_previous_tool_call);
    tcase_add_test(tc_core, test_completion_null_tool_call_clears_pending);
    tcase_add_test(tc_core, test_completion_finish_reason_stop);
    tcase_add_test(tc_core, test_completion_finish_reason_length);
    tcase_add_test(tc_core, test_completion_finish_reason_tool_use);
    tcase_add_test(tc_core, test_completion_finish_reason_content_filter);
    tcase_add_test(tc_core, test_completion_finish_reason_error);
    tcase_add_test(tc_core, test_completion_finish_reason_unknown);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = repl_http_completion_callback_advanced_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/repl/repl_http_completion_callback_advanced_test.xml");
    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
