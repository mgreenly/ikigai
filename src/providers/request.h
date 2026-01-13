#ifndef IK_PROVIDERS_REQUEST_H
#define IK_PROVIDERS_REQUEST_H

#include "error.h"
#include "providers/provider.h"
#include "tool_registry.h"
#include "vendor/yyjson/yyjson.h"
#include <talloc.h>

/**
 * Request Builder API
 *
 * This module provides builder functions for constructing ik_request_t
 * structures with system prompts, messages, content blocks, tools, and
 * thinking configuration.
 *
 * Memory model: All builders allocate on the provided TALLOC_CTX and
 * use talloc hierarchical ownership for automatic cleanup.
 */

/* ================================================================
 * Content Block Builders
 * ================================================================ */

/**
 * Create text content block
 *
 * @param ctx  Talloc parent context
 * @param text Text content (will be copied)
 * @return     Allocated content block, or NULL on OOM
 */
ik_content_block_t *ik_content_block_text(TALLOC_CTX *ctx, const char *text);

/**
 * Create tool call content block
 *
 * @param ctx       Talloc parent context
 * @param id        Tool call ID (will be copied)
 * @param name      Function name (will be copied)
 * @param arguments JSON arguments string (will be copied)
 * @return          Allocated content block, or NULL on OOM
 */
ik_content_block_t *ik_content_block_tool_call(TALLOC_CTX *ctx, const char *id,
                                                const char *name, const char *arguments);

/**
 * Create tool result content block
 *
 * @param ctx          Talloc parent context
 * @param tool_call_id Tool call ID this result is for (will be copied)
 * @param content      Result content (will be copied)
 * @param is_error     true if tool execution failed
 * @return             Allocated content block, or NULL on OOM
 */
ik_content_block_t *ik_content_block_tool_result(TALLOC_CTX *ctx, const char *tool_call_id,
                                                  const char *content, bool is_error);

/* ================================================================
 * Request Builder Functions
 * ================================================================ */

/**
 * Create empty request with model
 *
 * Allocates and initializes ik_request_t structure with the specified
 * model. All arrays (messages, tools, system_prompt) are initialized
 * to NULL, max_output_tokens is set to -1 (use provider default),
 * and thinking level is set to NONE.
 *
 * @param ctx   Talloc parent context
 * @param model Model identifier (will be copied)
 * @param out   Receives allocated request
 * @return      OK with request, ERR on allocation failure
 */
res_t ik_request_create(TALLOC_CTX *ctx, const char *model, ik_request_t **out);

/**
 * Set system prompt as single text block
 *
 * Creates a system prompt from the provided text. Replaces any
 * existing system prompt.
 *
 * @param req  Request to modify
 * @param text System prompt text (will be copied)
 * @return     OK on success, ERR on allocation failure
 */
res_t ik_request_set_system(ik_request_t *req, const char *text);

/**
 * Add simple text message
 *
 * Creates a message with a single text content block and appends
 * it to the request's message array.
 *
 * @param req  Request to modify
 * @param role Message role (USER, ASSISTANT, TOOL)
 * @param text Message text (will be copied)
 * @return     OK on success, ERR on allocation failure
 */
res_t ik_request_add_message(ik_request_t *req, ik_role_t role, const char *text);

/**
 * Add message with content blocks
 *
 * Creates a message with the provided content blocks and appends
 * it to the request's message array. The content blocks are owned
 * by the caller or the request after this call.
 *
 * @param req    Request to modify
 * @param role   Message role (USER, ASSISTANT, TOOL)
 * @param blocks Array of content blocks
 * @param count  Number of content blocks
 * @return       OK on success, ERR on allocation failure
 */
res_t ik_request_add_message_blocks(ik_request_t *req, ik_role_t role,
                                     ik_content_block_t *blocks, size_t count);

/**
 * Configure thinking level
 *
 * Sets the thinking level and summary inclusion flag for the entire
 * request. This is a void function as it cannot fail.
 *
 * @param req             Request to modify
 * @param level           Thinking level (NONE, LOW, MED, HIGH)
 * @param include_summary Include thinking summary in response
 */
void ik_request_set_thinking(ik_request_t *req, ik_thinking_level_t level,
                              bool include_summary);

/**
 * Add tool definition
 *
 * Creates a tool definition structure and appends it to the request's
 * tools array.
 *
 * @param req         Request to modify
 * @param name        Tool name (will be copied)
 * @param description Tool description (will be copied)
 * @param parameters  JSON schema string for parameters (will be copied)
 * @param strict      Enable strict schema validation
 * @return            OK on success, ERR on allocation failure
 */
res_t ik_request_add_tool(ik_request_t *req, const char *name, const char *description,
                           const char *parameters, bool strict);

/**
 * Build complete request from agent state
 *
 * Constructs a complete ik_request_t from agent context for provider
 * submission. This function bridges the agent's conversation state to
 * the normalized provider request format.
 *
 * Steps:
 * 1. Create request with agent->model
 * 2. Set system prompt from agent->system_prompt (if non-NULL)
 * 3. Iterate agent->messages and map to request messages
 * 4. Add tool definitions from registry (if non-NULL)
 * 5. Set thinking level from agent->thinking_level
 *
 * Note: This function expects agent context but since we don't have
 * the full agent.h included, we accept void* and cast internally.
 *
 * @param ctx      Talloc parent context
 * @param agent    Agent context (ik_agent_ctx_t*)
 * @param registry Tool registry (ik_tool_registry_t*), can be NULL
 * @param out      Receives allocated request
 * @return         OK with populated request, ERR on allocation failure
 */
res_t ik_request_build_from_conversation(TALLOC_CTX *ctx, void *agent, ik_tool_registry_t *registry, ik_request_t **out);

#endif /* IK_PROVIDERS_REQUEST_H */
