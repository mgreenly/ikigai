/**
 * @file request_helpers_coverage_test.c
 * @brief Coverage tests for Google request serialization helpers
 *
 * Tests for error paths and edge cases to achieve 100% coverage.
 */

// Disable cast-qual for test literals
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/google/request_helpers.h"
#include "providers/provider.h"
#include "vendor/yyjson/yyjson.h"
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
 * Mock Failure Injection
 * ================================================================ */

static int32_t g_mock_yyjson_mut_obj_add_str_fail_after = -1;
static int32_t g_mock_yyjson_mut_obj_add_str_call_count = 0;
static bool g_mock_yyjson_mut_obj_add_bool_fail = false;
static int32_t g_mock_yyjson_mut_obj_add_val_fail_after = -1;
static int32_t g_mock_yyjson_mut_obj_add_val_call_count = 0;
static bool g_mock_yyjson_val_mut_copy_fail = false;
static int32_t g_mock_yyjson_mut_arr_add_val_fail_after = -1;
static int32_t g_mock_yyjson_mut_arr_add_val_call_count = 0;

bool yyjson_mut_obj_add_str_(yyjson_mut_doc *doc, yyjson_mut_val *obj,
                             const char *key, const char *val)
{
    if (g_mock_yyjson_mut_obj_add_str_fail_after >= 0) {
        if (g_mock_yyjson_mut_obj_add_str_call_count == g_mock_yyjson_mut_obj_add_str_fail_after) {
            g_mock_yyjson_mut_obj_add_str_call_count++;
            return false;
        }
        g_mock_yyjson_mut_obj_add_str_call_count++;
    }
    return yyjson_mut_obj_add_str(doc, obj, key, val);
}

bool yyjson_mut_obj_add_bool_(yyjson_mut_doc *doc, yyjson_mut_val *obj,
                              const char *key, bool val)
{
    if (g_mock_yyjson_mut_obj_add_bool_fail) {
        return false;
    }
    return yyjson_mut_obj_add_bool(doc, obj, key, val);
}

bool yyjson_mut_obj_add_val_(yyjson_mut_doc *doc, yyjson_mut_val *obj,
                             const char *key, yyjson_mut_val *val)
{
    if (g_mock_yyjson_mut_obj_add_val_fail_after >= 0) {
        if (g_mock_yyjson_mut_obj_add_val_call_count == g_mock_yyjson_mut_obj_add_val_fail_after) {
            g_mock_yyjson_mut_obj_add_val_call_count++;
            return false;
        }
        g_mock_yyjson_mut_obj_add_val_call_count++;
    }
    return yyjson_mut_obj_add_val(doc, obj, key, val);
}

yyjson_mut_val *yyjson_val_mut_copy_(yyjson_mut_doc *doc, yyjson_val *val)
{
    if (g_mock_yyjson_val_mut_copy_fail) {
        return NULL;
    }
    return yyjson_val_mut_copy(doc, val);
}

bool yyjson_mut_arr_add_val_(yyjson_mut_val *arr, yyjson_mut_val *val)
{
    if (g_mock_yyjson_mut_arr_add_val_fail_after >= 0) {
        if (g_mock_yyjson_mut_arr_add_val_call_count == g_mock_yyjson_mut_arr_add_val_fail_after) {
            g_mock_yyjson_mut_arr_add_val_call_count++;
            return false;
        }
        g_mock_yyjson_mut_arr_add_val_call_count++;
    }
    return yyjson_mut_arr_add_val(arr, val);
}

static void reset_mocks(void)
{
    g_mock_yyjson_mut_obj_add_str_fail_after = -1;
    g_mock_yyjson_mut_obj_add_str_call_count = 0;
    g_mock_yyjson_mut_obj_add_bool_fail = false;
    g_mock_yyjson_mut_obj_add_val_fail_after = -1;
    g_mock_yyjson_mut_obj_add_val_call_count = 0;
    g_mock_yyjson_val_mut_copy_fail = false;
    g_mock_yyjson_mut_arr_add_val_fail_after = -1;
    g_mock_yyjson_mut_arr_add_val_call_count = 0;
}

/* ================================================================
 * Content Block Serialization Error Path Tests
 * ================================================================ */

