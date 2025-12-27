/**
 * @file anthropic_request_test.c
 * @brief Unit tests for Anthropic request serialization
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
 * Basic Request Serialization Tests
 * ================================================================ */

START_TEST(test_serialize_request_basic)
{
    ik_request_t *req = create_basic_request(test_ctx);
    char *json = NULL;

    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(json);

    // Parse and validate JSON structure
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert_ptr_nonnull(root);

    // Check model
    yyjson_val *model = yyjson_obj_get(root, "model");
    ck_assert_ptr_nonnull(model);
    ck_assert_str_eq(yyjson_get_str(model), "claude-3-5-sonnet-20241022");

    // Check max_tokens
    yyjson_val *max_tokens = yyjson_obj_get(root, "max_tokens");
    ck_assert_ptr_nonnull(max_tokens);
    ck_assert_int_eq(yyjson_get_int(max_tokens), 1024);

    // Check messages
    yyjson_val *messages = yyjson_obj_get(root, "messages");
    ck_assert_ptr_nonnull(messages);
    ck_assert(yyjson_is_arr(messages));

    // Check stream is not present for non-streaming request
    yyjson_val *stream = yyjson_obj_get(root, "stream");
    ck_assert_ptr_null(stream);

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_request_stream)
{
    ik_request_t *req = create_basic_request(test_ctx);
    char *json = NULL;

    res_t r = ik_anthropic_serialize_request_stream(test_ctx, req, &json);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(json);

    // Parse and validate JSON structure
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert_ptr_nonnull(root);

    // Check stream is true
    yyjson_val *stream = yyjson_obj_get(root, "stream");
    ck_assert_ptr_nonnull(stream);
    ck_assert(yyjson_get_bool(stream) == true);

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_request_null_model)
{
    ik_request_t *req = create_basic_request(test_ctx);
    req->model = NULL;
    char *json = NULL;

    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);

    ck_assert(is_err(&r));
    ck_assert_int_eq(r.err->code, ERR_INVALID_ARG);
}
END_TEST

START_TEST(test_serialize_request_default_max_tokens)
{
    ik_request_t *req = create_basic_request(test_ctx);
    req->max_output_tokens = 0;
    char *json = NULL;

    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);

    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *max_tokens = yyjson_obj_get(root, "max_tokens");

    // Should default to 4096
    ck_assert_int_eq(yyjson_get_int(max_tokens), 4096);

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_request_negative_max_tokens)
{
    ik_request_t *req = create_basic_request(test_ctx);
    req->max_output_tokens = -1;
    char *json = NULL;

    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);

    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *max_tokens = yyjson_obj_get(root, "max_tokens");

    // Should default to 4096
    ck_assert_int_eq(yyjson_get_int(max_tokens), 4096);

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_request_with_system_prompt)
{
    ik_request_t *req = create_basic_request(test_ctx);
    req->system_prompt = talloc_strdup(req, "You are a helpful assistant.");
    char *json = NULL;

    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);

    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *system = yyjson_obj_get(root, "system");

    ck_assert_ptr_nonnull(system);
    ck_assert_str_eq(yyjson_get_str(system), "You are a helpful assistant.");

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_serialize_request_without_system_prompt)
{
    ik_request_t *req = create_basic_request(test_ctx);
    req->system_prompt = NULL;
    char *json = NULL;

    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);

    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *system = yyjson_obj_get(root, "system");

    // System should not be present
    ck_assert_ptr_null(system);

    yyjson_doc_free(doc);
}
END_TEST

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
 * Role Mapping Tests
 * ================================================================ */

