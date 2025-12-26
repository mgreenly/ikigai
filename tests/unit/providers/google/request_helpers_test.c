/**
 * @file request_helpers_test.c
 * @brief Unit tests for Google request serialization helpers
 */

// Disable cast-qual for test literals
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/google/request_helpers.h"
#include "providers/google/thinking.h"
#include "providers/provider.h"
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
 * Role Mapping Tests
 * ================================================================ */

START_TEST(test_role_to_string_user) {
    const char *role = ik_google_role_to_string(IK_ROLE_USER);
    ck_assert_str_eq(role, "user");
}
END_TEST START_TEST(test_role_to_string_assistant)
{
    const char *role = ik_google_role_to_string(IK_ROLE_ASSISTANT);
    ck_assert_str_eq(role, "model");
}

END_TEST START_TEST(test_role_to_string_tool)
{
    const char *role = ik_google_role_to_string(IK_ROLE_TOOL);
    ck_assert_str_eq(role, "function");
}

END_TEST START_TEST(test_role_to_string_invalid)
{
    const char *role = ik_google_role_to_string((ik_role_t)999);
    ck_assert_str_eq(role, "user");
}

END_TEST
/* ================================================================
 * Content Block Serialization Tests
 * ================================================================ */

START_TEST(test_serialize_content_text)
{
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block = {0};
    block.type = IK_CONTENT_TEXT;
    block.data.text.text = (char *)"Hello, world!";

    bool result = ik_google_serialize_content_block(doc, arr, &block);
    ck_assert(result);
    ck_assert_uint_eq(yyjson_mut_arr_size(arr), 1);

    yyjson_mut_val *obj = yyjson_mut_arr_get_first(arr);
    yyjson_mut_val *text = yyjson_mut_obj_get(obj, "text");
    ck_assert_str_eq(yyjson_mut_get_str(text), "Hello, world!");

    yyjson_mut_doc_free(doc);
}

END_TEST START_TEST(test_serialize_content_thinking)
{
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block = {0};
    block.type = IK_CONTENT_THINKING;
    block.data.thinking.text = (char *)"Let me think...";

    bool result = ik_google_serialize_content_block(doc, arr, &block);
    ck_assert(result);
    ck_assert_uint_eq(yyjson_mut_arr_size(arr), 1);

    yyjson_mut_val *obj = yyjson_mut_arr_get_first(arr);
    yyjson_mut_val *text = yyjson_mut_obj_get(obj, "text");
    ck_assert_str_eq(yyjson_mut_get_str(text), "Let me think...");

    yyjson_mut_val *thought = yyjson_mut_obj_get(obj, "thought");
    ck_assert(yyjson_mut_get_bool(thought));

    yyjson_mut_doc_free(doc);
}

END_TEST START_TEST(test_serialize_content_tool_call)
{
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block = {0};
    block.type = IK_CONTENT_TOOL_CALL;
    block.data.tool_call.id = (char *)"call_123";
    block.data.tool_call.name = (char *)"get_weather";
    block.data.tool_call.arguments = (char *)"{\"city\":\"Boston\"}";

    bool result = ik_google_serialize_content_block(doc, arr, &block);
    ck_assert(result);
    ck_assert_uint_eq(yyjson_mut_arr_size(arr), 1);

    yyjson_mut_val *obj = yyjson_mut_arr_get_first(arr);
    yyjson_mut_val *func_call = yyjson_mut_obj_get(obj, "functionCall");
    ck_assert(func_call != NULL);

    yyjson_mut_val *name = yyjson_mut_obj_get(func_call, "name");
    ck_assert_str_eq(yyjson_mut_get_str(name), "get_weather");

    yyjson_mut_val *args = yyjson_mut_obj_get(func_call, "args");
    ck_assert(args != NULL);
    yyjson_mut_val *city = yyjson_mut_obj_get(args, "city");
    ck_assert_str_eq(yyjson_mut_get_str(city), "Boston");

    yyjson_mut_doc_free(doc);
}

END_TEST START_TEST(test_serialize_content_tool_call_invalid_json)
{
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block = {0};
    block.type = IK_CONTENT_TOOL_CALL;
    block.data.tool_call.id = (char *)"call_123";
    block.data.tool_call.name = (char *)"get_weather";
    block.data.tool_call.arguments = (char *)"not valid json";

    bool result = ik_google_serialize_content_block(doc, arr, &block);
    ck_assert(!result);

    yyjson_mut_doc_free(doc);
}

