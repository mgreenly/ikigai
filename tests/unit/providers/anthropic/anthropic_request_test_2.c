/**
 * @file anthropic_request_test_2.c
 * @brief Unit tests for Anthropic request serialization - Part 2: Message Content tests
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/anthropic/request.h"
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
 * Helper Functions
 * ================================================================ */

static ik_request_t *create_basic_request(TALLOC_CTX *ctx)
{
    ik_request_t *req = talloc_zero(ctx, ik_request_t);
    req->model = talloc_strdup(req, "claude-3-5-sonnet-20241022");
    req->max_output_tokens = 1024;
    req->thinking.level = IK_THINKING_NONE;

    // Add one simple message
    req->message_count = 1;
    req->messages = talloc_array(req, ik_message_t, 1);
    req->messages[0].role = IK_ROLE_USER;
    req->messages[0].content_count = 1;
    req->messages[0].content_blocks = talloc_array(req, ik_content_block_t, 1);
    req->messages[0].content_blocks[0].type = IK_CONTENT_TEXT;
    req->messages[0].content_blocks[0].data.text.text = talloc_strdup(req, "Hello");

    return req;
}

/* ================================================================
 * Message Content Tests
 * ================================================================ */

START_TEST(test_serialize_single_text_message)
{
    ik_request_t *req = create_basic_request(test_ctx);
    char *json = NULL;

    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);

    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *messages = yyjson_obj_get(root, "messages");

    ck_assert(yyjson_arr_size(messages) == 1);

    yyjson_val *msg = yyjson_arr_get(messages, 0);
    yyjson_val *role = yyjson_obj_get(msg, "role");
    yyjson_val *content = yyjson_obj_get(msg, "content");

    ck_assert_str_eq(yyjson_get_str(role), "user");
    // Single text block should be a string
    ck_assert(yyjson_is_str(content));
    ck_assert_str_eq(yyjson_get_str(content), "Hello");

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_multiple_content_blocks)
{
    ik_request_t *req = create_basic_request(test_ctx);

    // Add multiple content blocks
    req->messages[0].content_count = 2;
    req->messages[0].content_blocks = talloc_realloc(req, req->messages[0].content_blocks,
                                                      ik_content_block_t, 2);
    req->messages[0].content_blocks[1].type = IK_CONTENT_TEXT;
    req->messages[0].content_blocks[1].data.text.text = talloc_strdup(req, "World");

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);

    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *messages = yyjson_obj_get(root, "messages");
    yyjson_val *msg = yyjson_arr_get(messages, 0);
    yyjson_val *content = yyjson_obj_get(msg, "content");

    // Multiple blocks should be an array
    ck_assert(yyjson_is_arr(content));
    ck_assert(yyjson_arr_size(content) == 2);

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_thinking_content)
{
    ik_request_t *req = create_basic_request(test_ctx);
    req->messages[0].content_blocks[0].type = IK_CONTENT_THINKING;
    req->messages[0].content_blocks[0].data.thinking.text = talloc_strdup(req, "Let me think...");

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);

    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *messages = yyjson_obj_get(root, "messages");
    yyjson_val *msg = yyjson_arr_get(messages, 0);
    yyjson_val *content = yyjson_obj_get(msg, "content");

    // Single thinking block should still be array format
    ck_assert(yyjson_is_arr(content));
    yyjson_val *block = yyjson_arr_get(content, 0);
    yyjson_val *type = yyjson_obj_get(block, "type");
    yyjson_val *thinking = yyjson_obj_get(block, "thinking");

    ck_assert_str_eq(yyjson_get_str(type), "thinking");
    ck_assert_str_eq(yyjson_get_str(thinking), "Let me think...");

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_tool_call_content)
{
    ik_request_t *req = create_basic_request(test_ctx);
    req->messages[0].role = IK_ROLE_ASSISTANT;
    req->messages[0].content_blocks[0].type = IK_CONTENT_TOOL_CALL;
    req->messages[0].content_blocks[0].data.tool_call.id = talloc_strdup(req, "call_123");
    req->messages[0].content_blocks[0].data.tool_call.name = talloc_strdup(req, "get_weather");
    req->messages[0].content_blocks[0].data.tool_call.arguments = talloc_strdup(req, "{\"city\":\"SF\"}");

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);

    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *messages = yyjson_obj_get(root, "messages");
    yyjson_val *msg = yyjson_arr_get(messages, 0);
    yyjson_val *content = yyjson_obj_get(msg, "content");

    ck_assert(yyjson_is_arr(content));
    yyjson_val *block = yyjson_arr_get(content, 0);
    yyjson_val *type = yyjson_obj_get(block, "type");
    yyjson_val *id = yyjson_obj_get(block, "id");
    yyjson_val *name = yyjson_obj_get(block, "name");
    yyjson_val *input = yyjson_obj_get(block, "input");

    ck_assert_str_eq(yyjson_get_str(type), "tool_use");
    ck_assert_str_eq(yyjson_get_str(id), "call_123");
    ck_assert_str_eq(yyjson_get_str(name), "get_weather");
    ck_assert(yyjson_is_obj(input));

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_tool_result_content)
{
    ik_request_t *req = create_basic_request(test_ctx);
    req->messages[0].role = IK_ROLE_TOOL;
    req->messages[0].content_blocks[0].type = IK_CONTENT_TOOL_RESULT;
    req->messages[0].content_blocks[0].data.tool_result.tool_call_id = talloc_strdup(req, "call_123");
    req->messages[0].content_blocks[0].data.tool_result.content = talloc_strdup(req, "Sunny, 72F");
    req->messages[0].content_blocks[0].data.tool_result.is_error = false;

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);

    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *messages = yyjson_obj_get(root, "messages");
    yyjson_val *msg = yyjson_arr_get(messages, 0);
    yyjson_val *content = yyjson_obj_get(msg, "content");

    ck_assert(yyjson_is_arr(content));
    yyjson_val *block = yyjson_arr_get(content, 0);
    yyjson_val *type = yyjson_obj_get(block, "type");
    yyjson_val *tool_use_id = yyjson_obj_get(block, "tool_use_id");
    yyjson_val *result_content = yyjson_obj_get(block, "content");
    yyjson_val *is_error = yyjson_obj_get(block, "is_error");

    ck_assert_str_eq(yyjson_get_str(type), "tool_result");
    ck_assert_str_eq(yyjson_get_str(tool_use_id), "call_123");
    ck_assert_str_eq(yyjson_get_str(result_content), "Sunny, 72F");
    ck_assert(yyjson_get_bool(is_error) == false);

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_tool_result_error)
{
    ik_request_t *req = create_basic_request(test_ctx);
    req->messages[0].role = IK_ROLE_TOOL;
    req->messages[0].content_blocks[0].type = IK_CONTENT_TOOL_RESULT;
    req->messages[0].content_blocks[0].data.tool_result.tool_call_id = talloc_strdup(req, "call_123");
    req->messages[0].content_blocks[0].data.tool_result.content = talloc_strdup(req, "API error");
    req->messages[0].content_blocks[0].data.tool_result.is_error = true;

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);

    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *messages = yyjson_obj_get(root, "messages");
    yyjson_val *msg = yyjson_arr_get(messages, 0);
    yyjson_val *content = yyjson_obj_get(msg, "content");
    yyjson_val *block = yyjson_arr_get(content, 0);
    yyjson_val *is_error = yyjson_obj_get(block, "is_error");

    ck_assert(yyjson_get_bool(is_error) == true);

    yyjson_doc_free(doc);
}
END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *anthropic_request_suite_2(void)
{
    Suite *s = suite_create("Anthropic Request - Part 2");

    TCase *tc_content = tcase_create("Message Content");
    tcase_add_unchecked_fixture(tc_content, setup, teardown);
    tcase_add_test(tc_content, test_serialize_single_text_message);
    tcase_add_test(tc_content, test_serialize_multiple_content_blocks);
    tcase_add_test(tc_content, test_serialize_thinking_content);
    tcase_add_test(tc_content, test_serialize_tool_call_content);
    tcase_add_test(tc_content, test_serialize_tool_result_content);
    tcase_add_test(tc_content, test_serialize_tool_result_error);
    suite_add_tcase(s, tc_content);

    return s;
}

int main(void)
{
    Suite *s = anthropic_request_suite_2();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
