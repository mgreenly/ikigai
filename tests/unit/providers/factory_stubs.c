/**
 * @file factory_stubs.c
 * @brief Stub implementations of provider-specific factories for testing
 *
 * These stubs are used to allow factory tests to link without requiring
 * the actual provider implementations. They simply return ERR_NOT_IMPLEMENTED.
 */

#include "factory_stubs.h"

// Stub implementation for OpenAI factory
res_t ik_openai_create(TALLOC_CTX *ctx, const char *api_key, ik_provider_t **out)
{
    (void)api_key;
    (void)out;
    return ERR(ctx, NOT_IMPLEMENTED, "OpenAI provider not yet implemented");
}

// Stub implementation for Anthropic factory
res_t ik_anthropic_create(TALLOC_CTX *ctx, const char *api_key, ik_provider_t **out)
{
    (void)api_key;
    (void)out;
    return ERR(ctx, NOT_IMPLEMENTED, "Anthropic provider not yet implemented");
}

// Stub implementation for Google factory
res_t ik_google_create(TALLOC_CTX *ctx, const char *api_key, ik_provider_t **out)
{
    (void)api_key;
    (void)out;
    return ERR(ctx, NOT_IMPLEMENTED, "Google provider not yet implemented");
}
