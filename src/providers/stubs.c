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

// Stub implementation for OpenAI factory
// TODO: Replace with actual implementation from openai-core.md
res_t ik_openai_create(TALLOC_CTX *ctx, const char *api_key, ik_provider_t **out)
{
    (void)api_key;
    (void)out;
    return ERR(ctx, NOT_IMPLEMENTED, "OpenAI provider not yet implemented");
}

// Stub implementation for Anthropic factory
// TODO: Replace with actual implementation from anthropic-core.md
res_t ik_anthropic_create(TALLOC_CTX *ctx, const char *api_key, ik_provider_t **out)
{
    (void)api_key;
    (void)out;
    return ERR(ctx, NOT_IMPLEMENTED, "Anthropic provider not yet implemented");
}

// Stub implementation for Google factory
// TODO: Replace with actual implementation from google-core.md
res_t ik_google_create(TALLOC_CTX *ctx, const char *api_key, ik_provider_t **out)
{
    (void)api_key;
    (void)out;
    return ERR(ctx, NOT_IMPLEMENTED, "Google provider not yet implemented");
}
