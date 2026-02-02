/**
 * @file error.h
 * @brief OpenAI error handling (internal)
 *
 * Parses OpenAI API error responses and maps them to provider-agnostic
 * error categories for retry logic.
 */

#ifndef IK_PROVIDERS_OPENAI_ERROR_H
#define IK_PROVIDERS_OPENAI_ERROR_H

#include <stdint.h>
#include <talloc.h>
#include "error.h"
#include "providers/provider.h"

/**
 * Parse OpenAI error response and map to category
 *
 * @param ctx         Talloc context for error allocation
 * @param status      HTTP status code
 * @param body        Response body (JSON)
 * @param out_category Output: error category
 * @return            OK with category set, ERR if body parsing fails
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
 * HTTP status to category mapping:
 * - 401 invalid_api_key -> IK_ERR_CAT_AUTH
 * - 401 invalid_org -> IK_ERR_CAT_AUTH
 * - 429 rate_limit_exceeded -> IK_ERR_CAT_RATE_LIMIT
 * - 429 quota_exceeded -> IK_ERR_CAT_RATE_LIMIT
 * - 400 invalid_request_error -> IK_ERR_CAT_INVALID_ARG
 * - 404 model_not_found -> IK_ERR_CAT_NOT_FOUND
 * - 500 server_error -> IK_ERR_CAT_SERVER
 * - 503 service_unavailable -> IK_ERR_CAT_SERVER
 * - content_filter (any code/type) -> IK_ERR_CAT_CONTENT_FILTER
 */
res_t ik_openai_handle_error(TALLOC_CTX *ctx, int32_t status, const char *body, ik_error_category_t *out_category);

#endif /* IK_PROVIDERS_OPENAI_ERROR_H */
