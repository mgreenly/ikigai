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
