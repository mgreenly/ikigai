/**
 * @file serialize_responses_test.c
 * @brief Unit tests for OpenAI Responses API message serialization
 */

#include <check.h>
#include <talloc.h>
#include <stdbool.h>
#include <string.h>

#include "apps/ikigai/providers/openai/serialize.h"
#include "apps/ikigai/providers/provider.h"
#include "vendor/yyjson/yyjson.h"

#define IK_TEST_TIMEOUT 30

static TALLOC_CTX *test_ctx = NULL;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
    test_ctx = NULL;
}

/* ================================================================
 * User Message Tests
 * ================================================================ */

START_TEST(test_responses_serialize_user_text) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block = {
        .type = IK_CONTENT_TEXT
    };
    block.data.text.text = talloc_strdup(test_ctx, "Hello world");

    ik_message_t msg = {
        .role = IK_ROLE_USER,
        .content_count = 1,
        .content_blocks = &block
    };

    bool result = ik_openai_serialize_responses_message(doc, &msg, arr);
    ck_assert(result);

    // Array should have one item
    ck_assert_uint_eq(yyjson_mut_arr_size(arr), 1);

    yyjson_mut_val *item = yyjson_mut_arr_get(arr, 0);
    ck_assert_ptr_nonnull(item);

    // Check role and content
    yyjson_mut_val *role = yyjson_mut_obj_get(item, "role");
    ck_assert_ptr_nonnull(role);
    const char *role_str = yyjson_mut_get_str(role);
    ck_assert_str_eq(role_str, "user");

    yyjson_mut_val *content = yyjson_mut_obj_get(item, "content");
    ck_assert_ptr_nonnull(content);
    const char *content_str = yyjson_mut_get_str(content);
    ck_assert_str_eq(content_str, "Hello world");

    yyjson_mut_doc_free(doc);
}

END_TEST

START_TEST(test_responses_serialize_user_multiple_text_blocks) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t blocks[2] = {
        {.type = IK_CONTENT_TEXT},
        {.type = IK_CONTENT_TEXT}
    };
    blocks[0].data.text.text = talloc_strdup(test_ctx, "First block");
    blocks[1].data.text.text = talloc_strdup(test_ctx, "Second block");

    ik_message_t msg = {
        .role = IK_ROLE_USER,
        .content_count = 2,
        .content_blocks = blocks
    };

    bool result = ik_openai_serialize_responses_message(doc, &msg, arr);
    ck_assert(result);

    // Each text block becomes separate item
    ck_assert_uint_eq(yyjson_mut_arr_size(arr), 2);

    yyjson_mut_val *item1 = yyjson_mut_arr_get(arr, 0);
    yyjson_mut_val *content1 = yyjson_mut_obj_get(item1, "content");
    const char *content1_str = yyjson_mut_get_str(content1);
    ck_assert_str_eq(content1_str, "First block");

    yyjson_mut_val *item2 = yyjson_mut_arr_get(arr, 1);
    yyjson_mut_val *content2 = yyjson_mut_obj_get(item2, "content");
    const char *content2_str = yyjson_mut_get_str(content2);
    ck_assert_str_eq(content2_str, "Second block");

    yyjson_mut_doc_free(doc);
}

END_TEST

/* ================================================================
 * Assistant Message Tests
 * ================================================================ */

START_TEST(test_responses_serialize_assistant_text) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block = {
        .type = IK_CONTENT_TEXT
    };
    block.data.text.text = talloc_strdup(test_ctx, "Assistant response");

    ik_message_t msg = {
        .role = IK_ROLE_ASSISTANT,
        .content_count = 1,
        .content_blocks = &block
    };

    bool result = ik_openai_serialize_responses_message(doc, &msg, arr);
    ck_assert(result);

    ck_assert_uint_eq(yyjson_mut_arr_size(arr), 1);

    yyjson_mut_val *item = yyjson_mut_arr_get(arr, 0);
    yyjson_mut_val *role = yyjson_mut_obj_get(item, "role");
    const char *role_str = yyjson_mut_get_str(role);
    ck_assert_str_eq(role_str, "assistant");

    yyjson_mut_val *content = yyjson_mut_obj_get(item, "content");
    const char *content_str = yyjson_mut_get_str(content);
    ck_assert_str_eq(content_str, "Assistant response");

    yyjson_mut_doc_free(doc);
}

