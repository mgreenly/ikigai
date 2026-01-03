/**
 * @file stubs.h
 * @brief Stub declarations for provider-specific factories
 *
 * This file declares the stub implementations that will be replaced
 * when actual provider implementations are completed.
 *
 * This file will be removed once the actual provider implementations
 * (openai-core.md, anthropic-core.md, google-core.md) are complete.
 */

#ifndef IK_PROVIDERS_STUBS_H
#define IK_PROVIDERS_STUBS_H

#include <talloc.h>
#include "error.h"
#include "providers/provider.h"

// Stub implementations (to be replaced by actual implementations)
res_t ik_openai_create(TALLOC_CTX *ctx, const char *api_key, ik_provider_t **out);
res_t ik_anthropic_create(TALLOC_CTX *ctx, const char *api_key, ik_provider_t **out);
res_t ik_google_create(TALLOC_CTX *ctx, const char *api_key, ik_provider_t **out);

#endif // IK_PROVIDERS_STUBS_H