START_TEST(test_role_user)
{
    ik_request_t *req = create_basic_request(test_ctx);
    req->messages[0].role = IK_ROLE_USER;
    char *json = NULL;

    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *messages = yyjson_obj_get(root, "messages");
    yyjson_val *msg = yyjson_arr_get(messages, 0);
    yyjson_val *role = yyjson_obj_get(msg, "role");

    ck_assert_str_eq(yyjson_get_str(role), "user");

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_role_assistant)
{
    ik_request_t *req = create_basic_request(test_ctx);
    req->messages[0].role = IK_ROLE_ASSISTANT;
    char *json = NULL;

    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *messages = yyjson_obj_get(root, "messages");
    yyjson_val *msg = yyjson_arr_get(messages, 0);
    yyjson_val *role = yyjson_obj_get(msg, "role");

    ck_assert_str_eq(yyjson_get_str(role), "assistant");

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_role_tool_mapped_to_user)
{
    ik_request_t *req = create_basic_request(test_ctx);
    req->messages[0].role = IK_ROLE_TOOL;
    char *json = NULL;

    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *messages = yyjson_obj_get(root, "messages");
    yyjson_val *msg = yyjson_arr_get(messages, 0);
    yyjson_val *role = yyjson_obj_get(msg, "role");

    // IK_ROLE_TOOL maps to "user" in Anthropic
    ck_assert_str_eq(yyjson_get_str(role), "user");

    yyjson_doc_free(doc);
}
END_TEST

/* ================================================================
 * Thinking Configuration Tests
 * ================================================================ */

START_TEST(test_thinking_none)
{
    ik_request_t *req = create_basic_request(test_ctx);
    req->thinking.level = IK_THINKING_NONE;
    char *json = NULL;

    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *thinking = yyjson_obj_get(root, "thinking");

    // Should not have thinking field
    ck_assert_ptr_null(thinking);

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_thinking_low)
{
    ik_request_t *req = create_basic_request(test_ctx);
    req->model = talloc_strdup(req, "claude-sonnet-4-5-20250929");
    req->thinking.level = IK_THINKING_LOW;
    req->max_output_tokens = 32768;
    char *json = NULL;

    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *thinking = yyjson_obj_get(root, "thinking");

    ck_assert_ptr_nonnull(thinking);
    yyjson_val *type = yyjson_obj_get(thinking, "type");
    yyjson_val *budget = yyjson_obj_get(thinking, "budget_tokens");

    ck_assert_str_eq(yyjson_get_str(type), "enabled");
    // min=1024, max=64000, range=62976, LOW = 1024 + 62976/3 = 22016
    ck_assert_int_eq(yyjson_get_int(budget), 22016);

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_thinking_med)
{
    ik_request_t *req = create_basic_request(test_ctx);
    req->model = talloc_strdup(req, "claude-sonnet-4-5-20250929");
    req->thinking.level = IK_THINKING_MED;
    req->max_output_tokens = 65536;
    char *json = NULL;

    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *thinking = yyjson_obj_get(root, "thinking");

    ck_assert_ptr_nonnull(thinking);
    yyjson_val *budget = yyjson_obj_get(thinking, "budget_tokens");
    // min=1024, max=64000, range=62976, MED = 1024 + 2*62976/3 = 43008
    ck_assert_int_eq(yyjson_get_int(budget), 43008);

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_thinking_high)
{
    ik_request_t *req = create_basic_request(test_ctx);
    req->model = talloc_strdup(req, "claude-sonnet-4-5-20250929");
    req->thinking.level = IK_THINKING_HIGH;
    req->max_output_tokens = 128000;
    char *json = NULL;

    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *thinking = yyjson_obj_get(root, "thinking");

    ck_assert_ptr_nonnull(thinking);
    yyjson_val *budget = yyjson_obj_get(thinking, "budget_tokens");
    ck_assert_int_eq(yyjson_get_int(budget), 64000);

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_thinking_adjusts_max_tokens)
{
    ik_request_t *req = create_basic_request(test_ctx);
    req->model = talloc_strdup(req, "claude-sonnet-4-5-20250929");
    req->thinking.level = IK_THINKING_LOW;
    req->max_output_tokens = 512; // Less than thinking budget
    char *json = NULL;

    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *max_tokens = yyjson_obj_get(root, "max_tokens");

    // Should be adjusted to budget + 4096 = 22016 + 4096 = 26112
    ck_assert_int_eq(yyjson_get_int(max_tokens), 22016 + 4096);

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_thinking_unsupported_model)
{
    ik_request_t *req = create_basic_request(test_ctx);
    req->model = talloc_strdup(req, "gpt-4");
    req->thinking.level = IK_THINKING_LOW;
    char *json = NULL;

    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *thinking = yyjson_obj_get(root, "thinking");

    // Should skip thinking for non-Claude model
    ck_assert_ptr_null(thinking);

    yyjson_doc_free(doc);
}
END_TEST

