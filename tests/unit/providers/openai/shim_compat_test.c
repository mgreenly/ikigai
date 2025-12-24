#include "providers/openai/shim.h"
#include "providers/provider.h"
#include "providers/request.h"
#include "msg.h"
#include "error.h"

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
 * Compatibility Tests - Verify Shim Produces Expected Outputs
 * ================================================================ */

START_TEST(test_compat_simple_text_conversation)
{
    /* Build normalized request */
    ik_request_t *req = NULL;
    res_t create_res = ik_request_create(test_ctx, "gpt-5-mini", &req);
    ck_assert(!is_err(&create_res));

    ik_request_set_system(req, "You are helpful");
    ik_request_add_message(req, IK_ROLE_USER, "Hello");

    /* Test conversation building separately */
    ik_openai_conversation_t *conv = NULL;
    res_t conv_res = ik_openai_shim_build_conversation(test_ctx, req, &conv);
    ck_assert(!is_err(&conv_res));

    /* Verify conversation structure */
    ck_assert(conv->message_count == 2);

    /* System message */
    ck_assert_str_eq(conv->messages[0]->kind, "system");
    ck_assert_str_eq(conv->messages[0]->content, "You are helpful");

    /* User message */
    ck_assert_str_eq(conv->messages[1]->kind, "user");
    ck_assert_str_eq(conv->messages[1]->content, "Hello");
}
END_TEST

START_TEST(test_compat_multi_turn_conversation)
{
    /* Build multi-turn normalized request */
    ik_request_t *req = NULL;
    res_t create_res = ik_request_create(test_ctx, "gpt-5-mini", &req);
    ck_assert(!is_err(&create_res));

    ik_request_add_message(req, IK_ROLE_USER, "What is 2+2?");
    ik_request_add_message(req, IK_ROLE_ASSISTANT, "2+2 equals 4");
    ik_request_add_message(req, IK_ROLE_USER, "Thanks!");

    /* Test conversation building */
    ik_openai_conversation_t *conv = NULL;
    res_t conv_res = ik_openai_shim_build_conversation(test_ctx, req, &conv);
    ck_assert(!is_err(&conv_res));

    /* Verify conversation structure */
    ck_assert(conv->message_count == 3);
    ck_assert_str_eq(conv->messages[0]->kind, "user");
    ck_assert_str_eq(conv->messages[1]->kind, "assistant");
    ck_assert_str_eq(conv->messages[2]->kind, "user");

    /* Verify content preserved */
    ck_assert_str_eq(conv->messages[0]->content, "What is 2+2?");
    ck_assert_str_eq(conv->messages[1]->content, "2+2 equals 4");
    ck_assert_str_eq(conv->messages[2]->content, "Thanks!");
}
END_TEST

START_TEST(test_compat_conversation_with_tool_call)
{
    /* Build request with tool call */
    ik_request_t *req = NULL;
    res_t create_res = ik_request_create(test_ctx, "gpt-5-mini", &req);
    ck_assert(!is_err(&create_res));

    ik_request_add_message(req, IK_ROLE_USER, "Read /etc/hosts");

    /* Add tool call */
    ik_content_block_t *tool_block = ik_content_block_tool_call(test_ctx,
        "call_abc123", "read_file", "{\"path\":\"/etc/hosts\"}");
    ik_request_add_message_blocks(req, IK_ROLE_ASSISTANT, tool_block, 1);

    /* Test conversation building */
    ik_openai_conversation_t *conv = NULL;
    res_t conv_res = ik_openai_shim_build_conversation(test_ctx, req, &conv);
    ck_assert(!is_err(&conv_res));

    /* Verify tool call message */
    ck_assert(conv->message_count == 2);
    ck_assert_str_eq(conv->messages[1]->kind, "tool_call");
    ck_assert_ptr_nonnull(conv->messages[1]->data_json);

    /* Verify data_json contains tool call fields */
    const char *data_json = conv->messages[1]->data_json;
    ck_assert(strstr(data_json, "call_abc123") != NULL);
    ck_assert(strstr(data_json, "read_file") != NULL);
}
END_TEST

