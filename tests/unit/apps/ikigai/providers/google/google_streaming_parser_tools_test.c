#include "tests/test_constants.h"
/**
 * @file google_streaming_parser_tools_test.c
 * @brief Unit tests for Google provider tool call streaming
 *
 * Tests verify function call streaming and tool call state transitions.
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
 * Function Call Streaming Tests
 * ================================================================ */

START_TEST(test_parse_function_call_part) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process chunk with functionCall */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"functionCall\":{\"name\":\"get_weather\",\"args\":{\"location\":\"London\"}}}]}}],\"modelVersion\":\"gemini-2.5-flash\"}";
    process_chunk(sctx, chunk);

    /* Verify TOOL_CALL_START event */
    const ik_stream_event_t *start_event = find_event(IK_STREAM_TOOL_CALL_START);
    ck_assert_ptr_nonnull(start_event);
    ck_assert_ptr_nonnull(start_event->data.tool_start.id);
    ck_assert_str_eq(start_event->data.tool_start.name, "get_weather");

    /* Verify TOOL_CALL_DELTA event */
    const ik_stream_event_t *delta_event = find_event(IK_STREAM_TOOL_CALL_DELTA);
    ck_assert_ptr_nonnull(delta_event);
    ck_assert_ptr_nonnull(strstr(delta_event->data.tool_delta.arguments, "location"));
    ck_assert_ptr_nonnull(strstr(delta_event->data.tool_delta.arguments, "London"));
}

END_TEST

START_TEST(test_generate_22_char_base64url_uuid) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process chunk with functionCall */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"functionCall\":{\"name\":\"test_func\",\"args\":{}}}]}}],\"modelVersion\":\"gemini-2.5-flash\"}";
    process_chunk(sctx, chunk);

    /* Verify generated ID is 22 characters */
    const ik_stream_event_t *start_event = find_event(IK_STREAM_TOOL_CALL_START);
    ck_assert_ptr_nonnull(start_event);
    ck_assert_ptr_nonnull(start_event->data.tool_start.id);
    ck_assert_int_eq((int)strlen(start_event->data.tool_start.id), 22);

    /* Verify ID contains only base64url characters (A-Z, a-z, 0-9, -, _) */
    const char *id = start_event->data.tool_start.id;
    for (size_t i = 0; i < 22; i++) {
        char c = id[i];
        bool valid = (c >= 'A' && c <= 'Z') ||
                     (c >= 'a' && c <= 'z') ||
                     (c >= '0' && c <= '9') ||
                     c == '-' || c == '_';
        ck_assert(valid);
    }
}

END_TEST

START_TEST(test_parse_function_arguments_from_function_call) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process chunk with functionCall with complex args */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"functionCall\":{\"name\":\"calc\",\"args\":{\"operation\":\"add\",\"values\":[1,2,3]}}}]}}],\"modelVersion\":\"gemini-2.5-flash\"}";
    process_chunk(sctx, chunk);

    /* Verify TOOL_CALL_DELTA contains serialized args */
    const ik_stream_event_t *delta_event = find_event(IK_STREAM_TOOL_CALL_DELTA);
    ck_assert_ptr_nonnull(delta_event);
    ck_assert_ptr_nonnull(strstr(delta_event->data.tool_delta.arguments, "operation"));
    ck_assert_ptr_nonnull(strstr(delta_event->data.tool_delta.arguments, "add"));
    ck_assert_ptr_nonnull(strstr(delta_event->data.tool_delta.arguments, "values"));
}

END_TEST
/* ================================================================
 * Tool Call State Transition Tests
 * ================================================================ */

START_TEST(test_tool_call_followed_by_text_ends_tool_call) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process tool call */
    process_chunk(sctx,
                  "{\"candidates\":[{\"content\":{\"parts\":[{\"functionCall\":{\"name\":\"test\",\"args\":{}}}]}}],\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Process text part (should end tool call) */
    process_chunk(sctx, "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Done\"}]}}]}");

    /* Verify TOOL_CALL_DONE event emitted before text */
    size_t done_idx = 0;
    size_t text_idx = 0;
    for (size_t i = 0; i < captured_count; i++) {
        if (captured_events[i].type == IK_STREAM_TOOL_CALL_DONE) {
            done_idx = i;
        }
        if (captured_events[i].type == IK_STREAM_TEXT_DELTA) {
            text_idx = i;
        }
    }

    /* TOOL_CALL_DONE should come before TEXT_DELTA */
    ck_assert(done_idx > 0);
    ck_assert(text_idx > done_idx);
}

END_TEST

START_TEST(test_usage_metadata_ends_tool_call) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process tool call */
    process_chunk(sctx,
                  "{\"candidates\":[{\"content\":{\"parts\":[{\"functionCall\":{\"name\":\"test\",\"args\":{}}}]}}],\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Process usage metadata (should end tool call) */
    process_chunk(sctx,
                  "{\"usageMetadata\":{\"promptTokenCount\":10,\"candidatesTokenCount\":5,\"totalTokenCount\":15}}");

    /* Verify TOOL_CALL_DONE emitted before STREAM_DONE */
    const ik_stream_event_t *done_event = find_event(IK_STREAM_TOOL_CALL_DONE);
    ck_assert_ptr_nonnull(done_event);

    const ik_stream_event_t *stream_done = find_event(IK_STREAM_DONE);
    ck_assert_ptr_nonnull(stream_done);
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *google_streaming_parser_tools_suite(void)
{
    Suite *s = suite_create("Google Streaming Parser - Tools");

    TCase *tc_function = tcase_create("Function Call Streaming");
    tcase_set_timeout(tc_function, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_function, setup, teardown);
    tcase_add_test(tc_function, test_parse_function_call_part);
    tcase_add_test(tc_function, test_generate_22_char_base64url_uuid);
    tcase_add_test(tc_function, test_parse_function_arguments_from_function_call);
    suite_add_tcase(s, tc_function);

    TCase *tc_state = tcase_create("State Transitions");
    tcase_set_timeout(tc_state, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_state, setup, teardown);
    tcase_add_test(tc_state, test_tool_call_followed_by_text_ends_tool_call);
    tcase_add_test(tc_state, test_usage_metadata_ends_tool_call);
    suite_add_tcase(s, tc_state);

    return s;
}

int main(void)
{
    Suite *s = google_streaming_parser_tools_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/google/google_streaming_parser_tools_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
