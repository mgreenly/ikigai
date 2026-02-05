#include "tests/test_constants.h"
/**
 * @file request_coverage_test.c
 * @brief Coverage tests for gaps in src/providers/anthropic/request.c
 *
 * This test file targets specific uncovered lines and branches:
 * - Lines 231-233: Non-streaming version of serialize_request
 * - Line 77 branch 1: stream parameter is false (non-streaming path)
 * - Lines 94-95: Error handling when ik_anthropic_serialize_messages fails
 * - Line 93 branch 0: ik_anthropic_serialize_messages returns false
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "apps/ikigai/providers/anthropic/request.h"
#include "apps/ikigai/providers/anthropic/request_serialize.h"
#include "apps/ikigai/providers/provider.h"
#include "apps/ikigai/providers/provider_types.h"
#include "vendor/yyjson/yyjson.h"
#include "shared/wrapper.h"

static TALLOC_CTX *test_ctx;

// Note: serialize_request_internal is static and cannot be tested directly
// We need to find another way to test the non-streaming path

static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

/* ================================================================
 * Mock Override for JSON Operations Failure
 * ================================================================ */

static bool mock_yyjson_should_fail = false;
static int mock_yyjson_fail_count = 0;
static int mock_yyjson_call_count = 0;

// Override yyjson_mut_arr_ to fail when configured
yyjson_mut_val *yyjson_mut_arr_(yyjson_mut_doc *doc)
{
    mock_yyjson_call_count++;
    if (mock_yyjson_should_fail && mock_yyjson_call_count > mock_yyjson_fail_count) {
        return NULL;  // Simulate allocation failure
    }
    return yyjson_mut_arr(doc);
}

/* ================================================================
 * Helper Functions
 * ================================================================ */

static ik_request_t *create_basic_request(TALLOC_CTX *ctx)
{
    ik_request_t *req = talloc_zero(ctx, ik_request_t);
    req->model = talloc_strdup(req, "claude-3-5-sonnet-20241022");
    req->max_output_tokens = 1024;
    req->thinking.level = IK_THINKING_NONE;

    // Add one simple message
    req->message_count = 1;
    req->messages = talloc_array(req, ik_message_t, 1);
    req->messages[0].role = IK_ROLE_USER;
    req->messages[0].content_count = 1;
    req->messages[0].content_blocks = talloc_array(req, ik_content_block_t, 1);
    req->messages[0].content_blocks[0].type = IK_CONTENT_TEXT;
    req->messages[0].content_blocks[0].data.text.text = talloc_strdup(req, "Hello");

    return req;
}

/* ================================================================
 * Coverage Tests for Missing Branches and Lines
 * ================================================================ */

START_TEST(test_serialize_messages_failure) {
    // Test lines 94-95: Error handling when ik_anthropic_serialize_messages fails
    // Test line 93 branch 0: ik_anthropic_serialize_messages returns false

    // Configure mock to fail after the initial arrays are created
    // The failure will happen in ik_anthropic_serialize_messages when it tries to create messages array
    mock_yyjson_should_fail = true;
    mock_yyjson_fail_count = 0;  // Fail on first call within ik_anthropic_serialize_messages
    mock_yyjson_call_count = 0;  // Reset call counter

    ik_request_t *req = create_basic_request(test_ctx);
    char *json = NULL;

    res_t r = ik_anthropic_serialize_request_stream(test_ctx, req, &json);

    // Should return error when message serialization fails
    ck_assert(is_err(&r));
    ck_assert_str_eq(r.err->msg, "Failed to serialize messages");
    ck_assert_ptr_null(json);

    // Reset mock for other tests
    mock_yyjson_should_fail = false;
    mock_yyjson_call_count = 0;
}
END_TEST

START_TEST(test_serialize_request_streaming_with_verification) {
    // Test line 77 branch 1: Ensure stream=true path is tested

    ik_request_t *req = create_basic_request(test_ctx);
    char *json = NULL;

    // Call the streaming version
    res_t r = ik_anthropic_serialize_request_stream(test_ctx, req, &json);

    // Should succeed
    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(json);

    // Parse the JSON to verify it contains stream field set to true
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *stream_field = yyjson_obj_get(root, "stream");

    // Streaming request MUST have stream field set to true
    ck_assert_ptr_nonnull(stream_field);
    ck_assert(yyjson_is_bool(stream_field));
    ck_assert(yyjson_get_bool(stream_field) == true);

    yyjson_doc_free(doc);
}
END_TEST

// Note: We cannot test the non-streaming path directly as serialize_request_internal
// is a static function. The non-streaming path appears to be dead code since
// only ik_anthropic_serialize_request_stream is exposed in the public API.

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *request_coverage_suite(void)
{
    Suite *s = suite_create("Anthropic Request Coverage Gaps");

    TCase *tc_coverage = tcase_create("Missing Coverage");
    tcase_set_timeout(tc_coverage, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_coverage, setup, teardown);
    tcase_add_test(tc_coverage, test_serialize_messages_failure);
    tcase_add_test(tc_coverage, test_serialize_request_streaming_with_verification);
    suite_add_tcase(s, tc_coverage);

    return s;
}

int main(void)
{
    Suite *s = request_coverage_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/anthropic/request_coverage_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
