#include "providers/openai/shim.h"
#include "providers/provider.h"
#include "providers/request.h"
#include "error.h"
#include "msg.h"

#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

/* Forward declare the conversation type structure */
struct ik_openai_conversation {
    ik_msg_t **messages;
    size_t message_count;
};

/* Test fixture */
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
 * Message Transformation Tests
 * ================================================================ */

START_TEST(test_transform_text_user_message)
{
    /* Create normalized message */
    ik_message_t msg = {0};
    msg.role = IK_ROLE_USER;
    msg.content_count = 1;

    ik_content_block_t block = {0};
    block.type = IK_CONTENT_TEXT;
    block.data.text.text = talloc_strdup(test_ctx, "hello");
    msg.content_blocks = &block;

    /* Transform to legacy format */
    ik_msg_t *legacy_msg = NULL;
    res_t result = ik_openai_shim_transform_message(test_ctx, &msg, &legacy_msg);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(legacy_msg);
    ck_assert_str_eq(legacy_msg->kind, "user");
    ck_assert_str_eq(legacy_msg->content, "hello");
    ck_assert_ptr_null(legacy_msg->data_json);
}
END_TEST

START_TEST(test_transform_text_assistant_message)
{
    /* Create normalized message */
    ik_message_t msg = {0};
    msg.role = IK_ROLE_ASSISTANT;
    msg.content_count = 1;

    ik_content_block_t block = {0};
    block.type = IK_CONTENT_TEXT;
    block.data.text.text = talloc_strdup(test_ctx, "hi");
    msg.content_blocks = &block;

    /* Transform to legacy format */
    ik_msg_t *legacy_msg = NULL;
    res_t result = ik_openai_shim_transform_message(test_ctx, &msg, &legacy_msg);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(legacy_msg);
    ck_assert_str_eq(legacy_msg->kind, "assistant");
    ck_assert_str_eq(legacy_msg->content, "hi");
    ck_assert_ptr_null(legacy_msg->data_json);
}
END_TEST

START_TEST(test_transform_tool_call_message)
{
    /* Create normalized message with tool call */
    ik_message_t msg = {0};
    msg.role = IK_ROLE_ASSISTANT;
    msg.content_count = 1;

    ik_content_block_t block = {0};
    block.type = IK_CONTENT_TOOL_CALL;
    block.data.tool_call.id = talloc_strdup(test_ctx, "call_123");
    block.data.tool_call.name = talloc_strdup(test_ctx, "read_file");
    block.data.tool_call.arguments = talloc_strdup(test_ctx, "{\"path\":\"/etc/hosts\"}");
    msg.content_blocks = &block;

    /* Transform to legacy format */
    ik_msg_t *legacy_msg = NULL;
    res_t result = ik_openai_shim_transform_message(test_ctx, &msg, &legacy_msg);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(legacy_msg);
    ck_assert_str_eq(legacy_msg->kind, "tool_call");
    ck_assert_ptr_nonnull(legacy_msg->data_json);

    /* Verify data_json contains the tool call data */
    ck_assert(strstr(legacy_msg->data_json, "call_123") != NULL);
    ck_assert(strstr(legacy_msg->data_json, "read_file") != NULL);
}
END_TEST

START_TEST(test_transform_tool_result_message)
{
    /* Create normalized message with tool result */
    ik_message_t msg = {0};
    msg.role = IK_ROLE_TOOL;
    msg.content_count = 1;

    ik_content_block_t block = {0};
    block.type = IK_CONTENT_TOOL_RESULT;
    block.data.tool_result.tool_call_id = talloc_strdup(test_ctx, "call_123");
    block.data.tool_result.content = talloc_strdup(test_ctx, "file contents");
    block.data.tool_result.is_error = false;
    msg.content_blocks = &block;

    /* Transform to legacy format */
    ik_msg_t *legacy_msg = NULL;
    res_t result = ik_openai_shim_transform_message(test_ctx, &msg, &legacy_msg);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(legacy_msg);
    ck_assert_str_eq(legacy_msg->kind, "tool_result");
    ck_assert_ptr_nonnull(legacy_msg->data_json);

    /* Verify data_json contains the tool result data */
    ck_assert(strstr(legacy_msg->data_json, "call_123") != NULL);
}
END_TEST

