/**
 * @file streaming_signature_test.c
 * @brief Tests for Anthropic streaming signature and redacted thinking capture
 *
 * Tests: signature_delta handling, thinking text accumulation, redacted_thinking blocks
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/anthropic/streaming_events.h"
#include "providers/anthropic/streaming.h"
#include "providers/provider.h"
#include "vendor/yyjson/yyjson.h"

static TALLOC_CTX *test_ctx;
static ik_anthropic_stream_ctx_t *stream_ctx;

#define MAX_EVENTS 16
static ik_stream_event_t captured_events[MAX_EVENTS];
static size_t captured_count;

static res_t test_stream_cb(const ik_stream_event_t *event, void *ctx)
{
    (void)ctx;
    if (captured_count < MAX_EVENTS) {
        captured_events[captured_count] = *event;
        captured_count++;
    }
    return OK(NULL);
}

static void setup(void)
{
    test_ctx = talloc_new(NULL);
    res_t r = ik_anthropic_stream_ctx_create(test_ctx, test_stream_cb, NULL, &stream_ctx);
    ck_assert(!is_err(&r));
    captured_count = 0;
    memset(captured_events, 0, sizeof(captured_events));
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

START_TEST(test_signature_delta_captured) {
    // Verify signature stored in context
    const char *json = "{\"index\": 0, \"delta\": {\"type\": \"signature_delta\", \"signature\": \"EqQBCgIYAhIM...\"}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_content_block_delta(stream_ctx, yyjson_doc_get_root(doc));

    // Signature should be stored in context
    ck_assert_ptr_nonnull(stream_ctx->current_thinking_signature);
    ck_assert_str_eq(stream_ctx->current_thinking_signature, "EqQBCgIYAhIM...");

    // No event should be emitted for signature_delta
    ck_assert_int_eq((int)captured_count, 0);

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_thinking_text_accumulated) {
    // Verify thinking text accumulated across multiple deltas
    const char *json1 = "{\"index\": 0, \"delta\": {\"type\": \"thinking_delta\", \"thinking\": \"Let me think\"}}";
    yyjson_doc *doc1 = yyjson_read(json1, strlen(json1), 0);
    ck_assert_ptr_nonnull(doc1);
    ik_anthropic_process_content_block_delta(stream_ctx, yyjson_doc_get_root(doc1));
    yyjson_doc_free(doc1);

    // First delta should be stored
    ck_assert_ptr_nonnull(stream_ctx->current_thinking_text);
    ck_assert_str_eq(stream_ctx->current_thinking_text, "Let me think");

    // Second delta should be accumulated
    const char *json2 = "{\"index\": 0, \"delta\": {\"type\": \"thinking_delta\", \"thinking\": \" about this.\"}}";
    yyjson_doc *doc2 = yyjson_read(json2, strlen(json2), 0);
    ck_assert_ptr_nonnull(doc2);
    ik_anthropic_process_content_block_delta(stream_ctx, yyjson_doc_get_root(doc2));
    yyjson_doc_free(doc2);

    // Text should be accumulated
    ck_assert_str_eq(stream_ctx->current_thinking_text, "Let me think about this.");

    // Events should still be emitted
    ck_assert_int_eq((int)captured_count, 2);
    ck_assert_int_eq(captured_events[0].type, IK_STREAM_THINKING_DELTA);
    ck_assert_int_eq(captured_events[1].type, IK_STREAM_THINKING_DELTA);
}
END_TEST

START_TEST(test_redacted_thinking_captured) {
    // Verify redacted data stored in context
    const char *json = "{\"index\": 1, \"content_block\": {\"type\": \"redacted_thinking\", \"data\": \"EmwKAhgBEgy...\"}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_content_block_start(stream_ctx, yyjson_doc_get_root(doc));

    // Block type should be set
    ck_assert_int_eq(stream_ctx->current_block_type, IK_CONTENT_REDACTED_THINKING);

    // Data should be stored
    ck_assert_ptr_nonnull(stream_ctx->current_redacted_data);
    ck_assert_str_eq(stream_ctx->current_redacted_data, "EmwKAhgBEgy...");

    // No event should be emitted
    ck_assert_int_eq((int)captured_count, 0);

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_signature_delta_no_field) {
    // Handle missing signature field gracefully
    const char *json = "{\"index\": 0, \"delta\": {\"type\": \"signature_delta\"}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_content_block_delta(stream_ctx, yyjson_doc_get_root(doc));

    // Should not crash, signature should remain NULL
    ck_assert_ptr_null(stream_ctx->current_thinking_signature);
    ck_assert_int_eq((int)captured_count, 0);

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_redacted_thinking_no_data) {
    // Handle missing data field gracefully
    const char *json = "{\"index\": 1, \"content_block\": {\"type\": \"redacted_thinking\"}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_content_block_start(stream_ctx, yyjson_doc_get_root(doc));

    // Block type should still be set
    ck_assert_int_eq(stream_ctx->current_block_type, IK_CONTENT_REDACTED_THINKING);

    // Data should remain NULL
    ck_assert_ptr_null(stream_ctx->current_redacted_data);

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_signature_delta_not_string) {
    // Handle non-string signature field
    const char *json = "{\"index\": 0, \"delta\": {\"type\": \"signature_delta\", \"signature\": 12345}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_content_block_delta(stream_ctx, yyjson_doc_get_root(doc));

    // Should not crash, signature should remain NULL
    ck_assert_ptr_null(stream_ctx->current_thinking_signature);
    ck_assert_int_eq((int)captured_count, 0);

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_redacted_thinking_data_not_string) {
    // Handle non-string data field
    const char *json = "{\"index\": 1, \"content_block\": {\"type\": \"redacted_thinking\", \"data\": 12345}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_content_block_start(stream_ctx, yyjson_doc_get_root(doc));

    // Block type should still be set
    ck_assert_int_eq(stream_ctx->current_block_type, IK_CONTENT_REDACTED_THINKING);

    // Data should remain NULL
    ck_assert_ptr_null(stream_ctx->current_redacted_data);

    yyjson_doc_free(doc);
}
END_TEST

static Suite *streaming_signature_suite(void)
{
    Suite *s = suite_create("Anthropic Streaming Signature");
    TCase *tc = tcase_create("Signature and Redacted Thinking");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_signature_delta_captured);
    tcase_add_test(tc, test_thinking_text_accumulated);
    tcase_add_test(tc, test_redacted_thinking_captured);
    tcase_add_test(tc, test_signature_delta_no_field);
    tcase_add_test(tc, test_redacted_thinking_no_data);
    tcase_add_test(tc, test_signature_delta_not_string);
    tcase_add_test(tc, test_redacted_thinking_data_not_string);
    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    Suite *s = streaming_signature_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