END_TEST START_TEST(test_serialize_content_tool_result)
{
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block = {0};
    block.type = IK_CONTENT_TOOL_RESULT;
    block.data.tool_result.tool_call_id = (char *)"call_123";
    block.data.tool_result.content = (char *)"Sunny, 72F";

    bool result = ik_google_serialize_content_block(doc, arr, &block);
    ck_assert(result);
    ck_assert_uint_eq(yyjson_mut_arr_size(arr), 1);

    yyjson_mut_val *obj = yyjson_mut_arr_get_first(arr);
    yyjson_mut_val *func_resp = yyjson_mut_obj_get(obj, "functionResponse");
    ck_assert(func_resp != NULL);

    yyjson_mut_val *name = yyjson_mut_obj_get(func_resp, "name");
    ck_assert_str_eq(yyjson_mut_get_str(name), "call_123");

    yyjson_mut_val *response = yyjson_mut_obj_get(func_resp, "response");
    ck_assert(response != NULL);
    yyjson_mut_val *content = yyjson_mut_obj_get(response, "content");
    ck_assert_str_eq(yyjson_mut_get_str(content), "Sunny, 72F");

    yyjson_mut_doc_free(doc);
}

END_TEST
/* ================================================================
 * Message Parts Serialization Tests
 * ================================================================ */

START_TEST(test_serialize_message_parts_basic)
{
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *content_obj = yyjson_mut_obj(doc);

    ik_content_block_t block = {0};
    block.type = IK_CONTENT_TEXT;
    block.data.text.text = (char *)"Hello";

    ik_message_t message = {0};
    message.role = IK_ROLE_USER;
    message.content_blocks = &block;
    message.content_count = 1;

    bool result = ik_google_serialize_message_parts(doc, content_obj, &message, NULL, false);
    ck_assert(result);

    yyjson_mut_val *parts = yyjson_mut_obj_get(content_obj, "parts");
    ck_assert(parts != NULL);
    ck_assert_uint_eq(yyjson_mut_arr_size(parts), 1);

    yyjson_mut_doc_free(doc);
}

END_TEST START_TEST(test_serialize_message_parts_with_thought_signature)
{
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *content_obj = yyjson_mut_obj(doc);

    ik_content_block_t block = {0};
    block.type = IK_CONTENT_TEXT;
    block.data.text.text = (char *)"Hello";

    ik_message_t message = {0};
    message.role = IK_ROLE_ASSISTANT;
    message.content_blocks = &block;
    message.content_count = 1;

    bool result = ik_google_serialize_message_parts(doc, content_obj, &message, "sig-123", true);
    ck_assert(result);

    yyjson_mut_val *parts = yyjson_mut_obj_get(content_obj, "parts");
    ck_assert(parts != NULL);
    ck_assert_uint_eq(yyjson_mut_arr_size(parts), 2);

    // Check thought signature is first
    yyjson_mut_val *first_part = yyjson_mut_arr_get_first(parts);
    yyjson_mut_val *sig_val = yyjson_mut_obj_get(first_part, "thoughtSignature");
    ck_assert_str_eq(yyjson_mut_get_str(sig_val), "sig-123");

    yyjson_mut_doc_free(doc);
}

END_TEST START_TEST(test_serialize_message_parts_thought_not_first_assistant)
{
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *content_obj = yyjson_mut_obj(doc);

    ik_content_block_t block = {0};
    block.type = IK_CONTENT_TEXT;
    block.data.text.text = (char *)"Hello";

    ik_message_t message = {0};
    message.role = IK_ROLE_ASSISTANT;
    message.content_blocks = &block;
    message.content_count = 1;

    // thought_sig present but is_first_assistant is false
    bool result = ik_google_serialize_message_parts(doc, content_obj, &message, "sig-123", false);
    ck_assert(result);

    yyjson_mut_val *parts = yyjson_mut_obj_get(content_obj, "parts");
    ck_assert(parts != NULL);
    // Should only have 1 part (the text block), not the thought signature
    ck_assert_uint_eq(yyjson_mut_arr_size(parts), 1);

    yyjson_mut_doc_free(doc);
}

