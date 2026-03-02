/**
 * @file count_tokens.h
 * @brief Anthropic count_tokens vtable method
 */

#ifndef IK_PROVIDERS_ANTHROPIC_COUNT_TOKENS_H
#define IK_PROVIDERS_ANTHROPIC_COUNT_TOKENS_H

#include "shared/error.h"
#include "apps/ikigai/providers/provider.h"
#include <talloc.h>
#include <stdint.h>

/**
 * Count input tokens for a request using the Anthropic count_tokens API.
 *
 * Synchronous blocking HTTP POST to /v1/messages/count_tokens.
 * Falls back to bytes estimator on any HTTP or parse error.
 *
 * @param ctx             Provider context (ik_anthropic_ctx_t *)
 * @param req             Request to count tokens for
 * @param token_count_out Output: token count
 * @return                OK(NULL) always (fallback on error)
 */
res_t ik_anthropic_count_tokens(void *ctx,
                                const ik_request_t *req,
                                int32_t *token_count_out);

/**
 * Perform the HTTP POST to the count_tokens endpoint.
 *
 * Real curl-based implementation. Exposed for wrapper system.
 * Tests override ik_anthropic_count_tokens_http_ (the mockable wrapper).
 *
 * @param ctx              Talloc context for allocations
 * @param url              Full URL for the count_tokens endpoint
 * @param api_key          Anthropic API key
 * @param body             Request JSON body (serialized)
 * @param response_out     Output: response body string (allocated on ctx)
 * @param http_status_out  Output: HTTP status code
 * @return                 OK(NULL) on success, ERR on network failure
 */
res_t ik_anthropic_count_tokens_http(TALLOC_CTX *ctx,
                                     const char *url,
                                     const char *api_key,
                                     const char *body,
                                     char **response_out,
                                     long *http_status_out);

#endif /* IK_PROVIDERS_ANTHROPIC_COUNT_TOKENS_H */
