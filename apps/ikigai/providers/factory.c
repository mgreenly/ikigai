/**
 * @file factory.c
 * @brief Provider factory implementation
 */

#include "apps/ikigai/providers/factory.h"
#include "shared/credentials.h"
#include "shared/panic.h"
#include <string.h>


#include "shared/poison.h"
// Forward declarations for provider-specific factories
// These are implemented in later tasks (openai-core.md, anthropic-core.md, google-core.md)
extern res_t ik_openai_create(TALLOC_CTX *ctx, const char *api_key, ik_provider_t **out);
extern res_t ik_anthropic_create(TALLOC_CTX *ctx, const char *api_key, ik_provider_t **out);
extern res_t ik_google_create(TALLOC_CTX *ctx, const char *api_key, ik_provider_t **out);

// Static list of supported providers
static const char *SUPPORTED_PROVIDERS[] = {
    "openai",
    "anthropic",
    "google",
    NULL
};

const char *ik_provider_env_var(const char *provider)
{
    if (provider == NULL) {
        return NULL;
    }

    if (strcmp(provider, "openai") == 0) {
        return "OPENAI_API_KEY";
    } else if (strcmp(provider, "anthropic") == 0) {
        return "ANTHROPIC_API_KEY";
    } else if (strcmp(provider, "google") == 0) {
        return "GOOGLE_API_KEY";
    }

    return NULL;
}

bool ik_provider_is_valid(const char *name)
{
    if (name == NULL) {
        return false;
    }

    for (size_t i = 0; SUPPORTED_PROVIDERS[i] != NULL; i++) {
        if (strcmp(name, SUPPORTED_PROVIDERS[i]) == 0) {
            return true;
        }
    }

    return false;
}

const char **ik_provider_list(void)
{
    return SUPPORTED_PROVIDERS;
}

res_t ik_provider_create(TALLOC_CTX *ctx, const char *name, ik_provider_t **out)
{
    assert(ctx != NULL);    // LCOV_EXCL_BR_LINE
    assert(name != NULL);   // LCOV_EXCL_BR_LINE
    assert(out != NULL);    // LCOV_EXCL_BR_LINE

    // Validate provider name
    if (!ik_provider_is_valid(name)) {
        return ERR(ctx, INVALID_ARG, "Unknown provider: %s", name);
    }

    // Load credentials on temporary context
    TALLOC_CTX *tmp_ctx = talloc_new(ctx);
    if (tmp_ctx == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    ik_credentials_t *creds = NULL;
    // Pass ctx (not tmp_ctx) so errors survive tmp_ctx cleanup
    res_t load_res = ik_credentials_load(ctx, NULL, &creds);
    if (is_err(&load_res)) {
        talloc_free(tmp_ctx);
        return load_res;
    }

    // Get API key for the provider
    const char *env_var = ik_provider_env_var(name);
    const char *api_key = ik_credentials_get(creds, env_var);
    if (api_key == NULL) {
        talloc_free(tmp_ctx);
        return ERR(ctx, MISSING_CREDENTIALS,
                   "No credentials found for provider '%s'. "
                   "Set %s environment variable or add to ~/.config/ikigai/credentials.json",
                   name, env_var);
    }

    // Dispatch to provider-specific factory
    res_t factory_res;
    if (strcmp(name, "openai") == 0) {
        factory_res = ik_openai_create(ctx, api_key, out);
    } else if (strcmp(name, "anthropic") == 0) {
        factory_res = ik_anthropic_create(ctx, api_key, out);
    } else if (strcmp(name, "google") == 0) { // LCOV_EXCL_BR_LINE
        factory_res = ik_google_create(ctx, api_key, out);
    } else {
        // Should not reach here due to validation above
        talloc_free(tmp_ctx); // LCOV_EXCL_LINE
        return ERR(ctx, INVALID_ARG, "Unknown provider: %s", name); // LCOV_EXCL_LINE
    }

    // Clean up temporary context
    talloc_free(tmp_ctx);

    return factory_res;
}
