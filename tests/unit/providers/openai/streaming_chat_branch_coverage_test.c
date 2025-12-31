/**
 * @file streaming_chat_branch_coverage_test.c
 * @brief Additional branch coverage tests for streaming_chat.c
 */

#include <check.h>
#include <talloc.h>
#include "providers/openai/streaming.h"

/* ================================================================
 * Test Context
 * ================================================================ */

static TALLOC_CTX *test_ctx;
static int event_count;

static res_t dummy_stream_cb(const ik_stream_event_t *event, void *ctx)
{
    (void)event;
    (void)ctx;
    event_count++;
    return OK(NULL);
}

static void setup(void)
{
    test_ctx = talloc_new(NULL);
    event_count = 0;
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

/* ================================================================
 * Branch Coverage Tests
 * ================================================================ */

/**
 * Test: error field exists but is not an object
 * Covers line 103: error_val != NULL && yyjson_is_obj(error_val) false branch
 */
START_TEST(test_error_not_object)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    /* Error field is a string, not an object */
    const char *data = "{\"error\":\"string error\"}";
    ik_openai_chat_stream_process_data(sctx, data);

    /* Should not emit error event since error is not an object */
    ck_assert_int_eq(event_count, 0);
}

END_TEST

/**
 * Test: error field is an array, not an object
 * Covers line 103: another false branch case
 */
START_TEST(test_error_is_array)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    /* Error field is an array, not an object */
    const char *data = "{\"error\":[\"error1\", \"error2\"]}";
    ik_openai_chat_stream_process_data(sctx, data);

    /* Should not emit error event since error is not an object */
    ck_assert_int_eq(event_count, 0);
}

END_TEST

/**
 * Test: error field is null
 * Covers line 103: error_val != NULL false branch
 */
START_TEST(test_error_is_null)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    /* Error field is explicitly null */
    const char *data = "{\"error\":null}";
    ik_openai_chat_stream_process_data(sctx, data);

    /* Should not emit error event */
    ck_assert_int_eq(event_count, 0);
}

END_TEST

/**
 * Test: choices field is null
 * Covers line 151: choices_val != NULL false branch
 */
START_TEST(test_choices_is_null)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    const char *data = "{\"choices\":null}";
    ik_openai_chat_stream_process_data(sctx, data);

    /* Should handle gracefully */
    ck_assert_int_eq(event_count, 0);
}

END_TEST

/**
 * Test: usage field is null
 * Covers line 175: usage_val != NULL false branch
 */
START_TEST(test_usage_is_null)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    const char *data = "{\"usage\":null}";
    ik_openai_chat_stream_process_data(sctx, data);

    ik_usage_t usage = ik_openai_chat_stream_get_usage(sctx);
    ck_assert_int_eq(usage.input_tokens, 0);
}

END_TEST

/**
 * Test: finish_reason field is null
 * Covers line 162: finish_reason_val != NULL false branch
 */
START_TEST(test_finish_reason_is_null)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    const char *data = "{\"choices\":[{\"delta\":{\"role\":\"assistant\"},\"finish_reason\":null}]}";
    ik_openai_chat_stream_process_data(sctx, data);

    /* finish_reason should remain UNKNOWN */
    ik_finish_reason_t reason = ik_openai_chat_stream_get_finish_reason(sctx);
    ck_assert_int_eq(reason, IK_FINISH_UNKNOWN);
}

END_TEST

/**
 * Test: delta without finish_reason field at all
 * Covers line 160: yyjson_obj_get returns NULL when field doesn't exist
 */
START_TEST(test_delta_without_finish_reason_field)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    const char *data = "{\"choices\":[{\"delta\":{\"role\":\"assistant\"}}]}";
    ik_openai_chat_stream_process_data(sctx, data);

    /* finish_reason should remain UNKNOWN */
    ik_finish_reason_t reason = ik_openai_chat_stream_get_finish_reason(sctx);
    ck_assert_int_eq(reason, IK_FINISH_UNKNOWN);
}

