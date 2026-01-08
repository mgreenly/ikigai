/**
 * @file google_streaming_yyjson_branches_test.c
 * @brief Tests to improve branch coverage by hitting yyjson internal branches
 *
 * Targets uncovered branches in streaming.c by providing various JSON patterns
 * that may trigger different code paths in yyjson inline functions.
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/google/streaming.h"
#include "providers/google/response.h"
#include "providers/provider.h"

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
            case IK_STREAM_ERROR:
                if (event->data.error.message) {
                    strncpy(captured_strings1[captured_count], event->data.error.message, MAX_STRING_LEN - 1);
                    captured_strings1[captured_count][MAX_STRING_LEN - 1] = '\0';
                    captured_events[captured_count].data.error.message = captured_strings1[captured_count];
                }
                break;
            case IK_STREAM_TEXT_DELTA:
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

static const ik_stream_event_t *find_event(ik_stream_event_type_t type)
{
    for (size_t i = 0; i < captured_count; i++) {
        if (captured_events[i].type == type) {
            return &captured_events[i];
        }
    }
    return NULL;
}

/* ================================================================
 * YYJSON Branch Coverage Tests
 * ================================================================ */

START_TEST(test_usage_with_large_object_many_keys) {
    /* Large object with many keys may trigger different yyjson_obj_get internal paths */
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Process usage with many extra keys to trigger different hash paths */
    const char *chunk =
        "{\"usageMetadata\":{"
        "\"promptTokenCount\":100,"
        "\"candidatesTokenCount\":200,"
        "\"thoughtsTokenCount\":50,"
        "\"totalTokenCount\":350,"
        "\"extraKey1\":1,"
        "\"extraKey2\":2,"
        "\"extraKey3\":3,"
        "\"extraKey4\":4,"
        "\"extraKey5\":5"
        "}}";
    process_chunk(sctx, chunk);

    /* Verify DONE event */
    const ik_stream_event_t *event = find_event(IK_STREAM_DONE);
    ck_assert_ptr_nonnull(event);
    ck_assert_int_eq(event->data.done.usage.input_tokens, 100);
}
END_TEST

START_TEST(test_parts_with_empty_array) {
    /* Empty parts array may trigger different iteration branch */
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Reset to focus on content events */
    captured_count = 0;

    /* Process candidate with empty parts array */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[]}}]}";
    process_chunk(sctx, chunk);

    /* Should not crash, no text events */
    ck_assert_int_eq((int)captured_count, 0);
}
END_TEST

START_TEST(test_parts_with_single_element) {
    /* Single element array may trigger different iteration path than multiple */
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Reset */
    captured_count = 0;

    /* Single part */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"X\"}]}}]}";
    process_chunk(sctx, chunk);

    /* Verify one text event */
    ck_assert_int_gt((int)captured_count, 0);
}
END_TEST

START_TEST(test_parts_with_many_elements) {
    /* Many elements may trigger different iteration branches */
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Reset */
    captured_count = 0;

    /* Many parts */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":["
        "{\"text\":\"A\"},"
        "{\"text\":\"B\"},"
        "{\"text\":\"C\"},"
        "{\"text\":\"D\"},"
        "{\"text\":\"E\"},"
        "{\"text\":\"F\"},"
        "{\"text\":\"G\"},"
        "{\"text\":\"H\"}"
        "]}}]}";
    process_chunk(sctx, chunk);

    /* Should have multiple text events */
    ck_assert_int_gt((int)captured_count, 5);
}
END_TEST

START_TEST(test_thought_with_false_value) {
    /* thought field with explicit false may trigger different bool path */
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Reset */
    captured_count = 0;

    /* Explicit thought:false */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Hi\",\"thought\":false}]}}]}";
    process_chunk(sctx, chunk);

    /* Should be text, not thinking */
    const ik_stream_event_t *event = find_event(IK_STREAM_TEXT_DELTA);
    ck_assert_ptr_nonnull(event);
}
END_TEST

