#include "tests/test_constants.h"
/**
 * @file google_streaming_multi_tool_test.c
 * @brief Unit tests for multiple function calls in a single Google streaming response
 *
 * Tests verify that when Gemini returns multiple functionCall parts in one
 * response, each is parsed as a separate tool call with its own ID, name,
 * and valid JSON arguments.
 */

#include <check.h>
#include <stdint.h>
#include <talloc.h>
#include <string.h>
#include "apps/ikigai/providers/google/streaming.h"
#include "apps/ikigai/providers/google/response.h"
#include "apps/ikigai/providers/provider.h"

/* Test context */
static TALLOC_CTX *test_ctx;

/* Captured stream events */
#define MAX_EVENTS 100
#define MAX_STRING_LEN 512
static ik_stream_event_t captured_events[MAX_EVENTS];
static char captured_strings1[MAX_EVENTS][MAX_STRING_LEN];
static char captured_strings2[MAX_EVENTS][MAX_STRING_LEN];
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
            case IK_STREAM_START:
                if (event->data.start.model) {
                    strncpy(captured_strings1[captured_count], event->data.start.model, MAX_STRING_LEN - 1);
                    captured_strings1[captured_count][MAX_STRING_LEN - 1] = '\0';
                    captured_events[captured_count].data.start.model = captured_strings1[captured_count];
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
            case IK_STREAM_TOOL_CALL_START:
                if (event->data.tool_start.id) {
                    strncpy(captured_strings1[captured_count], event->data.tool_start.id, MAX_STRING_LEN - 1);
                    captured_strings1[captured_count][MAX_STRING_LEN - 1] = '\0';
                    captured_events[captured_count].data.tool_start.id = captured_strings1[captured_count];
                }
                if (event->data.tool_start.name) {
                    strncpy(captured_strings2[captured_count], event->data.tool_start.name, MAX_STRING_LEN - 1);
                    captured_strings2[captured_count][MAX_STRING_LEN - 1] = '\0';
                    captured_events[captured_count].data.tool_start.name = captured_strings2[captured_count];
                }
                break;
            case IK_STREAM_TOOL_CALL_DELTA:
                if (event->data.tool_delta.arguments) {
                    strncpy(captured_strings1[captured_count], event->data.tool_delta.arguments, MAX_STRING_LEN - 1);
                    captured_strings1[captured_count][MAX_STRING_LEN - 1] = '\0';
                    captured_events[captured_count].data.tool_delta.arguments = captured_strings1[captured_count];
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

static const ik_stream_event_t *find_nth_event(ik_stream_event_type_t type, size_t n)
{
    size_t found = 0;
    for (size_t i = 0; i < captured_count; i++) {
        if (captured_events[i].type == type) {
            if (found == n) {
                return &captured_events[i];
            }
            found++;
        }
    }
    return NULL;
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

static size_t event_position(ik_stream_event_type_t type, size_t n)
{
    size_t found = 0;
    for (size_t i = 0; i < captured_count; i++) {
        if (captured_events[i].type == type) {
            if (found == n) {
                return i;
            }
            found++;
        }
    }
    return SIZE_MAX;
}

/* ================================================================
 * Multiple Function Calls Tests
 * ================================================================ */

START_TEST(test_two_function_calls_same_name) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* One chunk with two file_read function calls */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":["
        "{\"functionCall\":{\"name\":\"file_read\",\"args\":{\"file_path\":\"a.h\"}}},"
        "{\"functionCall\":{\"name\":\"file_read\",\"args\":{\"file_path\":\"b.c\"}}}"
        "]}}],\"modelVersion\":\"gemini-2.5-flash\"}";
    process_chunk(sctx, chunk);

    /* Process usage to flush the last tool call */
    process_chunk(sctx,
                  "{\"usageMetadata\":{\"promptTokenCount\":20,"
                  "\"candidatesTokenCount\":10,\"totalTokenCount\":30}}");

    /* Two TOOL_CALL_START and two TOOL_CALL_DONE events */
    ck_assert_int_eq((int)count_events(IK_STREAM_TOOL_CALL_START), 2);
    ck_assert_int_eq((int)count_events(IK_STREAM_TOOL_CALL_DONE),  2);

    /* Both have name "file_read" */
    const ik_stream_event_t *start0 = find_nth_event(IK_STREAM_TOOL_CALL_START, 0);
    const ik_stream_event_t *start1 = find_nth_event(IK_STREAM_TOOL_CALL_START, 1);
    ck_assert_ptr_nonnull(start0);
    ck_assert_ptr_nonnull(start1);
    ck_assert_str_eq(start0->data.tool_start.name, "file_read");
    ck_assert_str_eq(start1->data.tool_start.name, "file_read");

    /* IDs must be non-null and distinct */
    ck_assert_ptr_nonnull(start0->data.tool_start.id);
    ck_assert_ptr_nonnull(start1->data.tool_start.id);
    ck_assert_str_ne(start0->data.tool_start.id, start1->data.tool_start.id);

    /* Events use distinct part_index values */
    ck_assert_int_ne((int)start0->index, (int)start1->index);

    /* First DONE precedes second START */
    size_t done0_pos  = event_position(IK_STREAM_TOOL_CALL_DONE,  0);
    size_t start1_pos = event_position(IK_STREAM_TOOL_CALL_START, 1);
    ck_assert(done0_pos < start1_pos);

    /* Each DELTA carries the matching file_path */
    const ik_stream_event_t *delta0 = find_nth_event(IK_STREAM_TOOL_CALL_DELTA, 0);
    const ik_stream_event_t *delta1 = find_nth_event(IK_STREAM_TOOL_CALL_DELTA, 1);
    ck_assert_ptr_nonnull(delta0);
    ck_assert_ptr_nonnull(delta1);
    ck_assert_ptr_nonnull(strstr(delta0->data.tool_delta.arguments, "a.h"));
    ck_assert_ptr_nonnull(strstr(delta1->data.tool_delta.arguments, "b.c"));
}

END_TEST

START_TEST(test_three_function_calls_in_one_response) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* One chunk with three function calls */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":["
        "{\"functionCall\":{\"name\":\"file_read\",\"args\":{\"file_path\":\"a.h\"}}},"
        "{\"functionCall\":{\"name\":\"file_read\",\"args\":{\"file_path\":\"b.c\"}}},"
        "{\"functionCall\":{\"name\":\"file_read\",\"args\":{\"file_path\":\"c.h\"}}}"
        "]}}],\"modelVersion\":\"gemini-2.5-flash\"}";
    process_chunk(sctx, chunk);

    process_chunk(sctx,
                  "{\"usageMetadata\":{\"promptTokenCount\":30,"
                  "\"candidatesTokenCount\":15,\"totalTokenCount\":45}}");

    /* Three TOOL_CALL_START and three TOOL_CALL_DONE events */
    ck_assert_int_eq((int)count_events(IK_STREAM_TOOL_CALL_START), 3);
    ck_assert_int_eq((int)count_events(IK_STREAM_TOOL_CALL_DONE),  3);

    /* All three have distinct IDs */
    const ik_stream_event_t *s0 = find_nth_event(IK_STREAM_TOOL_CALL_START, 0);
    const ik_stream_event_t *s1 = find_nth_event(IK_STREAM_TOOL_CALL_START, 1);
    const ik_stream_event_t *s2 = find_nth_event(IK_STREAM_TOOL_CALL_START, 2);
    ck_assert_ptr_nonnull(s0);
    ck_assert_ptr_nonnull(s1);
    ck_assert_ptr_nonnull(s2);
    ck_assert_str_ne(s0->data.tool_start.id, s1->data.tool_start.id);
    ck_assert_str_ne(s1->data.tool_start.id, s2->data.tool_start.id);
    ck_assert_str_ne(s0->data.tool_start.id, s2->data.tool_start.id);

    /* part_index values are monotonically increasing */
    ck_assert_int_lt((int)s0->index, (int)s1->index);
    ck_assert_int_lt((int)s1->index, (int)s2->index);

    /* Correct file_path in each delta */
    const ik_stream_event_t *d0 = find_nth_event(IK_STREAM_TOOL_CALL_DELTA, 0);
    const ik_stream_event_t *d1 = find_nth_event(IK_STREAM_TOOL_CALL_DELTA, 1);
    const ik_stream_event_t *d2 = find_nth_event(IK_STREAM_TOOL_CALL_DELTA, 2);
    ck_assert_ptr_nonnull(strstr(d0->data.tool_delta.arguments, "a.h"));
    ck_assert_ptr_nonnull(strstr(d1->data.tool_delta.arguments, "b.c"));
    ck_assert_ptr_nonnull(strstr(d2->data.tool_delta.arguments, "c.h"));
}

END_TEST

START_TEST(test_mixed_function_call_names) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* One chunk with file_read followed by file_edit */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":["
        "{\"functionCall\":{\"name\":\"file_read\","
        "\"args\":{\"file_path\":\"x.c\"}}},"
        "{\"functionCall\":{\"name\":\"file_edit\","
        "\"args\":{\"old_string\":\"foo\",\"new_string\":\"bar\"}}}"
        "]}}],\"modelVersion\":\"gemini-2.5-flash\"}";
    process_chunk(sctx, chunk);

    process_chunk(sctx,
                  "{\"usageMetadata\":{\"promptTokenCount\":25,"
                  "\"candidatesTokenCount\":12,\"totalTokenCount\":37}}");

    /* Two TOOL_CALL_START and two TOOL_CALL_DONE events */
    ck_assert_int_eq((int)count_events(IK_STREAM_TOOL_CALL_START), 2);
    ck_assert_int_eq((int)count_events(IK_STREAM_TOOL_CALL_DONE),  2);

    /* Correct names in correct order */
    const ik_stream_event_t *s0 = find_nth_event(IK_STREAM_TOOL_CALL_START, 0);
    const ik_stream_event_t *s1 = find_nth_event(IK_STREAM_TOOL_CALL_START, 1);
    ck_assert_str_eq(s0->data.tool_start.name, "file_read");
    ck_assert_str_eq(s1->data.tool_start.name, "file_edit");

    /* Correct arguments in correct order */
    const ik_stream_event_t *d0 = find_nth_event(IK_STREAM_TOOL_CALL_DELTA, 0);
    const ik_stream_event_t *d1 = find_nth_event(IK_STREAM_TOOL_CALL_DELTA, 1);
    ck_assert_ptr_nonnull(strstr(d0->data.tool_delta.arguments, "x.c"));
    ck_assert_ptr_nonnull(strstr(d1->data.tool_delta.arguments, "old_string"));
    ck_assert_ptr_nonnull(strstr(d1->data.tool_delta.arguments, "foo"));

    /* IDs are distinct */
    ck_assert_str_ne(s0->data.tool_start.id, s1->data.tool_start.id);

    /* DONE for first call comes before START of second call */
    size_t done0_pos  = event_position(IK_STREAM_TOOL_CALL_DONE,  0);
    size_t start1_pos = event_position(IK_STREAM_TOOL_CALL_START, 1);
    ck_assert(done0_pos < start1_pos);
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *google_streaming_multi_tool_suite(void)
{
    Suite *s = suite_create("Google Streaming - Multiple Tool Calls");

    TCase *tc_multi = tcase_create("Multiple Function Calls");
    tcase_set_timeout(tc_multi, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_multi, setup, teardown);
    tcase_add_test(tc_multi, test_two_function_calls_same_name);
    tcase_add_test(tc_multi, test_three_function_calls_in_one_response);
    tcase_add_test(tc_multi, test_mixed_function_call_names);
    suite_add_tcase(s, tc_multi);

    return s;
}

int main(void)
{
    Suite *s = google_streaming_multi_tool_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr,
                    "reports/check/unit/apps/ikigai/providers/google/"
                    "google_streaming_multi_tool_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
