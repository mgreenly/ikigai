/**
 * @file request_responses_test.c
 * @brief Tests for OpenAI Responses API request serialization
 */

#include "../../../../src/providers/openai/request.h"
#include "../../../../src/providers/request.h"
#include "../../../../src/providers/provider.h"
#include "../../../../src/error.h"
#include "../../../../src/vendor/yyjson/yyjson.h"

#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

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
 * URL Building Tests
 * ================================================================ */

START_TEST(test_build_responses_url_success) {
    char *url = NULL;
    res_t result = ik_openai_build_responses_url(test_ctx, "https://api.openai.com", &url);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(url);
    ck_assert_str_eq(url, "https://api.openai.com/v1/responses");
}

END_TEST START_TEST(test_build_responses_url_custom_base)
{
    char *url = NULL;
    res_t result = ik_openai_build_responses_url(test_ctx, "https://custom.openai.azure.com", &url);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(url);
    ck_assert_str_eq(url, "https://custom.openai.azure.com/v1/responses");
}

END_TEST
/* ================================================================
 * Basic Request Serialization Tests
 * ================================================================ */

START_TEST(test_serialize_minimal_request)
{
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    res_t add_result = ik_request_add_message(req, IK_ROLE_USER, "Hello");
    ck_assert(!is_err(&add_result));

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(json);

    // Parse JSON to verify structure
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert_ptr_nonnull(root);

    // Check model
    yyjson_val *model = yyjson_obj_get(root, "model");
    ck_assert_ptr_nonnull(model);
    ck_assert_str_eq(yyjson_get_str(model), "o1");

    // Check input (should be string for single user message)
    yyjson_val *input = yyjson_obj_get(root, "input");
    ck_assert_ptr_nonnull(input);
    ck_assert(yyjson_is_str(input));
    ck_assert_str_eq(yyjson_get_str(input), "Hello");

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_serialize_request_with_system_prompt)
{
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1-mini", &req);
    ck_assert(!is_err(&create_result));

    res_t sys_result = ik_request_set_system(req, "You are a helpful assistant.");
    ck_assert(!is_err(&sys_result));

    res_t add_result = ik_request_add_message(req, IK_ROLE_USER, "What is 2+2?");
    ck_assert(!is_err(&add_result));

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(json);

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *instructions = yyjson_obj_get(root, "instructions");
    ck_assert_ptr_nonnull(instructions);
    ck_assert_str_eq(yyjson_get_str(instructions), "You are a helpful assistant.");

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_serialize_request_streaming)
{
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    res_t add_result = ik_request_add_message(req, IK_ROLE_USER, "Test");
    ck_assert(!is_err(&add_result));

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, true, &json);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(json);

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *stream = yyjson_obj_get(root, "stream");
    ck_assert_ptr_nonnull(stream);
    ck_assert(yyjson_get_bool(stream));

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_serialize_request_max_output_tokens)
{
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    req->max_output_tokens = 4096;

    res_t add_result = ik_request_add_message(req, IK_ROLE_USER, "Test");
    ck_assert(!is_err(&add_result));

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(json);

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *max_tokens = yyjson_obj_get(root, "max_output_tokens");
    ck_assert_ptr_nonnull(max_tokens);
    ck_assert_int_eq(yyjson_get_int(max_tokens), 4096);

    yyjson_doc_free(doc);
}

END_TEST
/* ================================================================
 * Multi-turn Conversation Tests
 * ================================================================ */

