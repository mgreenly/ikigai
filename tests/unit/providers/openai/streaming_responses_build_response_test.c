/**
 * @file streaming_responses_build_response_test.c
 * @brief Tests for ik_openai_responses_stream_build_response function
 *
 * Tests the response builder that converts accumulated streaming context
 * into a complete ik_response_t structure.
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/openai/streaming.h"
#include "providers/openai/streaming_responses_internal.h"
#include "providers/provider.h"

/* ================================================================
 * Test Fixtures
 * ================================================================ */

static TALLOC_CTX *test_ctx;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

/* Dummy stream callback (not used in build_response tests) */
static res_t dummy_stream_cb(const ik_stream_event_t *event, void *ctx)
{
    (void)event;
    (void)ctx;
    return OK(NULL);
}

/* ================================================================
 * Test Cases
 * ================================================================ */

START_TEST(test_build_response_no_tool_call_no_model) {
    // Create streaming context
    ik_openai_responses_stream_ctx_t *sctx = ik_openai_responses_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    // Set up context with no tool call and no model
    sctx->finish_reason = IK_FINISH_STOP;
    sctx->usage.input_tokens = 10;
    sctx->usage.output_tokens = 20;
    sctx->usage.thinking_tokens = 5;
    sctx->usage.total_tokens = 35;

    // Build response
    ik_response_t *resp = ik_openai_responses_stream_build_response(test_ctx, sctx);

    // Verify response
    ck_assert_ptr_nonnull(resp);
    ck_assert_ptr_null(resp->model);
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_STOP);
    ck_assert_int_eq(resp->usage.input_tokens, 10);
    ck_assert_int_eq(resp->usage.output_tokens, 20);
    ck_assert_int_eq(resp->usage.thinking_tokens, 5);
    ck_assert_int_eq(resp->usage.total_tokens, 35);
    ck_assert_ptr_null(resp->content_blocks);
    ck_assert_int_eq((int)resp->content_count, 0);
}
END_TEST

START_TEST(test_build_response_no_tool_call_with_model) {
    // Create streaming context
    ik_openai_responses_stream_ctx_t *sctx = ik_openai_responses_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    // Set up context with model but no tool call
    sctx->model = talloc_strdup(sctx, "gpt-4o");
    sctx->finish_reason = IK_FINISH_LENGTH;
    sctx->usage.input_tokens = 100;
    sctx->usage.output_tokens = 200;
    sctx->usage.total_tokens = 300;

    // Build response
    ik_response_t *resp = ik_openai_responses_stream_build_response(test_ctx, sctx);

    // Verify response
    ck_assert_ptr_nonnull(resp);
    ck_assert_ptr_nonnull(resp->model);
    ck_assert_str_eq(resp->model, "gpt-4o");
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_LENGTH);
    ck_assert_int_eq(resp->usage.input_tokens, 100);
    ck_assert_int_eq(resp->usage.output_tokens, 200);
    ck_assert_int_eq(resp->usage.total_tokens, 300);
    ck_assert_ptr_null(resp->content_blocks);
    ck_assert_int_eq((int)resp->content_count, 0);
}
END_TEST

START_TEST(test_build_response_with_tool_call_all_fields) {
    // Create streaming context
    ik_openai_responses_stream_ctx_t *sctx = ik_openai_responses_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    // Set up context with tool call
    sctx->model = talloc_strdup(sctx, "gpt-4o");
    sctx->finish_reason = IK_FINISH_STOP; // Will be overridden to IK_FINISH_TOOL_USE
    sctx->usage.input_tokens = 50;
    sctx->usage.output_tokens = 75;
    sctx->usage.total_tokens = 125;
    sctx->current_tool_id = talloc_strdup(sctx, "call_abc123");
    sctx->current_tool_name = talloc_strdup(sctx, "get_weather");
    sctx->current_tool_args = talloc_strdup(sctx, "{\"location\":\"Paris\"}");

    // Build response
    ik_response_t *resp = ik_openai_responses_stream_build_response(test_ctx, sctx);

    // Verify response
    ck_assert_ptr_nonnull(resp);
    ck_assert_ptr_nonnull(resp->model);
    ck_assert_str_eq(resp->model, "gpt-4o");
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_TOOL_USE); // Overridden
    ck_assert_int_eq(resp->usage.input_tokens, 50);
    ck_assert_int_eq(resp->usage.output_tokens, 75);
    ck_assert_int_eq(resp->usage.total_tokens, 125);

    // Verify content blocks
    ck_assert_ptr_nonnull(resp->content_blocks);
    ck_assert_int_eq((int)resp->content_count, 1);
    ck_assert_int_eq(resp->content_blocks[0].type, IK_CONTENT_TOOL_CALL);
    ck_assert_str_eq(resp->content_blocks[0].data.tool_call.id, "call_abc123");
    ck_assert_str_eq(resp->content_blocks[0].data.tool_call.name, "get_weather");
    ck_assert_str_eq(resp->content_blocks[0].data.tool_call.arguments, "{\"location\":\"Paris\"}");
}
END_TEST

