/**
 * @file stubs.c
 * @brief Stub implementations of provider-specific factories
 *
 * These stub implementations allow the codebase to compile and link
 * before the actual provider implementations are completed.
 * They return ERR_NOT_IMPLEMENTED for all provider creation attempts.
 *
 * This file will be removed once the actual provider implementations
 * (openai-core.md, anthropic-core.md, google-core.md) are complete.
 */

#include "providers/stubs.h"

// OpenAI factory implementation moved to providers/openai/shim.c
// Anthropic factory implementation moved to providers/anthropic/anthropic.c

// Stub implementation for Google factory
// TODO: Replace with actual implementation from google-core.md
res_t ik_google_create(TALLOC_CTX *ctx, const char *api_key, ik_provider_t **out)
{
    (void)api_key;
    (void)out;
    return ERR(ctx, NOT_IMPLEMENTED, "Google provider not yet implemented");
}
