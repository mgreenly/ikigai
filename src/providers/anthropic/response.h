/**
 * @file response.h
 * @brief Anthropic response parsing (internal)
 *
 * Transforms Anthropic JSON response to internal ik_response_t format.
 */

#ifndef IK_PROVIDERS_ANTHROPIC_RESPONSE_H
#define IK_PROVIDERS_ANTHROPIC_RESPONSE_H

#include <stddef.h>
#include <talloc.h>
#include "error.h"
#include "providers/provider.h"

/**
 * Parse Anthropic JSON response to internal format
 *
 * @param ctx      Talloc context for response allocation
 * @param json     JSON string from Anthropic API
 * @param json_len Length of JSON string
 * @param out_resp Output: parsed response (allocated on ctx)
 * @return         OK with response, ERR on parse error
 *
 * Extracts:
 * - Model name
 * - Stop reason (mapped to finish reason)
 * - Content blocks (text, thinking, tool_use)
 * - Usage statistics (input/output/thinking/cached tokens)
 *
 * Returns ERR(PARSE) if:
 * - JSON is invalid
 * - Root is not an object
 *
 * Returns ERR(PROVIDER) if:
 * - Response type is "error"
 */
res_t ik_anthropic_parse_response(TALLOC_CTX *ctx, const char *json, size_t json_len,
                                   ik_response_t **out_resp);

/**
 * Parse Anthropic error response
 *
 * @param ctx          Talloc context for error allocation
 * @param http_status  HTTP status code
 * @param json         JSON error response (may be NULL)
 * @param json_len     Length of JSON (0 if NULL)
 * @param out_category Output: error category
 * @param out_message  Output: error message (allocated on ctx)
 * @return             OK with category/message, ERR on parse failure
 *
 * Maps HTTP status to category:
 * - 400 -> IK_ERR_CAT_INVALID_ARG
 * - 401, 403 -> IK_ERR_CAT_AUTH
 * - 404 -> IK_ERR_CAT_NOT_FOUND
 * - 429 -> IK_ERR_CAT_RATE_LIMIT
 * - 500, 502, 503, 529 -> IK_ERR_CAT_SERVER
 * - Other -> IK_ERR_CAT_UNKNOWN
 *
 * Extracts error.message and error.type from JSON if available.
 * Falls back to "HTTP <status>" if JSON unavailable or invalid.
 */
res_t ik_anthropic_parse_error(TALLOC_CTX *ctx, int http_status, const char *json,
                                size_t json_len, ik_error_category_t *out_category,
                                char **out_message);

/**
 * Map Anthropic stop_reason to internal finish reason
 *
 * @param stop_reason Anthropic stop_reason string (may be NULL)
 * @return            Internal finish reason enum
 *
 * Mapping:
 * - "end_turn" -> IK_FINISH_STOP
 * - "max_tokens" -> IK_FINISH_LENGTH
 * - "tool_use" -> IK_FINISH_TOOL_USE
 * - "stop_sequence" -> IK_FINISH_STOP
 * - "refusal" -> IK_FINISH_CONTENT_FILTER
 * - NULL or unknown -> IK_FINISH_UNKNOWN
 */
ik_finish_reason_t ik_anthropic_map_finish_reason(const char *stop_reason);

/**
 * Start non-streaming request (async vtable implementation)
 *
 * @param impl_ctx      Anthropic provider context
 * @param req           Request to send
 * @param cb            Completion callback
 * @param cb_ctx        User context for callback
 * @return              OK if request started, ERR on failure
 *
 * Returns immediately. Callback invoked from info_read() when complete.
 */
res_t ik_anthropic_start_request(void *impl_ctx, const ik_request_t *req,
                                  ik_provider_completion_cb_t cb, void *cb_ctx);


#endif /* IK_PROVIDERS_ANTHROPIC_RESPONSE_H */