START_TEST(test_serialize_multi_turn_conversation)
{
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    ik_request_add_message(req, IK_ROLE_USER, "Hello");
    ik_request_add_message(req, IK_ROLE_ASSISTANT, "Hi there!");
    ik_request_add_message(req, IK_ROLE_USER, "How are you?");

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(json);

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *input = yyjson_obj_get(root, "input");
    ck_assert_ptr_nonnull(input);

    // Multi-turn should use array format
    ck_assert(yyjson_is_arr(input));
    ck_assert_int_eq((int)yyjson_arr_size(input), 3);

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_serialize_single_user_message_with_multiple_text_blocks)
{
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    // Create content blocks
    ik_content_block_t blocks[2];
    blocks[0].type = IK_CONTENT_TEXT;
    blocks[0].data.text.text = talloc_strdup(test_ctx, "First block");
    blocks[1].type = IK_CONTENT_TEXT;
    blocks[1].data.text.text = talloc_strdup(test_ctx, "Second block");

    ik_request_add_message_blocks(req, IK_ROLE_USER, blocks, 2);

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(json);

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *input = yyjson_obj_get(root, "input");
    ck_assert_ptr_nonnull(input);

    // Single user message should use string format with blocks concatenated
    ck_assert(yyjson_is_str(input));
    ck_assert_str_eq(yyjson_get_str(input), "First block\n\nSecond block");

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_serialize_single_user_message_no_text_blocks)
{
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    // Create content blocks with non-text type (tool call)
    ik_content_block_t blocks[1];
    blocks[0].type = IK_CONTENT_TOOL_CALL;
    blocks[0].data.tool_call.id = talloc_strdup(test_ctx, "call_123");
    blocks[0].data.tool_call.name = talloc_strdup(test_ctx, "test");
    blocks[0].data.tool_call.arguments = talloc_strdup(test_ctx, "{}");

    ik_request_add_message_blocks(req, IK_ROLE_USER, blocks, 1);

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(json);

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *input = yyjson_obj_get(root, "input");
    ck_assert_ptr_nonnull(input);

    // Single user message with no text content should still use string input (empty)
    ck_assert(yyjson_is_str(input));
    ck_assert_str_eq(yyjson_get_str(input), "");

    yyjson_doc_free(doc);
}

END_TEST
/* ================================================================
 * Reasoning Configuration Tests
 * ================================================================ */

START_TEST(test_serialize_reasoning_low)
{
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    ik_request_set_thinking(req, IK_THINKING_LOW, false);
    ik_request_add_message(req, IK_ROLE_USER, "Solve this problem");

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(json);

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *reasoning = yyjson_obj_get(root, "reasoning");
    ck_assert_ptr_nonnull(reasoning);

    yyjson_val *effort = yyjson_obj_get(reasoning, "effort");
    ck_assert_ptr_nonnull(effort);
    ck_assert_str_eq(yyjson_get_str(effort), "low");

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_serialize_reasoning_medium)
{
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1-mini", &req);
    ck_assert(!is_err(&create_result));

    ik_request_set_thinking(req, IK_THINKING_MED, false);
    ik_request_add_message(req, IK_ROLE_USER, "Complex task");

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *reasoning = yyjson_obj_get(root, "reasoning");
    ck_assert_ptr_nonnull(reasoning);

    yyjson_val *effort = yyjson_obj_get(reasoning, "effort");
    ck_assert_str_eq(yyjson_get_str(effort), "medium");

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_serialize_reasoning_high)
{
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o3-mini", &req);
    ck_assert(!is_err(&create_result));

    ik_request_set_thinking(req, IK_THINKING_HIGH, false);
    ik_request_add_message(req, IK_ROLE_USER, "Very hard problem");

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *reasoning = yyjson_obj_get(root, "reasoning");
    ck_assert_ptr_nonnull(reasoning);

    yyjson_val *effort = yyjson_obj_get(reasoning, "effort");
    ck_assert_str_eq(yyjson_get_str(effort), "high");

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_serialize_reasoning_none)
{
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    ik_request_set_thinking(req, IK_THINKING_NONE, false);
    ik_request_add_message(req, IK_ROLE_USER, "Test");

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    // No reasoning field should be present
    yyjson_val *reasoning = yyjson_obj_get(root, "reasoning");
    ck_assert_ptr_null(reasoning);

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_serialize_non_reasoning_model_with_thinking)
{
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "gpt-4o", &req);
    ck_assert(!is_err(&create_result));

    // Non-reasoning model, thinking level set (but should be ignored)
    ik_request_set_thinking(req, IK_THINKING_HIGH, false);
    ik_request_add_message(req, IK_ROLE_USER, "Test");

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    // No reasoning field for non-reasoning models
    yyjson_val *reasoning = yyjson_obj_get(root, "reasoning");
    ck_assert_ptr_null(reasoning);

    yyjson_doc_free(doc);
}

END_TEST
/* ================================================================
 * Tool Definition Tests
 * ================================================================ */