END_TEST

/* ================================================================
 * Tool Call Tests
 * ================================================================ */

START_TEST(test_responses_serialize_tool_call) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block = {
        .type = IK_CONTENT_TOOL_CALL
    };
    block.data.tool_call.id = talloc_strdup(test_ctx, "call_123");
    block.data.tool_call.name = talloc_strdup(test_ctx, "get_weather");
    block.data.tool_call.arguments = talloc_strdup(test_ctx, "{\"city\":\"Boston\"}");

    ik_message_t msg = {
        .role = IK_ROLE_ASSISTANT,
        .content_count = 1,
        .content_blocks = &block
    };

    bool result = ik_openai_serialize_responses_message(doc, &msg, arr);
    ck_assert(result);

    ck_assert_uint_eq(yyjson_mut_arr_size(arr), 1);

    yyjson_mut_val *item = yyjson_mut_arr_get(arr, 0);

    // Check type field
    yyjson_mut_val *type = yyjson_mut_obj_get(item, "type");
    ck_assert_ptr_nonnull(type);
    const char *type_str = yyjson_mut_get_str(type);
    ck_assert_str_eq(type_str, "function_call");

    // Check call_id
    yyjson_mut_val *call_id = yyjson_mut_obj_get(item, "call_id");
    ck_assert_ptr_nonnull(call_id);
    const char *call_id_str = yyjson_mut_get_str(call_id);
    ck_assert_str_eq(call_id_str, "call_123");

    // Check name
    yyjson_mut_val *name = yyjson_mut_obj_get(item, "name");
    ck_assert_ptr_nonnull(name);
    const char *name_str = yyjson_mut_get_str(name);
    ck_assert_str_eq(name_str, "get_weather");

    // Check arguments
    yyjson_mut_val *arguments = yyjson_mut_obj_get(item, "arguments");
    ck_assert_ptr_nonnull(arguments);
    const char *arguments_str = yyjson_mut_get_str(arguments);
    ck_assert_str_eq(arguments_str, "{\"city\":\"Boston\"}");

    // Should NOT have role field
    yyjson_mut_val *role = yyjson_mut_obj_get(item, "role");
    ck_assert_ptr_null(role);

    yyjson_mut_doc_free(doc);
}

END_TEST

START_TEST(test_responses_serialize_multiple_tool_calls) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t blocks[2] = {
        {.type = IK_CONTENT_TOOL_CALL},
        {.type = IK_CONTENT_TOOL_CALL}
    };
    blocks[0].data.tool_call.id = talloc_strdup(test_ctx, "call_1");
    blocks[0].data.tool_call.name = talloc_strdup(test_ctx, "tool_a");
    blocks[0].data.tool_call.arguments = talloc_strdup(test_ctx, "{}");
    blocks[1].data.tool_call.id = talloc_strdup(test_ctx, "call_2");
    blocks[1].data.tool_call.name = talloc_strdup(test_ctx, "tool_b");
    blocks[1].data.tool_call.arguments = talloc_strdup(test_ctx, "{\"x\":1}");

    ik_message_t msg = {
        .role = IK_ROLE_ASSISTANT,
        .content_count = 2,
        .content_blocks = blocks
    };

    bool result = ik_openai_serialize_responses_message(doc, &msg, arr);
    ck_assert(result);

    // Each tool call becomes separate item
    ck_assert_uint_eq(yyjson_mut_arr_size(arr), 2);

    yyjson_mut_val *item1 = yyjson_mut_arr_get(arr, 0);
    yyjson_mut_val *call_id1 = yyjson_mut_obj_get(item1, "call_id");
    const char *call_id1_str = yyjson_mut_get_str(call_id1);
    ck_assert_str_eq(call_id1_str, "call_1");

    yyjson_mut_val *item2 = yyjson_mut_arr_get(arr, 1);
    yyjson_mut_val *call_id2 = yyjson_mut_obj_get(item2, "call_id");
    const char *call_id2_str = yyjson_mut_get_str(call_id2);
    ck_assert_str_eq(call_id2_str, "call_2");

    yyjson_mut_doc_free(doc);
}

