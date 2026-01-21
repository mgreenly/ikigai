#include "../../../test_constants.h"
/**
 * @file streaming_events_coverage_test_5.c
 * @brief Coverage tests for Anthropic streaming events - Part 5
 *
 * Tests additional edge cases for complete branch coverage:
 * - content_block_delta missing index, delta object, type
 * - content_block_stop missing index
 * - message_delta missing delta, stop_reason, usage fields
 * - error missing type, message
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

/* ================================================================
 * content_block_delta Tests - Index edge cases (line 134)
 * ================================================================ */

START_TEST(test_delta_index_not_int) {
    /* Test delta with index that is not an int - line 134 branch */
    const char *json = "{\"index\": \"not an int\", \"delta\": {\"type\": \"text_delta\", \"text\": \"test\"}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_content_block_delta(stream_ctx, yyjson_doc_get_root(doc));
    /* Should use default index 0 */
    ck_assert_int_eq(captured_events[0].index, 0);
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_delta_delta_not_object) {
    /* Test delta with delta field that is not an object - line 140 branch */
    const char *json = "{\"index\": 0, \"delta\": \"not an object\"}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_content_block_delta(stream_ctx, yyjson_doc_get_root(doc));
    /* Should return early, no events */
    ck_assert_int_eq((int)captured_count, 0);
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_delta_no_type) {
    /* Test delta without type field - line 146 branch */
    const char *json = "{\"index\": 0, \"delta\": {}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_content_block_delta(stream_ctx, yyjson_doc_get_root(doc));
    /* Should return early, no events */
    ck_assert_int_eq((int)captured_count, 0);
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_delta_type_not_string) {
    /* Test delta with type that is not a string - line 151 branch */
    const char *json = "{\"index\": 0, \"delta\": {\"type\": 12345}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_content_block_delta(stream_ctx, yyjson_doc_get_root(doc));
    /* Should return early, no events */
    ck_assert_int_eq((int)captured_count, 0);
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_delta_thinking_not_string) {
    /* Test thinking_delta with thinking field not a string - line 175 branch */
    const char *json = "{\"index\": 1, \"delta\": {\"type\": \"thinking_delta\", \"thinking\": 12345}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_content_block_delta(stream_ctx, yyjson_doc_get_root(doc));
    /* Should not emit event */
    ck_assert_int_eq((int)captured_count, 0);
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_delta_input_json_not_string) {
    /* Test input_json_delta with partial_json not a string - line 191 branch */
    const char *json = "{\"index\": 2, \"delta\": {\"type\": \"input_json_delta\", \"partial_json\": 12345}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_content_block_delta(stream_ctx, yyjson_doc_get_root(doc));
    /* Should not emit event */
    ck_assert_int_eq((int)captured_count, 0);
    yyjson_doc_free(doc);
}
END_TEST

/* ================================================================
 * content_block_stop Tests - Index edge cases (line 215)
 * ================================================================ */

START_TEST(test_stop_no_index) {
    /* Test stop without index field - line 215 branch */
    stream_ctx->current_block_type = IK_CONTENT_TOOL_CALL;
    const char *json = "{}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_content_block_stop(stream_ctx, yyjson_doc_get_root(doc));
    /* Should emit event with default index 0 */
    ck_assert_int_eq(captured_events[0].index, 0);
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_stop_index_not_int) {
    /* Test stop with index not int - line 215 branch */
    stream_ctx->current_block_type = IK_CONTENT_TOOL_CALL;
    const char *json = "{\"index\": \"not an int\"}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_content_block_stop(stream_ctx, yyjson_doc_get_root(doc));
    /* Should emit event with default index 0 */
    ck_assert_int_eq(captured_events[0].index, 0);
    yyjson_doc_free(doc);
}
END_TEST

/* ================================================================
 * message_delta Tests - Delta and usage edge cases
 * ================================================================ */

