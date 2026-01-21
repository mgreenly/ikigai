#include "../../../test_constants.h"
/**
 * @file streaming_response_builder_test.c
 * @brief Unit tests for Anthropic streaming response builder
 *
 * Tests ik_anthropic_stream_build_response():
 * - Empty context (no tool call)
 * - With model and usage
 * - With complete tool call
 * - With tool call but no args (defaults to "{}")
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/anthropic/streaming.h"
#include "providers/provider.h"

static TALLOC_CTX *test_ctx;
static ik_anthropic_stream_ctx_t *stream_ctx;

static res_t dummy_stream_cb(const ik_stream_event_t *event, void *ctx)
{
    (void)event;
    (void)ctx;
    return OK(NULL);
}

static void setup(void)
{
    test_ctx = talloc_new(NULL);
    res_t r = ik_anthropic_stream_ctx_create(test_ctx, dummy_stream_cb, NULL, &stream_ctx);
    ck_assert(!is_err(&r));
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

/* ================================================================
 * Empty Context Tests
 * ================================================================ */

START_TEST(test_build_response_empty_context) {
    // Fresh context with no data
    ik_response_t *resp = ik_anthropic_stream_build_response(test_ctx, stream_ctx);

    ck_assert_ptr_nonnull(resp);
    ck_assert_ptr_null(resp->model);
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_UNKNOWN);
    ck_assert_int_eq(resp->usage.input_tokens, 0);
    ck_assert_int_eq(resp->usage.output_tokens, 0);
    ck_assert_ptr_null(resp->content_blocks);
    ck_assert_int_eq((int)resp->content_count, 0);
}

END_TEST

/* ================================================================
 * Model and Usage Tests
 * ================================================================ */

START_TEST(test_build_response_with_model) {
    // Set model
    stream_ctx->model = talloc_strdup(stream_ctx, "claude-sonnet-4-20250514");

    ik_response_t *resp = ik_anthropic_stream_build_response(test_ctx, stream_ctx);

    ck_assert_ptr_nonnull(resp);
    ck_assert_str_eq(resp->model, "claude-sonnet-4-20250514");
    // Model should be a copy, not the same pointer
    ck_assert_ptr_ne(resp->model, stream_ctx->model);
}

END_TEST

START_TEST(test_build_response_with_usage) {
    // Set usage
    stream_ctx->usage.input_tokens = 100;
    stream_ctx->usage.output_tokens = 50;
    stream_ctx->usage.thinking_tokens = 25;
    stream_ctx->usage.cached_tokens = 10;
    stream_ctx->usage.total_tokens = 175;

    ik_response_t *resp = ik_anthropic_stream_build_response(test_ctx, stream_ctx);

    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq(resp->usage.input_tokens, 100);
    ck_assert_int_eq(resp->usage.output_tokens, 50);
    ck_assert_int_eq(resp->usage.thinking_tokens, 25);
    ck_assert_int_eq(resp->usage.cached_tokens, 10);
    ck_assert_int_eq(resp->usage.total_tokens, 175);
}

END_TEST

START_TEST(test_build_response_with_finish_reason) {
    stream_ctx->finish_reason = IK_FINISH_TOOL_USE;

    ik_response_t *resp = ik_anthropic_stream_build_response(test_ctx, stream_ctx);

    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_TOOL_USE);
}

END_TEST

/* ================================================================
 * Tool Call Tests
 * ================================================================ */

START_TEST(test_build_response_with_tool_call) {
    // Set up complete tool call data
    stream_ctx->current_tool_id = talloc_strdup(stream_ctx, "toolu_01A2B3C4");
    stream_ctx->current_tool_name = talloc_strdup(stream_ctx, "glob");
    stream_ctx->current_tool_args = talloc_strdup(stream_ctx, "{\"pattern\":\"*.c\"}");
    stream_ctx->finish_reason = IK_FINISH_TOOL_USE;

    ik_response_t *resp = ik_anthropic_stream_build_response(test_ctx, stream_ctx);

    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 1);
    ck_assert_ptr_nonnull(resp->content_blocks);

    ik_content_block_t *block = &resp->content_blocks[0];
    ck_assert_int_eq(block->type, IK_CONTENT_TOOL_CALL);
    ck_assert_str_eq(block->data.tool_call.id, "toolu_01A2B3C4");
    ck_assert_str_eq(block->data.tool_call.name, "glob");
    ck_assert_str_eq(block->data.tool_call.arguments, "{\"pattern\":\"*.c\"}");
}

END_TEST

START_TEST(test_build_response_tool_call_no_args) {
    // Tool call without arguments - should default to "{}"
    stream_ctx->current_tool_id = talloc_strdup(stream_ctx, "toolu_123");
    stream_ctx->current_tool_name = talloc_strdup(stream_ctx, "file_read");
    stream_ctx->current_tool_args = NULL;

    ik_response_t *resp = ik_anthropic_stream_build_response(test_ctx, stream_ctx);

    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 1);
    ck_assert_str_eq(resp->content_blocks[0].data.tool_call.arguments, "{}");
}

END_TEST

