/**
 * @file openai_serialize_assistant_test.c
 * @brief Unit tests for OpenAI assistant message serialization
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/openai/serialize.h"
#include "providers/provider.h"
#include "vendor/yyjson/yyjson.h"
#include "openai_serialize_assistant_test.h"

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
 * Assistant Message Tests
 * ================================================================ */

START_TEST(test_serialize_assistant_message_text)
{
    ik_message_t *msg = talloc_zero(test_ctx, ik_message_t);
    msg->role = IK_ROLE_ASSISTANT;
    msg->content_count = 1;
    msg->content_blocks = talloc_array(msg, ik_content_block_t, 1);
    msg->content_blocks[0].type = IK_CONTENT_TEXT;
    msg->content_blocks[0].data.text.text = talloc_strdup(msg, "Assistant response");

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *val = ik_openai_serialize_message(doc, msg);

    ck_assert_ptr_nonnull(val);

    yyjson_mut_val *role = yyjson_mut_obj_get(val, "role");
    ck_assert_ptr_nonnull(role);
    ck_assert_str_eq(yyjson_mut_get_str(role), "assistant");

    yyjson_mut_val *content = yyjson_mut_obj_get(val, "content");
    ck_assert_ptr_nonnull(content);
    ck_assert_str_eq(yyjson_mut_get_str(content), "Assistant response");

    yyjson_mut_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_assistant_message_with_tool_calls)
{
    ik_message_t *msg = talloc_zero(test_ctx, ik_message_t);
    msg->role = IK_ROLE_ASSISTANT;
    msg->content_count = 1;
    msg->content_blocks = talloc_array(msg, ik_content_block_t, 1);
    msg->content_blocks[0].type = IK_CONTENT_TOOL_CALL;
    msg->content_blocks[0].data.tool_call.id = talloc_strdup(msg, "call_123");
    msg->content_blocks[0].data.tool_call.name = talloc_strdup(msg, "get_weather");
    msg->content_blocks[0].data.tool_call.arguments = talloc_strdup(msg, "{\"city\":\"SF\"}");

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *val = ik_openai_serialize_message(doc, msg);

    ck_assert_ptr_nonnull(val);

    yyjson_mut_val *content = yyjson_mut_obj_get(val, "content");
    ck_assert_ptr_nonnull(content);
    ck_assert(yyjson_mut_is_null(content));

    yyjson_mut_val *tool_calls = yyjson_mut_obj_get(val, "tool_calls");
    ck_assert_ptr_nonnull(tool_calls);
    ck_assert(yyjson_mut_is_arr(tool_calls));
    ck_assert_int_eq((int)yyjson_mut_arr_size(tool_calls), 1);

    yyjson_mut_val *tc = yyjson_mut_arr_get_first(tool_calls);
    ck_assert_ptr_nonnull(tc);

    yyjson_mut_val *id = yyjson_mut_obj_get(tc, "id");
    ck_assert_str_eq(yyjson_mut_get_str(id), "call_123");

    yyjson_mut_val *type = yyjson_mut_obj_get(tc, "type");
    ck_assert_str_eq(yyjson_mut_get_str(type), "function");

    yyjson_mut_val *func = yyjson_mut_obj_get(tc, "function");
    ck_assert_ptr_nonnull(func);

    yyjson_mut_val *name = yyjson_mut_obj_get(func, "name");
    ck_assert_str_eq(yyjson_mut_get_str(name), "get_weather");

    yyjson_mut_val *args = yyjson_mut_obj_get(func, "arguments");
    ck_assert_str_eq(yyjson_mut_get_str(args), "{\"city\":\"SF\"}");

    yyjson_mut_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_assistant_message_multiple_tool_calls)
{
    ik_message_t *msg = talloc_zero(test_ctx, ik_message_t);
    msg->role = IK_ROLE_ASSISTANT;
    msg->content_count = 2;
    msg->content_blocks = talloc_array(msg, ik_content_block_t, 2);

    msg->content_blocks[0].type = IK_CONTENT_TOOL_CALL;
    msg->content_blocks[0].data.tool_call.id = talloc_strdup(msg, "call_1");
    msg->content_blocks[0].data.tool_call.name = talloc_strdup(msg, "tool_a");
    msg->content_blocks[0].data.tool_call.arguments = talloc_strdup(msg, "{}");

    msg->content_blocks[1].type = IK_CONTENT_TOOL_CALL;
    msg->content_blocks[1].data.tool_call.id = talloc_strdup(msg, "call_2");
    msg->content_blocks[1].data.tool_call.name = talloc_strdup(msg, "tool_b");
    msg->content_blocks[1].data.tool_call.arguments = talloc_strdup(msg, "{\"x\":1}");

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *val = ik_openai_serialize_message(doc, msg);

    ck_assert_ptr_nonnull(val);

    yyjson_mut_val *tool_calls = yyjson_mut_obj_get(val, "tool_calls");
    ck_assert_ptr_nonnull(tool_calls);
    ck_assert_int_eq((int)yyjson_mut_arr_size(tool_calls), 2);

    yyjson_mut_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_assistant_message_mixed_content_and_tool_calls)
{
    ik_message_t *msg = talloc_zero(test_ctx, ik_message_t);
    msg->role = IK_ROLE_ASSISTANT;
    msg->content_count = 2;
    msg->content_blocks = talloc_array(msg, ik_content_block_t, 2);

    msg->content_blocks[0].type = IK_CONTENT_TEXT;
    msg->content_blocks[0].data.text.text = talloc_strdup(msg, "Text");

    msg->content_blocks[1].type = IK_CONTENT_TOOL_CALL;
    msg->content_blocks[1].data.tool_call.id = talloc_strdup(msg, "call_1");
    msg->content_blocks[1].data.tool_call.name = talloc_strdup(msg, "tool");
    msg->content_blocks[1].data.tool_call.arguments = talloc_strdup(msg, "{}");

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *val = ik_openai_serialize_message(doc, msg);

    ck_assert_ptr_nonnull(val);

    yyjson_mut_val *content = yyjson_mut_obj_get(val, "content");
    ck_assert_ptr_nonnull(content);
    ck_assert(yyjson_mut_is_null(content));

    yyjson_mut_val *tool_calls = yyjson_mut_obj_get(val, "tool_calls");
    ck_assert_ptr_nonnull(tool_calls);
    ck_assert_int_eq((int)yyjson_mut_arr_size(tool_calls), 1);

    yyjson_mut_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_assistant_message_empty_content)
{
    ik_message_t *msg = talloc_zero(test_ctx, ik_message_t);
    msg->role = IK_ROLE_ASSISTANT;
    msg->content_count = 0;
    msg->content_blocks = NULL;

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *val = ik_openai_serialize_message(doc, msg);

    ck_assert_ptr_nonnull(val);

    yyjson_mut_val *role = yyjson_mut_obj_get(val, "role");
    ck_assert_ptr_nonnull(role);
    ck_assert_str_eq(yyjson_mut_get_str(role), "assistant");

    yyjson_mut_val *content = yyjson_mut_obj_get(val, "content");
    ck_assert_ptr_nonnull(content);
    ck_assert_str_eq(yyjson_mut_get_str(content), "");

    yyjson_mut_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_assistant_message_non_text_blocks)
{
    // Message with blocks that are neither text nor tool calls (e.g., tool_result)
    ik_message_t *msg = talloc_zero(test_ctx, ik_message_t);
    msg->role = IK_ROLE_ASSISTANT;
    msg->content_count = 1;
    msg->content_blocks = talloc_array(msg, ik_content_block_t, 1);
    msg->content_blocks[0].type = IK_CONTENT_TOOL_RESULT;
    msg->content_blocks[0].data.tool_result.tool_call_id = talloc_strdup(msg, "call_1");
    msg->content_blocks[0].data.tool_result.content = talloc_strdup(msg, "Result");

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *val = ik_openai_serialize_message(doc, msg);

    ck_assert_ptr_nonnull(val);

    yyjson_mut_val *content = yyjson_mut_obj_get(val, "content");
    ck_assert_ptr_nonnull(content);
    ck_assert_str_eq(yyjson_mut_get_str(content), "");

    yyjson_mut_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_assistant_message_tool_call_with_non_tool_block)
{
    // Message with tool call followed by non-tool-call block
    // This covers the false branch of line 72 (type != TOOL_CALL) in serialize.c
    ik_message_t *msg = talloc_zero(test_ctx, ik_message_t);
    msg->role = IK_ROLE_ASSISTANT;
    msg->content_count = 2;
    msg->content_blocks = talloc_array(msg, ik_content_block_t, 2);

    msg->content_blocks[0].type = IK_CONTENT_TOOL_CALL;
    msg->content_blocks[0].data.tool_call.id = talloc_strdup(msg, "call_1");
    msg->content_blocks[0].data.tool_call.name = talloc_strdup(msg, "tool");
    msg->content_blocks[0].data.tool_call.arguments = talloc_strdup(msg, "{}");

    // Second block is NOT a tool call - this will be skipped in the tool_calls loop
    msg->content_blocks[1].type = IK_CONTENT_TOOL_RESULT;
    msg->content_blocks[1].data.tool_result.tool_call_id = talloc_strdup(msg, "call_2");
    msg->content_blocks[1].data.tool_result.content = talloc_strdup(msg, "Result");

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *val = ik_openai_serialize_message(doc, msg);

    ck_assert_ptr_nonnull(val);

    yyjson_mut_val *content = yyjson_mut_obj_get(val, "content");
    ck_assert_ptr_nonnull(content);
    ck_assert(yyjson_mut_is_null(content));

    yyjson_mut_val *tool_calls = yyjson_mut_obj_get(val, "tool_calls");
    ck_assert_ptr_nonnull(tool_calls);
    // Should only have 1 tool call (the TOOL_RESULT block is skipped)
    ck_assert_int_eq((int)yyjson_mut_arr_size(tool_calls), 1);

    yyjson_mut_doc_free(doc);
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

Suite *openai_serialize_assistant_suite(void)
{
    Suite *s = suite_create("OpenAI Serialize Assistant Messages");

    TCase *tc_assistant = tcase_create("Assistant Messages");
    tcase_set_timeout(tc_assistant, 30);
    tcase_add_unchecked_fixture(tc_assistant, setup, teardown);
    tcase_add_test(tc_assistant, test_serialize_assistant_message_text);
    tcase_add_test(tc_assistant, test_serialize_assistant_message_with_tool_calls);
    tcase_add_test(tc_assistant, test_serialize_assistant_message_multiple_tool_calls);
    tcase_add_test(tc_assistant, test_serialize_assistant_message_mixed_content_and_tool_calls);
    tcase_add_test(tc_assistant, test_serialize_assistant_message_empty_content);
    tcase_add_test(tc_assistant, test_serialize_assistant_message_non_text_blocks);
    tcase_add_test(tc_assistant, test_serialize_assistant_message_tool_call_with_non_tool_block);
    suite_add_tcase(s, tc_assistant);

    return s;
}