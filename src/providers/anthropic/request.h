/**
 * @file request.h
 * @brief Anthropic request serialization (internal)
 *
 * Transforms internal ik_request_t to Anthropic JSON wire format.
 */

#ifndef IK_PROVIDERS_ANTHROPIC_REQUEST_H
#define IK_PROVIDERS_ANTHROPIC_REQUEST_H

#include <talloc.h>
#include "error.h"
#include "providers/provider.h"

/**
 * Serialize internal request to Anthropic JSON format
 *
 * @param ctx      Talloc context for error allocation
 * @param req      Internal request structure
 * @param out_json Output: JSON string (allocated on ctx)
 * @return         OK with JSON string, ERR on failure
 *
 * Transformation:
 * - System prompt: concatenate blocks with "\n\n", add as "system" field
 * - Messages: serialize role/content blocks
 * - Thinking: calculate budget from level, add "thinking" config
 * - Tools: map to Anthropic schema with "input_schema" field
 * - Max tokens: default to 4096 if 0 or -1
 *
 * Errors:
 * - ERR(INVALID_ARG) if model is NULL
 * - ERR(PARSE) if JSON serialization fails
 */
res_t ik_anthropic_serialize_request(TALLOC_CTX *ctx, const ik_request_t *req, char **out_json);

/**
 * Serialize internal request to Anthropic JSON format with streaming enabled
 *
 * @param ctx      Talloc context for error allocation
 * @param req      Internal request structure
 * @param out_json Output: JSON string (allocated on ctx)
 * @return         OK with JSON string, ERR on failure
 *
 * Same as ik_anthropic_serialize_request but adds "stream": true to the JSON.
 */
res_t ik_anthropic_serialize_request_stream(TALLOC_CTX *ctx, const ik_request_t *req, char **out_json);

/**
 * Build HTTP headers for Anthropic API
 *
 * @param ctx        Talloc context for header allocation
 * @param api_key    API key for x-api-key header
 * @param out_headers Output: NULL-terminated array of header strings
 * @return           OK with headers, ERR on failure
 *
 * Returns array of 4 strings:
 * - "x-api-key: <api_key>"
 * - "anthropic-version: 2023-06-01"
 * - "content-type: application/json"
 * - NULL
 */
res_t ik_anthropic_build_headers(TALLOC_CTX *ctx, const char *api_key, char ***out_headers);

#endif /* IK_PROVIDERS_ANTHROPIC_REQUEST_H */
