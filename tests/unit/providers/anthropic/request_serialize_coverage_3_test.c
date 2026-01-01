/**
 * @file request_serialize_coverage_test_3.c
 * @brief Coverage tests for Anthropic request serialization - Part 3: Specific Branch Coverage
 *
 * This test file targets specific uncovered branches in the serialization code,
 * particularly the second and subsequent field additions for each content type.
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/anthropic/request_serialize.h"
#include "providers/provider.h"
#include "wrapper_json.h"

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
 * Mock Overrides with Call Counter
 * ================================================================ */

static int mock_yyjson_mut_obj_add_str_fail_on_call = -1;
static int mock_yyjson_mut_obj_add_str_call_count = 0;
static int mock_yyjson_mut_obj_add_bool_fail_on_call = -1;
static int mock_yyjson_mut_obj_add_bool_call_count = 0;

bool yyjson_mut_obj_add_str_(yyjson_mut_doc *doc, yyjson_mut_val *obj,
                             const char *key, const char *val)
{
    mock_yyjson_mut_obj_add_str_call_count++;
    if (mock_yyjson_mut_obj_add_str_fail_on_call == mock_yyjson_mut_obj_add_str_call_count) {
        return false;
    }
    return yyjson_mut_obj_add_str(doc, obj, key, val);
}

bool yyjson_mut_obj_add_bool_(yyjson_mut_doc *doc, yyjson_mut_val *obj,
                              const char *key, bool val)
{
    mock_yyjson_mut_obj_add_bool_call_count++;
    if (mock_yyjson_mut_obj_add_bool_fail_on_call == mock_yyjson_mut_obj_add_bool_call_count) {
        return false;
    }
    return yyjson_mut_obj_add_bool(doc, obj, key, val);
}

/* ================================================================
 * Helper Functions
 * ================================================================ */

static void reset_mocks(void)
{
    mock_yyjson_mut_obj_add_str_fail_on_call = -1;
    mock_yyjson_mut_obj_add_str_call_count = 0;
    mock_yyjson_mut_obj_add_bool_fail_on_call = -1;
    mock_yyjson_mut_obj_add_bool_call_count = 0;
}

/* ================================================================
 * Content Block Serialization - Specific Field Failure Tests
 * ================================================================ */

