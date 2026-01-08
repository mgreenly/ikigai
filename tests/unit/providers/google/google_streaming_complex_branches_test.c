/**
 * @file google_streaming_complex_branches_test.c
 * @brief Tests for complex branch conditions (&&, ||) in Google streaming
 *
 * Targets specific branch combinations in complex conditions.
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
 * Complex Branch Coverage Tests
 * ================================================================ */

/* Line 234: thought_val != NULL && yyjson_get_bool(thought_val)
 * Need to cover: thought_val == NULL (already covered)
 *                thought_val != NULL && get_bool == false
 *                thought_val != NULL && get_bool == true (already covered)
 */
START_TEST(test_thought_field_false_bool) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Reset to focus on content events */
    captured_count = 0;
    memset(captured_events, 0, sizeof(captured_events));

    /* Process part with thought field explicitly false - covers line 234 second branch false */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Hello\",\"thought\":false}]}}]}";
    process_chunk(sctx, chunk);

    /* Verify TEXT_DELTA (not THINKING_DELTA) since thought is false */
    ck_assert_int_eq((int)count_events(IK_STREAM_TEXT_DELTA), 1);
    ck_assert_int_eq((int)count_events(IK_STREAM_THINKING_DELTA), 0);
}
END_TEST

/* Line 404: candidates != NULL && yyjson_is_arr(candidates)
 * Need to cover: candidates == NULL (when field doesn't exist)
 *                candidates != NULL && !is_arr (when it's not an array)
 */
START_TEST(test_candidates_field_missing) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process chunk without candidates field - covers line 404 first branch false */
    const char *chunk = "{\"modelVersion\":\"gemini-2.5-flash\"}";
    process_chunk(sctx, chunk);

    /* Verify only START event */
    ck_assert_int_eq((int)captured_count, 1);
    ck_assert_int_eq(captured_events[0].type, IK_STREAM_START);
}
END_TEST

/* Line 419: parts != NULL && yyjson_is_arr(parts)
 * Already covered by existing tests (parts NULL and parts not array)
 */

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *google_streaming_complex_branches_suite(void)
{
    Suite *s = suite_create("Google Streaming - Complex Branches");

    TCase *tc_main = tcase_create("Complex Branch Conditions");
    tcase_set_timeout(tc_main, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_main, setup, teardown);
    tcase_add_test(tc_main, test_thought_field_false_bool);
    tcase_add_test(tc_main, test_candidates_field_missing);
    suite_add_tcase(s, tc_main);

    return s;
}

int main(void)
{
    Suite *s = google_streaming_complex_branches_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
