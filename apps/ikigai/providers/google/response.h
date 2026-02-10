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
#include "shared/error.h"
#include "apps/ikigai/providers/provider.h"


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
res_t ik_google_start_request(void *impl_ctx, const ik_request_t *req, ik_provider_completion_cb_t cb, void *cb_ctx);


#endif /* IK_PROVIDERS_GOOGLE_RESPONSE_H */
