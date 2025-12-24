#include "providers/request.h"
#include "agent.h"
#include "error.h"
#include "wrapper.h"
#include <assert.h>
#include <string.h>
#include <talloc.h>

/**
 * Request Builder Implementation
 *
 * This module provides builder functions for constructing ik_request_t
 * structures with system prompts, messages, content blocks, tools, and
 * thinking configuration.
 */

/* ================================================================
 * Content Block Builders
 * ================================================================ */

ik_content_block_t *ik_content_block_text(TALLOC_CTX *ctx, const char *text) {
    assert(text != NULL); // LCOV_EXCL_BR_LINE

    ik_content_block_t *block = talloc_zero(ctx, ik_content_block_t);
    if (!block) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    block->type = IK_CONTENT_TEXT;
    block->data.text.text = talloc_strdup(block, text);
    if (!block->data.text.text) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    return block;
}

ik_content_block_t *ik_content_block_tool_call(TALLOC_CTX *ctx, const char *id,
                                                const char *name, const char *arguments) {
    assert(id != NULL);        // LCOV_EXCL_BR_LINE
    assert(name != NULL);      // LCOV_EXCL_BR_LINE
    assert(arguments != NULL); // LCOV_EXCL_BR_LINE

    ik_content_block_t *block = talloc_zero(ctx, ik_content_block_t);
    if (!block) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    block->type = IK_CONTENT_TOOL_CALL;
    block->data.tool_call.id = talloc_strdup(block, id);
    if (!block->data.tool_call.id) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    block->data.tool_call.name = talloc_strdup(block, name);
    if (!block->data.tool_call.name) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    block->data.tool_call.arguments = talloc_strdup(block, arguments);
    if (!block->data.tool_call.arguments) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    return block;
}

ik_content_block_t *ik_content_block_tool_result(TALLOC_CTX *ctx, const char *tool_call_id,
                                                  const char *content, bool is_error) {
    assert(tool_call_id != NULL); // LCOV_EXCL_BR_LINE
    assert(content != NULL);      // LCOV_EXCL_BR_LINE

    ik_content_block_t *block = talloc_zero(ctx, ik_content_block_t);
    if (!block) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    block->type = IK_CONTENT_TOOL_RESULT;
    block->data.tool_result.tool_call_id = talloc_strdup(block, tool_call_id);
    if (!block->data.tool_result.tool_call_id) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    block->data.tool_result.content = talloc_strdup(block, content);
    if (!block->data.tool_result.content) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    block->data.tool_result.is_error = is_error;

    return block;
}

ik_content_block_t *ik_content_block_thinking(TALLOC_CTX *ctx, const char *text) {
    assert(text != NULL); // LCOV_EXCL_BR_LINE

    ik_content_block_t *block = talloc_zero(ctx, ik_content_block_t);
    if (!block) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    block->type = IK_CONTENT_THINKING;
    block->data.thinking.text = talloc_strdup(block, text);
    if (!block->data.thinking.text) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    return block;
}

/* ================================================================
 * Request Builder Functions
 * ================================================================ */

res_t ik_request_create(TALLOC_CTX *ctx, const char *model, ik_request_t **out) {
    assert(model != NULL); // LCOV_EXCL_BR_LINE
    assert(out != NULL);   // LCOV_EXCL_BR_LINE

    ik_request_t *req = talloc_zero(ctx, ik_request_t);
    if (!req) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    req->model = talloc_strdup(req, model);
    if (!req->model) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    req->system_prompt = NULL;
    req->messages = NULL;
    req->message_count = 0;
    req->tools = NULL;
    req->tool_count = 0;
    req->max_output_tokens = -1;
    req->thinking.level = IK_THINKING_NONE;
    req->thinking.include_summary = false;
    req->tool_choice_mode = 0;  // IK_TOOL_AUTO (temporarily int during coexistence)
    req->tool_choice_name = NULL;

    *out = req;
    return OK(*out);
}

res_t ik_request_set_system(ik_request_t *req, const char *text) {
    assert(req != NULL);  // LCOV_EXCL_BR_LINE
    assert(text != NULL); // LCOV_EXCL_BR_LINE

    /* Free old system prompt if it exists */
    if (req->system_prompt) {
        talloc_free(req->system_prompt);
    }

    /* Create new system prompt as child of request */
    req->system_prompt = talloc_strdup(req, text);
    if (!req->system_prompt) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    return OK(NULL);
}

