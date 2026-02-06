#include "tests/test_constants.h"
/**
 * @file anthropic_request_test_3.c
 * @brief Unit tests for Anthropic request serialization - Part 3: Adaptive thinking (opus-4-6)
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
 * Adaptive Thinking Tests (claude-opus-4-6)
 * ================================================================ */

START_TEST(test_serialize_request_opus_4_6_adaptive_thinking_none) {
    ik_request_t *req = create_basic_request(test_ctx);
    talloc_free(req->model);
    req->model = talloc_strdup(req, "claude-opus-4-6");
    req->thinking.level = IK_THINKING_NONE;

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request_stream(test_ctx, req, &json);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(json);

    // Parse and validate JSON structure
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *thinking = yyjson_obj_get(root, "thinking");
    ck_assert_ptr_null(thinking); // NONE -> omit thinking parameter

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_request_opus_4_6_adaptive_thinking_low) {
    ik_request_t *req = create_basic_request(test_ctx);
    talloc_free(req->model);
    req->model = talloc_strdup(req, "claude-opus-4-6");
    req->thinking.level = IK_THINKING_LOW;

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request_stream(test_ctx, req, &json);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(json);

    // Parse and validate JSON structure
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *thinking = yyjson_obj_get(root, "thinking");
    ck_assert_ptr_nonnull(thinking);

    yyjson_val *type = yyjson_obj_get(thinking, "type");
    ck_assert_ptr_nonnull(type);
    ck_assert_str_eq(yyjson_get_str(type), "adaptive");

    // Should NOT have budget_tokens field
    yyjson_val *budget = yyjson_obj_get(thinking, "budget_tokens");
    ck_assert_ptr_null(budget);

    // Effort is in output_config, not inside thinking
    yyjson_val *output_config = yyjson_obj_get(root, "output_config");
    ck_assert_ptr_nonnull(output_config);

    yyjson_val *effort = yyjson_obj_get(output_config, "effort");
    ck_assert_ptr_nonnull(effort);
    ck_assert_str_eq(yyjson_get_str(effort), "low");

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_request_opus_4_6_adaptive_thinking_med) {
    ik_request_t *req = create_basic_request(test_ctx);
    talloc_free(req->model);
    req->model = talloc_strdup(req, "claude-opus-4-6");
    req->thinking.level = IK_THINKING_MED;

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request_stream(test_ctx, req, &json);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(json);

    // Parse and validate JSON structure
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *thinking = yyjson_obj_get(root, "thinking");
    ck_assert_ptr_nonnull(thinking);

    yyjson_val *type = yyjson_obj_get(thinking, "type");
    ck_assert_ptr_nonnull(type);
    ck_assert_str_eq(yyjson_get_str(type), "adaptive");

    // Effort is in output_config, not inside thinking
    yyjson_val *output_config = yyjson_obj_get(root, "output_config");
    ck_assert_ptr_nonnull(output_config);

    yyjson_val *effort = yyjson_obj_get(output_config, "effort");
    ck_assert_ptr_nonnull(effort);
    ck_assert_str_eq(yyjson_get_str(effort), "medium");

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_request_opus_4_6_adaptive_thinking_high) {
    ik_request_t *req = create_basic_request(test_ctx);
    talloc_free(req->model);
    req->model = talloc_strdup(req, "claude-opus-4-6");
    req->thinking.level = IK_THINKING_HIGH;

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request_stream(test_ctx, req, &json);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(json);

    // Parse and validate JSON structure
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *thinking = yyjson_obj_get(root, "thinking");
    ck_assert_ptr_nonnull(thinking);

    yyjson_val *type = yyjson_obj_get(thinking, "type");
    ck_assert_ptr_nonnull(type);
    ck_assert_str_eq(yyjson_get_str(type), "adaptive");

    // Effort is in output_config, not inside thinking
    yyjson_val *output_config = yyjson_obj_get(root, "output_config");
    ck_assert_ptr_nonnull(output_config);

    yyjson_val *effort = yyjson_obj_get(output_config, "effort");
    ck_assert_ptr_nonnull(effort);
    ck_assert_str_eq(yyjson_get_str(effort), "high");

    yyjson_doc_free(doc);
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *anthropic_request_suite_3(void)
{
    Suite *s = suite_create("Anthropic Request - Part 3");

    TCase *tc_adaptive = tcase_create("Adaptive Thinking");
    tcase_set_timeout(tc_adaptive, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_adaptive, setup, teardown);
    tcase_add_test(tc_adaptive, test_serialize_request_opus_4_6_adaptive_thinking_none);
    tcase_add_test(tc_adaptive, test_serialize_request_opus_4_6_adaptive_thinking_low);
    tcase_add_test(tc_adaptive, test_serialize_request_opus_4_6_adaptive_thinking_med);
    tcase_add_test(tc_adaptive, test_serialize_request_opus_4_6_adaptive_thinking_high);
    suite_add_tcase(s, tc_adaptive);

    return s;
}

int main(void)
{
    Suite *s = anthropic_request_suite_3();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/anthropic/anthropic_request_3_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