/* ================================================================
 * Tool Definition Tests
 * ================================================================ */

START_TEST(test_tools_none)
{
    ik_request_t *req = create_basic_request(test_ctx);
    req->tool_count = 0;
    req->tools = NULL;
    char *json = NULL;

    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *tools = yyjson_obj_get(root, "tools");

    // Should not have tools field
    ck_assert_ptr_null(tools);

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_tools_single)
{
    ik_request_t *req = create_basic_request(test_ctx);
    req->tool_count = 1;
    req->tools = talloc_array(req, ik_tool_def_t, 1);
    req->tools[0].name = talloc_strdup(req, "get_weather");
    req->tools[0].description = talloc_strdup(req, "Get weather for a city");
    req->tools[0].parameters = talloc_strdup(req, "{\"type\":\"object\",\"properties\":{}}");

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *tools = yyjson_obj_get(root, "tools");

    ck_assert_ptr_nonnull(tools);
    ck_assert(yyjson_is_arr(tools));
    ck_assert(yyjson_arr_size(tools) == 1);

    yyjson_val *tool = yyjson_arr_get(tools, 0);
    yyjson_val *name = yyjson_obj_get(tool, "name");
    yyjson_val *description = yyjson_obj_get(tool, "description");
    yyjson_val *input_schema = yyjson_obj_get(tool, "input_schema");

    ck_assert_str_eq(yyjson_get_str(name), "get_weather");
    ck_assert_str_eq(yyjson_get_str(description), "Get weather for a city");
    ck_assert(yyjson_is_obj(input_schema));

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_tools_multiple)
{
    ik_request_t *req = create_basic_request(test_ctx);
    req->tool_count = 2;
    req->tools = talloc_array(req, ik_tool_def_t, 2);
    req->tools[0].name = talloc_strdup(req, "get_weather");
    req->tools[0].description = talloc_strdup(req, "Get weather");
    req->tools[0].parameters = talloc_strdup(req, "{\"type\":\"object\"}");
    req->tools[1].name = talloc_strdup(req, "get_time");
    req->tools[1].description = talloc_strdup(req, "Get time");
    req->tools[1].parameters = talloc_strdup(req, "{\"type\":\"object\"}");

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *tools = yyjson_obj_get(root, "tools");

    ck_assert(yyjson_arr_size(tools) == 2);

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_tool_choice_auto)
{
    ik_request_t *req = create_basic_request(test_ctx);
    req->tool_count = 1;
    req->tools = talloc_array(req, ik_tool_def_t, 1);
    req->tools[0].name = talloc_strdup(req, "test");
    req->tools[0].description = talloc_strdup(req, "test");
    req->tools[0].parameters = talloc_strdup(req, "{}");
    req->tool_choice_mode = 0; // IK_TOOL_AUTO

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *tool_choice = yyjson_obj_get(root, "tool_choice");

    ck_assert_ptr_nonnull(tool_choice);
    yyjson_val *type = yyjson_obj_get(tool_choice, "type");
    ck_assert_str_eq(yyjson_get_str(type), "auto");

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_tool_choice_none)
{
    ik_request_t *req = create_basic_request(test_ctx);
    req->tool_count = 1;
    req->tools = talloc_array(req, ik_tool_def_t, 1);
    req->tools[0].name = talloc_strdup(req, "test");
    req->tools[0].description = talloc_strdup(req, "test");
    req->tools[0].parameters = talloc_strdup(req, "{}");
    req->tool_choice_mode = 1; // IK_TOOL_NONE

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *tool_choice = yyjson_obj_get(root, "tool_choice");

    ck_assert_ptr_nonnull(tool_choice);
    yyjson_val *type = yyjson_obj_get(tool_choice, "type");
    ck_assert_str_eq(yyjson_get_str(type), "none");

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_tool_choice_required)
{
    ik_request_t *req = create_basic_request(test_ctx);
    req->tool_count = 1;
    req->tools = talloc_array(req, ik_tool_def_t, 1);
    req->tools[0].name = talloc_strdup(req, "test");
    req->tools[0].description = talloc_strdup(req, "test");
    req->tools[0].parameters = talloc_strdup(req, "{}");
    req->tool_choice_mode = 2; // IK_TOOL_REQUIRED

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *tool_choice = yyjson_obj_get(root, "tool_choice");

    ck_assert_ptr_nonnull(tool_choice);
    yyjson_val *type = yyjson_obj_get(tool_choice, "type");
    ck_assert_str_eq(yyjson_get_str(type), "any");

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_tool_choice_default)
{
    ik_request_t *req = create_basic_request(test_ctx);
    req->tool_count = 1;
    req->tools = talloc_array(req, ik_tool_def_t, 1);
    req->tools[0].name = talloc_strdup(req, "test");
    req->tools[0].description = talloc_strdup(req, "test");
    req->tools[0].parameters = talloc_strdup(req, "{}");
    req->tool_choice_mode = 99; // Unknown mode

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);
    ck_assert(!is_err(&r));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *tool_choice = yyjson_obj_get(root, "tool_choice");

    ck_assert_ptr_nonnull(tool_choice);
    yyjson_val *type = yyjson_obj_get(tool_choice, "type");
    // Should default to auto
    ck_assert_str_eq(yyjson_get_str(type), "auto");

    yyjson_doc_free(doc);
}
END_TEST

