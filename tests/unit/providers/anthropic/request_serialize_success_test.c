/**
 * @file request_serialize_success_test.c
 * @brief Success path coverage tests for Anthropic request serialization
 *
 * This test file focuses on successful serialization paths that were
 * missing from the existing coverage tests.
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
 * Content Block Serialization - Success Paths
 * ================================================================ */

START_TEST(test_serialize_content_block_text_success) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block;
    block.type = IK_CONTENT_TEXT;
    block.data.text.text = talloc_strdup(test_ctx, "Hello, world!");

    bool result = ik_anthropic_serialize_content_block(doc, arr, &block);

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

    bool result = ik_anthropic_serialize_content_block(doc, arr, &block);

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

START_TEST(test_serialize_content_block_tool_call_success) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *arr = yyjson_mut_arr(doc);

    ik_content_block_t block;
    block.type = IK_CONTENT_TOOL_CALL;
    block.data.tool_call.id = talloc_strdup(test_ctx, "call_abc123");
    block.data.tool_call.name = talloc_strdup(test_ctx, "get_weather");
    block.data.tool_call.arguments = talloc_strdup(test_ctx, "{\"location\":\"San Francisco\"}");

    bool result = ik_anthropic_serialize_content_block(doc, arr, &block);

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

    bool result = ik_anthropic_serialize_content_block(doc, arr, &block);

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

    bool result = ik_anthropic_serialize_content_block(doc, arr, &block);

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

    bool result = ik_anthropic_serialize_content_block(doc, arr, &block);

    ck_assert(result);

    // Verify the error flag
    yyjson_mut_val *obj = yyjson_mut_arr_get(arr, 0);
    yyjson_mut_val *is_error = yyjson_mut_obj_get(obj, "is_error");
    ck_assert(yyjson_mut_get_bool(is_error));

    yyjson_mut_doc_free(doc);
}
END_TEST

/* ================================================================
 * Message Content Serialization - Success Paths
 * ================================================================ */

START_TEST(test_serialize_message_content_single_text_success) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *msg_obj = yyjson_mut_obj(doc);

    ik_message_t message;
    message.content_count = 1;
    ik_content_block_t blocks[1];
    blocks[0].type = IK_CONTENT_TEXT;
    blocks[0].data.text.text = talloc_strdup(test_ctx, "Single text block");
    message.content_blocks = blocks;

    bool result = ik_anthropic_serialize_message_content(doc, msg_obj, &message);

    ck_assert(result);

    // For single text block, content should be a string
    yyjson_mut_val *content = yyjson_mut_obj_get(msg_obj, "content");
    ck_assert_ptr_nonnull(content);
    ck_assert(yyjson_mut_is_str(content));
    ck_assert_str_eq(yyjson_mut_get_str(content), "Single text block");

    yyjson_mut_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_message_content_multiple_blocks_success) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *msg_obj = yyjson_mut_obj(doc);

    ik_message_t message;
    message.content_count = 2;
    ik_content_block_t blocks[2];
    blocks[0].type = IK_CONTENT_TEXT;
    blocks[0].data.text.text = talloc_strdup(test_ctx, "First block");
    blocks[1].type = IK_CONTENT_TEXT;
    blocks[1].data.text.text = talloc_strdup(test_ctx, "Second block");
    message.content_blocks = blocks;

    bool result = ik_anthropic_serialize_message_content(doc, msg_obj, &message);

    ck_assert(result);

    // For multiple blocks, content should be an array
    yyjson_mut_val *content = yyjson_mut_obj_get(msg_obj, "content");
    ck_assert_ptr_nonnull(content);
    ck_assert(yyjson_mut_is_arr(content));
    ck_assert_int_eq((int)yyjson_mut_arr_size(content), 2);

    yyjson_mut_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_message_content_non_text_block) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *msg_obj = yyjson_mut_obj(doc);

    ik_message_t message;
    message.content_count = 1;
    ik_content_block_t blocks[1];
    blocks[0].type = IK_CONTENT_THINKING;
    blocks[0].data.thinking.text = talloc_strdup(test_ctx, "Thinking...");
    message.content_blocks = blocks;

    bool result = ik_anthropic_serialize_message_content(doc, msg_obj, &message);

    ck_assert(result);

    // Even for single non-text block, content should be an array
    yyjson_mut_val *content = yyjson_mut_obj_get(msg_obj, "content");
    ck_assert_ptr_nonnull(content);
    ck_assert(yyjson_mut_is_arr(content));

    yyjson_mut_doc_free(doc);
}
END_TEST

/* ================================================================
 * Role Mapping Tests
 * ================================================================ */

START_TEST(test_role_to_string_user) {
    const char *role = ik_anthropic_role_to_string(IK_ROLE_USER);
    ck_assert_str_eq(role, "user");
}
END_TEST

START_TEST(test_role_to_string_assistant) {
    const char *role = ik_anthropic_role_to_string(IK_ROLE_ASSISTANT);
    ck_assert_str_eq(role, "assistant");
}
END_TEST

