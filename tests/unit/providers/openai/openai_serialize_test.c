/**
 * @file openai_serialize_test.c
 * @brief Unit tests for OpenAI message serialization
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/openai/serialize.h"
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
 * User Message Tests
 * ================================================================ */

START_TEST(test_serialize_user_message_single_text)
{
    ik_message_t *msg = talloc_zero(test_ctx, ik_message_t);
    msg->role = IK_ROLE_USER;
    msg->content_count = 1;
    msg->content_blocks = talloc_array(msg, ik_content_block_t, 1);
    msg->content_blocks[0].type = IK_CONTENT_TEXT;
    msg->content_blocks[0].data.text.text = talloc_strdup(msg, "Hello world");

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *val = ik_openai_serialize_message(doc, msg);

    ck_assert_ptr_nonnull(val);
    ck_assert(yyjson_mut_is_obj(val));

    yyjson_mut_val *role = yyjson_mut_obj_get(val, "role");
    ck_assert_ptr_nonnull(role);
    ck_assert_str_eq(yyjson_mut_get_str(role), "user");

    yyjson_mut_val *content = yyjson_mut_obj_get(val, "content");
    ck_assert_ptr_nonnull(content);
    ck_assert_str_eq(yyjson_mut_get_str(content), "Hello world");

    yyjson_mut_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_user_message_multiple_text_blocks)
{
    ik_message_t *msg = talloc_zero(test_ctx, ik_message_t);
    msg->role = IK_ROLE_USER;
    msg->content_count = 3;
    msg->content_blocks = talloc_array(msg, ik_content_block_t, 3);

    msg->content_blocks[0].type = IK_CONTENT_TEXT;
    msg->content_blocks[0].data.text.text = talloc_strdup(msg, "First");

    msg->content_blocks[1].type = IK_CONTENT_TEXT;
    msg->content_blocks[1].data.text.text = talloc_strdup(msg, "Second");

    msg->content_blocks[2].type = IK_CONTENT_TEXT;
    msg->content_blocks[2].data.text.text = talloc_strdup(msg, "Third");

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *val = ik_openai_serialize_message(doc, msg);

    ck_assert_ptr_nonnull(val);

    yyjson_mut_val *content = yyjson_mut_obj_get(val, "content");
    ck_assert_ptr_nonnull(content);
    ck_assert_str_eq(yyjson_mut_get_str(content), "First\n\nSecond\n\nThird");

    yyjson_mut_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_user_message_empty_content)
{
    ik_message_t *msg = talloc_zero(test_ctx, ik_message_t);
    msg->role = IK_ROLE_USER;
    msg->content_count = 0;
    msg->content_blocks = NULL;

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *val = ik_openai_serialize_message(doc, msg);

    ck_assert_ptr_nonnull(val);

    yyjson_mut_val *content = yyjson_mut_obj_get(val, "content");
    ck_assert_ptr_nonnull(content);
    ck_assert_str_eq(yyjson_mut_get_str(content), "");

    yyjson_mut_doc_free(doc);
}
END_TEST

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
    // If there are any tool calls, content should be null even if text blocks exist
    ik_message_t *msg = talloc_zero(test_ctx, ik_message_t);
    msg->role = IK_ROLE_ASSISTANT;
    msg->content_count = 2;
    msg->content_blocks = talloc_array(msg, ik_content_block_t, 2);

    msg->content_blocks[0].type = IK_CONTENT_TEXT;
    msg->content_blocks[0].data.text.text = talloc_strdup(msg, "Some text");

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
    // Only one tool call in array (text block not serialized as tool call)
    ck_assert_int_eq((int)yyjson_mut_arr_size(tool_calls), 1);

    yyjson_mut_doc_free(doc);
}
END_TEST

/* ================================================================
 * Tool Message Tests
 * ================================================================ */