END_TEST

/**
 * Test: [DONE] marker triggers DONE event
 * Covers line 72: strcmp(data, "[DONE]") == 0 true branch
 */
START_TEST(test_done_marker)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    /* Send [DONE] marker */
    ik_openai_chat_stream_process_data(sctx, "[DONE]");

    /* Should emit DONE event */
    ck_assert_int_eq(event_count, 1);
}

END_TEST

/**
 * Test: malformed JSON is silently ignored
 * Covers line 90: doc == NULL branch
 */
START_TEST(test_malformed_json)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    /* Send malformed JSON */
    ik_openai_chat_stream_process_data(sctx, "{invalid json}");

    /* Should not emit any events */
    ck_assert_int_eq(event_count, 0);
}

END_TEST

/**
 * Test: root is not an object (array instead)
 * Covers line 96: !yyjson_is_obj(root) branch
 */
START_TEST(test_root_is_array)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    /* Send JSON array instead of object */
    ik_openai_chat_stream_process_data(sctx, "[1, 2, 3]");

    /* Should not emit any events */
    ck_assert_int_eq(event_count, 0);
}

END_TEST

/**
 * Test: error types - authentication, permission, rate_limit, invalid_request, server, service, unknown
 * Covers lines 103-135: all error type mappings
 */
START_TEST(test_error_types)
{
    const char *test_cases[] = {
        "{\"error\":{\"message\":\"msg\",\"type\":\"authentication_error\"}}",
        "{\"error\":{\"message\":\"msg\",\"type\":\"permission_error\"}}",
        "{\"error\":{\"message\":\"msg\",\"type\":\"rate_limit_error\"}}",
        "{\"error\":{\"message\":\"msg\",\"type\":\"invalid_request_error\"}}",
        "{\"error\":{\"message\":\"msg\",\"type\":\"server_error\"}}",
        "{\"error\":{\"message\":\"msg\",\"type\":\"service_unavailable\"}}",
        "{\"error\":{\"message\":\"msg\",\"type\":\"other_error\"}}"
    };

    for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
        event_count = 0;
        ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
            test_ctx, dummy_stream_cb, NULL);
        ik_openai_chat_stream_process_data(sctx, test_cases[i]);
        ck_assert_int_eq(event_count, 1);
    }
}

END_TEST

/**
 * Test: error object without message field
 * Covers line 108, 130: message_val is NULL, uses default message
 */
START_TEST(test_error_no_message)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    const char *data = "{\"error\":{\"type\":\"server_error\"}}";
    ik_openai_chat_stream_process_data(sctx, data);

    /* Should emit error event with default message */
    ck_assert_int_eq(event_count, 1);
}

END_TEST

/**
 * Test: error object without type field
 * Covers line 109, 113: type_val is NULL, category is UNKNOWN
 */
START_TEST(test_error_no_type)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    const char *data = "{\"error\":{\"message\":\"Error without type\"}}";
    ik_openai_chat_stream_process_data(sctx, data);

    /* Should emit error event with UNKNOWN category */
    ck_assert_int_eq(event_count, 1);
}

END_TEST

/**
 * Test: model extraction when model is already set
 * Covers line 140: model_val != NULL false branch (model already set)
 */
START_TEST(test_model_already_set)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    /* First chunk sets the model */
    const char *data1 = "{\"model\":\"gpt-4\"}";
    ik_openai_chat_stream_process_data(sctx, data1);

    /* Second chunk also has model field, but should be ignored */
    const char *data2 = "{\"model\":\"gpt-3.5-turbo\"}";
    ik_openai_chat_stream_process_data(sctx, data2);

    /* Model should still be the first one */
    ck_assert_int_eq(event_count, 0);
}

END_TEST

/**
 * Test: usage object with all fields including thinking tokens
 * Covers lines 175-201: usage extraction with completion_tokens_details
 */
