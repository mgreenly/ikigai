#include "tests/test_constants.h"
/**
 * @file streaming_chat_edge_coverage_test.c
 * @brief Edge case tests for streaming_chat.c branch coverage
 *
 * Targets uncovered branches in ik_openai_chat_stream_build_response:
 * - Line 71: tool_id != NULL && tool_name == NULL
 * - Line 85: current_tool_args == NULL
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "apps/ikigai/providers/openai/streaming_chat_internal.h"
#include "apps/ikigai/providers/openai/streaming.h"
#include "apps/ikigai/providers/provider.h"

/* ================================================================
 * Test Context
 * ================================================================ */

static TALLOC_CTX *test_ctx;

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
 * Edge Case Tests
 * ================================================================ */

/**
 * Test: Build response when tool_id is set but tool_name is NULL
 *
 * This covers the branch where current_tool_id != NULL but current_tool_name == NULL
 * (line 71, branch 3)
 */
START_TEST(test_build_response_tool_id_without_name) {
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    // Manually set tool_id without tool_name (simulates incomplete/malformed streaming)
    sctx->current_tool_id = talloc_strdup(sctx, "call_orphan_id");
    sctx->current_tool_name = NULL;  // Explicitly NULL
    sctx->current_tool_args = NULL;

    // Build response - should NOT include tool call (requires both id AND name)
    ik_response_t *resp = ik_openai_chat_stream_build_response(test_ctx, sctx);

    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 0);
    ck_assert_ptr_null(resp->content_blocks);
}

END_TEST

/**
 * Test: Build response with tool call but NULL arguments
 *
 * This covers the branch where current_tool_args == NULL (line 85, branch 1)
 * Should default to "{}"
 */
START_TEST(test_build_response_tool_call_null_args) {
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    // Manually set tool call with NULL arguments
    sctx->current_tool_id = talloc_strdup(sctx, "call_no_args");
    sctx->current_tool_name = talloc_strdup(sctx, "no_arg_tool");
    sctx->current_tool_args = NULL;  // Explicitly NULL (not empty string)

    // Build response - arguments should default to "{}"
    ik_response_t *resp = ik_openai_chat_stream_build_response(test_ctx, sctx);

    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 1);
    ck_assert_ptr_nonnull(resp->content_blocks);
    ck_assert_str_eq(resp->content_blocks[0].data.tool_call.id, "call_no_args");
    ck_assert_str_eq(resp->content_blocks[0].data.tool_call.name, "no_arg_tool");
    ck_assert_str_eq(resp->content_blocks[0].data.tool_call.arguments, "{}");
}

END_TEST

/* ================================================================
 * Test Suite
 * ================================================================ */

static Suite *streaming_chat_edge_coverage_suite(void)
{
    Suite *s = suite_create("OpenAI Streaming Chat Edge Coverage");

    TCase *tc_edge = tcase_create("EdgeCases");
    tcase_set_timeout(tc_edge, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_edge, setup, teardown);
    tcase_add_test(tc_edge, test_build_response_tool_id_without_name);
    tcase_add_test(tc_edge, test_build_response_tool_call_null_args);
    suite_add_tcase(s, tc_edge);

    return s;
}

int main(void)
{
    Suite *s = streaming_chat_edge_coverage_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/openai/streaming_chat_edge_coverage_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