/* ================================================================
 * Header Building Tests
 * ================================================================ */

START_TEST(test_build_headers)
{
    char **headers = NULL;
    res_t r = ik_anthropic_build_headers(test_ctx, "test-api-key", &headers);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(headers);

    // Check all 3 headers
    ck_assert_str_eq(headers[0], "x-api-key: test-api-key");
    ck_assert_str_eq(headers[1], "anthropic-version: 2023-06-01");
    ck_assert_str_eq(headers[2], "content-type: application/json");
    ck_assert_ptr_null(headers[3]);
}
END_TEST

START_TEST(test_build_headers_different_key)
{
    char **headers = NULL;
    res_t r = ik_anthropic_build_headers(test_ctx, "another-key", &headers);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(headers);
    ck_assert_str_eq(headers[0], "x-api-key: another-key");
}
END_TEST

/* ================================================================
 * Error Case Tests
 * ================================================================ */

START_TEST(test_serialize_invalid_tool_call_json)
{
    ik_request_t *req = create_basic_request(test_ctx);
    req->messages[0].role = IK_ROLE_ASSISTANT;
    req->messages[0].content_blocks[0].type = IK_CONTENT_TOOL_CALL;
    req->messages[0].content_blocks[0].data.tool_call.id = talloc_strdup(req, "call_123");
    req->messages[0].content_blocks[0].data.tool_call.name = talloc_strdup(req, "get_weather");
    // Invalid JSON - should cause serialization to fail
    req->messages[0].content_blocks[0].data.tool_call.arguments = talloc_strdup(req, "not valid json");

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);

    ck_assert(is_err(&r));
    ck_assert_int_eq(r.err->code, ERR_PARSE);
}
END_TEST