START_TEST(test_build_response_partial_tool_call_id_only) {
    // Only ID set, no name - should not create tool call block
    stream_ctx->current_tool_id = talloc_strdup(stream_ctx, "toolu_456");
    stream_ctx->current_tool_name = NULL;

    ik_response_t *resp = ik_anthropic_stream_build_response(test_ctx, stream_ctx);

    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 0);
    ck_assert_ptr_null(resp->content_blocks);
}

END_TEST

START_TEST(test_build_response_partial_tool_call_name_only) {
    // Only name set, no ID - should not create tool call block
    stream_ctx->current_tool_id = NULL;
    stream_ctx->current_tool_name = talloc_strdup(stream_ctx, "bash");

    ik_response_t *resp = ik_anthropic_stream_build_response(test_ctx, stream_ctx);

    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 0);
    ck_assert_ptr_null(resp->content_blocks);
}

END_TEST

/* ================================================================
 * Thinking Block Tests
 * ================================================================ */

START_TEST(test_response_with_thinking_block) {
    // Set up thinking block with signature
    stream_ctx->current_thinking_text = talloc_strdup(stream_ctx, "Let me analyze this carefully...");
    stream_ctx->current_thinking_signature = talloc_strdup(stream_ctx, "EqQBCgIYAhIM...");
    stream_ctx->finish_reason = IK_FINISH_STOP;

    ik_response_t *resp = ik_anthropic_stream_build_response(test_ctx, stream_ctx);

    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 1);
    ck_assert_ptr_nonnull(resp->content_blocks);

    ik_content_block_t *block = &resp->content_blocks[0];
    ck_assert_int_eq(block->type, IK_CONTENT_THINKING);
    ck_assert_str_eq(block->data.thinking.text, "Let me analyze this carefully...");
    ck_assert_str_eq(block->data.thinking.signature, "EqQBCgIYAhIM...");
}

END_TEST

START_TEST(test_response_with_thinking_no_signature) {
    // Thinking block without signature (edge case)
    stream_ctx->current_thinking_text = talloc_strdup(stream_ctx, "Some thinking...");
    stream_ctx->current_thinking_signature = NULL;

    ik_response_t *resp = ik_anthropic_stream_build_response(test_ctx, stream_ctx);

    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 1);

    ik_content_block_t *block = &resp->content_blocks[0];
    ck_assert_int_eq(block->type, IK_CONTENT_THINKING);
    ck_assert_str_eq(block->data.thinking.text, "Some thinking...");
    ck_assert_ptr_null(block->data.thinking.signature);
}

END_TEST

START_TEST(test_response_with_redacted_thinking) {
    // Set up redacted thinking block
    stream_ctx->current_redacted_data = talloc_strdup(stream_ctx, "EmwKAhgBEgy...");
    stream_ctx->finish_reason = IK_FINISH_STOP;

    ik_response_t *resp = ik_anthropic_stream_build_response(test_ctx, stream_ctx);

    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 1);
    ck_assert_ptr_nonnull(resp->content_blocks);

    ik_content_block_t *block = &resp->content_blocks[0];
    ck_assert_int_eq(block->type, IK_CONTENT_REDACTED_THINKING);
    ck_assert_str_eq(block->data.redacted_thinking.data, "EmwKAhgBEgy...");
}

END_TEST

START_TEST(test_response_thinking_and_tool_call) {
    // Set up thinking block with signature
    stream_ctx->current_thinking_text = talloc_strdup(stream_ctx, "I should use a tool...");
    stream_ctx->current_thinking_signature = talloc_strdup(stream_ctx, "SigABC123...");

    // Set up tool call
    stream_ctx->current_tool_id = talloc_strdup(stream_ctx, "toolu_xyz");
    stream_ctx->current_tool_name = talloc_strdup(stream_ctx, "file_read");
    stream_ctx->current_tool_args = talloc_strdup(stream_ctx, "{\"path\":\"/tmp/test\"}");
    stream_ctx->finish_reason = IK_FINISH_TOOL_USE;

    ik_response_t *resp = ik_anthropic_stream_build_response(test_ctx, stream_ctx);

    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 2);
    ck_assert_ptr_nonnull(resp->content_blocks);

    // Thinking block should come first (Anthropic ordering)
    ik_content_block_t *block0 = &resp->content_blocks[0];
    ck_assert_int_eq(block0->type, IK_CONTENT_THINKING);
    ck_assert_str_eq(block0->data.thinking.text, "I should use a tool...");
    ck_assert_str_eq(block0->data.thinking.signature, "SigABC123...");

    // Tool call block should come second
    ik_content_block_t *block1 = &resp->content_blocks[1];
    ck_assert_int_eq(block1->type, IK_CONTENT_TOOL_CALL);
    ck_assert_str_eq(block1->data.tool_call.id, "toolu_xyz");
    ck_assert_str_eq(block1->data.tool_call.name, "file_read");
    ck_assert_str_eq(block1->data.tool_call.arguments, "{\"path\":\"/tmp/test\"}");
}

END_TEST