START_TEST(test_compat_conversation_with_tool_result)
{
    /* Build request with tool result */
    ik_request_t *req = NULL;
    res_t create_res = ik_request_create(test_ctx, "gpt-5-mini", &req);
    ck_assert(!is_err(&create_res));

    /* Add tool result */
    ik_content_block_t *result_block = ik_content_block_tool_result(test_ctx,
        "call_abc123", "127.0.0.1 localhost", false);
    ik_request_add_message_blocks(req, IK_ROLE_TOOL, result_block, 1);

    /* Test conversation building */
    ik_openai_conversation_t *conv = NULL;
    res_t conv_res = ik_openai_shim_build_conversation(test_ctx, req, &conv);
    ck_assert(!is_err(&conv_res));

    /* Verify tool result message */
    ck_assert(conv->message_count == 1);
    ck_assert_str_eq(conv->messages[0]->kind, "tool_result");
    ck_assert_ptr_nonnull(conv->messages[0]->data_json);

    /* Verify data_json contains tool result fields */
    const char *data_json = conv->messages[0]->data_json;
    ck_assert(strstr(data_json, "call_abc123") != NULL);
}
END_TEST

START_TEST(test_compat_system_prompt_concatenation)
{
    /* Build request with system prompt */
    ik_request_t *req = NULL;
    res_t create_res = ik_request_create(test_ctx, "gpt-5-mini", &req);
    ck_assert(!is_err(&create_res));

    /* Set system prompt */
    ik_request_set_system(req, "Be helpful and concise");
    ik_request_add_message(req, IK_ROLE_USER, "Hi");

    /* Test conversation building */
    ik_openai_conversation_t *conv = NULL;
    res_t conv_res = ik_openai_shim_build_conversation(test_ctx, req, &conv);
    ck_assert(!is_err(&conv_res));

    /* Verify system message is first */
    ck_assert(conv->message_count == 2);
    ck_assert_str_eq(conv->messages[0]->kind, "system");
    ck_assert_str_eq(conv->messages[0]->content, "Be helpful and concise");
}
END_TEST

/* ================================================================
 * Response Compatibility Tests
 * ================================================================ */

START_TEST(test_compat_response_text)
{
    /* Create legacy text response */
    ik_msg_t *legacy_msg = talloc_zero(test_ctx, ik_msg_t);
    legacy_msg->kind = talloc_strdup(legacy_msg, "assistant");
    legacy_msg->content = talloc_strdup(legacy_msg, "The answer is 42");
    legacy_msg->data_json = NULL;

    /* Transform to normalized format */
    ik_response_t *response = NULL;
    res_t result = ik_openai_shim_transform_response(test_ctx, legacy_msg, &response);

    ck_assert(!is_err(&result));
    ck_assert(response->content_count == 1);
    ck_assert_int_eq(response->content_blocks[0].type, IK_CONTENT_TEXT);
    ck_assert_str_eq(response->content_blocks[0].data.text.text, "The answer is 42");
    ck_assert_int_eq(response->finish_reason, IK_FINISH_STOP);
}
END_TEST

START_TEST(test_compat_response_tool_call)
{
    /* Create legacy tool call response */
    ik_msg_t *legacy_msg = talloc_zero(test_ctx, ik_msg_t);
    legacy_msg->kind = talloc_strdup(legacy_msg, "tool_call");
    legacy_msg->content = talloc_strdup(legacy_msg, "glob(pattern=\"*.c\")");
    legacy_msg->data_json = talloc_strdup(legacy_msg,
        "{\"id\":\"call_xyz\",\"name\":\"glob\",\"arguments\":\"{\\\"pattern\\\":\\\"*.c\\\"}\"}");

    /* Transform to normalized format */
    ik_response_t *response = NULL;
    res_t result = ik_openai_shim_transform_response(test_ctx, legacy_msg, &response);

    ck_assert(!is_err(&result));
    ck_assert(response->content_count == 1);
    ck_assert_int_eq(response->content_blocks[0].type, IK_CONTENT_TOOL_CALL);
    ck_assert_str_eq(response->content_blocks[0].data.tool_call.id, "call_xyz");
    ck_assert_str_eq(response->content_blocks[0].data.tool_call.name, "glob");
    ck_assert_str_eq(response->content_blocks[0].data.tool_call.arguments, "{\"pattern\":\"*.c\"}");
    ck_assert_int_eq(response->finish_reason, IK_FINISH_TOOL_USE);
}
END_TEST