START_TEST(test_serialize_single_tool)
{
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    ik_request_add_message(req, IK_ROLE_USER, "Use a tool");

    const char *params = "{\"type\":\"object\",\"properties\":{\"x\":{\"type\":\"number\"}}}";
    ik_request_add_tool(req, "calculator", "Performs calculations", params, true);

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    yyjson_val *tools = yyjson_obj_get(root, "tools");
    ck_assert_ptr_nonnull(tools);
    ck_assert(yyjson_is_arr(tools));
    ck_assert_int_eq((int)yyjson_arr_size(tools), 1);

    yyjson_val *tool = yyjson_arr_get_first(tools);
    ck_assert_ptr_nonnull(tool);

    yyjson_val *type = yyjson_obj_get(tool, "type");
    ck_assert_str_eq(yyjson_get_str(type), "function");

    yyjson_val *func = yyjson_obj_get(tool, "function");
    ck_assert_ptr_nonnull(func);

    yyjson_val *name = yyjson_obj_get(func, "name");
    ck_assert_str_eq(yyjson_get_str(name), "calculator");

    yyjson_val *desc = yyjson_obj_get(func, "description");
    ck_assert_str_eq(yyjson_get_str(desc), "Performs calculations");

    yyjson_val *strict = yyjson_obj_get(func, "strict");
    ck_assert(yyjson_get_bool(strict));

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_serialize_multiple_tools)
{
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    ik_request_add_message(req, IK_ROLE_USER, "Use tools");

    const char *params = "{\"type\":\"object\"}";
    ik_request_add_tool(req, "tool1", "First tool", params, true);
    ik_request_add_tool(req, "tool2", "Second tool", params, false);

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    yyjson_val *tools = yyjson_obj_get(root, "tools");
    ck_assert_ptr_nonnull(tools);
    ck_assert_int_eq((int)yyjson_arr_size(tools), 2);

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_serialize_tool_choice_auto)
{
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    ik_request_add_message(req, IK_ROLE_USER, "Test");

    const char *params = "{\"type\":\"object\"}";
    ik_request_add_tool(req, "test_tool", "Test", params, true);

    req->tool_choice_mode = 0; // IK_TOOL_AUTO

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    yyjson_val *choice = yyjson_obj_get(root, "tool_choice");
    ck_assert_ptr_nonnull(choice);
    ck_assert_str_eq(yyjson_get_str(choice), "auto");

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_serialize_tool_choice_none)
{
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    ik_request_add_message(req, IK_ROLE_USER, "Test");

    const char *params = "{\"type\":\"object\"}";
    ik_request_add_tool(req, "test_tool", "Test", params, true);

    req->tool_choice_mode = 1; // IK_TOOL_NONE

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    yyjson_val *choice = yyjson_obj_get(root, "tool_choice");
    ck_assert_ptr_nonnull(choice);
    ck_assert_str_eq(yyjson_get_str(choice), "none");

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_serialize_tool_choice_required)
{
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    ik_request_add_message(req, IK_ROLE_USER, "Test");

    const char *params = "{\"type\":\"object\"}";
    ik_request_add_tool(req, "test_tool", "Test", params, true);

    req->tool_choice_mode = 2; // IK_TOOL_REQUIRED

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    yyjson_val *choice = yyjson_obj_get(root, "tool_choice");
    ck_assert_ptr_nonnull(choice);
    ck_assert_str_eq(yyjson_get_str(choice), "required");

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_serialize_tool_choice_unknown)
{
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    ik_request_add_message(req, IK_ROLE_USER, "Test");

    const char *params = "{\"type\":\"object\"}";
    ik_request_add_tool(req, "test_tool", "Test", params, true);

    req->tool_choice_mode = 999; // Unknown mode

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    // Unknown mode defaults to "auto"
    yyjson_val *choice = yyjson_obj_get(root, "tool_choice");
    ck_assert_ptr_nonnull(choice);
    ck_assert_str_eq(yyjson_get_str(choice), "auto");

    yyjson_doc_free(doc);
}

END_TEST
/* ================================================================
 * Error Handling Tests
 * ================================================================ */

START_TEST(test_serialize_null_model)
{
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    req->model = NULL; // Invalid

    ik_request_add_message(req, IK_ROLE_USER, "Test");

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(is_err(&result));
}

END_TEST START_TEST(test_serialize_invalid_tool_params)
{
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    ik_request_add_message(req, IK_ROLE_USER, "Test");

    // Invalid JSON in parameters
    const char *bad_params = "{invalid json}";
    ik_request_add_tool(req, "bad_tool", "Bad", bad_params, true);

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(is_err(&result));
}

END_TEST
/* ================================================================
 * Edge Cases
 * ================================================================ */

