/**
 * @file error.h
 * @brief Google error handling (internal)
 *
 * Parses Google API error responses and maps them to provider-agnostic
 * error categories for retry logic.
 */

#ifndef IK_PROVIDERS_GOOGLE_ERROR_H
#define IK_PROVIDERS_GOOGLE_ERROR_H

#include <stdint.h>
#include <talloc.h>
#include "shared/error.h"
#include "apps/ikigai/providers/provider.h"

/**
 * Parse Google error response and map to category
 *
 * @param ctx         Talloc context for error allocation
 * @param status      HTTP status code
 * @param body        Response body (JSON)
 * @param out_category Output: error category
 * @return            OK with category set, ERR if body parsing fails
 *
 * Google error response format:
 * {
 *   "error": {
 *     "code": 403,
 *     "message": "Your API key was reported as leaked...",
 *     "status": "PERMISSION_DENIED"
 *   }
 * }
 *
 * HTTP status to category mapping:
 * - 403 PERMISSION_DENIED -> IK_ERR_CAT_AUTH
 * - 429 RESOURCE_EXHAUSTED -> IK_ERR_CAT_RATE_LIMIT
 * - 400 INVALID_ARGUMENT -> IK_ERR_CAT_INVALID_ARG
 * - 404 NOT_FOUND -> IK_ERR_CAT_NOT_FOUND
 * - 500 INTERNAL -> IK_ERR_CAT_SERVER
 * - 503 UNAVAILABLE -> IK_ERR_CAT_SERVER
 * - 504 DEADLINE_EXCEEDED -> IK_ERR_CAT_TIMEOUT
 */
res_t ik_google_handle_error(TALLOC_CTX *ctx, int32_t status, const char *body, ik_error_category_t *out_category);

/**
 * Extract retryDelay from response body
 *
 * @param body Response body (JSON)
 * @return     Retry delay in seconds, or -1 if not present
 *
 * Rate limit response format:
 * {
 *   "error": { ... },
 *   "retryDelay": "60s"
 * }
 *
 * Parses retryDelay string ("60s" format), extracts integer seconds.
 */
int32_t ik_google_get_retry_after(const char *body);

#endif /* IK_PROVIDERS_GOOGLE_ERROR_H */
