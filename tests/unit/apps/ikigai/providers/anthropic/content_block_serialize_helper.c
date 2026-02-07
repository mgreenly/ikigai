#include "tests/test_constants.h"
/**
 * @file content_block_serialize_helpers.c
 * @brief Content block serialization tests for Anthropic provider
 *
 * This file contains tests for serializing individual content blocks.
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "apps/ikigai/providers/anthropic/request_serialize.h"
#include "apps/ikigai/providers/provider.h"
#include "shared/wrapper_json.h"
#include "content_block_serialize_helper.h"

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
 * Content Block Serialization - Success Paths
 * ================================================================ */

START_TEST(test_serialize_content_block_text_success) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block;
    block.type = IK_CONTENT_TEXT;
    block.data.text.text = talloc_strdup(test_ctx, "Hello, world!");

    bool result = ik_anthropic_serialize_content_block(doc, arr, &block, 0, 0);

    ck_assert(result);
    ck_assert_int_eq((int)yyjson_mut_arr_size(arr), 1);

    // Verify the serialized content
    yyjson_mut_val *obj = yyjson_mut_arr_get(arr, 0);
    ck_assert_ptr_nonnull(obj);

    yyjson_mut_val *type = yyjson_mut_obj_get(obj, "type");
    ck_assert_str_eq(yyjson_mut_get_str(type), "text");

    yyjson_mut_val *text = yyjson_mut_obj_get(obj, "text");
    ck_assert_str_eq(yyjson_mut_get_str(text), "Hello, world!");

    yyjson_mut_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_content_block_thinking_success) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block;
    block.type = IK_CONTENT_THINKING;
    block.data.thinking.text = talloc_strdup(test_ctx, "Let me think about this...");
    block.data.thinking.signature = NULL;

    bool result = ik_anthropic_serialize_content_block(doc, arr, &block, 0, 0);

    ck_assert(result);
    ck_assert_int_eq((int)yyjson_mut_arr_size(arr), 1);

    // Verify the serialized content
    yyjson_mut_val *obj = yyjson_mut_arr_get(arr, 0);
    ck_assert_ptr_nonnull(obj);

    yyjson_mut_val *type = yyjson_mut_obj_get(obj, "type");
    ck_assert_str_eq(yyjson_mut_get_str(type), "thinking");

    yyjson_mut_val *thinking = yyjson_mut_obj_get(obj, "thinking");
    ck_assert_str_eq(yyjson_mut_get_str(thinking), "Let me think about this...");

    yyjson_mut_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_thinking_with_signature) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block;
    block.type = IK_CONTENT_THINKING;
    block.data.thinking.text = talloc_strdup(test_ctx, "Deep analysis...");
    block.data.thinking.signature = talloc_strdup(test_ctx, "EqQBCgIYAhIM...");

    bool result = ik_anthropic_serialize_content_block(doc, arr, &block, 0, 0);

    ck_assert(result);
    ck_assert_int_eq((int)yyjson_mut_arr_size(arr), 1);

    // Verify the serialized content
    yyjson_mut_val *obj = yyjson_mut_arr_get(arr, 0);
    ck_assert_ptr_nonnull(obj);

    yyjson_mut_val *type = yyjson_mut_obj_get(obj, "type");
    ck_assert_str_eq(yyjson_mut_get_str(type), "thinking");

    yyjson_mut_val *thinking = yyjson_mut_obj_get(obj, "thinking");
    ck_assert_str_eq(yyjson_mut_get_str(thinking), "Deep analysis...");

    yyjson_mut_val *signature = yyjson_mut_obj_get(obj, "signature");
    ck_assert_ptr_nonnull(signature);
    ck_assert_str_eq(yyjson_mut_get_str(signature), "EqQBCgIYAhIM...");

    yyjson_mut_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_thinking_null_signature) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block;
    block.type = IK_CONTENT_THINKING;
    block.data.thinking.text = talloc_strdup(test_ctx, "Thinking without signature...");
    block.data.thinking.signature = NULL;

    bool result = ik_anthropic_serialize_content_block(doc, arr, &block, 0, 0);

    ck_assert(result);

    // Verify no signature field when NULL
    yyjson_mut_val *obj = yyjson_mut_arr_get(arr, 0);
    ck_assert_ptr_nonnull(obj);

    yyjson_mut_val *signature = yyjson_mut_obj_get(obj, "signature");
    ck_assert_ptr_null(signature);

    yyjson_mut_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_redacted_thinking) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block;
    block.type = IK_CONTENT_REDACTED_THINKING;
    block.data.redacted_thinking.data = talloc_strdup(test_ctx, "EmwKAhgBEgy...");

    bool result = ik_anthropic_serialize_content_block(doc, arr, &block, 0, 0);

    ck_assert(result);
    ck_assert_int_eq((int)yyjson_mut_arr_size(arr), 1);

    // Verify the serialized content
    yyjson_mut_val *obj = yyjson_mut_arr_get(arr, 0);
    ck_assert_ptr_nonnull(obj);

    yyjson_mut_val *type = yyjson_mut_obj_get(obj, "type");
    ck_assert_str_eq(yyjson_mut_get_str(type), "redacted_thinking");

    yyjson_mut_val *data = yyjson_mut_obj_get(obj, "data");
    ck_assert_ptr_nonnull(data);
    ck_assert_str_eq(yyjson_mut_get_str(data), "EmwKAhgBEgy...");

    yyjson_mut_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_content_block_tool_call_success) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block;
    block.type = IK_CONTENT_TOOL_CALL;
    block.data.tool_call.id = talloc_strdup(test_ctx, "call_abc123");
    block.data.tool_call.name = talloc_strdup(test_ctx, "get_weather");
    block.data.tool_call.arguments = talloc_strdup(test_ctx, "{\"location\":\"San Francisco\"}");

    bool result = ik_anthropic_serialize_content_block(doc, arr, &block, 0, 0);

    ck_assert(result);
    ck_assert_int_eq((int)yyjson_mut_arr_size(arr), 1);

    // Verify the serialized content
    yyjson_mut_val *obj = yyjson_mut_arr_get(arr, 0);
    ck_assert_ptr_nonnull(obj);

    yyjson_mut_val *type = yyjson_mut_obj_get(obj, "type");
    ck_assert_str_eq(yyjson_mut_get_str(type), "tool_use");

    yyjson_mut_val *id = yyjson_mut_obj_get(obj, "id");
    ck_assert_str_eq(yyjson_mut_get_str(id), "call_abc123");

    yyjson_mut_val *name = yyjson_mut_obj_get(obj, "name");
    ck_assert_str_eq(yyjson_mut_get_str(name), "get_weather");

    yyjson_mut_val *input = yyjson_mut_obj_get(obj, "input");
    ck_assert_ptr_nonnull(input);

    yyjson_mut_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_content_block_tool_call_invalid_json) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block;
    block.type = IK_CONTENT_TOOL_CALL;
    block.data.tool_call.id = talloc_strdup(test_ctx, "call_xyz");
    block.data.tool_call.name = talloc_strdup(test_ctx, "test_tool");
    // Invalid JSON - missing closing brace
    block.data.tool_call.arguments = talloc_strdup(test_ctx, "{\"key\":\"value\"");

    bool result = ik_anthropic_serialize_content_block(doc, arr, &block, 0, 0);

    // Should fail because arguments are invalid JSON
    ck_assert(!result);

    yyjson_mut_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_content_block_tool_result_success) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block;
    block.type = IK_CONTENT_TOOL_RESULT;
    block.data.tool_result.tool_call_id = talloc_strdup(test_ctx, "call_abc123");
    block.data.tool_result.content = talloc_strdup(test_ctx, "Sunny, 72°F");
    block.data.tool_result.is_error = false;

    bool result = ik_anthropic_serialize_content_block(doc, arr, &block, 0, 0);

    ck_assert(result);
    ck_assert_int_eq((int)yyjson_mut_arr_size(arr), 1);

    // Verify the serialized content
    yyjson_mut_val *obj = yyjson_mut_arr_get(arr, 0);
    ck_assert_ptr_nonnull(obj);

    yyjson_mut_val *type = yyjson_mut_obj_get(obj, "type");
    ck_assert_str_eq(yyjson_mut_get_str(type), "tool_result");

    yyjson_mut_val *tool_use_id = yyjson_mut_obj_get(obj, "tool_use_id");
    ck_assert_str_eq(yyjson_mut_get_str(tool_use_id), "call_abc123");

    yyjson_mut_val *content = yyjson_mut_obj_get(obj, "content");
    ck_assert_str_eq(yyjson_mut_get_str(content), "Sunny, 72°F");

    yyjson_mut_val *is_error = yyjson_mut_obj_get(obj, "is_error");
    ck_assert(!yyjson_mut_get_bool(is_error));

    yyjson_mut_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_content_block_tool_result_with_error) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block;
    block.type = IK_CONTENT_TOOL_RESULT;
    block.data.tool_result.tool_call_id = talloc_strdup(test_ctx, "call_def456");
    block.data.tool_result.content = talloc_strdup(test_ctx, "Location not found");
    block.data.tool_result.is_error = true;

    bool result = ik_anthropic_serialize_content_block(doc, arr, &block, 0, 0);

    ck_assert(result);

    // Verify the error flag
    yyjson_mut_val *obj = yyjson_mut_arr_get(arr, 0);
    yyjson_mut_val *is_error = yyjson_mut_obj_get(obj, "is_error");
    ck_assert(yyjson_mut_get_bool(is_error));

    yyjson_mut_doc_free(doc);
}
END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

Suite *content_block_serialize_suite(void)
{
    Suite *s = suite_create("Content Block Serialization");

    TCase *tc_content = tcase_create("Content Block Success");
    tcase_set_timeout(tc_content, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_content, setup, teardown);
    tcase_add_test(tc_content, test_serialize_content_block_text_success);
    tcase_add_test(tc_content, test_serialize_content_block_thinking_success);
    tcase_add_test(tc_content, test_serialize_thinking_with_signature);
    tcase_add_test(tc_content, test_serialize_thinking_null_signature);
    tcase_add_test(tc_content, test_serialize_redacted_thinking);
    tcase_add_test(tc_content, test_serialize_content_block_tool_call_success);
    tcase_add_test(tc_content, test_serialize_content_block_tool_call_invalid_json);
    tcase_add_test(tc_content, test_serialize_content_block_tool_result_success);
    tcase_add_test(tc_content, test_serialize_content_block_tool_result_with_error);
    suite_add_tcase(s, tc_content);

    return s;
}
