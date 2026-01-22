#include "../../../test_constants.h"
/**
 * @file streaming_response_builder_test.c
 * @brief Unit tests for Google streaming response builder
 *
 * Tests ik_google_stream_build_response():
 * - Empty context (no tool call)
 * - With model and usage
 * - With complete tool call
 * - With tool call but no args (defaults to "{}")
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/google/streaming.h"
#include "providers/google/streaming_internal.h"
#include "providers/provider.h"

static TALLOC_CTX *test_ctx;
static ik_google_stream_ctx_t *stream_ctx;

static res_t dummy_stream_cb(const ik_stream_event_t *event, void *ctx)
{
    (void)event;
    (void)ctx;
    return OK(NULL);
}

static void setup(void)
{
    test_ctx = talloc_new(NULL);
    res_t r = ik_google_stream_ctx_create(test_ctx, dummy_stream_cb, NULL, &stream_ctx);
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
    ik_response_t *resp = ik_google_stream_build_response(test_ctx, stream_ctx);

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
    // Process chunk with model
    const char *chunk =
        "{\"modelVersion\":\"gemini-2.5-flash-preview-05-20\",\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Hello\"}]}}]}";
    ik_google_stream_process_data(stream_ctx, chunk);

    ik_response_t *resp = ik_google_stream_build_response(test_ctx, stream_ctx);

    ck_assert_ptr_nonnull(resp);
    ck_assert_str_eq(resp->model, "gemini-2.5-flash-preview-05-20");
}

END_TEST

START_TEST(test_build_response_with_usage) {
    // Process chunks including usage metadata
    const char *start_chunk =
        "{\"modelVersion\":\"gemini-2.5-pro\",\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Hi\"}]}}]}";
    ik_google_stream_process_data(stream_ctx, start_chunk);

    const char *usage_chunk =
        "{\"usageMetadata\":{\"promptTokenCount\":100,\"candidatesTokenCount\":75,\"thoughtsTokenCount\":25,\"totalTokenCount\":175}}";
    ik_google_stream_process_data(stream_ctx, usage_chunk);

    ik_response_t *resp = ik_google_stream_build_response(test_ctx, stream_ctx);

    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq(resp->usage.input_tokens, 100);
    ck_assert_int_eq(resp->usage.output_tokens, 50); // candidates - thoughts
    ck_assert_int_eq(resp->usage.thinking_tokens, 25);
    ck_assert_int_eq(resp->usage.total_tokens, 175);
}

END_TEST

START_TEST(test_build_response_with_finish_reason) {
    // Process chunk with STOP finish reason
    const char *chunk =
        "{\"modelVersion\":\"gemini-2.5-flash\",\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Done\"}]},\"finishReason\":\"STOP\"}]}";
    ik_google_stream_process_data(stream_ctx, chunk);

    ik_response_t *resp = ik_google_stream_build_response(test_ctx, stream_ctx);

    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_STOP);
}

END_TEST

/* ================================================================
 * Tool Call Tests
 * ================================================================ */

START_TEST(test_build_response_with_tool_call) {
    // Process chunk with functionCall
    const char *chunk =
        "{\"modelVersion\":\"gemini-2.5-flash\",\"candidates\":[{\"content\":{\"parts\":[{\"functionCall\":{\"name\":\"glob\",\"args\":{\"pattern\":\"*.c\"}}}]}}]}";
    ik_google_stream_process_data(stream_ctx, chunk);

    ik_response_t *resp = ik_google_stream_build_response(test_ctx, stream_ctx);

    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 1);
    ck_assert_ptr_nonnull(resp->content_blocks);

    ik_content_block_t *block = &resp->content_blocks[0];
    ck_assert_int_eq(block->type, IK_CONTENT_TOOL_CALL);
    ck_assert_ptr_nonnull(block->data.tool_call.id);
    ck_assert_int_eq((int)strlen(block->data.tool_call.id), 22); // 22-char base64url UUID
    ck_assert_str_eq(block->data.tool_call.name, "glob");
    ck_assert_ptr_nonnull(strstr(block->data.tool_call.arguments, "pattern"));
    ck_assert_ptr_nonnull(strstr(block->data.tool_call.arguments, "*.c"));
}

END_TEST

START_TEST(test_build_response_tool_call_no_args) {
    // Process chunk with functionCall without args
    const char *chunk =
        "{\"modelVersion\":\"gemini-2.5-flash\",\"candidates\":[{\"content\":{\"parts\":[{\"functionCall\":{\"name\":\"file_read\"}}]}}]}";
    ik_google_stream_process_data(stream_ctx, chunk);

    ik_response_t *resp = ik_google_stream_build_response(test_ctx, stream_ctx);

    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 1);
    // When no args provided during streaming, args are initialized to ""
    // The response builder uses "{}" as fallback when args is NULL, but
    // here it's empty string, not NULL
    ck_assert_str_eq(resp->content_blocks[0].data.tool_call.arguments, "");
}

END_TEST

