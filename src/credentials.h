/**
 * @file credentials.h
 * @brief API credentials management for multiple providers
 *
 * Provides unified interface for loading API credentials from environment
 * variables and configuration files, with environment variables taking
 * precedence over file-based configuration.
 */

#ifndef IK_CREDENTIALS_H
#define IK_CREDENTIALS_H

#include <stdbool.h>
#include <talloc.h>
#include "error.h"

/**
 * Container for API credentials from all supported providers
 *
 * Fields are NULL if the provider is not configured in either
 * environment variables or the credentials file.
 */
typedef struct {
    char *openai_api_key;
    char *anthropic_api_key;
    char *google_api_key;
} ik_credentials_t;

/**
 * Load credentials from file and environment variables
 *
 * Precedence:
 * 1. Environment variables (OPENAI_API_KEY, ANTHROPIC_API_KEY, GOOGLE_API_KEY)
 * 2. credentials.json file
 *
 * @param ctx Parent context for allocation
 * @param path Path to credentials file (NULL = ~/.config/ikigai/credentials.json)
 * @param out_creds Output parameter for loaded credentials
 * @return OK on success, ERR on parse error (missing file is OK)
 */
res_t ik_credentials_load(TALLOC_CTX *ctx, const char *path, ik_credentials_t **out_creds);

/**
 * Get API key for a provider by name
 *
 * @param creds Credentials structure
 * @param provider Provider name ("openai", "anthropic", "google")
 * @return API key or NULL if not configured
 */
const char *ik_credentials_get(const ik_credentials_t *creds, const char *provider);

/**
 * Check if credentials file has insecure permissions
 *
 * @param path Path to credentials file
 * @return true if permissions are insecure (not 0600), false if secure
 */
bool ik_credentials_insecure_permissions(const char *path);

#endif // IK_CREDENTIALS_H