START_TEST(test_message_delta_no_delta) {
    /* Test message_delta without delta field - line 242 branch (NULL) */
    const char *json = "{\"usage\": {\"output_tokens\": 100}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_message_delta(stream_ctx, yyjson_doc_get_root(doc));
    /* Should still process usage */
    ck_assert_int_eq(stream_ctx->usage.output_tokens, 100);
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_message_delta_delta_not_object) {
    /* Test message_delta with delta not object - line 242 branch */
    const char *json = "{\"delta\": \"not an object\"}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_message_delta(stream_ctx, yyjson_doc_get_root(doc));
    /* Should skip delta processing */
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_message_delta_no_stop_reason) {
    /* Test message_delta without stop_reason - line 245 branch (NULL) */
    const char *json = "{\"delta\": {}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_message_delta(stream_ctx, yyjson_doc_get_root(doc));
    /* finish_reason should remain default */
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_message_delta_stop_reason_not_string) {
    /* Test message_delta with stop_reason not string - line 247 branch */
    const char *json = "{\"delta\": {\"stop_reason\": 12345}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_message_delta(stream_ctx, yyjson_doc_get_root(doc));
    /* finish_reason should remain default */
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_message_delta_usage_not_object) {
    /* Test message_delta with usage not object - line 255 branch */
    const char *json = "{\"usage\": \"not an object\"}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_message_delta(stream_ctx, yyjson_doc_get_root(doc));
    /* Should skip usage processing */
    ck_assert_int_eq(stream_ctx->usage.output_tokens, 0);
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_message_delta_no_output_tokens) {
    /* Test message_delta without output_tokens - line 258 branch (NULL) */
    const char *json = "{\"usage\": {\"thinking_tokens\": 50}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_message_delta(stream_ctx, yyjson_doc_get_root(doc));
    /* output_tokens should remain 0 */
    ck_assert_int_eq(stream_ctx->usage.output_tokens, 0);
    ck_assert_int_eq(stream_ctx->usage.thinking_tokens, 50);
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_message_delta_output_tokens_not_int) {
    /* Test message_delta with output_tokens not int - line 258 branch */
    const char *json = "{\"usage\": {\"output_tokens\": \"not an int\"}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_message_delta(stream_ctx, yyjson_doc_get_root(doc));
    /* output_tokens should remain 0 */
    ck_assert_int_eq(stream_ctx->usage.output_tokens, 0);
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_message_delta_no_thinking_tokens) {
    /* Test message_delta without thinking_tokens - line 264 branch (NULL) */
    const char *json = "{\"usage\": {\"output_tokens\": 100}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_message_delta(stream_ctx, yyjson_doc_get_root(doc));
    /* thinking_tokens should remain 0 */
    ck_assert_int_eq(stream_ctx->usage.thinking_tokens, 0);
    ck_assert_int_eq(stream_ctx->usage.output_tokens, 100);
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_message_delta_thinking_tokens_not_int) {
    /* Test message_delta with thinking_tokens not int - line 264 branch */
    const char *json = "{\"usage\": {\"output_tokens\": 100, \"thinking_tokens\": \"not an int\"}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_message_delta(stream_ctx, yyjson_doc_get_root(doc));
    /* thinking_tokens should remain 0 */
    ck_assert_int_eq(stream_ctx->usage.thinking_tokens, 0);
    ck_assert_int_eq(stream_ctx->usage.output_tokens, 100);
    yyjson_doc_free(doc);
}
END_TEST

/* ================================================================
 * error Tests - Type and message edge cases
 * ================================================================ */

