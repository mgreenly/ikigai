/**
 * @file factory.h
 * @brief Provider factory for creating provider instances
 *
 * Factory interface for dispatching to provider-specific factories based on
 * provider name. Handles credential loading from environment variables or
 * credentials.json and creates async provider instances with curl_multi handles.
 */

#ifndef IK_PROVIDER_FACTORY_H
#define IK_PROVIDER_FACTORY_H

#include <stdbool.h>
#include <talloc.h>
#include "shared/error.h"
#include "apps/ikigai/providers/provider.h"

/**
 * Get environment variable name for provider
 *
 * @param provider Provider name ("openai", "anthropic", "google")
 * @return Environment variable name (e.g., "OPENAI_API_KEY") or NULL if unknown
 */
const char *ik_provider_env_var(const char *provider);

/**
 * Create provider instance with credentials
 *
 * Loads credentials from environment variables or credentials.json,
 * validates provider name, and dispatches to provider-specific factory.
 *
 * @param ctx Parent context for allocation
 * @param name Provider name ("openai", "anthropic", "google")
 * @param out Output parameter for created provider
 * @return OK on success, ERR on validation or factory failure
 */
res_t ik_provider_create(TALLOC_CTX *ctx, const char *name, ik_provider_t **out);

/**
 * Check if provider name is valid
 *
 * @param name Provider name to validate
 * @return true if provider is supported, false otherwise
 */
bool ik_provider_is_valid(const char *name);

/**
 * Get NULL-terminated list of supported providers
 *
 * @return Static array of provider names, terminated with NULL
 */
const char **ik_provider_list(void);

#endif // IK_PROVIDER_FACTORY_H