START_TEST(test_serialize_empty_system_prompt)
{
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    ik_request_set_system(req, "");
    ik_request_add_message(req, IK_ROLE_USER, "Test");

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    // Empty system prompt should not be included
    yyjson_val *instructions = yyjson_obj_get(root, "instructions");
    ck_assert_ptr_null(instructions);

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_serialize_max_output_tokens_zero)
{
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    req->max_output_tokens = 0; // Not set
    ik_request_add_message(req, IK_ROLE_USER, "Test");

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    // max_output_tokens should not be present
    yyjson_val *max_tokens = yyjson_obj_get(root, "max_output_tokens");
    ck_assert_ptr_null(max_tokens);

    yyjson_doc_free(doc);
}

END_TEST START_TEST(test_serialize_no_streaming)
{
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    ik_request_add_message(req, IK_ROLE_USER, "Test");

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    // stream field should not be present when streaming is false
    yyjson_val *stream = yyjson_obj_get(root, "stream");
    ck_assert_ptr_null(stream);

    yyjson_doc_free(doc);
}

END_TEST

/* ================================================================
 * Test Suite
 * ================================================================ */

static Suite *request_responses_suite(void)
{
    Suite *s = suite_create("OpenAI Responses API Request Serialization");

    TCase *tc_url = tcase_create("URL Building");
    tcase_add_checked_fixture(tc_url, setup, teardown);
    tcase_add_test(tc_url, test_build_responses_url_success);
    tcase_add_test(tc_url, test_build_responses_url_custom_base);
    suite_add_tcase(s, tc_url);

    TCase *tc_basic = tcase_create("Basic Serialization");
    tcase_add_checked_fixture(tc_basic, setup, teardown);
    tcase_add_test(tc_basic, test_serialize_minimal_request);
    tcase_add_test(tc_basic, test_serialize_request_with_system_prompt);
    tcase_add_test(tc_basic, test_serialize_request_streaming);
    tcase_add_test(tc_basic, test_serialize_request_max_output_tokens);
    suite_add_tcase(s, tc_basic);

    TCase *tc_messages = tcase_create("Message Handling");
    tcase_add_checked_fixture(tc_messages, setup, teardown);
    tcase_add_test(tc_messages, test_serialize_multi_turn_conversation);
    tcase_add_test(tc_messages, test_serialize_single_user_message_with_multiple_text_blocks);
    tcase_add_test(tc_messages, test_serialize_single_user_message_no_text_blocks);
    suite_add_tcase(s, tc_messages);

    TCase *tc_reasoning = tcase_create("Reasoning Configuration");
    tcase_add_checked_fixture(tc_reasoning, setup, teardown);
    tcase_add_test(tc_reasoning, test_serialize_reasoning_low);
    tcase_add_test(tc_reasoning, test_serialize_reasoning_medium);
    tcase_add_test(tc_reasoning, test_serialize_reasoning_high);
    tcase_add_test(tc_reasoning, test_serialize_reasoning_none);
    tcase_add_test(tc_reasoning, test_serialize_non_reasoning_model_with_thinking);
    suite_add_tcase(s, tc_reasoning);

    TCase *tc_tools = tcase_create("Tool Definitions");
    tcase_add_checked_fixture(tc_tools, setup, teardown);
    tcase_add_test(tc_tools, test_serialize_single_tool);
    tcase_add_test(tc_tools, test_serialize_multiple_tools);
    tcase_add_test(tc_tools, test_serialize_tool_choice_auto);
    tcase_add_test(tc_tools, test_serialize_tool_choice_none);
    tcase_add_test(tc_tools, test_serialize_tool_choice_required);
    tcase_add_test(tc_tools, test_serialize_tool_choice_unknown);
    suite_add_tcase(s, tc_tools);

    TCase *tc_errors = tcase_create("Error Handling");
    tcase_add_checked_fixture(tc_errors, setup, teardown);
    tcase_add_test(tc_errors, test_serialize_null_model);
    tcase_add_test(tc_errors, test_serialize_invalid_tool_params);
    suite_add_tcase(s, tc_errors);

    TCase *tc_edge = tcase_create("Edge Cases");
    tcase_add_checked_fixture(tc_edge, setup, teardown);
    tcase_add_test(tc_edge, test_serialize_empty_system_prompt);
    tcase_add_test(tc_edge, test_serialize_max_output_tokens_zero);
    tcase_add_test(tc_edge, test_serialize_no_streaming);
    suite_add_tcase(s, tc_edge);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = request_responses_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