START_TEST(test_response_only_tool_call) {
    // Only tool call, no thinking
    stream_ctx->current_tool_id = talloc_strdup(stream_ctx, "toolu_solo");
    stream_ctx->current_tool_name = talloc_strdup(stream_ctx, "bash");
    stream_ctx->current_tool_args = talloc_strdup(stream_ctx, "{\"cmd\":\"ls\"}");
    stream_ctx->finish_reason = IK_FINISH_TOOL_USE;

    ik_response_t *resp = ik_anthropic_stream_build_response(test_ctx, stream_ctx);

    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 1);
    ck_assert_ptr_nonnull(resp->content_blocks);

    ik_content_block_t *block = &resp->content_blocks[0];
    ck_assert_int_eq(block->type, IK_CONTENT_TOOL_CALL);
    ck_assert_str_eq(block->data.tool_call.id, "toolu_solo");
    ck_assert_str_eq(block->data.tool_call.name, "bash");
    ck_assert_str_eq(block->data.tool_call.arguments, "{\"cmd\":\"ls\"}");
}

END_TEST

/* ================================================================
 * Complete Response Tests
 * ================================================================ */

START_TEST(test_build_response_full_context) {
    // Set up a complete streaming context
    stream_ctx->model = talloc_strdup(stream_ctx, "claude-opus-4-20250514");
    stream_ctx->finish_reason = IK_FINISH_TOOL_USE;
    stream_ctx->usage.input_tokens = 500;
    stream_ctx->usage.output_tokens = 200;
    stream_ctx->usage.total_tokens = 700;
    stream_ctx->current_tool_id = talloc_strdup(stream_ctx, "toolu_abc123");
    stream_ctx->current_tool_name = talloc_strdup(stream_ctx, "file_write");
    stream_ctx->current_tool_args = talloc_strdup(stream_ctx, "{\"path\":\"/tmp/test.txt\",\"content\":\"hello\"}");

    ik_response_t *resp = ik_anthropic_stream_build_response(test_ctx, stream_ctx);

    // Verify all fields
    ck_assert_ptr_nonnull(resp);
    ck_assert_str_eq(resp->model, "claude-opus-4-20250514");
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_TOOL_USE);
    ck_assert_int_eq(resp->usage.input_tokens, 500);
    ck_assert_int_eq(resp->usage.output_tokens, 200);
    ck_assert_int_eq(resp->usage.total_tokens, 700);
    ck_assert_int_eq((int)resp->content_count, 1);
    ck_assert_int_eq(resp->content_blocks[0].type, IK_CONTENT_TOOL_CALL);
    ck_assert_str_eq(resp->content_blocks[0].data.tool_call.id, "toolu_abc123");
    ck_assert_str_eq(resp->content_blocks[0].data.tool_call.name, "file_write");
    ck_assert_str_eq(resp->content_blocks[0].data.tool_call.arguments,
                     "{\"path\":\"/tmp/test.txt\",\"content\":\"hello\"}");
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *streaming_response_builder_suite(void)
{
    Suite *s = suite_create("Anthropic Streaming Response Builder");

    TCase *tc_empty = tcase_create("Empty Context");
    tcase_set_timeout(tc_empty, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_empty, setup, teardown);
    tcase_add_test(tc_empty, test_build_response_empty_context);
    suite_add_tcase(s, tc_empty);

    TCase *tc_model = tcase_create("Model and Usage");
    tcase_set_timeout(tc_model, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_model, setup, teardown);
    tcase_add_test(tc_model, test_build_response_with_model);
    tcase_add_test(tc_model, test_build_response_with_usage);
    tcase_add_test(tc_model, test_build_response_with_finish_reason);
    suite_add_tcase(s, tc_model);

    TCase *tc_tool = tcase_create("Tool Call");
    tcase_set_timeout(tc_tool, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_tool, setup, teardown);
    tcase_add_test(tc_tool, test_build_response_with_tool_call);
    tcase_add_test(tc_tool, test_build_response_tool_call_no_args);
    tcase_add_test(tc_tool, test_build_response_partial_tool_call_id_only);
    tcase_add_test(tc_tool, test_build_response_partial_tool_call_name_only);
    suite_add_tcase(s, tc_tool);

    TCase *tc_thinking = tcase_create("Thinking Blocks");
    tcase_set_timeout(tc_thinking, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_thinking, setup, teardown);
    tcase_add_test(tc_thinking, test_response_with_thinking_block);
    tcase_add_test(tc_thinking, test_response_with_thinking_no_signature);
    tcase_add_test(tc_thinking, test_response_with_redacted_thinking);
    tcase_add_test(tc_thinking, test_response_thinking_and_tool_call);
    tcase_add_test(tc_thinking, test_response_only_tool_call);
    suite_add_tcase(s, tc_thinking);

    TCase *tc_full = tcase_create("Complete Response");
    tcase_set_timeout(tc_full, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_full, setup, teardown);
    tcase_add_test(tc_full, test_build_response_full_context);
    suite_add_tcase(s, tc_full);

    return s;
}

int main(void)
{
    Suite *s = streaming_response_builder_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/providers/anthropic/streaming_response_builder_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
