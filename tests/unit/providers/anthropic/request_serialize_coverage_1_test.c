#include "../../../test_constants.h"
/**
 * @file request_serialize_coverage_test_1.c
 * @brief Coverage tests for Anthropic request serialization - Part 1: Content Blocks
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
 * Mock Overrides
 * ================================================================ */

static bool mock_yyjson_mut_obj_fail = false;
static bool mock_yyjson_mut_obj_add_str_fail = false;
static bool mock_yyjson_mut_arr_add_val_fail = false;
static bool mock_yyjson_mut_obj_add_val_fail = false;
static bool mock_yyjson_val_mut_copy_fail = false;

yyjson_mut_val *yyjson_mut_obj_(yyjson_mut_doc *doc)
{
    if (mock_yyjson_mut_obj_fail) {
        return NULL;
    }
    return yyjson_mut_obj(doc);
}

bool yyjson_mut_obj_add_str_(yyjson_mut_doc *doc, yyjson_mut_val *obj,
                             const char *key, const char *val)
{
    if (mock_yyjson_mut_obj_add_str_fail) {
        return false;
    }
    return yyjson_mut_obj_add_str(doc, obj, key, val);
}

bool yyjson_mut_arr_add_val_(yyjson_mut_val *arr, yyjson_mut_val *val)
{
    if (mock_yyjson_mut_arr_add_val_fail) {
        return false;
    }
    return yyjson_mut_arr_add_val(arr, val);
}

bool yyjson_mut_obj_add_val_(yyjson_mut_doc *doc, yyjson_mut_val *obj,
                             const char *key, yyjson_mut_val *val)
{
    if (mock_yyjson_mut_obj_add_val_fail) {
        return false;
    }
    return yyjson_mut_obj_add_val(doc, obj, key, val);
}

yyjson_mut_val *yyjson_val_mut_copy_(yyjson_mut_doc *doc, yyjson_val *val)
{
    if (mock_yyjson_val_mut_copy_fail) {
        return NULL;
    }
    return yyjson_val_mut_copy(doc, val);
}

/* ================================================================
 * Helper Functions
 * ================================================================ */

static void reset_mocks(void)
{
    mock_yyjson_mut_obj_fail = false;
    mock_yyjson_mut_obj_add_str_fail = false;
    mock_yyjson_mut_arr_add_val_fail = false;
    mock_yyjson_mut_obj_add_val_fail = false;
    mock_yyjson_val_mut_copy_fail = false;
}

/* ================================================================
 * Content Block Serialization - OOM Tests
 * ================================================================ */

START_TEST(test_serialize_content_block_obj_alloc_fail) {
    reset_mocks();

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block;
    block.type = IK_CONTENT_TEXT;
    block.data.text.text = talloc_strdup(test_ctx, "Hello");

    mock_yyjson_mut_obj_fail = true;

    bool result = ik_anthropic_serialize_content_block(doc, arr, &block);

    ck_assert(!result);

    yyjson_mut_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_content_block_text_type_fail) {
    reset_mocks();

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block;
    block.type = IK_CONTENT_TEXT;
    block.data.text.text = talloc_strdup(test_ctx, "Hello");

    mock_yyjson_mut_obj_add_str_fail = true;

    bool result = ik_anthropic_serialize_content_block(doc, arr, &block);

    ck_assert(!result);

    yyjson_mut_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_content_block_thinking_type_fail) {
    reset_mocks();

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block;
    block.type = IK_CONTENT_THINKING;
    block.data.thinking.text = talloc_strdup(test_ctx, "Thinking...");
    block.data.thinking.signature = NULL;

    mock_yyjson_mut_obj_add_str_fail = true;

    bool result = ik_anthropic_serialize_content_block(doc, arr, &block);

    ck_assert(!result);

    yyjson_mut_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_content_block_tool_call_type_fail) {
    reset_mocks();

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block;
    block.type = IK_CONTENT_TOOL_CALL;
    block.data.tool_call.id = talloc_strdup(test_ctx, "call_123");
    block.data.tool_call.name = talloc_strdup(test_ctx, "test_tool");
    block.data.tool_call.arguments = talloc_strdup(test_ctx, "{}");

    mock_yyjson_mut_obj_add_str_fail = true;

    bool result = ik_anthropic_serialize_content_block(doc, arr, &block);

    ck_assert(!result);

    yyjson_mut_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_content_block_tool_call_copy_fail) {
    reset_mocks();

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block;
    block.type = IK_CONTENT_TOOL_CALL;
    block.data.tool_call.id = talloc_strdup(test_ctx, "call_123");
    block.data.tool_call.name = talloc_strdup(test_ctx, "test_tool");
    block.data.tool_call.arguments = talloc_strdup(test_ctx, "{\"key\":\"value\"}");

    mock_yyjson_val_mut_copy_fail = true;

    bool result = ik_anthropic_serialize_content_block(doc, arr, &block);

    ck_assert(!result);

    yyjson_mut_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_content_block_tool_call_add_input_fail) {
    reset_mocks();

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block;
    block.type = IK_CONTENT_TOOL_CALL;
    block.data.tool_call.id = talloc_strdup(test_ctx, "call_123");
    block.data.tool_call.name = talloc_strdup(test_ctx, "test_tool");
    block.data.tool_call.arguments = talloc_strdup(test_ctx, "{\"key\":\"value\"}");

    mock_yyjson_mut_obj_add_val_fail = true;

    bool result = ik_anthropic_serialize_content_block(doc, arr, &block);

    ck_assert(!result);

    yyjson_mut_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_content_block_tool_result_type_fail) {
    reset_mocks();

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block;
    block.type = IK_CONTENT_TOOL_RESULT;
    block.data.tool_result.tool_call_id = talloc_strdup(test_ctx, "call_123");
    block.data.tool_result.content = talloc_strdup(test_ctx, "result");
    block.data.tool_result.is_error = false;

    mock_yyjson_mut_obj_add_str_fail = true;

    bool result = ik_anthropic_serialize_content_block(doc, arr, &block);

    ck_assert(!result);

    yyjson_mut_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_content_block_arr_add_fail) {
    reset_mocks();

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block;
    block.type = IK_CONTENT_TEXT;
    block.data.text.text = talloc_strdup(test_ctx, "Hello");

    mock_yyjson_mut_arr_add_val_fail = true;

    bool result = ik_anthropic_serialize_content_block(doc, arr, &block);

    ck_assert(!result);

    yyjson_mut_doc_free(doc);
}
END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *request_serialize_coverage_suite_1(void)
{
    Suite *s = suite_create("Anthropic Request Serialize Coverage - Part 1");

    TCase *tc_content = tcase_create("Content Block OOM");
    tcase_set_timeout(tc_content, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_content, setup, teardown);
    tcase_add_test(tc_content, test_serialize_content_block_obj_alloc_fail);
    tcase_add_test(tc_content, test_serialize_content_block_text_type_fail);
    tcase_add_test(tc_content, test_serialize_content_block_thinking_type_fail);
    tcase_add_test(tc_content, test_serialize_content_block_tool_call_type_fail);
    tcase_add_test(tc_content, test_serialize_content_block_tool_call_copy_fail);
    tcase_add_test(tc_content, test_serialize_content_block_tool_call_add_input_fail);
    tcase_add_test(tc_content, test_serialize_content_block_tool_result_type_fail);
    tcase_add_test(tc_content, test_serialize_content_block_arr_add_fail);
    suite_add_tcase(s, tc_content);

    return s;
}

int main(void)
{
    Suite *s = request_serialize_coverage_suite_1();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