START_TEST(test_role_to_string_tool) {
    const char *role = ik_anthropic_role_to_string(IK_ROLE_TOOL);
    ck_assert_str_eq(role, "user"); // Tool results are sent as user messages
}
END_TEST

/* ================================================================
 * Message Serialization - Success Paths
 * ================================================================ */

START_TEST(test_serialize_messages_success) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *root = yyjson_mut_obj(doc);

    ik_request_t req;
    req.message_count = 2;
    ik_message_t messages[2];

    // First message: user
    messages[0].role = IK_ROLE_USER;
    messages[0].content_count = 1;
    ik_content_block_t blocks1[1];
    blocks1[0].type = IK_CONTENT_TEXT;
    blocks1[0].data.text.text = talloc_strdup(test_ctx, "Hello");
    messages[0].content_blocks = blocks1;

    // Second message: assistant
    messages[1].role = IK_ROLE_ASSISTANT;
    messages[1].content_count = 1;
    ik_content_block_t blocks2[1];
    blocks2[0].type = IK_CONTENT_TEXT;
    blocks2[0].data.text.text = talloc_strdup(test_ctx, "Hi there!");
    messages[1].content_blocks = blocks2;

    req.messages = messages;

    bool result = ik_anthropic_serialize_messages(doc, root, &req);

    ck_assert(result);

    // Verify messages array was added
    yyjson_mut_val *messages_arr = yyjson_mut_obj_get(root, "messages");
    ck_assert_ptr_nonnull(messages_arr);
    ck_assert(yyjson_mut_is_arr(messages_arr));
    ck_assert_int_eq((int)yyjson_mut_arr_size(messages_arr), 2);

    // Verify first message
    yyjson_mut_val *msg1 = yyjson_mut_arr_get(messages_arr, 0);
    yyjson_mut_val *role1 = yyjson_mut_obj_get(msg1, "role");
    ck_assert_str_eq(yyjson_mut_get_str(role1), "user");

    // Verify second message
    yyjson_mut_val *msg2 = yyjson_mut_arr_get(messages_arr, 1);
    yyjson_mut_val *role2 = yyjson_mut_obj_get(msg2, "role");
    ck_assert_str_eq(yyjson_mut_get_str(role2), "assistant");

    yyjson_mut_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_messages_empty_array) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *root = yyjson_mut_obj(doc);

    ik_request_t req;
    req.message_count = 0;
    req.messages = NULL;

    bool result = ik_anthropic_serialize_messages(doc, root, &req);

    ck_assert(result);

    // Verify empty messages array was added
    yyjson_mut_val *messages_arr = yyjson_mut_obj_get(root, "messages");
    ck_assert_ptr_nonnull(messages_arr);
    ck_assert(yyjson_mut_is_arr(messages_arr));
    ck_assert_int_eq((int)yyjson_mut_arr_size(messages_arr), 0);

    yyjson_mut_doc_free(doc);
}
END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *request_serialize_success_suite(void)
{
    Suite *s = suite_create("Anthropic Request Serialize Success Paths");

    TCase *tc_content = tcase_create("Content Block Success");
    tcase_set_timeout(tc_content, 30);
    tcase_add_unchecked_fixture(tc_content, setup, teardown);
    tcase_add_test(tc_content, test_serialize_content_block_text_success);
    tcase_add_test(tc_content, test_serialize_content_block_thinking_success);
    tcase_add_test(tc_content, test_serialize_content_block_tool_call_success);
    tcase_add_test(tc_content, test_serialize_content_block_tool_call_invalid_json);
    tcase_add_test(tc_content, test_serialize_content_block_tool_result_success);
    tcase_add_test(tc_content, test_serialize_content_block_tool_result_with_error);
    suite_add_tcase(s, tc_content);

    TCase *tc_message_content = tcase_create("Message Content Success");
    tcase_set_timeout(tc_message_content, 30);
    tcase_add_unchecked_fixture(tc_message_content, setup, teardown);
    tcase_add_test(tc_message_content, test_serialize_message_content_single_text_success);
    tcase_add_test(tc_message_content, test_serialize_message_content_multiple_blocks_success);
    tcase_add_test(tc_message_content, test_serialize_message_content_non_text_block);
    suite_add_tcase(s, tc_message_content);

    TCase *tc_role = tcase_create("Role Mapping Success");
    tcase_set_timeout(tc_role, 30);
    tcase_add_unchecked_fixture(tc_role, setup, teardown);
    tcase_add_test(tc_role, test_role_to_string_user);
    tcase_add_test(tc_role, test_role_to_string_assistant);
    tcase_add_test(tc_role, test_role_to_string_tool);
    suite_add_tcase(s, tc_role);

    TCase *tc_messages = tcase_create("Message Serialization Success");
    tcase_set_timeout(tc_messages, 30);
    tcase_add_unchecked_fixture(tc_messages, setup, teardown);
    tcase_add_test(tc_messages, test_serialize_messages_success);
    tcase_add_test(tc_messages, test_serialize_messages_empty_array);
    suite_add_tcase(s, tc_messages);

    return s;
}

int main(void)
{
    Suite *s = request_serialize_success_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