START_TEST(test_serialize_content_block_text_text_field_fail) {
    reset_mocks();

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block;
    block.type = IK_CONTENT_TEXT;
    block.data.text.text = talloc_strdup(test_ctx, "Hello");

    // Fail on the 2nd call (adding "text" field, after "type" succeeds)
    mock_yyjson_mut_obj_add_str_fail_on_call = 2;

    bool result = ik_anthropic_serialize_content_block(doc, arr, &block);

    ck_assert(!result);

    yyjson_mut_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_content_block_thinking_thinking_field_fail) {
    reset_mocks();

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block;
    block.type = IK_CONTENT_THINKING;
    block.data.thinking.text = talloc_strdup(test_ctx, "Thinking...");

    // Fail on the 2nd call (adding "thinking" field, after "type" succeeds)
    mock_yyjson_mut_obj_add_str_fail_on_call = 2;

    bool result = ik_anthropic_serialize_content_block(doc, arr, &block);

    ck_assert(!result);

    yyjson_mut_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_content_block_tool_call_id_field_fail) {
    reset_mocks();

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block;
    block.type = IK_CONTENT_TOOL_CALL;
    block.data.tool_call.id = talloc_strdup(test_ctx, "call_123");
    block.data.tool_call.name = talloc_strdup(test_ctx, "test_tool");
    block.data.tool_call.arguments = talloc_strdup(test_ctx, "{}");

    // Fail on the 2nd call (adding "id" field, after "type" succeeds)
    mock_yyjson_mut_obj_add_str_fail_on_call = 2;

    bool result = ik_anthropic_serialize_content_block(doc, arr, &block);

    ck_assert(!result);

    yyjson_mut_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_content_block_tool_call_name_field_fail) {
    reset_mocks();

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block;
    block.type = IK_CONTENT_TOOL_CALL;
    block.data.tool_call.id = talloc_strdup(test_ctx, "call_123");
    block.data.tool_call.name = talloc_strdup(test_ctx, "test_tool");
    block.data.tool_call.arguments = talloc_strdup(test_ctx, "{}");

    // Fail on the 3rd call (adding "name" field, after "type" and "id" succeed)
    mock_yyjson_mut_obj_add_str_fail_on_call = 3;

    bool result = ik_anthropic_serialize_content_block(doc, arr, &block);

    ck_assert(!result);

    yyjson_mut_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_content_block_tool_result_tool_use_id_field_fail) {
    reset_mocks();

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block;
    block.type = IK_CONTENT_TOOL_RESULT;
    block.data.tool_result.tool_call_id = talloc_strdup(test_ctx, "call_123");
    block.data.tool_result.content = talloc_strdup(test_ctx, "result");
    block.data.tool_result.is_error = false;

    // Fail on the 2nd call (adding "tool_use_id" field, after "type" succeeds)
    mock_yyjson_mut_obj_add_str_fail_on_call = 2;

    bool result = ik_anthropic_serialize_content_block(doc, arr, &block);

    ck_assert(!result);

    yyjson_mut_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_content_block_tool_result_content_field_fail) {
    reset_mocks();

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block;
    block.type = IK_CONTENT_TOOL_RESULT;
    block.data.tool_result.tool_call_id = talloc_strdup(test_ctx, "call_123");
    block.data.tool_result.content = talloc_strdup(test_ctx, "result");
    block.data.tool_result.is_error = false;

    // Fail on the 3rd call (adding "content" field, after "type" and "tool_use_id" succeed)
    mock_yyjson_mut_obj_add_str_fail_on_call = 3;

    bool result = ik_anthropic_serialize_content_block(doc, arr, &block);

    ck_assert(!result);

    yyjson_mut_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_content_block_tool_result_is_error_field_fail) {
    reset_mocks();

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block;
    block.type = IK_CONTENT_TOOL_RESULT;
    block.data.tool_result.tool_call_id = talloc_strdup(test_ctx, "call_123");
    block.data.tool_result.content = talloc_strdup(test_ctx, "result");
    block.data.tool_result.is_error = false;

    // Fail on the 1st bool call (adding "is_error" field)
    mock_yyjson_mut_obj_add_bool_fail_on_call = 1;

    bool result = ik_anthropic_serialize_content_block(doc, arr, &block);

    ck_assert(!result);

    yyjson_mut_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_content_block_invalid_type) {
    reset_mocks();

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block;
    // Use an invalid type value outside the enum range
    block.type = (ik_content_type_t)999;

    bool result = ik_anthropic_serialize_content_block(doc, arr, &block);

    // Should return false for invalid type
    ck_assert(!result);

    yyjson_mut_doc_free(doc);
}
END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *request_serialize_coverage_suite_3(void)
{
    Suite *s = suite_create("Anthropic Request Serialize Coverage - Part 3");

    TCase *tc_specific = tcase_create("Specific Field Failures");
    tcase_set_timeout(tc_specific, 30);
    tcase_add_unchecked_fixture(tc_specific, setup, teardown);
    tcase_add_test(tc_specific, test_serialize_content_block_text_text_field_fail);
    tcase_add_test(tc_specific, test_serialize_content_block_thinking_thinking_field_fail);
    tcase_add_test(tc_specific, test_serialize_content_block_tool_call_id_field_fail);
    tcase_add_test(tc_specific, test_serialize_content_block_tool_call_name_field_fail);
    tcase_add_test(tc_specific, test_serialize_content_block_tool_result_tool_use_id_field_fail);
    tcase_add_test(tc_specific, test_serialize_content_block_tool_result_content_field_fail);
    tcase_add_test(tc_specific, test_serialize_content_block_tool_result_is_error_field_fail);
    tcase_add_test(tc_specific, test_serialize_content_block_invalid_type);
    suite_add_tcase(s, tc_specific);

    return s;
}

int main(void)
{
    Suite *s = request_serialize_coverage_suite_3();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
