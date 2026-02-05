/**
 * @file google.h
 * @brief Google (Gemini) provider public interface
 *
 * Factory function for creating Google provider instances.
 * Provider implements async vtable for select()-based event loop integration.
 */

#ifndef IK_PROVIDERS_GOOGLE_H
#define IK_PROVIDERS_GOOGLE_H

#include <talloc.h>
#include "shared/error.h"
#include "apps/ikigai/providers/provider.h"

/**
 * Create Google provider instance
 *
 * @param ctx     Talloc context for allocation
 * @param api_key Google API key
 * @param out     Output parameter for created provider
 * @return        OK with provider, ERR on failure
 *
 * Provider configuration:
 * - Base URL: https://generativelanguage.googleapis.com/v1beta
 * - API key: Query parameter (?key=...)
 * - Async vtable with fdset/perform/timeout/info_read for event loop
 * - start_request/start_stream for non-blocking request initiation
 */
res_t ik_google_create(TALLOC_CTX *ctx, const char *api_key, ik_provider_t **out);

#endif /* IK_PROVIDERS_GOOGLE_H */
