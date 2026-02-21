#include "tests/test_constants.h"
/**
 * @file anthropic_request_test_2.c
 * @brief Unit tests for Anthropic request serialization - Part 2: Branch coverage
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "apps/ikigai/providers/anthropic/request.h"
#include "apps/ikigai/providers/provider.h"
#include "apps/ikigai/providers/provider_types.h"
#include "vendor/yyjson/yyjson.h"

static TALLOC_CTX *test_ctx;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

/* ================================================================
 * Helper Functions
 * ================================================================ */

static ik_request_t *create_basic_request(TALLOC_CTX *ctx)
{
    ik_request_t *req = talloc_zero(ctx, ik_request_t);
    req->model = talloc_strdup(req, "claude-3-5-sonnet-20241022");
    req->max_output_tokens = 1024;
    req->thinking.level = IK_THINKING_MIN;

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
 * Branch Coverage Tests
 * ================================================================ */

START_TEST(test_serialize_request_thinking_budget_negative) {
    ik_request_t *req = create_basic_request(test_ctx);
    // Use a non-Claude model that returns -1 for thinking budget
    talloc_free(req->model);
    req->model = talloc_strdup(req, "gpt-4o");  // Non-Claude model doesn't support Anthropic thinking
    req->thinking.level = IK_THINKING_HIGH;

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request_stream(test_ctx, req, &json);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(json);

    // Parse and validate JSON structure
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);

    // Thinking should not be present when budget is -1
    yyjson_val *thinking = yyjson_obj_get(root, "thinking");
    ck_assert_ptr_null(thinking);

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_request_max_tokens_exceeds_budget) {
    ik_request_t *req = create_basic_request(test_ctx);
    req->thinking.level = IK_THINKING_LOW;
    req->max_output_tokens = 100000;  // Very large, exceeds thinking budget

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request_stream(test_ctx, req, &json);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(json);

    // Parse and validate JSON structure
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);

    // max_tokens should remain as specified since it's already larger than budget
    yyjson_val *max_tokens = yyjson_obj_get(root, "max_tokens");
    ck_assert_ptr_nonnull(max_tokens);
    ck_assert_int_eq(yyjson_get_int(max_tokens), 100000);

    yyjson_doc_free(doc);
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *anthropic_request_suite_2(void)
{
    Suite *s = suite_create("Anthropic Request - Part 2");

    TCase *tc_branch = tcase_create("Branch Coverage");
    tcase_set_timeout(tc_branch, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_branch, setup, teardown);
    tcase_add_test(tc_branch, test_serialize_request_thinking_budget_negative);
    tcase_add_test(tc_branch, test_serialize_request_max_tokens_exceeds_budget);
    suite_add_tcase(s, tc_branch);

    return s;
}

int main(void)
{
    Suite *s = anthropic_request_suite_2();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/anthropic/anthropic_request_2_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