END_TEST START_TEST(test_serialize_message_parts_with_tool_call)
{
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *content_obj = yyjson_mut_obj(doc);

    ik_content_block_t block = {0};
    block.type = IK_CONTENT_TOOL_CALL;
    block.data.tool_call.id = (char *)"call_123";
    block.data.tool_call.name = (char *)"get_weather";
    block.data.tool_call.arguments = (char *)"{\"city\":\"Boston\"}";

    ik_message_t message = {0};
    message.role = IK_ROLE_ASSISTANT;
    message.content_blocks = &block;
    message.content_count = 1;

    bool result = ik_google_serialize_message_parts(doc, content_obj, &message, NULL, false);
    ck_assert(result);

    yyjson_mut_val *parts = yyjson_mut_obj_get(content_obj, "parts");
    ck_assert(parts != NULL);
    ck_assert_uint_eq(yyjson_mut_arr_size(parts), 1);

    yyjson_mut_doc_free(doc);
}

END_TEST START_TEST(test_serialize_message_parts_with_tool_result)
{
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *content_obj = yyjson_mut_obj(doc);

    ik_content_block_t block = {0};
    block.type = IK_CONTENT_TOOL_RESULT;
    block.data.tool_result.tool_call_id = (char *)"call_123";
    block.data.tool_result.content = (char *)"Sunny, 72F";

    ik_message_t message = {0};
    message.role = IK_ROLE_TOOL;
    message.content_blocks = &block;
    message.content_count = 1;

    bool result = ik_google_serialize_message_parts(doc, content_obj, &message, NULL, false);
    ck_assert(result);

    yyjson_mut_val *parts = yyjson_mut_obj_get(content_obj, "parts");
    ck_assert(parts != NULL);
    ck_assert_uint_eq(yyjson_mut_arr_size(parts), 1);

    yyjson_mut_doc_free(doc);
}

END_TEST START_TEST(test_serialize_message_parts_invalid_block_stops)
{
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *content_obj = yyjson_mut_obj(doc);

    ik_content_block_t blocks[2];
    memset(blocks, 0, sizeof(blocks));

    blocks[0].type = IK_CONTENT_TEXT;
    blocks[0].data.text.text = (char *)"Hello";
    blocks[1].type = IK_CONTENT_TOOL_CALL;
    blocks[1].data.tool_call.id = (char *)"call_123";
    blocks[1].data.tool_call.name = (char *)"get_weather";
    blocks[1].data.tool_call.arguments = (char *)"invalid json";

    ik_message_t message = {0};
    message.role = IK_ROLE_ASSISTANT;
    message.content_blocks = blocks;
    message.content_count = 2;

    bool result = ik_google_serialize_message_parts(doc, content_obj, &message, NULL, false);
    ck_assert(!result);

    yyjson_mut_doc_free(doc);
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *request_helpers_suite(void)
{
    Suite *s = suite_create("Google Request Helpers");

    TCase *tc_role = tcase_create("Role Mapping");
    tcase_add_checked_fixture(tc_role, setup, teardown);
    tcase_add_test(tc_role, test_role_to_string_user);
    tcase_add_test(tc_role, test_role_to_string_assistant);
    tcase_add_test(tc_role, test_role_to_string_tool);
    tcase_add_test(tc_role, test_role_to_string_invalid);
    suite_add_tcase(s, tc_role);

    TCase *tc_content = tcase_create("Content Block Serialization");
    tcase_add_checked_fixture(tc_content, setup, teardown);
    tcase_add_test(tc_content, test_serialize_content_text);
    tcase_add_test(tc_content, test_serialize_content_thinking);
    tcase_add_test(tc_content, test_serialize_content_tool_call);
    tcase_add_test(tc_content, test_serialize_content_tool_call_invalid_json);
    tcase_add_test(tc_content, test_serialize_content_tool_result);
    suite_add_tcase(s, tc_content);

    TCase *tc_parts = tcase_create("Message Parts Serialization");
    tcase_add_checked_fixture(tc_parts, setup, teardown);
    tcase_add_test(tc_parts, test_serialize_message_parts_basic);
    tcase_add_test(tc_parts, test_serialize_message_parts_with_thought_signature);
    tcase_add_test(tc_parts, test_serialize_message_parts_thought_not_first_assistant);
    tcase_add_test(tc_parts, test_serialize_message_parts_with_tool_call);
    tcase_add_test(tc_parts, test_serialize_message_parts_with_tool_result);
    tcase_add_test(tc_parts, test_serialize_message_parts_invalid_block_stops);
    suite_add_tcase(s, tc_parts);

    return s;
}

int32_t main(void)
{
    Suite *s = request_helpers_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int32_t number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}

#pragma GCC diagnostic pop
