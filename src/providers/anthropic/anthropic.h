/**
 * @file anthropic.h
 * @brief Anthropic provider public interface
 *
 * Factory function for creating Anthropic provider instances.
 * Provider implements async vtable for select()-based event loop integration.
 */

#ifndef IK_PROVIDERS_ANTHROPIC_H
#define IK_PROVIDERS_ANTHROPIC_H

#include <talloc.h>
#include "error.h"
#include "providers/provider.h"

/**
 * Create Anthropic provider instance
 *
 * @param ctx     Talloc context for allocation
 * @param api_key Anthropic API key
 * @param out     Output parameter for created provider
 * @return        OK with provider, ERR on failure
 *
 * Provider configuration:
 * - Base URL: https://api.anthropic.com
 * - API version: 2023-06-01
 * - Async vtable with fdset/perform/timeout/info_read for event loop
 * - start_request/start_stream for non-blocking request initiation
 */
res_t ik_anthropic_create(TALLOC_CTX *ctx, const char *api_key, ik_provider_t **out);

#endif /* IK_PROVIDERS_ANTHROPIC_H */
