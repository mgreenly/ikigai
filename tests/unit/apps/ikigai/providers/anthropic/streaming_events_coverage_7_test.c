#include "tests/test_constants.h"
/**
 * @file streaming_events_coverage_test_7.c
 * @brief Coverage tests for Anthropic streaming events - Part 7
 *
 * Tests final edge cases for 100% branch coverage:
 * - Ensure all branch combinations are tested
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
 * Branch coverage completion tests
 * ================================================================ */

START_TEST(test_delta_with_null_index) {
    /* Ensure we hit the NULL branch for index_val in content_block_delta (line 134) */
    const char *json = "{\"delta\": {\"type\": \"text_delta\", \"text\": \"test\"}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_content_block_delta(stream_ctx, yyjson_doc_get_root(doc));
    /* Should emit event with default index 0 */
    ck_assert_int_eq(captured_events[0].index, 0);
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_error_with_valid_type_val) {
    /* Ensure we hit the non-NULL branch for type_val (line 321) */
    const char *json = "{\"error\": {\"type\": \"some_error\"}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_error(stream_ctx, yyjson_doc_get_root(doc));
    /* Should process error */
    ck_assert_int_eq((int)captured_count, 1);
    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_error_with_valid_msg_val) {
    /* Ensure we hit the non-NULL branch for msg_val (line 328) */
    const char *json = "{\"error\": {\"message\": \"test error\"}}";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    ik_anthropic_process_error(stream_ctx, yyjson_doc_get_root(doc));
    /* Should process error with message */
    ck_assert_str_eq(captured_events[0].data.error.message, "test error");
    yyjson_doc_free(doc);
}
END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *streaming_events_coverage_suite_7(void)
{
    Suite *s = suite_create("Anthropic Streaming Events Coverage 7");

    TCase *tc = tcase_create("Final Branch Coverage");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_delta_with_null_index);
    tcase_add_test(tc, test_error_with_valid_type_val);
    tcase_add_test(tc, test_error_with_valid_msg_val);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    Suite *s = streaming_events_coverage_suite_7();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/anthropic/streaming_events_coverage_7_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? 0 : 1;
}
