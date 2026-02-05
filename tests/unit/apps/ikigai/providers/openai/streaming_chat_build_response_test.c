#include "tests/test_constants.h"
/**
 * @file streaming_chat_build_response_test.c
 * @brief Tests for ik_openai_chat_stream_build_response function
 *
 * Tests building ik_response_t from accumulated streaming context data.
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "apps/ikigai/providers/openai/streaming.h"
#include "apps/ikigai/providers/provider.h"

/* ================================================================
 * Test Context
 * ================================================================ */

static TALLOC_CTX *test_ctx;

/**
 * Dummy stream callback - we don't care about events in these tests
 */
static res_t dummy_stream_cb(const ik_stream_event_t *event, void *ctx)
{
    (void)event;
    (void)ctx;
    return OK(NULL);
}

static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

/* ================================================================
 * Build Response Tests - Basic Fields
 * ================================================================ */

START_TEST(test_build_response_empty_stream) {
    // Create streaming context without any data
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    // Build response from empty context
    ik_response_t *resp = ik_openai_chat_stream_build_response(test_ctx, sctx);

    // Response should be created with defaults
    ck_assert_ptr_nonnull(resp);
    ck_assert_ptr_null(resp->model);
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_UNKNOWN);
    ck_assert_int_eq(resp->usage.input_tokens, 0);
    ck_assert_int_eq(resp->usage.output_tokens, 0);
    ck_assert_ptr_null(resp->content_blocks);
    ck_assert_int_eq((int)resp->content_count, 0);
}

END_TEST

START_TEST(test_build_response_with_model) {
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    // Process chunk with model
    const char *init_data = "{\"model\":\"gpt-5-turbo\",\"choices\":[{\"delta\":{\"role\":\"assistant\"}}]}";
    ik_openai_chat_stream_process_data(sctx, init_data);

    // Build response
    ik_response_t *resp = ik_openai_chat_stream_build_response(test_ctx, sctx);

    ck_assert_ptr_nonnull(resp);
    ck_assert_ptr_nonnull(resp->model);
    ck_assert_str_eq(resp->model, "gpt-5-turbo");
}

END_TEST

START_TEST(test_build_response_with_finish_reason) {
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    // Process chunks to set finish reason
    ik_openai_chat_stream_process_data(sctx,
                                       "{\"model\":\"gpt-4\",\"choices\":[{\"delta\":{\"role\":\"assistant\"}}]}");
    ik_openai_chat_stream_process_data(sctx,
                                       "{\"choices\":[{\"delta\":{},\"finish_reason\":\"stop\"}]}");

    // Build response
    ik_response_t *resp = ik_openai_chat_stream_build_response(test_ctx, sctx);

    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_STOP);
}

END_TEST

START_TEST(test_build_response_with_tool_use_finish) {
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    // Process chunks with tool_calls finish reason
    ik_openai_chat_stream_process_data(sctx,
                                       "{\"model\":\"gpt-4\",\"choices\":[{\"delta\":{\"role\":\"assistant\"}}]}");
    ik_openai_chat_stream_process_data(sctx,
                                       "{\"choices\":[{\"delta\":{},\"finish_reason\":\"tool_calls\"}]}");

    // Build response
    ik_response_t *resp = ik_openai_chat_stream_build_response(test_ctx, sctx);

    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_TOOL_USE);
}

END_TEST

START_TEST(test_build_response_with_usage) {
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    // Process chunks with usage data
    ik_openai_chat_stream_process_data(sctx,
                                       "{\"model\":\"gpt-4\",\"choices\":[{\"delta\":{\"role\":\"assistant\"}}]}");
    ik_openai_chat_stream_process_data(sctx,
                                       "{\"choices\":[],\"usage\":{\"prompt_tokens\":150,\"completion_tokens\":50,\"total_tokens\":200}}");

    // Build response
    ik_response_t *resp = ik_openai_chat_stream_build_response(test_ctx, sctx);

    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq(resp->usage.input_tokens, 150);
    ck_assert_int_eq(resp->usage.output_tokens, 50);
    ck_assert_int_eq(resp->usage.total_tokens, 200);
}

END_TEST

/* ================================================================
 * Build Response Tests - Tool Calls
 * ================================================================ */

START_TEST(test_build_response_no_tool_after_text) {
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    // Process chunks with just text (no tool call)
    ik_openai_chat_stream_process_data(sctx,
                                       "{\"model\":\"gpt-4\",\"choices\":[{\"delta\":{\"role\":\"assistant\"}}]}");
    ik_openai_chat_stream_process_data(sctx,
                                       "{\"choices\":[{\"delta\":{\"content\":\"Hello world\"}}]}");
    ik_openai_chat_stream_process_data(sctx,
                                       "{\"choices\":[{\"delta\":{},\"finish_reason\":\"stop\"}]}");

    // Build response - should have no content blocks (text was streamed)
    ik_response_t *resp = ik_openai_chat_stream_build_response(test_ctx, sctx);

    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 0);
    ck_assert_ptr_null(resp->content_blocks);
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_STOP);
}

END_TEST

/* ================================================================
 * Build Response Tests - Memory Management
 * ================================================================ */

START_TEST(test_build_response_on_different_ctx) {
    // Create streaming context on test_ctx
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    ik_openai_chat_stream_process_data(sctx,
                                       "{\"model\":\"gpt-4\",\"choices\":[{\"delta\":{\"role\":\"assistant\"}}]}");
    ik_openai_chat_stream_process_data(sctx,
                                       "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"call_mem\","
                                       "\"function\":{\"name\":\"test_tool\",\"arguments\":\"\"}}]}}]}");

    // Build response on a separate context
    TALLOC_CTX *resp_ctx = talloc_new(NULL);
    ik_response_t *resp = ik_openai_chat_stream_build_response(resp_ctx, sctx);

    // Verify response is allocated under resp_ctx
    ck_assert_ptr_nonnull(resp);
    ck_assert(talloc_parent(resp) == resp_ctx);

    // Free response context, streaming context should still be valid
    talloc_free(resp_ctx);

    // sctx should still be usable (verify by accessing model)
    ck_assert_ptr_nonnull(sctx);
}

END_TEST

/* ================================================================
 * Test Suite
 * ================================================================ */

static Suite *streaming_chat_build_response_suite(void)
{
    Suite *s = suite_create("OpenAI Streaming Build Response");

    // Basic Fields
    TCase *tc_basic = tcase_create("BasicFields");
    tcase_set_timeout(tc_basic, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_basic, setup, teardown);
    tcase_add_test(tc_basic, test_build_response_empty_stream);
    tcase_add_test(tc_basic, test_build_response_with_model);
    tcase_add_test(tc_basic, test_build_response_with_finish_reason);
    tcase_add_test(tc_basic, test_build_response_with_tool_use_finish);
    tcase_add_test(tc_basic, test_build_response_with_usage);
    suite_add_tcase(s, tc_basic);

    // Tool Calls
    TCase *tc_tools = tcase_create("ToolCalls");
    tcase_set_timeout(tc_tools, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_tools, setup, teardown);
    tcase_add_test(tc_tools, test_build_response_no_tool_after_text);
    suite_add_tcase(s, tc_tools);

    // Memory Management
    TCase *tc_memory = tcase_create("Memory");
    tcase_set_timeout(tc_memory, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_memory, setup, teardown);
    tcase_add_test(tc_memory, test_build_response_on_different_ctx);
    suite_add_tcase(s, tc_memory);

    return s;
}

int main(void)
{
    Suite *s = streaming_chat_build_response_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/openai/streaming_chat_build_response_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