START_TEST(test_build_response_tool_call_preserved_after_text) {
    // Process tool call followed by text
    const char *tool_chunk =
        "{\"modelVersion\":\"gemini-2.5-flash\",\"candidates\":[{\"content\":{\"parts\":[{\"functionCall\":{\"name\":\"test\",\"args\":{}}}]}}]}";
    ik_google_stream_process_data(stream_ctx, tool_chunk);

    const char *text_chunk = "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Done\"}]}}]}";
    ik_google_stream_process_data(stream_ctx, text_chunk);

    // Tool data is preserved even after text ends the tool call state
    // This is needed so the response builder can create the tool call response
    ik_response_t *resp = ik_google_stream_build_response(test_ctx, stream_ctx);

    ck_assert_ptr_nonnull(resp);
    // Tool call data is preserved for response builder
    ck_assert_int_eq((int)resp->content_count, 1);
    ck_assert_ptr_nonnull(resp->content_blocks);
    ck_assert_str_eq(resp->content_blocks[0].data.tool_call.name, "test");
}

END_TEST

START_TEST(test_build_response_inconsistent_tool_state_name_null) {
    // Edge case: current_tool_id is set but current_tool_name is NULL
    // This tests the false branch of the second part of the AND condition
    stream_ctx->current_tool_id = talloc_strdup(stream_ctx, "tool_id_abc");
    stream_ctx->current_tool_name = NULL;
    stream_ctx->current_tool_args = talloc_strdup(stream_ctx, "{}");

    ik_response_t *resp = ik_google_stream_build_response(test_ctx, stream_ctx);

    // Should take the else branch (no tool call) because condition requires BOTH to be non-NULL
    ck_assert_ptr_nonnull(resp);
    ck_assert_ptr_null(resp->content_blocks);
    ck_assert_int_eq((int)resp->content_count, 0);
}

END_TEST

START_TEST(test_build_response_tool_call_null_args) {
    // Edge case: tool call with NULL args (should use "{}" default)
    // This tests line 59 branch 1 (the uncovered ternary false branch)
    stream_ctx->current_tool_id = talloc_strdup(stream_ctx, "test_id_123");
    stream_ctx->current_tool_name = talloc_strdup(stream_ctx, "test_tool");
    stream_ctx->current_tool_args = NULL; // Explicitly NULL to trigger "{}" fallback

    ik_response_t *resp = ik_google_stream_build_response(test_ctx, stream_ctx);

    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 1);
    ck_assert_ptr_nonnull(resp->content_blocks);
    ck_assert_str_eq(resp->content_blocks[0].data.tool_call.name, "test_tool");
    ck_assert_str_eq(resp->content_blocks[0].data.tool_call.arguments, "{}");
}

END_TEST

/* ================================================================
 * Complete Response Tests
 * ================================================================ */

START_TEST(test_build_response_full_context) {
    // Process a complete streaming context with tool call and usage
    const char *tool_chunk =
        "{\"modelVersion\":\"gemini-2.5-pro-preview-06-05\",\"candidates\":[{\"content\":{\"parts\":[{\"functionCall\":{\"name\":\"file_write\",\"args\":{\"path\":\"/tmp/test.txt\",\"content\":\"hello\"}}}]},\"finishReason\":\"STOP\"}]}";
    ik_google_stream_process_data(stream_ctx, tool_chunk);

    // Usage metadata ends the tool call but preserves tool data for response builder
    const char *usage_chunk =
        "{\"usageMetadata\":{\"promptTokenCount\":500,\"candidatesTokenCount\":200,\"totalTokenCount\":700}}";
    ik_google_stream_process_data(stream_ctx, usage_chunk);

    ik_response_t *resp = ik_google_stream_build_response(test_ctx, stream_ctx);

    // Verify all fields
    ck_assert_ptr_nonnull(resp);
    ck_assert_str_eq(resp->model, "gemini-2.5-pro-preview-06-05");
    // When there's a tool call, finish_reason is overridden to IK_FINISH_TOOL_USE
    // so the tool loop continues (Google returns "STOP" even for tool calls)
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_TOOL_USE);
    ck_assert_int_eq(resp->usage.input_tokens, 500);
    ck_assert_int_eq(resp->usage.output_tokens, 200);
    ck_assert_int_eq(resp->usage.total_tokens, 700);
    ck_assert_int_eq((int)resp->content_count, 1);
    ck_assert_int_eq(resp->content_blocks[0].type, IK_CONTENT_TOOL_CALL);
    ck_assert_ptr_nonnull(resp->content_blocks[0].data.tool_call.id);
    ck_assert_str_eq(resp->content_blocks[0].data.tool_call.name, "file_write");
    ck_assert_ptr_nonnull(strstr(resp->content_blocks[0].data.tool_call.arguments, "path"));
    ck_assert_ptr_nonnull(strstr(resp->content_blocks[0].data.tool_call.arguments, "/tmp/test.txt"));
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *streaming_response_builder_suite(void)
{
    Suite *s = suite_create("Google Streaming Response Builder");

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
    tcase_add_test(tc_tool, test_build_response_tool_call_preserved_after_text);
    tcase_add_test(tc_tool, test_build_response_inconsistent_tool_state_name_null);
    tcase_add_test(tc_tool, test_build_response_tool_call_null_args);
    suite_add_tcase(s, tc_tool);

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
    srunner_set_xml(sr, "reports/check/unit/providers/google/streaming_response_builder_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
