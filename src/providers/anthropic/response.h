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

/**
 * Start streaming request (async vtable implementation)
 *
 * @param impl_ctx       Anthropic provider context
 * @param req            Request to send
 * @param stream_cb      Stream event callback
 * @param stream_ctx     User context for stream callback
 * @param completion_cb  Completion callback
 * @param completion_ctx User context for completion callback
 * @return               OK if request started, ERR on failure
 *
 * Returns immediately. Callbacks invoked as events arrive.
 */
res_t ik_anthropic_start_stream(void *impl_ctx, const ik_request_t *req,
                                 ik_stream_cb_t stream_cb, void *stream_ctx,
                                 ik_provider_completion_cb_t completion_cb,
                                 void *completion_ctx);

#endif /* IK_PROVIDERS_ANTHROPIC_RESPONSE_H */
