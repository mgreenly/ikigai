/**
 * @file streaming_events_coverage_test_2.c
 * @brief Coverage tests for Anthropic streaming events processors (part 2)
 *
 * Tests edge cases in content_block_start and content_block_delta:
 * - missing/invalid index field
 * - text_delta processing
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

/* Test event capture */
#define MAX_EVENTS 16
static ik_stream_event_t captured_events[MAX_EVENTS];
static size_t captured_count;

/* ================================================================
 * Callbacks
 * ================================================================ */

static res_t test_stream_cb(const ik_stream_event_t *event, void *ctx)
{
    (void)ctx;

    if (captured_count < MAX_EVENTS) {
        /* Deep copy event */
        captured_events[captured_count] = *event;

        /* Copy string data if present */
        if (event->type == IK_STREAM_TEXT_DELTA && event->data.delta.text) {
            captured_events[captured_count].data.delta.text =
                talloc_strdup(test_ctx, event->data.delta.text);
        }

        captured_count++;
    }

    return OK(NULL);
}

/* ================================================================
 * Fixtures
 * ================================================================ */

static void setup(void)
{
    test_ctx = talloc_new(NULL);

    /* Create streaming context */
    res_t r = ik_anthropic_stream_ctx_create(test_ctx, test_stream_cb, NULL, &stream_ctx);
    ck_assert(!is_err(&r));

    /* Reset event capture */
    captured_count = 0;
    memset(captured_events, 0, sizeof(captured_events));
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

/* ================================================================
 * content_block_start Tests - Line 61 branches
 * ================================================================ */

START_TEST(test_content_block_start_no_index_field)
{
    /* Test content_block_start without "index" field - line 61 branch (NULL) */
    const char *json = "{\"content_block\": {\"type\": \"text\"}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    /* Set initial index to non-zero to verify it's not updated */
    stream_ctx->current_block_index = 5;

    ik_anthropic_process_content_block_start(stream_ctx, yyjson_doc_get_root(doc));

    /* Index should remain unchanged when field is missing */
    ck_assert_int_eq(stream_ctx->current_block_index, 5);
    ck_assert_int_eq(stream_ctx->current_block_type, IK_CONTENT_TEXT);

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_content_block_start_index_not_int)
{
    /* Test content_block_start with "index" that is not an int - line 61 branch (!yyjson_is_int) */
    const char *json = "{\"index\": \"not an int\", \"content_block\": {\"type\": \"text\"}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    /* Set initial index to non-zero to verify it's not updated */
    stream_ctx->current_block_index = 7;

    ik_anthropic_process_content_block_start(stream_ctx, yyjson_doc_get_root(doc));

    /* Index should remain unchanged when field is not int */
    ck_assert_int_eq(stream_ctx->current_block_index, 7);
    ck_assert_int_eq(stream_ctx->current_block_type, IK_CONTENT_TEXT);

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_content_block_start_no_content_block)
{
    /* Test content_block_start without "content_block" field - line 67 branch (NULL) */
    const char *json = "{\"index\": 0}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    ik_anthropic_process_content_block_start(stream_ctx, yyjson_doc_get_root(doc));

    /* Should return early without changing block type */
    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_content_block_start_content_block_not_object)
{
    /* Test content_block_start with "content_block" not an object - line 67 branch (!yyjson_is_obj) */
    const char *json = "{\"index\": 0, \"content_block\": \"not an object\"}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    ik_anthropic_process_content_block_start(stream_ctx, yyjson_doc_get_root(doc));

    /* Should return early without changing block type */
    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_content_block_start_no_type_field)
{
    /* Test content_block_start without "type" field - line 73 branch (NULL) */
    const char *json = "{\"index\": 0, \"content_block\": {}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    ik_anthropic_process_content_block_start(stream_ctx, yyjson_doc_get_root(doc));

    /* Should return early without changing block type */
    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_content_block_start_type_not_string)
{
    /* Test content_block_start with "type" not a string - line 78 branch (NULL) */
    const char *json = "{\"index\": 0, \"content_block\": {\"type\": 12345}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    ik_anthropic_process_content_block_start(stream_ctx, yyjson_doc_get_root(doc));

    /* Should return early without changing block type */
    yyjson_doc_free(doc);
}

END_TEST

/* ================================================================
 * content_block_delta Tests - Lines 154-165 (text_delta)
 * ================================================================ */

START_TEST(test_content_block_delta_text_delta)
{
    /* Test content_block_delta with text_delta type - lines 156-170 */
    const char *json = "{\"index\": 0, \"delta\": {\"type\": \"text_delta\", \"text\": \"Hello world\"}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    ik_anthropic_process_content_block_delta(stream_ctx, yyjson_doc_get_root(doc));

    /* Should emit IK_STREAM_TEXT_DELTA event */
    ck_assert_int_eq((int)captured_count, 1);
    ck_assert_int_eq(captured_events[0].type, IK_STREAM_TEXT_DELTA);
    ck_assert_int_eq(captured_events[0].index, 0);
    ck_assert_str_eq(captured_events[0].data.delta.text, "Hello world");

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_content_block_delta_text_delta_no_text_field)
{
    /* Test content_block_delta text_delta without "text" field - line 159 branch (NULL) */
    const char *json = "{\"index\": 0, \"delta\": {\"type\": \"text_delta\"}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    ik_anthropic_process_content_block_delta(stream_ctx, yyjson_doc_get_root(doc));

    /* Should not emit any event */
    ck_assert_int_eq((int)captured_count, 0);

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_content_block_delta_text_delta_text_not_string)
{
    /* Test content_block_delta text_delta with "text" that is not a string - line 160 branch (NULL) */
    const char *json = "{\"index\": 0, \"delta\": {\"type\": \"text_delta\", \"text\": 12345}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    ik_anthropic_process_content_block_delta(stream_ctx, yyjson_doc_get_root(doc));

    /* Should not emit any event */
    ck_assert_int_eq((int)captured_count, 0);

    yyjson_doc_free(doc);
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *streaming_events_coverage_suite_2(void)
{
    Suite *s = suite_create("Anthropic Streaming Events Coverage 2");

    TCase *tc_content_block_start = tcase_create("content_block_start Edge Cases");
    tcase_add_checked_fixture(tc_content_block_start, setup, teardown);
    tcase_add_test(tc_content_block_start, test_content_block_start_no_index_field);
    tcase_add_test(tc_content_block_start, test_content_block_start_index_not_int);
    tcase_add_test(tc_content_block_start, test_content_block_start_no_content_block);
    tcase_add_test(tc_content_block_start, test_content_block_start_content_block_not_object);
    tcase_add_test(tc_content_block_start, test_content_block_start_no_type_field);
    tcase_add_test(tc_content_block_start, test_content_block_start_type_not_string);
    suite_add_tcase(s, tc_content_block_start);

    TCase *tc_content_block_delta = tcase_create("content_block_delta Edge Cases");
    tcase_add_checked_fixture(tc_content_block_delta, setup, teardown);
    tcase_add_test(tc_content_block_delta, test_content_block_delta_text_delta);
    tcase_add_test(tc_content_block_delta, test_content_block_delta_text_delta_no_text_field);
    tcase_add_test(tc_content_block_delta, test_content_block_delta_text_delta_text_not_string);
    suite_add_tcase(s, tc_content_block_delta);

    return s;
}

int main(void)
{
    Suite *s = streaming_events_coverage_suite_2();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