START_TEST(test_serialize_invalid_tool_params_json)
{
    ik_request_t *req = create_basic_request(test_ctx);
    req->tool_count = 1;
    req->tools = talloc_array(req, ik_tool_def_t, 1);
    req->tools[0].name = talloc_strdup(req, "get_weather");
    req->tools[0].description = talloc_strdup(req, "Get weather");
    // Invalid JSON - should cause serialization to fail
    req->tools[0].parameters = talloc_strdup(req, "invalid json");

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);

    ck_assert(is_err(&r));
    ck_assert_int_eq(r.err->code, ERR_PARSE);
}
END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *anthropic_request_suite(void)
{
    Suite *s = suite_create("Anthropic Request");

    TCase *tc_basic = tcase_create("Basic Serialization");
    tcase_add_unchecked_fixture(tc_basic, setup, teardown);
    tcase_add_test(tc_basic, test_serialize_request_basic);
    tcase_add_test(tc_basic, test_serialize_request_stream);
    tcase_add_test(tc_basic, test_serialize_request_null_model);
    tcase_add_test(tc_basic, test_serialize_request_default_max_tokens);
    tcase_add_test(tc_basic, test_serialize_request_negative_max_tokens);
    tcase_add_test(tc_basic, test_serialize_request_with_system_prompt);
    tcase_add_test(tc_basic, test_serialize_request_without_system_prompt);
    suite_add_tcase(s, tc_basic);

    TCase *tc_content = tcase_create("Message Content");
    tcase_add_unchecked_fixture(tc_content, setup, teardown);
    tcase_add_test(tc_content, test_serialize_single_text_message);
    tcase_add_test(tc_content, test_serialize_multiple_content_blocks);
    tcase_add_test(tc_content, test_serialize_thinking_content);
    tcase_add_test(tc_content, test_serialize_tool_call_content);
    tcase_add_test(tc_content, test_serialize_tool_result_content);
    tcase_add_test(tc_content, test_serialize_tool_result_error);
    suite_add_tcase(s, tc_content);

    TCase *tc_role = tcase_create("Role Mapping");
    tcase_add_unchecked_fixture(tc_role, setup, teardown);
    tcase_add_test(tc_role, test_role_user);
    tcase_add_test(tc_role, test_role_assistant);
    tcase_add_test(tc_role, test_role_tool_mapped_to_user);
    suite_add_tcase(s, tc_role);

    TCase *tc_thinking = tcase_create("Thinking Configuration");
    tcase_add_unchecked_fixture(tc_thinking, setup, teardown);
    tcase_add_test(tc_thinking, test_thinking_none);
    tcase_add_test(tc_thinking, test_thinking_low);
    tcase_add_test(tc_thinking, test_thinking_med);
    tcase_add_test(tc_thinking, test_thinking_high);
    tcase_add_test(tc_thinking, test_thinking_adjusts_max_tokens);
    tcase_add_test(tc_thinking, test_thinking_unsupported_model);
    suite_add_tcase(s, tc_thinking);

    TCase *tc_tools = tcase_create("Tool Definitions");
    tcase_add_unchecked_fixture(tc_tools, setup, teardown);
    tcase_add_test(tc_tools, test_tools_none);
    tcase_add_test(tc_tools, test_tools_single);
    tcase_add_test(tc_tools, test_tools_multiple);
    tcase_add_test(tc_tools, test_tool_choice_auto);
    tcase_add_test(tc_tools, test_tool_choice_none);
    tcase_add_test(tc_tools, test_tool_choice_required);
    tcase_add_test(tc_tools, test_tool_choice_default);
    suite_add_tcase(s, tc_tools);

    TCase *tc_headers = tcase_create("Header Building");
    tcase_add_unchecked_fixture(tc_headers, setup, teardown);
    tcase_add_test(tc_headers, test_build_headers);
    tcase_add_test(tc_headers, test_build_headers_different_key);
    suite_add_tcase(s, tc_headers);

    TCase *tc_errors = tcase_create("Error Cases");
    tcase_add_unchecked_fixture(tc_errors, setup, teardown);
    tcase_add_test(tc_errors, test_serialize_invalid_tool_call_json);
    tcase_add_test(tc_errors, test_serialize_invalid_tool_params_json);
    suite_add_tcase(s, tc_errors);

    return s;
}

int main(void)
{
    Suite *s = anthropic_request_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