res_t ik_request_add_message(ik_request_t *req, ik_role_t role, const char *text) {
    assert(req != NULL);  // LCOV_EXCL_BR_LINE
    assert(text != NULL); // LCOV_EXCL_BR_LINE

    /* Create text content block */
    ik_content_block_t *block = ik_content_block_text(req, text);

    /* Create message */
    ik_message_t *msg = talloc_zero(req, ik_message_t);
    if (!msg) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    msg->role = role;
    msg->content_blocks = talloc_array(msg, ik_content_block_t, 1);
    if (!msg->content_blocks) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    /* Copy block contents */
    msg->content_blocks[0] = *block;
    msg->content_count = 1;
    msg->provider_metadata = NULL;

    /* Steal block to message (becomes child of message) */
    talloc_steal(msg, block);

    /* Append to messages array */
    size_t new_count = req->message_count + 1;
    req->messages = talloc_realloc(req, req->messages, ik_message_t, (unsigned int)new_count);
    if (!req->messages) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    req->messages[req->message_count] = *msg;
    req->message_count = new_count;

    /* Steal message to request */
    talloc_steal(req, msg);

    return OK(NULL);
}

res_t ik_request_add_message_blocks(ik_request_t *req, ik_role_t role,
                                     ik_content_block_t *blocks, size_t count) {
    assert(req != NULL);    // LCOV_EXCL_BR_LINE
    assert(blocks != NULL); // LCOV_EXCL_BR_LINE
    assert(count > 0);      // LCOV_EXCL_BR_LINE

    /* Create message */
    ik_message_t *msg = talloc_zero(req, ik_message_t);
    if (!msg) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    msg->role = role;
    msg->content_blocks = talloc_array(msg, ik_content_block_t, (unsigned int)count);
    if (!msg->content_blocks) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    /* Copy block contents */
    for (size_t i = 0; i < count; i++) {
        msg->content_blocks[i] = blocks[i];
    }
    msg->content_count = count;
    msg->provider_metadata = NULL;

    /* Append to messages array */
    size_t new_msg_count = req->message_count + 1;
    req->messages = talloc_realloc(req, req->messages, ik_message_t, (unsigned int)new_msg_count);
    if (!req->messages) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    req->messages[req->message_count] = *msg;
    req->message_count = new_msg_count;

    /* Steal message to request */
    talloc_steal(req, msg);

    return OK(NULL);
}

void ik_request_set_thinking(ik_request_t *req, ik_thinking_level_t level,
                              bool include_summary) {
    assert(req != NULL); // LCOV_EXCL_BR_LINE

    req->thinking.level = level;
    req->thinking.include_summary = include_summary;
}

res_t ik_request_add_tool(ik_request_t *req, const char *name, const char *description,
                           const char *parameters, bool strict) {
    assert(req != NULL);         // LCOV_EXCL_BR_LINE
    assert(name != NULL);        // LCOV_EXCL_BR_LINE
    assert(description != NULL); // LCOV_EXCL_BR_LINE
    assert(parameters != NULL);  // LCOV_EXCL_BR_LINE

    /* Create tool definition */
    ik_tool_def_t *tool = talloc_zero(req, ik_tool_def_t);
    if (!tool) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    tool->name = talloc_strdup(tool, name);
    if (!tool->name) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    tool->description = talloc_strdup(tool, description);
    if (!tool->description) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    tool->parameters = talloc_strdup(tool, parameters);
    if (!tool->parameters) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    tool->strict = strict;

    /* Append to tools array */
    size_t new_tool_count = req->tool_count + 1;
    req->tools = talloc_realloc(req, req->tools, ik_tool_def_t, (unsigned int)new_tool_count);
    if (!req->tools) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    req->tools[req->tool_count] = *tool;
    req->tool_count = new_tool_count;

    /* Steal tool to request */
    talloc_steal(req, tool);

    return OK(NULL);
}

res_t ik_request_build_from_conversation(TALLOC_CTX *ctx, void *agent_ptr, ik_request_t **out) {
    assert(agent_ptr != NULL); // LCOV_EXCL_BR_LINE
    assert(out != NULL);       // LCOV_EXCL_BR_LINE

    /* This function is a stub for now since we don't have the full agent
     * conversation structure yet. This will be implemented when the agent
     * conversation types are defined. */

    return ERR(ctx, INVALID_ARG, "ik_request_build_from_conversation not yet implemented");
}
