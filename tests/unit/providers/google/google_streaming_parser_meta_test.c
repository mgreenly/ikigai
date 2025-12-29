/**
 * @file google_streaming_parser_meta_test.c
 * @brief Unit tests for Google provider streaming metadata and error handling
 *
 * Tests verify error handling, usage statistics, finish reasons, and stream context.
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
            case IK_STREAM_START:
                if (event->data.start.model) {
                    strncpy(captured_strings1[captured_count], event->data.start.model, MAX_STRING_LEN - 1);
                    captured_strings1[captured_count][MAX_STRING_LEN - 1] = '\0';
                    captured_events[captured_count].data.start.model = captured_strings1[captured_count];
                }
                break;
            case IK_STREAM_ERROR:
                if (event->data.error.message) {
                    strncpy(captured_strings1[captured_count], event->data.error.message, MAX_STRING_LEN - 1);
                    captured_strings1[captured_count][MAX_STRING_LEN - 1] = '\0';
                    captured_events[captured_count].data.error.message = captured_strings1[captured_count];
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
 * Error Handling Tests
 * ================================================================ */

START_TEST(test_handle_malformed_json_chunk) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process malformed JSON - should be silently ignored */
    const char *chunk = "{invalid json}";
    process_chunk(sctx, chunk);

    /* Verify no events emitted (malformed JSON ignored) */
    ck_assert_int_eq((int)captured_count, 0);
}

END_TEST START_TEST(test_handle_empty_data_chunk)
{
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process empty data */
    process_chunk(sctx, "");
    process_chunk(sctx, NULL);

    /* Verify no events emitted */
    ck_assert_int_eq((int)captured_count, 0);
}

END_TEST START_TEST(test_handle_error_object_in_chunk)
{
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process chunk with error object */
    const char *chunk = "{\"error\":{\"message\":\"API key invalid\",\"status\":\"UNAUTHENTICATED\"}}";
    process_chunk(sctx, chunk);

    /* Verify ERROR event emitted */
    const ik_stream_event_t *event = find_event(IK_STREAM_ERROR);
    ck_assert_ptr_nonnull(event);
    ck_assert_int_eq(event->data.error.category, IK_ERR_CAT_AUTH);
    ck_assert_str_eq(event->data.error.message, "API key invalid");
}

END_TEST
/* ================================================================
 * Usage Statistics Tests
 * ================================================================ */

START_TEST(test_usage_excludes_thinking_from_output_tokens)
{
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Process usage chunk */
    const char *chunk =
        "{\"usageMetadata\":{\"promptTokenCount\":100,\"candidatesTokenCount\":200,\"thoughtsTokenCount\":50,\"totalTokenCount\":300}}";
    process_chunk(sctx, chunk);

    /* Verify usage calculation */
    ik_usage_t usage = ik_google_stream_get_usage(sctx);
    ck_assert_int_eq(usage.input_tokens, 100);
    ck_assert_int_eq(usage.output_tokens, 150); /* candidatesTokenCount - thoughtsTokenCount */
    ck_assert_int_eq(usage.thinking_tokens, 50);
    ck_assert_int_eq(usage.total_tokens, 300);
}

END_TEST START_TEST(test_usage_handles_missing_thoughts_token_count)
{
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Process usage chunk without thoughtsTokenCount */
    const char *chunk =
        "{\"usageMetadata\":{\"promptTokenCount\":100,\"candidatesTokenCount\":200,\"totalTokenCount\":300}}";
    process_chunk(sctx, chunk);

    /* Verify usage calculation */
    ik_usage_t usage = ik_google_stream_get_usage(sctx);
    ck_assert_int_eq(usage.input_tokens, 100);
    ck_assert_int_eq(usage.output_tokens, 200); /* candidatesTokenCount when no thoughts */
    ck_assert_int_eq(usage.thinking_tokens, 0);
    ck_assert_int_eq(usage.total_tokens, 300);
}

END_TEST
/* ================================================================
 * Finish Reason Tests
 * ================================================================ */

