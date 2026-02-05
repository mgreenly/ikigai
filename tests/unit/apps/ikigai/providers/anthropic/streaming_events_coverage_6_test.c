#include "tests/test_constants.h"
/**
 * @file streaming_events_coverage_test_6.c
 * @brief Coverage tests for Anthropic streaming events - Part 6
 *
 * Tests remaining edge cases for 100% branch coverage:
 * - Unknown content block types
 * - Unknown delta types
 * - Error object edge cases
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "apps/ikigai/providers/anthropic/streaming_events.h"
#include "apps/ikigai/providers/anthropic/streaming.h"
#include "apps/ikigai/providers/provider.h"
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
 * content_block_start - Unknown block type
 * ================================================================ */

START_TEST(test_content_block_start_unknown_type) {
    /* Test content_block_start with unknown type - should not match any strcmp */
    const char *json = "{\"index\": 0, \"content_block\": {\"type\": \"unknown_type\"}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_content_block_start(stream_ctx, yyjson_doc_get_root(doc));
    /* Should not emit any events for unknown type */
    ck_assert_int_eq((int)captured_count, 0);
    yyjson_doc_free(doc);
}
END_TEST

/* ================================================================
 * content_block_delta - Unknown delta type
 * ================================================================ */

START_TEST(test_content_block_delta_unknown_type) {
    /* Test content_block_delta with unknown type - should not match any strcmp */
    const char *json = "{\"index\": 0, \"delta\": {\"type\": \"unknown_delta_type\"}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_content_block_delta(stream_ctx, yyjson_doc_get_root(doc));
    /* Should not emit any events for unknown type */
    ck_assert_int_eq((int)captured_count, 0);
    yyjson_doc_free(doc);
}
END_TEST

/* ================================================================
 * error - Error object not object edge case
 * ================================================================ */

START_TEST(test_error_error_not_object) {
    /* Test error with error field that is not an object - line 306 branch */
    const char *json = "{\"error\": \"not an object\"}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_error(stream_ctx, yyjson_doc_get_root(doc));
    /* Should emit generic error */
    ck_assert_int_eq(captured_events[0].data.error.category, IK_ERR_CAT_UNKNOWN);
    ck_assert_str_eq(captured_events[0].data.error.message, "Unknown error");
    yyjson_doc_free(doc);
}
END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *streaming_events_coverage_suite_6(void)
{
    Suite *s = suite_create("Anthropic Streaming Events Coverage 6");

    TCase *tc = tcase_create("Remaining Edge Cases");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_content_block_start_unknown_type);
    tcase_add_test(tc, test_content_block_delta_unknown_type);
    tcase_add_test(tc, test_error_error_not_object);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    Suite *s = streaming_events_coverage_suite_6();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/anthropic/streaming_events_coverage_6_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