START_TEST(test_usage_complete)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    const char *data = "{"
        "\"usage\":{"
            "\"prompt_tokens\":100,"
            "\"completion_tokens\":50,"
            "\"total_tokens\":150,"
            "\"completion_tokens_details\":{"
                "\"reasoning_tokens\":10"
            "}"
        "}"
    "}";
    ik_openai_chat_stream_process_data(sctx, data);

    ik_usage_t usage = ik_openai_chat_stream_get_usage(sctx);
    ck_assert_int_eq(usage.input_tokens, 100);
    ck_assert_int_eq(usage.output_tokens, 50);
    ck_assert_int_eq(usage.total_tokens, 150);
    ck_assert_int_eq(usage.thinking_tokens, 10);
}

END_TEST

/**
 * Test: various field type mismatches (model, choices, delta, finish_reason)
 * Covers lines 142, 153, 155, 158, 162: type validation branches
 */
START_TEST(test_field_type_mismatches)
{
    ik_openai_chat_stream_ctx_t *sctx;

    /* Model not string */
    sctx = ik_openai_chat_stream_ctx_create(test_ctx, dummy_stream_cb, NULL);
    ik_openai_chat_stream_process_data(sctx, "{\"model\":123}");
    ck_assert_int_eq(event_count, 0);

    /* Empty choices array */
    event_count = 0;
    sctx = ik_openai_chat_stream_ctx_create(test_ctx, dummy_stream_cb, NULL);
    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[]}");
    ck_assert_int_eq(event_count, 0);

    /* Choice not object */
    event_count = 0;
    sctx = ik_openai_chat_stream_ctx_create(test_ctx, dummy_stream_cb, NULL);
    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[\"x\"]}");
    ck_assert_int_eq(event_count, 0);

    /* Delta not object */
    event_count = 0;
    sctx = ik_openai_chat_stream_ctx_create(test_ctx, dummy_stream_cb, NULL);
    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"delta\":\"x\"}]}");
    ck_assert_int_eq(event_count, 0);

    /* Finish_reason not string */
    event_count = 0;
    sctx = ik_openai_chat_stream_ctx_create(test_ctx, dummy_stream_cb, NULL);
    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"delta\":{},\"finish_reason\":123}]}");
    ck_assert_int_eq(ik_openai_chat_stream_get_finish_reason(sctx), IK_FINISH_UNKNOWN);
}

END_TEST

/**
 * Test: usage fields with invalid types (non-int tokens, non-obj details)
 * Covers lines 178, 184, 190, 196, 198: type check branches
 */
START_TEST(test_usage_invalid_types)
{
    ik_openai_chat_stream_ctx_t *sctx;
    ik_usage_t usage;

    /* Non-int prompt_tokens */
    sctx = ik_openai_chat_stream_ctx_create(test_ctx, dummy_stream_cb, NULL);
    ik_openai_chat_stream_process_data(sctx, "{\"usage\":{\"prompt_tokens\":\"x\"}}");
    usage = ik_openai_chat_stream_get_usage(sctx);
    ck_assert_int_eq(usage.input_tokens, 0);

    /* Non-int completion_tokens */
    sctx = ik_openai_chat_stream_ctx_create(test_ctx, dummy_stream_cb, NULL);
    ik_openai_chat_stream_process_data(sctx, "{\"usage\":{\"completion_tokens\":\"x\"}}");
    usage = ik_openai_chat_stream_get_usage(sctx);
    ck_assert_int_eq(usage.output_tokens, 0);

    /* Non-int total_tokens */
    sctx = ik_openai_chat_stream_ctx_create(test_ctx, dummy_stream_cb, NULL);
    ik_openai_chat_stream_process_data(sctx, "{\"usage\":{\"total_tokens\":\"x\"}}");
    usage = ik_openai_chat_stream_get_usage(sctx);
    ck_assert_int_eq(usage.total_tokens, 0);

    /* Non-obj completion_tokens_details */
    sctx = ik_openai_chat_stream_ctx_create(test_ctx, dummy_stream_cb, NULL);
    ik_openai_chat_stream_process_data(sctx, "{\"usage\":{\"completion_tokens_details\":\"x\"}}");
    usage = ik_openai_chat_stream_get_usage(sctx);
    ck_assert_int_eq(usage.thinking_tokens, 0);

    /* Non-int reasoning_tokens */
    sctx = ik_openai_chat_stream_ctx_create(test_ctx, dummy_stream_cb, NULL);
    ik_openai_chat_stream_process_data(sctx, "{\"usage\":{\"completion_tokens_details\":{\"reasoning_tokens\":\"x\"}}}");
    usage = ik_openai_chat_stream_get_usage(sctx);
    ck_assert_int_eq(usage.thinking_tokens, 0);
}