END_TEST

/* ================================================================
 * Tool Result Tests
 * ================================================================ */

START_TEST(test_responses_serialize_tool_result) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block = {
        .type = IK_CONTENT_TOOL_RESULT,
        .data.tool_result.is_error = false
    };
    block.data.tool_result.tool_call_id = talloc_strdup(test_ctx, "call_456");
    block.data.tool_result.content = talloc_strdup(test_ctx, "Temperature is 72F");

    ik_message_t msg = {
        .role = IK_ROLE_TOOL,
        .content_count = 1,
        .content_blocks = &block
    };

    bool result = ik_openai_serialize_responses_message(doc, &msg, arr);
    ck_assert(result);

    ck_assert_uint_eq(yyjson_mut_arr_size(arr), 1);

    yyjson_mut_val *item = yyjson_mut_arr_get(arr, 0);

    // Check type field
    yyjson_mut_val *type = yyjson_mut_obj_get(item, "type");
    ck_assert_ptr_nonnull(type);
    const char *type_str = yyjson_mut_get_str(type);
    ck_assert_str_eq(type_str, "function_call_output");

    // Check call_id
    yyjson_mut_val *call_id = yyjson_mut_obj_get(item, "call_id");
    ck_assert_ptr_nonnull(call_id);
    const char *call_id_str = yyjson_mut_get_str(call_id);
    ck_assert_str_eq(call_id_str, "call_456");

    // Check output
    yyjson_mut_val *output = yyjson_mut_obj_get(item, "output");
    ck_assert_ptr_nonnull(output);
    const char *output_str = yyjson_mut_get_str(output);
    ck_assert_str_eq(output_str, "Temperature is 72F");

    // Should NOT have role field
    yyjson_mut_val *role = yyjson_mut_obj_get(item, "role");
    ck_assert_ptr_null(role);

    yyjson_mut_doc_free(doc);
}

END_TEST

/* ================================================================
 * Test Suite
 * ================================================================ */

static Suite *serialize_responses_suite(void)
{
    Suite *s = suite_create("serialize_responses");

    TCase *tc_user = tcase_create("user_messages");
    tcase_set_timeout(tc_user, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_user, setup, teardown);
    tcase_add_test(tc_user, test_responses_serialize_user_text);
    tcase_add_test(tc_user, test_responses_serialize_user_multiple_text_blocks);
    suite_add_tcase(s, tc_user);

    TCase *tc_assistant = tcase_create("assistant_messages");
    tcase_set_timeout(tc_assistant, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_assistant, setup, teardown);
    tcase_add_test(tc_assistant, test_responses_serialize_assistant_text);
    suite_add_tcase(s, tc_assistant);

    TCase *tc_tool_calls = tcase_create("tool_calls");
    tcase_set_timeout(tc_tool_calls, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_tool_calls, setup, teardown);
    tcase_add_test(tc_tool_calls, test_responses_serialize_tool_call);
    tcase_add_test(tc_tool_calls, test_responses_serialize_multiple_tool_calls);
    suite_add_tcase(s, tc_tool_calls);

    TCase *tc_tool_results = tcase_create("tool_results");
    tcase_set_timeout(tc_tool_results, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_tool_results, setup, teardown);
    tcase_add_test(tc_tool_results, test_responses_serialize_tool_result);
    suite_add_tcase(s, tc_tool_results);

    return s;
}

int main(void)
{
    Suite *s = serialize_responses_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/openai/serialize_responses_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