START_TEST(test_transform_message_empty_content)
{
    /* Message with no content blocks should fail */
    ik_message_t msg = {0};
    msg.role = IK_ROLE_USER;
    msg.content_count = 0;
    msg.content_blocks = NULL;

    ik_msg_t *legacy_msg = NULL;
    res_t result = ik_openai_shim_transform_message(test_ctx, &msg, &legacy_msg);

    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_INVALID_ARG);
}
END_TEST

START_TEST(test_transform_message_thinking_not_supported)
{
    /* Thinking blocks should not be supported */
    ik_message_t msg = {0};
    msg.role = IK_ROLE_ASSISTANT;
    msg.content_count = 1;

    ik_content_block_t block = {0};
    block.type = IK_CONTENT_THINKING;
    block.data.thinking.text = talloc_strdup(test_ctx, "thinking...");
    msg.content_blocks = &block;

    ik_msg_t *legacy_msg = NULL;
    res_t result = ik_openai_shim_transform_message(test_ctx, &msg, &legacy_msg);

    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_INVALID_ARG);
}
END_TEST

/* ================================================================
 * Request Transformation Tests
 * ================================================================ */

START_TEST(test_transform_request_simple)
{
    /* Create normalized request */
    ik_request_t *req = NULL;
    res_t create_res = ik_request_create(test_ctx, "gpt-5-mini", &req);
    ck_assert(!is_err(&create_res));

    /* Add system prompt */
    res_t sys_res = ik_request_set_system(req, "You are helpful");
    ck_assert(!is_err(&sys_res));

    /* Add user message */
    res_t msg_res = ik_request_add_message(req, IK_ROLE_USER, "Hello");
    ck_assert(!is_err(&msg_res));

    /* Build conversation */
    ik_openai_conversation_t *conv = NULL;
    res_t result = ik_openai_shim_build_conversation(test_ctx, req, &conv);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(conv);

    /* Verify conversation has system + user messages */
    ck_assert(conv->message_count == 2);
    ck_assert_str_eq(conv->messages[0]->kind, "system");
    ck_assert_str_eq(conv->messages[0]->content, "You are helpful");
    ck_assert_str_eq(conv->messages[1]->kind, "user");
    ck_assert_str_eq(conv->messages[1]->content, "Hello");
}
END_TEST

START_TEST(test_transform_request_no_system_prompt)
{
    /* Create request without system prompt */
    ik_request_t *req = NULL;
    res_t create_res = ik_request_create(test_ctx, "gpt-5-mini", &req);
    ck_assert(!is_err(&create_res));

    /* Add only user message */
    res_t msg_res = ik_request_add_message(req, IK_ROLE_USER, "Hello");
    ck_assert(!is_err(&msg_res));

    /* Build conversation */
    ik_openai_conversation_t *conv = NULL;
    res_t result = ik_openai_shim_build_conversation(test_ctx, req, &conv);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(conv);

    /* Should have only 1 message */
    ck_assert(conv->message_count == 1);
    ck_assert_str_eq(conv->messages[0]->kind, "user");
}
END_TEST

START_TEST(test_transform_request_multi_turn)
{
    /* Create multi-turn conversation */
    ik_request_t *req = NULL;
    res_t create_res = ik_request_create(test_ctx, "gpt-5-mini", &req);
    ck_assert(!is_err(&create_res));

    /* Add messages */
    ik_request_add_message(req, IK_ROLE_USER, "What is 2+2?");
    ik_request_add_message(req, IK_ROLE_ASSISTANT, "4");
    ik_request_add_message(req, IK_ROLE_USER, "What is 3+3?");

    /* Build conversation */
    ik_openai_conversation_t *conv = NULL;
    res_t result = ik_openai_shim_build_conversation(test_ctx, req, &conv);

    ck_assert(!is_err(&result));
    ck_assert(conv->message_count == 3);
    ck_assert_str_eq(conv->messages[0]->kind, "user");
    ck_assert_str_eq(conv->messages[1]->kind, "assistant");
    ck_assert_str_eq(conv->messages[2]->kind, "user");
}
END_TEST