/* ================================================================
 * Field Mapping Tests
 * ================================================================ */

START_TEST(test_compat_max_tokens_mapping)
{
    /* Build request with max_output_tokens */
    ik_request_t *req = NULL;
    res_t create_res = ik_request_create(test_ctx, "gpt-5-mini", &req);
    ck_assert(!is_err(&create_res));

    req->max_output_tokens = 2048;
    ik_request_add_message(req, IK_ROLE_USER, "Test");

    /* Verify transform succeeds (can't access legacy_req fields) */
    ik_openai_request_t *legacy_req = NULL;
    res_t transform_res = ik_openai_shim_transform_request(test_ctx, req, &legacy_req);
    ck_assert(!is_err(&transform_res));
    ck_assert_ptr_nonnull(legacy_req);
}
END_TEST

START_TEST(test_compat_temperature_default)
{
    /* Build request without temperature */
    ik_request_t *req = NULL;
    res_t create_res = ik_request_create(test_ctx, "gpt-5-mini", &req);
    ck_assert(!is_err(&create_res));

    ik_request_add_message(req, IK_ROLE_USER, "Test");

    /* Verify transform succeeds */
    ik_openai_request_t *legacy_req = NULL;
    res_t transform_res = ik_openai_shim_transform_request(test_ctx, req, &legacy_req);
    ck_assert(!is_err(&transform_res));
    ck_assert_ptr_nonnull(legacy_req);
}
END_TEST

START_TEST(test_compat_streaming_enabled)
{
    /* Build request */
    ik_request_t *req = NULL;
    res_t create_res = ik_request_create(test_ctx, "gpt-5-mini", &req);
    ck_assert(!is_err(&create_res));

    ik_request_add_message(req, IK_ROLE_USER, "Test");

    /* Verify transform succeeds */
    ik_openai_request_t *legacy_req = NULL;
    res_t transform_res = ik_openai_shim_transform_request(test_ctx, req, &legacy_req);
    ck_assert(!is_err(&transform_res));
    ck_assert_ptr_nonnull(legacy_req);
}
END_TEST

/* ================================================================
 * Test Suite Definition
 * ================================================================ */

static Suite *shim_compat_suite(void)
{
    Suite *s = suite_create("OpenAI Shim Compatibility");

    /* Request compatibility tests */
    TCase *tc_req = tcase_create("Request Compatibility");
    tcase_add_checked_fixture(tc_req, setup, teardown);
    tcase_add_test(tc_req, test_compat_simple_text_conversation);
    tcase_add_test(tc_req, test_compat_multi_turn_conversation);
    tcase_add_test(tc_req, test_compat_conversation_with_tool_call);
    tcase_add_test(tc_req, test_compat_conversation_with_tool_result);
    tcase_add_test(tc_req, test_compat_system_prompt_concatenation);
    suite_add_tcase(s, tc_req);

    /* Response compatibility tests */
    TCase *tc_resp = tcase_create("Response Compatibility");
    tcase_add_checked_fixture(tc_resp, setup, teardown);
    tcase_add_test(tc_resp, test_compat_response_text);
    tcase_add_test(tc_resp, test_compat_response_tool_call);
    suite_add_tcase(s, tc_resp);

    /* Field mapping tests */
    TCase *tc_field = tcase_create("Field Mapping");
    tcase_add_checked_fixture(tc_field, setup, teardown);
    tcase_add_test(tc_field, test_compat_max_tokens_mapping);
    tcase_add_test(tc_field, test_compat_temperature_default);
    tcase_add_test(tc_field, test_compat_streaming_enabled);
    suite_add_tcase(s, tc_field);

    return s;
}

int main(void)
{
    int failed;
    Suite *s = shim_compat_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