START_TEST(test_map_stop_finish_reason)
{
    ik_finish_reason_t reason = ik_google_map_finish_reason("STOP");
    ck_assert_int_eq(reason, IK_FINISH_STOP);
}

END_TEST START_TEST(test_map_max_tokens_finish_reason)
{
    ik_finish_reason_t reason = ik_google_map_finish_reason("MAX_TOKENS");
    ck_assert_int_eq(reason, IK_FINISH_LENGTH);
}

END_TEST START_TEST(test_map_safety_finish_reason)
{
    ik_finish_reason_t reason = ik_google_map_finish_reason("SAFETY");
    ck_assert_int_eq(reason, IK_FINISH_CONTENT_FILTER);
}

END_TEST START_TEST(test_map_unknown_finish_reason)
{
    ik_finish_reason_t reason = ik_google_map_finish_reason("UNKNOWN_REASON");
    ck_assert_int_eq(reason, IK_FINISH_UNKNOWN);
}

END_TEST START_TEST(test_map_null_finish_reason)
{
    ik_finish_reason_t reason = ik_google_map_finish_reason(NULL);
    ck_assert_int_eq(reason, IK_FINISH_UNKNOWN);
}

END_TEST
/* ================================================================
 * Stream Context Tests
 * ================================================================ */

START_TEST(test_stream_ctx_create_initializes_state)
{
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(sctx);

    /* Verify initial state */
    ik_usage_t usage = ik_google_stream_get_usage(sctx);
    ck_assert_int_eq(usage.input_tokens, 0);
    ck_assert_int_eq(usage.output_tokens, 0);
    ck_assert_int_eq(usage.thinking_tokens, 0);
    ck_assert_int_eq(usage.total_tokens, 0);

    ik_finish_reason_t reason = ik_google_stream_get_finish_reason(sctx);
    ck_assert_int_eq(reason, IK_FINISH_UNKNOWN);
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *google_streaming_parser_meta_suite(void)
{
    Suite *s = suite_create("Google Streaming Parser - Metadata");

    TCase *tc_error = tcase_create("Error Handling");
    tcase_set_timeout(tc_error, 30);
    tcase_add_checked_fixture(tc_error, setup, teardown);
    tcase_add_test(tc_error, test_handle_malformed_json_chunk);
    tcase_add_test(tc_error, test_handle_empty_data_chunk);
    tcase_add_test(tc_error, test_handle_error_object_in_chunk);
    suite_add_tcase(s, tc_error);

    TCase *tc_usage = tcase_create("Usage Statistics");
    tcase_set_timeout(tc_usage, 30);
    tcase_add_checked_fixture(tc_usage, setup, teardown);
    tcase_add_test(tc_usage, test_usage_excludes_thinking_from_output_tokens);
    tcase_add_test(tc_usage, test_usage_handles_missing_thoughts_token_count);
    suite_add_tcase(s, tc_usage);

    TCase *tc_finish = tcase_create("Finish Reason Mapping");
    tcase_set_timeout(tc_finish, 30);
    tcase_add_checked_fixture(tc_finish, setup, teardown);
    tcase_add_test(tc_finish, test_map_stop_finish_reason);
    tcase_add_test(tc_finish, test_map_max_tokens_finish_reason);
    tcase_add_test(tc_finish, test_map_safety_finish_reason);
    tcase_add_test(tc_finish, test_map_unknown_finish_reason);
    tcase_add_test(tc_finish, test_map_null_finish_reason);
    suite_add_tcase(s, tc_finish);

    TCase *tc_ctx = tcase_create("Stream Context");
    tcase_set_timeout(tc_ctx, 30);
    tcase_add_checked_fixture(tc_ctx, setup, teardown);
    tcase_add_test(tc_ctx, test_stream_ctx_create_initializes_state);
    suite_add_tcase(s, tc_ctx);

    return s;
}

int main(void)
{
    Suite *s = google_streaming_parser_meta_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
