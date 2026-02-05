#include "tests/test_constants.h"
/**
 * @file google_streaming_parts_coverage_test.c
 * @brief Branch coverage tests for Google streaming - Parts processing
 *
 * Tests parts processing edge cases.
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "apps/ikigai/providers/google/streaming.h"
#include "apps/ikigai/providers/google/response.h"
#include "apps/ikigai/providers/provider.h"

/* Test context */
static TALLOC_CTX *test_ctx;

/* Captured stream events */
#define MAX_EVENTS 50
#define MAX_STRING_LEN 512
static ik_stream_event_t captured_events[MAX_EVENTS];
static char captured_strings1[MAX_EVENTS][MAX_STRING_LEN];
static size_t captured_count;

/* ================================================================
 * Test Callbacks
 * ================================================================ */

static res_t test_stream_cb(const ik_stream_event_t *event, void *ctx)
{
    (void)ctx;

    if (captured_count < MAX_EVENTS) {
        captured_events[captured_count] = *event;

        switch (event->type) {
            case IK_STREAM_TEXT_DELTA:
                if (event->data.delta.text) {
                    strncpy(captured_strings1[captured_count], event->data.delta.text, MAX_STRING_LEN - 1);
                    captured_strings1[captured_count][MAX_STRING_LEN - 1] = '\0';
                    captured_events[captured_count].data.delta.text = captured_strings1[captured_count];
                }
                break;
            case IK_STREAM_THINKING_DELTA:
                if (event->data.delta.text) {
                    strncpy(captured_strings1[captured_count], event->data.delta.text, MAX_STRING_LEN - 1);
                    captured_strings1[captured_count][MAX_STRING_LEN - 1] = '\0';
                    captured_events[captured_count].data.delta.text = captured_strings1[captured_count];
                }
                break;
            default:
                break;
        }

        captured_count++;
    }

    return OK(NULL);
}

/* ================================================================
 * Test Fixtures
 * ================================================================ */

static void setup(void)
{
    test_ctx = talloc_new(NULL);
    captured_count = 0;
    memset(captured_events, 0, sizeof(captured_events));
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

/* ================================================================
 * Helper Functions
 * ================================================================ */

static void process_chunk(ik_google_stream_ctx_t *sctx, const char *chunk)
{
    ik_google_stream_process_data(sctx, chunk);
}

static size_t count_events(ik_stream_event_type_t type)
{
    size_t count = 0;
    for (size_t i = 0; i < captured_count; i++) {
        if (captured_events[i].type == type) {
            count++;
        }
    }
    return count;
}

/* ================================================================
 * Parts Processing Edge Cases
 * ================================================================ */

START_TEST(test_part_without_text_field) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Process part without text field (only has other fields) - covers line 237 true branch */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"someOtherField\":\"value\"}]}}]}";
    process_chunk(sctx, chunk);

    /* Verify only START event, no TEXT_DELTA */
    ck_assert_int_eq((int)captured_count, 1);
    ck_assert_int_eq(captured_events[0].type, IK_STREAM_START);
}
END_TEST

START_TEST(test_part_with_null_text) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Process part with null text value - covers line 243 branch 1 */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":null}]}}]}";
    process_chunk(sctx, chunk);

    /* Verify only START event, no TEXT_DELTA */
    ck_assert_int_eq((int)captured_count, 1);
    ck_assert_int_eq(captured_events[0].type, IK_STREAM_START);
}
END_TEST

START_TEST(test_part_with_empty_text) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Process part with empty text string - covers line 243 branch 2 */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"\"}]}}]}";
    process_chunk(sctx, chunk);

    /* Verify only START event, no TEXT_DELTA */
    ck_assert_int_eq((int)captured_count, 1);
    ck_assert_int_eq(captured_events[0].type, IK_STREAM_START);
}
END_TEST

START_TEST(test_part_with_non_string_text) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Process part with non-string text value - covers line 243 branch 1 */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":123}]}}]}";
    process_chunk(sctx, chunk);

    /* Verify only START event, no TEXT_DELTA */
    ck_assert_int_eq((int)captured_count, 1);
    ck_assert_int_eq(captured_events[0].type, IK_STREAM_START);
}
END_TEST

START_TEST(test_thought_field_non_boolean) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Process part with non-boolean thought field (string) - covers line 233 branches */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Hello\",\"thought\":\"not-a-bool\"}]}}]}";
    process_chunk(sctx, chunk);

    /* Verify TEXT_DELTA (not THINKING_DELTA) since yyjson_get_bool returns false for strings */
    ck_assert_int_eq((int)count_events(IK_STREAM_TEXT_DELTA), 1);
    ck_assert_int_eq((int)count_events(IK_STREAM_THINKING_DELTA), 0);
}
END_TEST

START_TEST(test_thought_field_false) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Process part with thought field explicitly false - covers line 233 branches */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Hello\",\"thought\":false}]}}]}";
    process_chunk(sctx, chunk);

    /* Verify TEXT_DELTA (not THINKING_DELTA) */
    ck_assert_int_eq((int)count_events(IK_STREAM_TEXT_DELTA), 1);
    ck_assert_int_eq((int)count_events(IK_STREAM_THINKING_DELTA), 0);
}
END_TEST

START_TEST(test_parts_empty_array) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Process empty parts array - covers line 223 loop edge cases */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[]}}]}";
    process_chunk(sctx, chunk);

    /* Verify only START event, no content events */
    ck_assert_int_eq((int)captured_count, 1);
    ck_assert_int_eq(captured_events[0].type, IK_STREAM_START);
}
END_TEST

START_TEST(test_thought_field_null) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Process part with thought field as null - covers line 233 branch 5 */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Hello\",\"thought\":null}]}}]}";
    process_chunk(sctx, chunk);

    /* Verify TEXT_DELTA (not THINKING_DELTA) since null is falsy */
    ck_assert_int_eq((int)count_events(IK_STREAM_TEXT_DELTA), 1);
    ck_assert_int_eq((int)count_events(IK_STREAM_THINKING_DELTA), 0);
}
END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *google_streaming_parts_coverage_suite(void)
{
    Suite *s = suite_create("Google Streaming - Parts Coverage");

    TCase *tc_parts = tcase_create("Parts Processing Edge Cases");
    tcase_set_timeout(tc_parts, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_parts, setup, teardown);
    tcase_add_test(tc_parts, test_part_without_text_field);
    tcase_add_test(tc_parts, test_part_with_null_text);
    tcase_add_test(tc_parts, test_part_with_empty_text);
    tcase_add_test(tc_parts, test_part_with_non_string_text);
    tcase_add_test(tc_parts, test_thought_field_non_boolean);
    tcase_add_test(tc_parts, test_thought_field_false);
    tcase_add_test(tc_parts, test_parts_empty_array);
    tcase_add_test(tc_parts, test_thought_field_null);
    suite_add_tcase(s, tc_parts);

    return s;
}

int main(void)
{
    Suite *s = google_streaming_parts_coverage_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/google/google_streaming_parts_coverage_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
