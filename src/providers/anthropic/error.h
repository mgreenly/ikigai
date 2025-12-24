/**
 * @file error.h
 * @brief Anthropic error handling (internal)
 *
 * Parses Anthropic API error responses and maps them to provider-agnostic
 * error categories for retry logic.
 */

#ifndef IK_PROVIDERS_ANTHROPIC_ERROR_H
#define IK_PROVIDERS_ANTHROPIC_ERROR_H

#include <stdint.h>
#include <talloc.h>
#include "error.h"
#include "providers/provider.h"

/**
 * Parse Anthropic error response and map to category
 *
 * @param ctx         Talloc context for error allocation
 * @param status      HTTP status code
 * @param body        Response body (JSON)
 * @param out_category Output: error category
 * @return            OK with category set, ERR if body parsing fails
 *
 * Anthropic error response format:
 * {
 *   "type": "error",
 *   "error": {
 *     "type": "rate_limit_error",
 *     "message": "Your request was rate-limited"
 *   }
 * }
 *
 * HTTP status to category mapping:
 * - 401, 403 -> IK_ERR_CAT_AUTH
 * - 429      -> IK_ERR_CAT_RATE_LIMIT
 * - 400      -> IK_ERR_CAT_INVALID_ARG
 * - 404      -> IK_ERR_CAT_NOT_FOUND
 * - 500, 529 -> IK_ERR_CAT_SERVER
 */
res_t ik_anthropic_handle_error(TALLOC_CTX *ctx, int32_t status, const char *body,
                                 ik_error_category_t *out_category);

/**
 * Extract retry-after header value
 *
 * @param headers NULL-terminated array of header strings
 * @return        Retry delay in seconds, or -1 if not present
 *
 * Searches for "retry-after: N" header (case-insensitive).
 * Returns parsed integer value or -1 if header not found.
 */
int32_t ik_anthropic_get_retry_after(const char **headers);

#endif /* IK_PROVIDERS_ANTHROPIC_ERROR_H */
