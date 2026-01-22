#include "../../../test_constants.h"
/**
 * @file message_serialize_helpers.c
 * @brief Message serialization tests for Anthropic provider
 *
 * This file contains tests for serializing message content, role mapping,
 * and complete message serialization.
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/anthropic/request_serialize.h"
#include "providers/provider.h"
#include "wrapper_json.h"
#include "message_serialize_helper.h"

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
 * Message Content Serialization - Success Paths
 * ================================================================ */

START_TEST(test_serialize_message_content_single_text_success) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *msg_obj = yyjson_mut_obj(doc);

    ik_message_t message;
    message.content_count = 1;
    ik_content_block_t blocks[1] = {0};
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
    ik_content_block_t blocks[2] = {0};
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
    ik_content_block_t blocks[1] = {0};
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

Suite *message_serialize_suite(void)
{
    Suite *s = suite_create("Message Serialization");

    TCase *tc_message_content = tcase_create("Message Content");
    tcase_set_timeout(tc_message_content, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_message_content, setup, teardown);
    tcase_add_test(tc_message_content, test_serialize_message_content_single_text_success);
    tcase_add_test(tc_message_content, test_serialize_message_content_multiple_blocks_success);
    tcase_add_test(tc_message_content, test_serialize_message_content_non_text_block);
    suite_add_tcase(s, tc_message_content);

    TCase *tc_role = tcase_create("Role Mapping");
    tcase_set_timeout(tc_role, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_role, setup, teardown);
    tcase_add_test(tc_role, test_role_to_string_user);
    tcase_add_test(tc_role, test_role_to_string_assistant);
    tcase_add_test(tc_role, test_role_to_string_tool);
    suite_add_tcase(s, tc_role);

    TCase *tc_messages = tcase_create("Message Serialization");
    tcase_set_timeout(tc_messages, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_messages, setup, teardown);
    tcase_add_test(tc_messages, test_serialize_messages_success);
    tcase_add_test(tc_messages, test_serialize_messages_empty_array);
    suite_add_tcase(s, tc_messages);

    return s;
}
