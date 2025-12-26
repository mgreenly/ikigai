/**
 * @file repl_http_completion_callback_test.c
 * @brief Unit tests for REPL provider completion callback
 *
 * Tests the ik_repl_completion_callback function which handles
 * provider request completion for both success and error cases.
 */

#include "repl.h"
#include "../../../src/agent.h"
#include "repl_callbacks.h"
#include "shared.h"
#include "scrollback.h"
#include "config.h"
#include "providers/provider.h"
#include "tool.h"
#include "debug_pipe.h"
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

/* Helper to create an error completion */
static ik_provider_completion_t make_error_completion(int http_status, ik_error_category_t category, char *msg)
{
    ik_provider_completion_t completion = {
        .success = false,
        .http_status = http_status,
        .response = NULL,
        .error_category = category,
        .error_message = msg,
        .retry_after_ms = -1
    };
    return completion;
}

static void teardown(void)
{
    talloc_free(ctx);
}

/* Test: Flush remaining buffered line content when completion occurs */
START_TEST(test_completion_flushes_streaming_buffer) {
    /* Simulate partial streaming content in buffer */
    repl->current->streaming_line_buffer = talloc_strdup(repl, "Partial line content");

    /* Create successful completion */
    ik_provider_completion_t completion = make_success_completion();

    /* Call callback */
    res_t result = ik_repl_completion_callback(&completion, repl->current);
    ck_assert(is_ok(&result));

    /* Verify buffer was flushed to scrollback (content + blank line) and cleared */
    ck_assert_ptr_null(repl->current->streaming_line_buffer);
    ck_assert_uint_eq((unsigned int)ik_scrollback_get_line_count(repl->current->scrollback), 2);
}
END_TEST
/* Test: Completion clears previous error message */
START_TEST(test_completion_clears_previous_error)
{
    /* Set up previous error */
    repl->current->http_error_message = talloc_strdup(repl, "Previous error");

    /* Create successful completion */
    ik_provider_completion_t completion = make_success_completion();

    /* Call callback */
    res_t result = ik_repl_completion_callback(&completion, repl->current);
    ck_assert(is_ok(&result));

    /* Verify error was cleared */
    ck_assert_ptr_null(repl->current->http_error_message);
}

END_TEST
/* Test: Completion stores error message on failure */
START_TEST(test_completion_stores_error_on_failure)
{
    /* Create failed completion */
    char *error_msg = talloc_strdup(ctx, "HTTP 500 server error");
    ik_provider_completion_t completion = make_error_completion(500, IK_ERR_CAT_SERVER, error_msg);

    /* Call callback */
    res_t result = ik_repl_completion_callback(&completion, repl->current);
    ck_assert(is_ok(&result));

    /* Verify error message was stored */
    ck_assert_ptr_nonnull(repl->current->http_error_message);
    ck_assert_str_eq(repl->current->http_error_message, "HTTP 500 server error");
}

END_TEST
/* Test: Completion stores response metadata on success */
START_TEST(test_completion_stores_metadata_on_success)
{
    /* Create response with metadata */
    ik_response_t *response = talloc_zero(ctx, ik_response_t);
    response->model = talloc_strdup(response, "gpt-4-turbo");
    response->finish_reason = IK_FINISH_STOP;
    response->usage.output_tokens = 42;
    response->content_blocks = NULL;
    response->content_count = 0;

    /* Create successful completion */
    ik_provider_completion_t completion = make_success_completion();
    completion.response = response;

    /* Call callback */
    res_t result = ik_repl_completion_callback(&completion, repl->current);
    ck_assert(is_ok(&result));

    /* Verify metadata was stored */
    ck_assert_ptr_nonnull(repl->current->response_model);
    ck_assert_str_eq(repl->current->response_model, "gpt-4-turbo");
    ck_assert_ptr_nonnull(repl->current->response_finish_reason);
    ck_assert_str_eq(repl->current->response_finish_reason, "stop");
    ck_assert_int_eq(repl->current->response_output_tokens, 42);
}

END_TEST
/* Test: Completion clears previous metadata before storing new */
START_TEST(test_completion_clears_previous_metadata)
{
    /* Set up previous metadata */
    repl->current->response_model = talloc_strdup(repl, "old-model");
    repl->current->response_finish_reason = talloc_strdup(repl, "old-reason");
    repl->current->response_output_tokens = 99;

    /* Create response with new metadata */
    ik_response_t *response = talloc_zero(ctx, ik_response_t);
    response->model = talloc_strdup(response, "new-model");
    response->finish_reason = IK_FINISH_STOP;
    response->usage.output_tokens = 50;
    response->content_blocks = NULL;
    response->content_count = 0;

    /* Create successful completion */
    ik_provider_completion_t completion = make_success_completion();
    completion.response = response;

    /* Call callback */
    res_t result = ik_repl_completion_callback(&completion, repl->current);
    ck_assert(is_ok(&result));

    /* Verify old metadata was replaced */
    ck_assert_str_eq(repl->current->response_model, "new-model");
    ck_assert_str_eq(repl->current->response_finish_reason, "stop");  // Note: finish_reason is now "stop" enum mapping
    ck_assert_int_eq(repl->current->response_output_tokens, 50);
}

