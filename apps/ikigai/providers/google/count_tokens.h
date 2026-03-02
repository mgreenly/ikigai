/**
 * @file count_tokens.h
 * @brief Google provider token counting via countTokens API
 */

#ifndef IK_PROVIDERS_GOOGLE_COUNT_TOKENS_H
#define IK_PROVIDERS_GOOGLE_COUNT_TOKENS_H

#include <stdint.h>
#include <talloc.h>
#include "shared/error.h"
#include "apps/ikigai/providers/provider.h"

/**
 * Count tokens for a request using the Google countTokens API
 *
 * Makes a synchronous blocking HTTP POST to the countTokens endpoint.
 * Wraps the serialized request body in a generateContentRequest envelope.
 * Falls back to the bytes estimator on any error.
 *
 * @param ctx             Talloc context
 * @param base_url        Base API URL (e.g., "https://generativelanguage.googleapis.com/v1beta")
 * @param api_key         API key for authentication
 * @param req             Request to count tokens for
 * @param token_count_out Output: token count (API result or bytes estimate on fallback)
 * @return                Always returns OK(NULL); fallback is transparent to caller
 */
res_t ik_google_count_tokens(void *ctx,
                              const char *base_url,
                              const char *api_key,
                              const ik_request_t *req,
                              int32_t *token_count_out);

#endif /* IK_PROVIDERS_GOOGLE_COUNT_TOKENS_H */