START_TEST(test_transform_request_with_tool_call)
{
    /* Create request with tool call */
    ik_request_t *req = NULL;
    res_t create_res = ik_request_create(test_ctx, "gpt-5-mini", &req);
    ck_assert(!is_err(&create_res));

    /* Add user message */
    ik_request_add_message(req, IK_ROLE_USER, "Read /etc/hosts");

    /* Add tool call message */
    ik_content_block_t *tool_block = ik_content_block_tool_call(test_ctx,
        "call_123", "read_file", "{\"path\":\"/etc/hosts\"}");
    ik_request_add_message_blocks(req, IK_ROLE_ASSISTANT, tool_block, 1);

    /* Build conversation */
    ik_openai_conversation_t *conv = NULL;
    res_t result = ik_openai_shim_build_conversation(test_ctx, req, &conv);

    ck_assert(!is_err(&result));
    ck_assert(conv->message_count == 2);
    ck_assert_str_eq(conv->messages[0]->kind, "user");
    ck_assert_str_eq(conv->messages[1]->kind, "tool_call");
}
END_TEST

START_TEST(test_transform_request_with_tool_result)
{
    /* Create request with tool result */
    ik_request_t *req = NULL;
    res_t create_res = ik_request_create(test_ctx, "gpt-5-mini", &req);
    ck_assert(!is_err(&create_res));

    /* Add tool result message */
    ik_content_block_t *result_block = ik_content_block_tool_result(test_ctx,
        "call_123", "file contents here", false);
    ik_request_add_message_blocks(req, IK_ROLE_TOOL, result_block, 1);

    /* Build conversation */
    ik_openai_conversation_t *conv = NULL;
    res_t result = ik_openai_shim_build_conversation(test_ctx, req, &conv);

    ck_assert(!is_err(&result));
    ck_assert(conv->message_count == 1);
    ck_assert_str_eq(conv->messages[0]->kind, "tool_result");
}
END_TEST

START_TEST(test_transform_request_empty_messages)
{
    /* Request with no messages should fail */
    ik_request_t *req = NULL;
    res_t create_res = ik_request_create(test_ctx, "gpt-5-mini", &req);
    ck_assert(!is_err(&create_res));

    /* Don't add any messages */

    /* Transform should fail */
    ik_openai_conversation_t *conv = NULL;
    res_t result = ik_openai_shim_build_conversation(test_ctx, req, &conv);

    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_INVALID_ARG);
}
END_TEST

/* ================================================================
 * Test Suite Definition
 * ================================================================ */

static Suite *shim_request_suite(void)
{
    Suite *s = suite_create("OpenAI Shim Request Transform");

    /* Message transformation tests */
    TCase *tc_msg = tcase_create("Message Transform");
    tcase_add_checked_fixture(tc_msg, setup, teardown);
    tcase_add_test(tc_msg, test_transform_text_user_message);
    tcase_add_test(tc_msg, test_transform_text_assistant_message);
    tcase_add_test(tc_msg, test_transform_tool_call_message);
    tcase_add_test(tc_msg, test_transform_tool_result_message);
    tcase_add_test(tc_msg, test_transform_message_empty_content);
    tcase_add_test(tc_msg, test_transform_message_thinking_not_supported);
    suite_add_tcase(s, tc_msg);

    /* Request transformation tests */
    TCase *tc_req = tcase_create("Request Transform");
    tcase_add_checked_fixture(tc_req, setup, teardown);
    tcase_add_test(tc_req, test_transform_request_simple);
    tcase_add_test(tc_req, test_transform_request_no_system_prompt);
    tcase_add_test(tc_req, test_transform_request_multi_turn);
    tcase_add_test(tc_req, test_transform_request_with_tool_call);
    tcase_add_test(tc_req, test_transform_request_with_tool_result);
    tcase_add_test(tc_req, test_transform_request_empty_messages);
    suite_add_tcase(s, tc_req);

    return s;
}

int main(void)
{
    int failed;
    Suite *s = shim_request_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