END_TEST
/* Test: Completion with NULL metadata doesn't store anything */
START_TEST(test_completion_null_metadata)
{
    /* Create successful completion without response (NULL metadata) */
    ik_provider_completion_t completion = make_success_completion();
    /* response is already NULL */

    /* Call callback */
    res_t result = ik_repl_completion_callback(&completion, repl->current);
    ck_assert(is_ok(&result));

    /* Verify no metadata was stored */
    ck_assert_ptr_null(repl->current->response_model);
    ck_assert_ptr_null(repl->current->response_finish_reason);
    ck_assert_int_eq(repl->current->response_output_tokens, 0);
}

END_TEST
/* Test: Completion with network error stores error message */
START_TEST(test_completion_network_error)
{
    /* Create network error completion */
    char *error_msg = talloc_strdup(ctx, "Connection error: Failed to connect");
    ik_provider_completion_t completion = make_error_completion(0, IK_ERR_CAT_NETWORK, error_msg);

    /* Call callback */
    res_t result = ik_repl_completion_callback(&completion, repl->current);
    ck_assert(is_ok(&result));

    /* Verify error message was stored */
    ck_assert_ptr_nonnull(repl->current->http_error_message);
    ck_assert_str_eq(repl->current->http_error_message, "Connection error: Failed to connect");
}

END_TEST
/* Test: Completion with client error (4xx) stores error */
START_TEST(test_completion_client_error)
{
    /* Create client error completion */
    char *error_msg = talloc_strdup(ctx, "HTTP 401 error");
    ik_provider_completion_t completion = make_error_completion(401, IK_ERR_CAT_AUTH, error_msg);

    /* Call callback */
    res_t result = ik_repl_completion_callback(&completion, repl->current);
    ck_assert(is_ok(&result));

    /* Verify error message was stored */
    ck_assert_ptr_nonnull(repl->current->http_error_message);
    ck_assert_str_eq(repl->current->http_error_message, "HTTP 401 error");
}

END_TEST
/* Test: Both streaming buffer and error handling */
START_TEST(test_completion_flushes_buffer_and_stores_error)
{
    /* Set up partial streaming content */
    repl->current->streaming_line_buffer = talloc_strdup(repl, "Incomplete response");

    /* Create failed completion */
    char *error_msg = talloc_strdup(ctx, "Request timeout");
    ik_provider_completion_t completion = make_error_completion(0, IK_ERR_CAT_TIMEOUT, error_msg);

    /* Call callback */
    res_t result = ik_repl_completion_callback(&completion, repl->current);
    ck_assert(is_ok(&result));

    /* Verify buffer was flushed */
    ck_assert_ptr_null(repl->current->streaming_line_buffer);
    ck_assert_uint_eq((unsigned int)ik_scrollback_get_line_count(repl->current->scrollback), 1);

    /* Verify error was stored */
    ck_assert_ptr_nonnull(repl->current->http_error_message);
    ck_assert_str_eq(repl->current->http_error_message, "Request timeout");
}

END_TEST
/* Test: Error completion with NULL error_message doesn't store error */
START_TEST(test_completion_error_null_message)
{
    /* Create error completion with NULL error message */
    ik_provider_completion_t completion = make_error_completion(500, IK_ERR_CAT_SERVER, NULL);

    /* Call callback */
    res_t result = ik_repl_completion_callback(&completion, repl->current);
    ck_assert(is_ok(&result));

    /* Verify no error message was stored (NULL && branch) */
    ck_assert_ptr_null(repl->current->http_error_message);
}

END_TEST
/* Test: Completion stores tool_call in pending_tool_call */
START_TEST(test_completion_stores_tool_call)
{
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
START_TEST(test_completion_clears_previous_tool_call)
{
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
START_TEST(test_completion_null_tool_call_clears_pending)
{
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

/*
 * Test suite
 */

static Suite *repl_http_completion_callback_suite(void)
{
    Suite *s = suite_create("repl_http_completion_callback");

    TCase *tc_core = tcase_create("callback_behavior");
    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc_core, test_completion_flushes_streaming_buffer);
    tcase_add_test(tc_core, test_completion_clears_previous_error);
    tcase_add_test(tc_core, test_completion_stores_error_on_failure);
    tcase_add_test(tc_core, test_completion_stores_metadata_on_success);
    tcase_add_test(tc_core, test_completion_clears_previous_metadata);
    tcase_add_test(tc_core, test_completion_null_metadata);
    tcase_add_test(tc_core, test_completion_network_error);
    tcase_add_test(tc_core, test_completion_client_error);
    tcase_add_test(tc_core, test_completion_flushes_buffer_and_stores_error);
    tcase_add_test(tc_core, test_completion_error_null_message);
    tcase_add_test(tc_core, test_completion_stores_tool_call);
    tcase_add_test(tc_core, test_completion_clears_previous_tool_call);
    tcase_add_test(tc_core, test_completion_null_tool_call_clears_pending);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = repl_http_completion_callback_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
