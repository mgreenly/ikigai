#include "tests/test_constants.h"
/**
 * @file streaming_text_delta_test.c
 * @brief Test text_delta handling in Anthropic streaming
 *
 * Verifies that text_delta events are processed correctly and emit
 * IK_STREAM_TEXT_DELTA events with the proper content.
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "apps/ikigai/providers/anthropic/streaming.h"
#include "apps/ikigai/providers/provider.h"

static TALLOC_CTX *test_ctx;
static ik_anthropic_stream_ctx_t *stream_ctx;

/* Event capture */
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
        captured_events[captured_count] = *event;

        /* Copy text delta data */
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
 * Text Delta Tests
 * ================================================================ */

START_TEST(test_text_delta_basic) {
    /* Process a basic text_delta event */
    const char *event_json = "{"
                             "\"index\": 0,"
                             "\"delta\": {"
                             "\"type\": \"text_delta\","
                             "\"text\": \"Hello, world!\""
                             "}"
                             "}";

    ik_anthropic_stream_process_event(stream_ctx, "content_block_delta", event_json);

    /* Should emit IK_STREAM_TEXT_DELTA event */
    ck_assert_int_eq((int)captured_count, 1);
    ck_assert_int_eq(captured_events[0].type, IK_STREAM_TEXT_DELTA);
    ck_assert_int_eq(captured_events[0].index, 0);
    ck_assert_str_eq(captured_events[0].data.delta.text, "Hello, world!");
}
END_TEST

START_TEST(test_text_delta_with_index) {
    /* Process text_delta with non-zero index */
    const char *event_json = "{"
                             "\"index\": 2,"
                             "\"delta\": {"
                             "\"type\": \"text_delta\","
                             "\"text\": \"Content block 2\""
                             "}"
                             "}";

    ik_anthropic_stream_process_event(stream_ctx, "content_block_delta", event_json);

    /* Should preserve index */
    ck_assert_int_eq((int)captured_count, 1);
    ck_assert_int_eq(captured_events[0].type, IK_STREAM_TEXT_DELTA);
    ck_assert_int_eq(captured_events[0].index, 2);
    ck_assert_str_eq(captured_events[0].data.delta.text, "Content block 2");
}
END_TEST

START_TEST(test_text_delta_multiple_chunks) {
    /* Process multiple text_delta chunks */
    const char *chunk1 = "{"
                         "\"index\": 0,"
                         "\"delta\": {"
                         "\"type\": \"text_delta\","
                         "\"text\": \"First \""
                         "}"
                         "}";

    const char *chunk2 = "{"
                         "\"index\": 0,"
                         "\"delta\": {"
                         "\"type\": \"text_delta\","
                         "\"text\": \"chunk. \""
                         "}"
                         "}";

    const char *chunk3 = "{"
                         "\"index\": 0,"
                         "\"delta\": {"
                         "\"type\": \"text_delta\","
                         "\"text\": \"Last chunk.\""
                         "}"
                         "}";

    ik_anthropic_stream_process_event(stream_ctx, "content_block_delta", chunk1);
    ik_anthropic_stream_process_event(stream_ctx, "content_block_delta", chunk2);
    ik_anthropic_stream_process_event(stream_ctx, "content_block_delta", chunk3);

    /* Should emit three separate events */
    ck_assert_int_eq((int)captured_count, 3);

    ck_assert_int_eq(captured_events[0].type, IK_STREAM_TEXT_DELTA);
    ck_assert_str_eq(captured_events[0].data.delta.text, "First ");

    ck_assert_int_eq(captured_events[1].type, IK_STREAM_TEXT_DELTA);
    ck_assert_str_eq(captured_events[1].data.delta.text, "chunk. ");

    ck_assert_int_eq(captured_events[2].type, IK_STREAM_TEXT_DELTA);
    ck_assert_str_eq(captured_events[2].data.delta.text, "Last chunk.");
}
END_TEST

START_TEST(test_text_delta_empty_string) {
    /* Process text_delta with empty text */
    const char *event_json = "{"
                             "\"index\": 0,"
                             "\"delta\": {"
                             "\"type\": \"text_delta\","
                             "\"text\": \"\""
                             "}"
                             "}";

    ik_anthropic_stream_process_event(stream_ctx, "content_block_delta", event_json);

    /* Should still emit event with empty text */
    ck_assert_int_eq((int)captured_count, 1);
    ck_assert_int_eq(captured_events[0].type, IK_STREAM_TEXT_DELTA);
    ck_assert_str_eq(captured_events[0].data.delta.text, "");
}
END_TEST

START_TEST(test_text_delta_missing_text_field) {
    /* Process text_delta with missing text field */
    const char *event_json = "{"
                             "\"index\": 0,"
                             "\"delta\": {"
                             "\"type\": \"text_delta\""
                             "}"
                             "}";

    ik_anthropic_stream_process_event(stream_ctx, "content_block_delta", event_json);

    /* Should not emit event when text field is missing */
    ck_assert_int_eq((int)captured_count, 0);
}
END_TEST

START_TEST(test_text_delta_null_text_field) {
    /* Process text_delta with null text field */
    const char *event_json = "{"
                             "\"index\": 0,"
                             "\"delta\": {"
                             "\"type\": \"text_delta\","
                             "\"text\": null"
                             "}"
                             "}";

    ik_anthropic_stream_process_event(stream_ctx, "content_block_delta", event_json);

    /* Should not emit event when text field is null */
    ck_assert_int_eq((int)captured_count, 0);
}
END_TEST

START_TEST(test_text_delta_with_special_chars) {
    /* Process text_delta with special characters */
    const char *event_json = "{"
                             "\"index\": 0,"
                             "\"delta\": {"
                             "\"type\": \"text_delta\","
                             "\"text\": \"Line 1\\nLine 2\\tTabbed\\r\\nWindows EOL\""
                             "}"
                             "}";

    ik_anthropic_stream_process_event(stream_ctx, "content_block_delta", event_json);

    /* Should handle escaped characters */
    ck_assert_int_eq((int)captured_count, 1);
    ck_assert_int_eq(captured_events[0].type, IK_STREAM_TEXT_DELTA);
    ck_assert_str_eq(captured_events[0].data.delta.text, "Line 1\nLine 2\tTabbed\r\nWindows EOL");
}
END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *streaming_text_delta_suite(void)
{
    Suite *s = suite_create("Anthropic Streaming Text Delta");

    TCase *tc_text_delta = tcase_create("Text Delta Processing");
    tcase_set_timeout(tc_text_delta, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_text_delta, setup, teardown);
    tcase_add_test(tc_text_delta, test_text_delta_basic);
    tcase_add_test(tc_text_delta, test_text_delta_with_index);
    tcase_add_test(tc_text_delta, test_text_delta_multiple_chunks);
    tcase_add_test(tc_text_delta, test_text_delta_empty_string);
    tcase_add_test(tc_text_delta, test_text_delta_missing_text_field);
    tcase_add_test(tc_text_delta, test_text_delta_null_text_field);
    tcase_add_test(tc_text_delta, test_text_delta_with_special_chars);
    suite_add_tcase(s, tc_text_delta);

    return s;
}

int main(void)
{
    Suite *s = streaming_text_delta_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/anthropic/streaming_text_delta_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