START_TEST(test_thought_with_true_value) {
    /* thought field with explicit true */
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Reset */
    captured_count = 0;

    /* Explicit thought:true */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Thinking\",\"thought\":true}]}}]}";
    process_chunk(sctx, chunk);

    /* Should be thinking */
    const ik_stream_event_t *event = find_event(IK_STREAM_THINKING_DELTA);
    ck_assert_ptr_nonnull(event);
}
END_TEST

START_TEST(test_error_with_minimal_object) {
    /* Minimal error object may trigger different object access pattern */
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Minimal error - just empty object */
    const char *chunk = "{\"error\":{}}";
    process_chunk(sctx, chunk);

    /* Should get error event with defaults */
    const ik_stream_event_t *event = find_event(IK_STREAM_ERROR);
    ck_assert_ptr_nonnull(event);
    ck_assert_int_eq(event->data.error.category, IK_ERR_CAT_UNKNOWN);
}
END_TEST

START_TEST(test_error_with_large_object) {
    /* Large error object with many fields */
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Large error object */
    const char *chunk =
        "{\"error\":{"
        "\"message\":\"Test error\","
        "\"status\":\"UNAUTHENTICATED\","
        "\"code\":401,"
        "\"details\":[]," "\"metadata\":{},"
        "\"field1\":1,"
        "\"field2\":2"
        "}}";
    process_chunk(sctx, chunk);

    /* Verify error */
    const ik_stream_event_t *event = find_event(IK_STREAM_ERROR);
    ck_assert_ptr_nonnull(event);
    ck_assert_int_eq(event->data.error.category, IK_ERR_CAT_AUTH);
}
END_TEST

START_TEST(test_complex_nested_json) {
    /* Complex nested structure may trigger different JSON parser branches */
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* START with nested metadata */
    const char *chunk =
        "{\"modelVersion\":\"gemini-2.5-flash\","
        "\"metadata\":{\"nested\":{\"deep\":{\"value\":123}}}}";
    process_chunk(sctx, chunk);

    /* Should still process correctly */
    const ik_stream_event_t *event = find_event(IK_STREAM_START);
    ck_assert_ptr_nonnull(event);
}
END_TEST

START_TEST(test_usage_with_zero_values) {
    /* Zero values in usage may trigger different paths */
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Usage with all zeros */
    const char *chunk =
        "{\"usageMetadata\":{"
        "\"promptTokenCount\":0,"
        "\"candidatesTokenCount\":0,"
        "\"thoughtsTokenCount\":0,"
        "\"totalTokenCount\":0"
        "}}";
    process_chunk(sctx, chunk);

    /* Verify DONE event */
    const ik_stream_event_t *event = find_event(IK_STREAM_DONE);
    ck_assert_ptr_nonnull(event);
    ck_assert_int_eq(event->data.done.usage.input_tokens, 0);
}
END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *google_streaming_yyjson_branches_suite(void)
{
    Suite *s = suite_create("Google Streaming - YYJSON Branch Coverage");

    TCase *tc_main = tcase_create("YYJSON Branches");
    tcase_set_timeout(tc_main, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_main, setup, teardown);
    tcase_add_test(tc_main, test_usage_with_large_object_many_keys);
    tcase_add_test(tc_main, test_parts_with_empty_array);
    tcase_add_test(tc_main, test_parts_with_single_element);
    tcase_add_test(tc_main, test_parts_with_many_elements);
    tcase_add_test(tc_main, test_thought_with_false_value);
    tcase_add_test(tc_main, test_thought_with_true_value);
    tcase_add_test(tc_main, test_error_with_minimal_object);
    tcase_add_test(tc_main, test_error_with_large_object);
    tcase_add_test(tc_main, test_complex_nested_json);
    tcase_add_test(tc_main, test_usage_with_zero_values);
    suite_add_tcase(s, tc_main);

    return s;
}

int main(void)
{
    Suite *s = google_streaming_yyjson_branches_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
