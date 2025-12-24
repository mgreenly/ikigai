/**
 * @file response.h
 * @brief OpenAI response parsing functions
 */

#ifndef IK_PROVIDERS_OPENAI_RESPONSE_H
#define IK_PROVIDERS_OPENAI_RESPONSE_H

#include <talloc.h>
#include <stdint.h>
#include "error.h"
#include "providers/provider.h"

/**
 * Parse OpenAI Chat Completions JSON response to internal format
 *
 * @param ctx      Talloc context for allocations
 * @param json     JSON response string
 * @param json_len Length of JSON string
 * @param out_resp Output: parsed response allocated on ctx
 * @return         OK(response) on success, ERR(...) on failure
 *
 * Chat Completions response format:
 * {
 *   "id": "chatcmpl-123",
 *   "object": "chat.completion",
 *   "created": 1677652288,
 *   "model": "gpt-4",
 *   "choices": [{
 *     "index": 0,
 *     "message": {
 *       "role": "assistant",
 *       "content": "Hello there, how may I assist you today?",
 *       "tool_calls": [...]  // Optional
 *     },
 *     "finish_reason": "stop"
 *   }],
 *   "usage": {
 *     "prompt_tokens": 9,
 *     "completion_tokens": 12,
 *     "total_tokens": 21,
 *     "completion_tokens_details": {
 *       "reasoning_tokens": 0
 *     }
 *   }
 * }
 *
 * Transformations:
 * - choices[0].message -> content_blocks array
 * - choices[0].finish_reason -> finish_reason enum
 * - usage -> ik_usage_t (including reasoning_tokens)
 * - Tool call arguments (JSON strings) are parsed to yyjson_val
 */
res_t ik_openai_parse_chat_response(TALLOC_CTX *ctx, const char *json,
                                     size_t json_len, ik_response_t **out_resp);

/**
 * Parse OpenAI error response and map HTTP status to error category
 *
 * @param ctx          Talloc context for allocations
 * @param http_status  HTTP status code
 * @param json         JSON error response (may be NULL)
 * @param json_len     Length of JSON string
 * @param out_category Output: error category
 * @param out_message  Output: error message allocated on ctx
 * @return             OK(NULL) on success, ERR(...) on parse failure
 *
 * OpenAI error response format:
 * {
 *   "error": {
 *     "message": "Incorrect API key provided",
 *     "type": "invalid_request_error",
 *     "code": "invalid_api_key"
 *   }
 * }
 *
 * HTTP status mappings:
 * - 400 -> IK_ERR_CAT_INVALID_ARG
 * - 401, 403 -> IK_ERR_CAT_AUTH
 * - 404 -> IK_ERR_CAT_NOT_FOUND
 * - 429 -> IK_ERR_CAT_RATE_LIMIT
 * - 500, 502, 503 -> IK_ERR_CAT_SERVER
 * - Other -> IK_ERR_CAT_UNKNOWN
 *
 * Message format:
 * - "{type} ({code}): {message}" if all fields present
 * - "{type}: {message}" if type and message present
 * - "{message}" if only message present
 * - "HTTP {status}" if no JSON or parse fails
 */
res_t ik_openai_parse_error(TALLOC_CTX *ctx, int http_status, const char *json,
                             size_t json_len, ik_error_category_t *out_category,
                             char **out_message);

/**
 * Map OpenAI finish_reason string to internal finish reason enum
 *
 * @param finish_reason Finish reason string from OpenAI response
 * @return              Internal finish reason enum
 *
 * Mappings:
 * - "stop" -> IK_FINISH_STOP
 * - "length" -> IK_FINISH_LENGTH
 * - "tool_calls" -> IK_FINISH_TOOL_USE
 * - "content_filter" -> IK_FINISH_CONTENT_FILTER
 * - "error" -> IK_FINISH_ERROR
 * - NULL or unknown -> IK_FINISH_UNKNOWN
 */
ik_finish_reason_t ik_openai_map_chat_finish_reason(const char *finish_reason);

#endif /* IK_PROVIDERS_OPENAI_RESPONSE_H */