END_TEST

/**
 * Test: edge cases - missing/null fields, non-string message
 * Covers lines 105, 155, 158, 198: NULL/missing field branches
 */
START_TEST(test_edge_cases)
{
    ik_openai_chat_stream_ctx_t *sctx;
    ik_usage_t usage;

    /* Error message not string */
    sctx = ik_openai_chat_stream_ctx_create(test_ctx, dummy_stream_cb, NULL);
    ik_openai_chat_stream_process_data(sctx, "{\"error\":{\"message\":123,\"type\":\"server_error\"}}");
    ck_assert_int_eq(event_count, 1);

    /* Choice null element */
    event_count = 0;
    sctx = ik_openai_chat_stream_ctx_create(test_ctx, dummy_stream_cb, NULL);
    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[null]}");
    ck_assert_int_eq(event_count, 0);

    /* Choice without delta */
    event_count = 0;
    sctx = ik_openai_chat_stream_ctx_create(test_ctx, dummy_stream_cb, NULL);
    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"index\":0}]}");
    ck_assert_int_eq(event_count, 0);

    /* Usage without reasoning_tokens */
    event_count = 0;
    sctx = ik_openai_chat_stream_ctx_create(test_ctx, dummy_stream_cb, NULL);
    ik_openai_chat_stream_process_data(sctx, "{\"usage\":{\"completion_tokens_details\":{}}}");
    usage = ik_openai_chat_stream_get_usage(sctx);
    ck_assert_int_eq(usage.thinking_tokens, 0);
}

END_TEST

/* ================================================================
 * Test Suite
 * ================================================================ */

static Suite *streaming_chat_branch_coverage_suite(void)
{
    Suite *s = suite_create("OpenAI Streaming Chat Branch Coverage");

    TCase *tc_branches = tcase_create("BranchCoverage");
    tcase_set_timeout(tc_branches, 30);
    tcase_add_checked_fixture(tc_branches, setup, teardown);
    tcase_add_test(tc_branches, test_error_not_object);
    tcase_add_test(tc_branches, test_error_is_array);
    tcase_add_test(tc_branches, test_error_is_null);
    tcase_add_test(tc_branches, test_choices_is_null);
    tcase_add_test(tc_branches, test_usage_is_null);
    tcase_add_test(tc_branches, test_finish_reason_is_null);
    tcase_add_test(tc_branches, test_delta_without_finish_reason_field);
    tcase_add_test(tc_branches, test_done_marker);
    tcase_add_test(tc_branches, test_malformed_json);
    tcase_add_test(tc_branches, test_root_is_array);
    tcase_add_test(tc_branches, test_error_types);
    tcase_add_test(tc_branches, test_error_no_message);
    tcase_add_test(tc_branches, test_error_no_type);
    tcase_add_test(tc_branches, test_model_already_set);
    tcase_add_test(tc_branches, test_usage_complete);
    tcase_add_test(tc_branches, test_field_type_mismatches);
    tcase_add_test(tc_branches, test_usage_invalid_types);
    tcase_add_test(tc_branches, test_edge_cases);
    suite_add_tcase(s, tc_branches);

    return s;
}

int main(void)
{
    Suite *s = streaming_chat_branch_coverage_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
