/**
 * @file count_tokens.h
 * @brief OpenAI token counting via POST /v1/responses/input_tokens
 */

#ifndef IK_PROVIDERS_OPENAI_COUNT_TOKENS_H
#define IK_PROVIDERS_OPENAI_COUNT_TOKENS_H

#include <stdint.h>
#include <talloc.h>
#include "shared/error.h"
#include "apps/ikigai/providers/request.h"

/**
 * Count input tokens for a request via the OpenAI Responses API.
 *
 * Makes a synchronous blocking POST to /v1/responses/input_tokens.
 * On any HTTP or parse error, falls back to the bytes estimator and
 * returns OK with the estimate.
 *
 * @param ctx_talloc  Talloc context for temporary allocations
 * @param base_url    Base URL (e.g., "https://api.openai.com")
 * @param api_key     OpenAI API key for Bearer authentication
 * @param req         Request to count tokens for
 * @param out         Output: number of input tokens
 * @return            OK(NULL) always (falls back to estimate on error)
 */
res_t ik_openai_count_tokens(TALLOC_CTX *ctx_talloc,
                              const char *base_url,
                              const char *api_key,
                              const ik_request_t *req,
                              int32_t *out);

#endif /* IK_PROVIDERS_OPENAI_COUNT_TOKENS_H */
