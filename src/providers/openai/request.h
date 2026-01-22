/**
 * @file request.h
 * @brief OpenAI request serialization functions
 */

#ifndef IK_PROVIDERS_OPENAI_REQUEST_H
#define IK_PROVIDERS_OPENAI_REQUEST_H

#include <talloc.h>
#include <stdbool.h>
#include "error.h"
#include "providers/provider.h"

/**
 * Serialize request to OpenAI Chat Completions JSON format
 *
 * @param ctx       Talloc context for allocations
 * @param req       Internal request structure
 * @param streaming True if streaming mode enabled
 * @param out_json  Output: JSON string allocated on ctx
 * @return          OK(json) on success, ERR(...) on failure
 *
 * Transforms internal request to Chat Completions wire format:
 * - System prompt becomes first message with role "system"
 * - Tool call arguments serialized as JSON strings (not objects)
 * - Streaming adds stream: true and stream_options
 * - Reasoning models do not include temperature parameter
 */
res_t ik_openai_serialize_chat_request(TALLOC_CTX *ctx, const ik_request_t *req, bool streaming, char **out_json);

/**
 * Build URL for OpenAI Chat Completions endpoint
 *
 * @param ctx      Talloc context for allocations
 * @param base_url Base URL (e.g., "https://api.openai.com")
 * @param out_url  Output: Full URL allocated on ctx
 * @return         OK(url) on success, ERR(...) on failure
 *
 * Returns: {base_url}/v1/chat/completions
 */
res_t ik_openai_build_chat_url(TALLOC_CTX *ctx, const char *base_url, char **out_url);

/**
 * Build HTTP headers for OpenAI API requests
 *
 * @param ctx         Talloc context for allocations
 * @param api_key     OpenAI API key
 * @param out_headers Output: NULL-terminated array of header strings
 * @return            OK(headers) on success, ERR(...) on failure
 *
 * Headers:
 * - Authorization: Bearer {api_key}
 * - Content-Type: application/json
 * - NULL terminator
 */
res_t ik_openai_build_headers(TALLOC_CTX *ctx, const char *api_key, char ***out_headers);

/**
 * Serialize request to OpenAI Responses API JSON format
 *
 * @param ctx       Talloc context for allocations
 * @param req       Internal request structure
 * @param streaming True if streaming mode enabled
 * @param out_json  Output: JSON string allocated on ctx
 * @return          OK(json) on success, ERR(...) on failure
 *
 * Transforms internal request to Responses API wire format:
 * - System prompt becomes top-level instructions field
 * - Single user message with simple text uses string input
 * - Multi-turn conversation uses array input
 * - Tool definitions use same nested format as Chat Completions
 * - Reasoning effort included for reasoning models only
 * - Uses max_output_tokens instead of max_completion_tokens
 */
res_t ik_openai_serialize_responses_request(TALLOC_CTX *ctx, const ik_request_t *req, bool streaming, char **out_json);

/**
 * Build URL for OpenAI Responses API endpoint
 *
 * @param ctx      Talloc context for allocations
 * @param base_url Base URL (e.g., "https://api.openai.com")
 * @param out_url  Output: Full URL allocated on ctx
 * @return         OK(url) on success, ERR(...) on failure
 *
 * Returns: {base_url}/v1/responses
 */
res_t ik_openai_build_responses_url(TALLOC_CTX *ctx, const char *base_url, char **out_url);

#endif /* IK_PROVIDERS_OPENAI_REQUEST_H */
