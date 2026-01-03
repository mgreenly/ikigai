/**
 * @file response.h
 * @brief Google response parsing (internal)
 *
 * Transforms Google JSON response to internal ik_response_t format.
 */

#ifndef IK_PROVIDERS_GOOGLE_RESPONSE_H
#define IK_PROVIDERS_GOOGLE_RESPONSE_H

#include <stddef.h>
#include <talloc.h>
#include "error.h"
#include "providers/provider.h"

/**
 * Parse Google JSON response to internal format
 *
 * @param ctx      Talloc context for response allocation
 * @param json     JSON string from Google API
 * @param json_len Length of JSON string
 * @param out_resp Output: parsed response (allocated on ctx)
 * @return         OK with response, ERR on parse error
 *
 * Extracts:
 * - Model name (modelVersion)
 * - Finish reason (finishReason mapped to internal enum)
 * - Content blocks from candidates[0].content.parts[]
 *   - Text parts -> IK_CONTENT_TEXT
 *   - Parts with thought=true -> IK_CONTENT_THINKING
 *   - functionCall -> IK_CONTENT_TOOL_CALL (with generated ID)
 * - Usage statistics (usageMetadata)
 * - Thought signatures (provider_data for Gemini 3)
 *
 * Returns ERR(PARSE) if:
 * - JSON is invalid
 * - Root is not an object
 *
 * Returns ERR(PROVIDER) if:
 * - Response has error object
 * - Response has promptFeedback.blockReason
 */
res_t ik_google_parse_response(TALLOC_CTX *ctx, const char *json, size_t json_len,
                                 ik_response_t **out_resp);

/**
 * Parse Google error response
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
 * - 500, 502, 503 -> IK_ERR_CAT_SERVER
 * - 504 -> IK_ERR_CAT_TIMEOUT
 * - Other -> IK_ERR_CAT_UNKNOWN
 *
 * Extracts error.message from JSON if available.
 * Falls back to "HTTP <status>" if JSON unavailable or invalid.
 */
res_t ik_google_parse_error(TALLOC_CTX *ctx, int http_status, const char *json,
                              size_t json_len, ik_error_category_t *out_category,
                              char **out_message);

/**
 * Map Google finishReason to internal finish reason
 *
 * @param finish_reason Google finishReason string (may be NULL)
 * @return              Internal finish reason enum
 *
 * Mapping:
 * - "STOP" -> IK_FINISH_STOP
 * - "MAX_TOKENS" -> IK_FINISH_LENGTH
 * - "SAFETY", "BLOCKLIST", "PROHIBITED_CONTENT", etc. -> IK_FINISH_CONTENT_FILTER
 * - "MALFORMED_FUNCTION_CALL", "UNEXPECTED_TOOL_CALL" -> IK_FINISH_ERROR
 * - NULL or unknown -> IK_FINISH_UNKNOWN
 */
ik_finish_reason_t ik_google_map_finish_reason(const char *finish_reason);

/**
 * Generate random 22-character base64url tool call ID
 *
 * @param ctx Talloc context for ID allocation
 * @return    Generated ID string (allocated on ctx)
 *
 * Uses random bytes encoded as base64url (A-Z, a-z, 0-9, -, _).
 * Seeds random generator on first use.
 */
char *ik_google_generate_tool_id(TALLOC_CTX *ctx);

/**
 * Start non-streaming request (async vtable implementation)
 *
 * @param impl_ctx      Google provider context
 * @param req           Request to send
 * @param cb            Completion callback
 * @param cb_ctx        User context for callback
 * @return              OK if request started, ERR on failure
 *
 * Returns immediately. Callback invoked from info_read() when complete.
 */
res_t ik_google_start_request(void *impl_ctx, const ik_request_t *req,
                                ik_provider_completion_cb_t cb, void *cb_ctx);

/**
 * Start streaming request (async vtable implementation)
 *
 * @param impl_ctx       Google provider context
 * @param req            Request to send
 * @param stream_cb      Stream event callback
 * @param stream_ctx     User context for stream callback
 * @param completion_cb  Completion callback
 * @param completion_ctx User context for completion callback
 * @return               OK if request started, ERR on failure
 *
 * Returns immediately. Callbacks invoked as events arrive.
 */
res_t ik_google_start_stream(void *impl_ctx, const ik_request_t *req,
                               ik_stream_cb_t stream_cb, void *stream_ctx,
                               ik_provider_completion_cb_t completion_cb,
                               void *completion_ctx);

#endif /* IK_PROVIDERS_GOOGLE_RESPONSE_H */