START_TEST(test_build_response_with_tool_call_null_args) {
    // Create streaming context
    ik_openai_responses_stream_ctx_t *sctx = ik_openai_responses_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    // Set up context with tool call but NULL arguments
    sctx->model = talloc_strdup(sctx, "gpt-4o");
    sctx->finish_reason = IK_FINISH_STOP;
    sctx->current_tool_id = talloc_strdup(sctx, "call_xyz789");
    sctx->current_tool_name = talloc_strdup(sctx, "list_files");
    sctx->current_tool_args = NULL; // NULL arguments

    // Build response
    ik_response_t *resp = ik_openai_responses_stream_build_response(test_ctx, sctx);

    // Verify response
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_TOOL_USE);

    // Verify content blocks - arguments should default to "{}"
    ck_assert_ptr_nonnull(resp->content_blocks);
    ck_assert_int_eq((int)resp->content_count, 1);
    ck_assert_int_eq(resp->content_blocks[0].type, IK_CONTENT_TOOL_CALL);
    ck_assert_str_eq(resp->content_blocks[0].data.tool_call.id, "call_xyz789");
    ck_assert_str_eq(resp->content_blocks[0].data.tool_call.name, "list_files");
    ck_assert_str_eq(resp->content_blocks[0].data.tool_call.arguments, "{}");
}
END_TEST

START_TEST(test_build_response_tool_call_missing_id) {
    // Create streaming context
    ik_openai_responses_stream_ctx_t *sctx = ik_openai_responses_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    // Set up context with tool name but no ID (incomplete tool call)
    sctx->finish_reason = IK_FINISH_STOP;
    sctx->current_tool_id = NULL; // Missing ID
    sctx->current_tool_name = talloc_strdup(sctx, "some_tool");
    sctx->current_tool_args = talloc_strdup(sctx, "{}");

    // Build response
    ik_response_t *resp = ik_openai_responses_stream_build_response(test_ctx, sctx);

    // Verify response - should have no content blocks (incomplete tool call)
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_STOP); // Not overridden
    ck_assert_ptr_null(resp->content_blocks);
    ck_assert_int_eq((int)resp->content_count, 0);
}
END_TEST

START_TEST(test_build_response_tool_call_missing_name) {
    // Create streaming context
    ik_openai_responses_stream_ctx_t *sctx = ik_openai_responses_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    // Set up context with tool ID but no name (incomplete tool call)
    sctx->finish_reason = IK_FINISH_STOP;
    sctx->current_tool_id = talloc_strdup(sctx, "call_123");
    sctx->current_tool_name = NULL; // Missing name
    sctx->current_tool_args = talloc_strdup(sctx, "{}");

    // Build response
    ik_response_t *resp = ik_openai_responses_stream_build_response(test_ctx, sctx);

    // Verify response - should have no content blocks (incomplete tool call)
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_STOP); // Not overridden
    ck_assert_ptr_null(resp->content_blocks);
    ck_assert_int_eq((int)resp->content_count, 0);
}
END_TEST

/* ================================================================
 * Test Suite
 * ================================================================ */

static Suite *streaming_responses_build_response_suite(void)
{
    Suite *s = suite_create("OpenAI Streaming Responses Build Response");

    TCase *tc_build = tcase_create("BuildResponse");
    tcase_set_timeout(tc_build, 30);
    tcase_add_checked_fixture(tc_build, setup, teardown);
    tcase_add_test(tc_build, test_build_response_no_tool_call_no_model);
    tcase_add_test(tc_build, test_build_response_no_tool_call_with_model);
    tcase_add_test(tc_build, test_build_response_with_tool_call_all_fields);
    tcase_add_test(tc_build, test_build_response_with_tool_call_null_args);
    tcase_add_test(tc_build, test_build_response_tool_call_missing_id);
    tcase_add_test(tc_build, test_build_response_tool_call_missing_name);
    suite_add_tcase(s, tc_build);

    return s;
}

int main(void)
{
    Suite *s = streaming_responses_build_response_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