START_TEST(test_serialize_tool_message)
{
    ik_message_t *msg = talloc_zero(test_ctx, ik_message_t);
    msg->role = IK_ROLE_TOOL;
    msg->content_count = 1;
    msg->content_blocks = talloc_array(msg, ik_content_block_t, 1);
    msg->content_blocks[0].type = IK_CONTENT_TOOL_RESULT;
    msg->content_blocks[0].data.tool_result.tool_call_id = talloc_strdup(msg, "call_123");
    msg->content_blocks[0].data.tool_result.content = talloc_strdup(msg, "Tool result");

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *val = ik_openai_serialize_message(doc, msg);

    ck_assert_ptr_nonnull(val);

    yyjson_mut_val *role = yyjson_mut_obj_get(val, "role");
    ck_assert_ptr_nonnull(role);
    ck_assert_str_eq(yyjson_mut_get_str(role), "tool");

    yyjson_mut_val *tool_call_id = yyjson_mut_obj_get(val, "tool_call_id");
    ck_assert_ptr_nonnull(tool_call_id);
    ck_assert_str_eq(yyjson_mut_get_str(tool_call_id), "call_123");

    yyjson_mut_val *content = yyjson_mut_obj_get(val, "content");
    ck_assert_ptr_nonnull(content);
    ck_assert_str_eq(yyjson_mut_get_str(content), "Tool result");

    yyjson_mut_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_tool_message_empty_content)
{
    ik_message_t *msg = talloc_zero(test_ctx, ik_message_t);
    msg->role = IK_ROLE_TOOL;
    msg->content_count = 0;
    msg->content_blocks = NULL;

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *val = ik_openai_serialize_message(doc, msg);

    ck_assert_ptr_nonnull(val);

    yyjson_mut_val *role = yyjson_mut_obj_get(val, "role");
    ck_assert_ptr_nonnull(role);
    ck_assert_str_eq(yyjson_mut_get_str(role), "tool");

    // Should not have tool_call_id or content fields
    yyjson_mut_val *tool_call_id = yyjson_mut_obj_get(val, "tool_call_id");
    ck_assert_ptr_null(tool_call_id);

    yyjson_mut_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_tool_message_wrong_block_type)
{
    ik_message_t *msg = talloc_zero(test_ctx, ik_message_t);
    msg->role = IK_ROLE_TOOL;
    msg->content_count = 1;
    msg->content_blocks = talloc_array(msg, ik_content_block_t, 1);
    msg->content_blocks[0].type = IK_CONTENT_TEXT;  // Wrong type
    msg->content_blocks[0].data.text.text = talloc_strdup(msg, "Text");

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *val = ik_openai_serialize_message(doc, msg);

    ck_assert_ptr_nonnull(val);

    yyjson_mut_val *role = yyjson_mut_obj_get(val, "role");
    ck_assert_ptr_nonnull(role);
    ck_assert_str_eq(yyjson_mut_get_str(role), "tool");

    // Should not have tool_call_id or content since block type is wrong
    yyjson_mut_val *tool_call_id = yyjson_mut_obj_get(val, "tool_call_id");
    ck_assert_ptr_null(tool_call_id);

    yyjson_mut_doc_free(doc);
}
END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *openai_serialize_suite(void)
{
    Suite *s = suite_create("OpenAI Serialize");

    TCase *tc_user = tcase_create("User Messages");
    tcase_add_unchecked_fixture(tc_user, setup, teardown);
    tcase_add_test(tc_user, test_serialize_user_message_single_text);
    tcase_add_test(tc_user, test_serialize_user_message_multiple_text_blocks);
    tcase_add_test(tc_user, test_serialize_user_message_empty_content);
    suite_add_tcase(s, tc_user);

    TCase *tc_assistant = tcase_create("Assistant Messages");
    tcase_add_unchecked_fixture(tc_assistant, setup, teardown);
    tcase_add_test(tc_assistant, test_serialize_assistant_message_text);
    tcase_add_test(tc_assistant, test_serialize_assistant_message_with_tool_calls);
    tcase_add_test(tc_assistant, test_serialize_assistant_message_multiple_tool_calls);
    tcase_add_test(tc_assistant, test_serialize_assistant_message_mixed_content_and_tool_calls);
    suite_add_tcase(s, tc_assistant);

    TCase *tc_tool = tcase_create("Tool Messages");
    tcase_add_unchecked_fixture(tc_tool, setup, teardown);
    tcase_add_test(tc_tool, test_serialize_tool_message);
    tcase_add_test(tc_tool, test_serialize_tool_message_empty_content);
    tcase_add_test(tc_tool, test_serialize_tool_message_wrong_block_type);
    suite_add_tcase(s, tc_tool);

    return s;
}

int main(void)
{
    Suite *s = openai_serialize_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
