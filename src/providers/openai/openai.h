/**
 * @file openai.h
 * @brief OpenAI provider public interface
 *
 * Factory functions for creating OpenAI provider instances.
 * Provider implements async vtable for select()-based event loop integration.
 */

#ifndef IK_PROVIDERS_OPENAI_H
#define IK_PROVIDERS_OPENAI_H

#include <stdbool.h>
#include <talloc.h>
#include "error.h"
#include "providers/provider.h"

/**
 * Base URL for OpenAI API
 */
#define IK_OPENAI_BASE_URL "https://api.openai.com"

/**
 * API endpoints
 */
#define IK_OPENAI_CHAT_ENDPOINT "/v1/chat/completions"
#define IK_OPENAI_RESPONSES_ENDPOINT "/v1/responses"

/**
 * Create OpenAI provider instance with Chat Completions API
 *
 * @param ctx     Talloc context for allocation
 * @param api_key OpenAI API key
 * @param out     Output parameter for created provider
 * @return        OK with provider, ERR on failure
 *
 * Provider configuration:
 * - Base URL: https://api.openai.com
 * - Endpoint: /v1/chat/completions (default)
 * - Async vtable with fdset/perform/timeout/info_read for event loop
 * - start_request/start_stream for non-blocking request initiation
 */
res_t ik_openai_create(TALLOC_CTX *ctx, const char *api_key, ik_provider_t **out);

/**
 * Create OpenAI provider instance with API selection
 *
 * @param ctx              Talloc context for allocation
 * @param api_key          OpenAI API key
 * @param use_responses_api true to use Responses API, false for Chat Completions
 * @param out              Output parameter for created provider
 * @return                 OK with provider, ERR on failure
 *
 * The Responses API performs 3% better with reasoning models (o1/o3).
 * Non-reasoning models should use Chat Completions API.
 */
res_t ik_openai_create_with_options(TALLOC_CTX *ctx, const char *api_key,
                                     bool use_responses_api, ik_provider_t **out);

#endif /* IK_PROVIDERS_OPENAI_H */
