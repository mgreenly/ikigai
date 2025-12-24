/**
 * @file equivalence_fixtures.c
 * @brief Test fixtures for OpenAI equivalence validation
 */

#include "equivalence_fixtures.h"
#include "providers/provider.h"
#include "panic.h"
#include <string.h>
#include <assert.h>

ik_request_t *ik_test_fixture_simple_text(TALLOC_CTX *ctx)
{
    assert(ctx != NULL);  // LCOV_EXCL_BR_LINE

    ik_request_t *req = talloc_zero(ctx, ik_request_t);
    if (req == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Model
    req->model = talloc_strdup(req, "gpt-4o-mini");
    if (req->model == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // System prompt
    req->system_prompt = talloc_strdup(req, "You are a helpful math assistant.");
    if (req->system_prompt == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Single user message
    req->message_count = 1;
    req->messages = talloc_array(req, ik_message_t, 1);
    if (req->messages == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    ik_message_t *msg = &req->messages[0];
    msg->role = IK_ROLE_USER;
    msg->content_count = 1;
    msg->content_blocks = talloc_array(req->messages, ik_content_block_t, 1);
    if (msg->content_blocks == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    msg->provider_metadata = NULL;

    ik_content_block_t *block = &msg->content_blocks[0];
    block->type = IK_CONTENT_TEXT;
    block->data.text.text = talloc_strdup(msg->content_blocks, "What is 2+2?");
    if (block->data.text.text == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // No tools
    req->tool_count = 0;
    req->tools = NULL;

    // Default thinking config
    req->thinking.level = IK_THINKING_NONE;
    req->thinking.include_summary = false;

    // Max tokens
    req->max_output_tokens = 1000;

    // Tool choice
    req->tool_choice_mode = 0;  // IK_TOOL_AUTO (temporarily int during coexistence)
    req->tool_choice_name = NULL;

    return req;
}

ik_request_t *ik_test_fixture_tool_call(TALLOC_CTX *ctx)
{
    assert(ctx != NULL);  // LCOV_EXCL_BR_LINE

    ik_request_t *req = talloc_zero(ctx, ik_request_t);
    if (req == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Model
    req->model = talloc_strdup(req, "gpt-4o-mini");
    if (req->model == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // System prompt
    req->system_prompt = talloc_strdup(req, "You have access to tools.");
    if (req->system_prompt == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Single user message requesting tool use
    req->message_count = 1;
    req->messages = talloc_array(req, ik_message_t, 1);
    if (req->messages == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    ik_message_t *msg = &req->messages[0];
    msg->role = IK_ROLE_USER;
    msg->content_count = 1;
    msg->content_blocks = talloc_array(req->messages, ik_content_block_t, 1);
    if (msg->content_blocks == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    msg->provider_metadata = NULL;

    ik_content_block_t *block = &msg->content_blocks[0];
    block->type = IK_CONTENT_TEXT;
    block->data.text.text = talloc_strdup(msg->content_blocks, "What is the weather in San Francisco?");
    if (block->data.text.text == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Define get_weather tool
    req->tool_count = 1;
    req->tools = talloc_array(req, ik_tool_def_t, 1);
    if (req->tools == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    ik_tool_def_t *tool = &req->tools[0];
    tool->name = talloc_strdup(req->tools, "get_weather");
    if (tool->name == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    tool->description = talloc_strdup(req->tools, "Get the current weather in a given location");
    if (tool->description == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    tool->parameters = talloc_strdup(req->tools,
        "{\"type\":\"object\",\"properties\":{\"location\":{\"type\":\"string\",\"description\":\"City name\"}},\"required\":[\"location\"]}");
    if (tool->parameters == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    tool->strict = false;

    // Default thinking config
    req->thinking.level = IK_THINKING_NONE;
    req->thinking.include_summary = false;

    // Max tokens
    req->max_output_tokens = 1000;

    // Tool choice - auto
    req->tool_choice_mode = 0;  // IK_TOOL_AUTO
    req->tool_choice_name = NULL;

    return req;
}

ik_request_t *ik_test_fixture_multi_turn(TALLOC_CTX *ctx)
{
    assert(ctx != NULL);  // LCOV_EXCL_BR_LINE

    ik_request_t *req = talloc_zero(ctx, ik_request_t);
    if (req == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Model
    req->model = talloc_strdup(req, "gpt-4o-mini");
    if (req->model == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // System prompt
    req->system_prompt = talloc_strdup(req, "You are a helpful assistant.");
    if (req->system_prompt == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Multi-turn: user -> assistant -> user
    req->message_count = 3;
    req->messages = talloc_array(req, ik_message_t, 3);
    if (req->messages == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Message 1: User
    ik_message_t *msg1 = &req->messages[0];
    msg1->role = IK_ROLE_USER;
    msg1->content_count = 1;
    msg1->content_blocks = talloc_array(req->messages, ik_content_block_t, 1);
    if (msg1->content_blocks == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    msg1->provider_metadata = NULL;

    ik_content_block_t *block1 = &msg1->content_blocks[0];
    block1->type = IK_CONTENT_TEXT;
    block1->data.text.text = talloc_strdup(msg1->content_blocks, "Hello!");
    if (block1->data.text.text == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Message 2: Assistant
    ik_message_t *msg2 = &req->messages[1];
    msg2->role = IK_ROLE_ASSISTANT;
    msg2->content_count = 1;
    msg2->content_blocks = talloc_array(req->messages, ik_content_block_t, 1);
    if (msg2->content_blocks == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    msg2->provider_metadata = NULL;

    ik_content_block_t *block2 = &msg2->content_blocks[0];
    block2->type = IK_CONTENT_TEXT;
    block2->data.text.text = talloc_strdup(msg2->content_blocks, "Hello! How can I help you today?");
    if (block2->data.text.text == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Message 3: User
    ik_message_t *msg3 = &req->messages[2];
    msg3->role = IK_ROLE_USER;
    msg3->content_count = 1;
    msg3->content_blocks = talloc_array(req->messages, ik_content_block_t, 1);
    if (msg3->content_blocks == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    msg3->provider_metadata = NULL;

    ik_content_block_t *block3 = &msg3->content_blocks[0];
    block3->type = IK_CONTENT_TEXT;
    block3->data.text.text = talloc_strdup(msg3->content_blocks, "Tell me a joke.");
    if (block3->data.text.text == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // No tools
    req->tool_count = 0;
    req->tools = NULL;

    // Default thinking config
    req->thinking.level = IK_THINKING_NONE;
    req->thinking.include_summary = false;

    // Max tokens
    req->max_output_tokens = 1000;

    // Tool choice
    req->tool_choice_mode = 0;  // IK_TOOL_AUTO
    req->tool_choice_name = NULL;

    return req;
}

ik_request_t *ik_test_fixture_invalid_model(TALLOC_CTX *ctx)
{
    assert(ctx != NULL);  // LCOV_EXCL_BR_LINE

    ik_request_t *req = talloc_zero(ctx, ik_request_t);
    if (req == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Invalid model name
    req->model = talloc_strdup(req, "gpt-nonexistent-model-99");
    if (req->model == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // System prompt
    req->system_prompt = talloc_strdup(req, "Test system prompt.");
    if (req->system_prompt == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Single user message
    req->message_count = 1;
    req->messages = talloc_array(req, ik_message_t, 1);
    if (req->messages == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    ik_message_t *msg = &req->messages[0];
    msg->role = IK_ROLE_USER;
    msg->content_count = 1;
    msg->content_blocks = talloc_array(req->messages, ik_content_block_t, 1);
    if (msg->content_blocks == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    msg->provider_metadata = NULL;

    ik_content_block_t *block = &msg->content_blocks[0];
    block->type = IK_CONTENT_TEXT;
    block->data.text.text = talloc_strdup(msg->content_blocks, "Test message.");
    if (block->data.text.text == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // No tools
    req->tool_count = 0;
    req->tools = NULL;

    // Default thinking config
    req->thinking.level = IK_THINKING_NONE;
    req->thinking.include_summary = false;

    // Max tokens
    req->max_output_tokens = 1000;

    // Tool choice
    req->tool_choice_mode = 0;  // IK_TOOL_AUTO
    req->tool_choice_name = NULL;

    return req;
}