START_TEST(test_serialize_content_text_add_str_fail)
{
    reset_mocks();
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block = {0};
    block.type = IK_CONTENT_TEXT;
    block.data.text.text = (char *)"Hello";

    g_mock_yyjson_mut_obj_add_str_fail_after = 0;
    bool result = ik_google_serialize_content_block(doc, arr, &block);
    ck_assert(!result);

    reset_mocks();
    yyjson_mut_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_content_thinking_add_str_fail)
{
    reset_mocks();
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block = {0};
    block.type = IK_CONTENT_THINKING;
    block.data.thinking.text = (char *)"Thinking...";

    g_mock_yyjson_mut_obj_add_str_fail_after = 0;
    bool result = ik_google_serialize_content_block(doc, arr, &block);
    ck_assert(!result);

    reset_mocks();
    yyjson_mut_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_content_thinking_add_bool_fail)
{
    reset_mocks();
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block = {0};
    block.type = IK_CONTENT_THINKING;
    block.data.thinking.text = (char *)"Thinking...";

    g_mock_yyjson_mut_obj_add_bool_fail = true;
    bool result = ik_google_serialize_content_block(doc, arr, &block);
    ck_assert(!result);

    reset_mocks();
    yyjson_mut_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_content_tool_call_add_name_fail)
{
    reset_mocks();
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block = {0};
    block.type = IK_CONTENT_TOOL_CALL;
    block.data.tool_call.id = (char *)"call_123";
    block.data.tool_call.name = (char *)"get_weather";
    block.data.tool_call.arguments = (char *)"{\"city\":\"Boston\"}";

    g_mock_yyjson_mut_obj_add_str_fail_after = 0;
    bool result = ik_google_serialize_content_block(doc, arr, &block);
    ck_assert(!result);

    reset_mocks();
    yyjson_mut_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_content_tool_call_copy_fail)
{
    reset_mocks();
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block = {0};
    block.type = IK_CONTENT_TOOL_CALL;
    block.data.tool_call.id = (char *)"call_123";
    block.data.tool_call.name = (char *)"get_weather";
    block.data.tool_call.arguments = (char *)"{\"city\":\"Boston\"}";

    g_mock_yyjson_val_mut_copy_fail = true;
    bool result = ik_google_serialize_content_block(doc, arr, &block);
    ck_assert(!result);

    reset_mocks();
    yyjson_mut_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_content_tool_call_add_args_fail)
{
    reset_mocks();
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block = {0};
    block.type = IK_CONTENT_TOOL_CALL;
    block.data.tool_call.id = (char *)"call_123";
    block.data.tool_call.name = (char *)"get_weather";
    block.data.tool_call.arguments = (char *)"{\"city\":\"Boston\"}";

    // Fail on first call to yyjson_mut_obj_add_val_ (adding args to func_obj)
    g_mock_yyjson_mut_obj_add_val_fail_after = 0;
    bool result = ik_google_serialize_content_block(doc, arr, &block);
    ck_assert(!result);

    reset_mocks();
    yyjson_mut_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_content_tool_call_add_function_call_fail)
{
    reset_mocks();
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block = {0};
    block.type = IK_CONTENT_TOOL_CALL;
    block.data.tool_call.id = (char *)"call_123";
    block.data.tool_call.name = (char *)"get_weather";
    block.data.tool_call.arguments = (char *)"{\"city\":\"Boston\"}";

    // Fail on the second call to yyjson_mut_obj_add_val_ (adding functionCall to obj)
    g_mock_yyjson_mut_obj_add_val_fail_after = 1;
    bool result = ik_google_serialize_content_block(doc, arr, &block);
    ck_assert(!result);

    reset_mocks();
    yyjson_mut_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_content_tool_result_add_name_fail)
{
    reset_mocks();
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block = {0};
    block.type = IK_CONTENT_TOOL_RESULT;
    block.data.tool_result.tool_call_id = (char *)"call_123";
    block.data.tool_result.content = (char *)"Sunny, 72F";

    // Fail on first call to add_str (adding "name" to func_resp)
    g_mock_yyjson_mut_obj_add_str_fail_after = 0;
    bool result = ik_google_serialize_content_block(doc, arr, &block);
    ck_assert(!result);

    reset_mocks();
    yyjson_mut_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_content_tool_result_add_content_fail)
{
    reset_mocks();
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block = {0};
    block.type = IK_CONTENT_TOOL_RESULT;
    block.data.tool_result.tool_call_id = (char *)"call_123";
    block.data.tool_result.content = (char *)"Sunny, 72F";

    // Fail on second call to add_str (adding "content" to response_obj at line 110-111)
    g_mock_yyjson_mut_obj_add_str_fail_after = 1;
    bool result = ik_google_serialize_content_block(doc, arr, &block);
    ck_assert(!result);

    reset_mocks();
    yyjson_mut_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_content_tool_result_add_response_fail)
{
    reset_mocks();
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block = {0};
    block.type = IK_CONTENT_TOOL_RESULT;
    block.data.tool_result.tool_call_id = (char *)"call_123";
    block.data.tool_result.content = (char *)"Sunny, 72F";

    // Fail on first call to yyjson_mut_obj_add_val_ (adding response to func_resp)
    g_mock_yyjson_mut_obj_add_val_fail_after = 0;
    bool result = ik_google_serialize_content_block(doc, arr, &block);
    ck_assert(!result);

    reset_mocks();
    yyjson_mut_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_content_tool_result_add_function_response_fail)
{
    reset_mocks();
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block = {0};
    block.type = IK_CONTENT_TOOL_RESULT;
    block.data.tool_result.tool_call_id = (char *)"call_123";
    block.data.tool_result.content = (char *)"Sunny, 72F";

    // Fail on second call to yyjson_mut_obj_add_val_ (adding functionResponse to obj)
    g_mock_yyjson_mut_obj_add_val_fail_after = 1;
    bool result = ik_google_serialize_content_block(doc, arr, &block);
    ck_assert(!result);

    reset_mocks();
    yyjson_mut_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_message_parts_arr_add_fail)
{
    reset_mocks();
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *content_obj = yyjson_mut_obj(doc);

    ik_content_block_t block = {0};
    block.type = IK_CONTENT_TEXT;
    block.data.text.text = (char *)"Hello";

    ik_message_t message = {0};
    message.role = IK_ROLE_USER;
    message.content_blocks = &block;
    message.content_count = 1;

    // Fail on first arr_add_val call (adding content block to parts array)
    g_mock_yyjson_mut_arr_add_val_fail_after = 0;
    bool result = ik_google_serialize_message_parts(doc, content_obj, &message, NULL, false);
    ck_assert(!result);

    reset_mocks();
    yyjson_mut_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_message_parts_thought_sig_add_str_fail)
{
    reset_mocks();
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *content_obj = yyjson_mut_obj(doc);

    ik_content_block_t block = {0};
    block.type = IK_CONTENT_TEXT;
    block.data.text.text = (char *)"Hello";

    ik_message_t message = {0};
    message.role = IK_ROLE_ASSISTANT;
    message.content_blocks = &block;
    message.content_count = 1;

    g_mock_yyjson_mut_obj_add_str_fail_after = 0;
    bool result = ik_google_serialize_message_parts(doc, content_obj, &message, "sig-123", true);
    ck_assert(!result);

    reset_mocks();
    yyjson_mut_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_message_parts_thought_sig_arr_add_fail)
{
    reset_mocks();
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *content_obj = yyjson_mut_obj(doc);

    ik_content_block_t block = {0};
    block.type = IK_CONTENT_TEXT;
    block.data.text.text = (char *)"Hello";

    ik_message_t message = {0};
    message.role = IK_ROLE_ASSISTANT;
    message.content_blocks = &block;
    message.content_count = 1;

    // Fail on first arr_add_val call (adding thought signature to parts array)
    g_mock_yyjson_mut_arr_add_val_fail_after = 0;
    bool result = ik_google_serialize_message_parts(doc, content_obj, &message, "sig-123", true);
    ck_assert(!result);

    reset_mocks();
    yyjson_mut_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_message_parts_add_parts_fail)
{
    reset_mocks();
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *content_obj = yyjson_mut_obj(doc);

    ik_content_block_t block = {0};
    block.type = IK_CONTENT_TEXT;
    block.data.text.text = (char *)"Hello";

    ik_message_t message = {0};
    message.role = IK_ROLE_USER;
    message.content_blocks = &block;
    message.content_count = 1;

    // Fail on obj_add_val call (adding parts array to content object)
    g_mock_yyjson_mut_obj_add_val_fail_after = 0;
    bool result = ik_google_serialize_message_parts(doc, content_obj, &message, NULL, false);
    ck_assert(!result);

    reset_mocks();
    yyjson_mut_doc_free(doc);
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *request_helpers_coverage_suite(void)
{
    Suite *s = suite_create("Google Request Helpers Coverage");

    TCase *tc_error = tcase_create("Error Paths");
    tcase_add_checked_fixture(tc_error, setup, teardown);
    tcase_add_test(tc_error, test_serialize_content_text_add_str_fail);
    tcase_add_test(tc_error, test_serialize_content_thinking_add_str_fail);
    tcase_add_test(tc_error, test_serialize_content_thinking_add_bool_fail);
    tcase_add_test(tc_error, test_serialize_content_tool_call_add_name_fail);
    tcase_add_test(tc_error, test_serialize_content_tool_call_copy_fail);
    tcase_add_test(tc_error, test_serialize_content_tool_call_add_args_fail);
    tcase_add_test(tc_error, test_serialize_content_tool_call_add_function_call_fail);
    tcase_add_test(tc_error, test_serialize_content_tool_result_add_name_fail);
    tcase_add_test(tc_error, test_serialize_content_tool_result_add_content_fail);
    tcase_add_test(tc_error, test_serialize_content_tool_result_add_response_fail);
    tcase_add_test(tc_error, test_serialize_content_tool_result_add_function_response_fail);
    tcase_add_test(tc_error, test_serialize_message_parts_arr_add_fail);
    tcase_add_test(tc_error, test_serialize_message_parts_thought_sig_add_str_fail);
    tcase_add_test(tc_error, test_serialize_message_parts_thought_sig_arr_add_fail);
    tcase_add_test(tc_error, test_serialize_message_parts_add_parts_fail);
    suite_add_tcase(s, tc_error);

    return s;
}

int32_t main(void)
{
    Suite *s = request_helpers_coverage_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int32_t number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}

#pragma GCC diagnostic pop