START_TEST(test_error_no_type) {
    /* Test error without type field - line 321 branch (NULL) */
    const char *json = "{\"error\": {\"message\": \"Some error\"}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_error(stream_ctx, yyjson_doc_get_root(doc));
    /* Should default to UNKNOWN category */
    ck_assert_int_eq(captured_events[0].data.error.category, IK_ERR_CAT_UNKNOWN);
    ck_assert_str_eq(captured_events[0].data.error.message, "Some error");
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_error_type_not_string) {
    /* Test error with type not a string - line 322 branch (NULL from yyjson_get_str) */
    const char *json = "{\"error\": {\"type\": 12345, \"message\": \"Some error\"}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_error(stream_ctx, yyjson_doc_get_root(doc));
    /* Should default to UNKNOWN category */
    ck_assert_int_eq(captured_events[0].data.error.category, IK_ERR_CAT_UNKNOWN);
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_error_no_message) {
    /* Test error without message field - line 328 branch (NULL) */
    const char *json = "{\"error\": {\"type\": \"authentication_error\"}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_error(stream_ctx, yyjson_doc_get_root(doc));
    /* Should use default "Unknown error" message */
    ck_assert_str_eq(captured_events[0].data.error.message, "Unknown error");
    ck_assert_int_eq(captured_events[0].data.error.category, IK_ERR_CAT_AUTH);
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_error_message_not_string) {
    /* Test error with message not string - line 330 branch */
    const char *json = "{\"error\": {\"type\": \"rate_limit_error\", \"message\": 12345}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_error(stream_ctx, yyjson_doc_get_root(doc));
    /* Should use default "Unknown error" message */
    ck_assert_str_eq(captured_events[0].data.error.message, "Unknown error");
    ck_assert_int_eq(captured_events[0].data.error.category, IK_ERR_CAT_RATE_LIMIT);
    yyjson_doc_free(doc);
}
END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *streaming_events_coverage_suite_5(void)
{
    Suite *s = suite_create("Anthropic Streaming Events Coverage 5");

    TCase *tc_delta = tcase_create("content_block_delta Edge Cases");
    tcase_set_timeout(tc_delta, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_delta, setup, teardown);
    tcase_add_test(tc_delta, test_delta_index_not_int);
    tcase_add_test(tc_delta, test_delta_delta_not_object);
    tcase_add_test(tc_delta, test_delta_no_type);
    tcase_add_test(tc_delta, test_delta_type_not_string);
    tcase_add_test(tc_delta, test_delta_thinking_not_string);
    tcase_add_test(tc_delta, test_delta_input_json_not_string);
    suite_add_tcase(s, tc_delta);

    TCase *tc_stop = tcase_create("content_block_stop Edge Cases");
    tcase_set_timeout(tc_stop, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_stop, setup, teardown);
    tcase_add_test(tc_stop, test_stop_no_index);
    tcase_add_test(tc_stop, test_stop_index_not_int);
    suite_add_tcase(s, tc_stop);

    TCase *tc_msg_delta = tcase_create("message_delta Edge Cases");
    tcase_set_timeout(tc_msg_delta, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_msg_delta, setup, teardown);
    tcase_add_test(tc_msg_delta, test_message_delta_no_delta);
    tcase_add_test(tc_msg_delta, test_message_delta_delta_not_object);
    tcase_add_test(tc_msg_delta, test_message_delta_no_stop_reason);
    tcase_add_test(tc_msg_delta, test_message_delta_stop_reason_not_string);
    tcase_add_test(tc_msg_delta, test_message_delta_usage_not_object);
    tcase_add_test(tc_msg_delta, test_message_delta_no_output_tokens);
    tcase_add_test(tc_msg_delta, test_message_delta_output_tokens_not_int);
    tcase_add_test(tc_msg_delta, test_message_delta_no_thinking_tokens);
    tcase_add_test(tc_msg_delta, test_message_delta_thinking_tokens_not_int);
    suite_add_tcase(s, tc_msg_delta);

    TCase *tc_error = tcase_create("error Edge Cases");
    tcase_set_timeout(tc_error, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_error, setup, teardown);
    tcase_add_test(tc_error, test_error_no_type);
    tcase_add_test(tc_error, test_error_type_not_string);
    tcase_add_test(tc_error, test_error_no_message);
    tcase_add_test(tc_error, test_error_message_not_string);
    suite_add_tcase(s, tc_error);

    return s;
}

int main(void)
{
    Suite *s = streaming_events_coverage_suite_5();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/providers/anthropic/streaming_events_coverage_5_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
