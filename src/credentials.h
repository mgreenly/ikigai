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
    char *brave_api_key;
    char *google_search_api_key;
    char *google_search_engine_id;
    char *ntfy_api_key;
    char *ntfy_topic;
    char *db_pass;
} ik_credentials_t;

/**
 * Load credentials from file and environment variables
 *
 * Flat JSON structure with env var names as keys:
 * {"OPENAI_API_KEY": "...", "ANTHROPIC_API_KEY": "...", ...}
 *
 * Precedence:
 * 1. Environment variables (8 vars: OPENAI_API_KEY, ANTHROPIC_API_KEY, etc)
 * 2. credentials.json file
 *
 * @param ctx Parent context for allocation
 * @param path Path to credentials file (NULL = ~/.config/ikigai/credentials.json)
 * @param out_creds Output parameter for loaded credentials
 * @return OK on success, ERR on parse error (missing file is OK)
 */
res_t ik_credentials_load(TALLOC_CTX *ctx, const char *path, ik_credentials_t **out_creds);

/**
 * Get credential value by environment variable name
 *
 * @param creds Credentials structure
 * @param env_var_name Environment variable name (e.g., "OPENAI_API_KEY", "NTFY_TOPIC")
 * @return Credential value or NULL if not configured
 */
const char *ik_credentials_get(const ik_credentials_t *creds, const char *env_var_name);

/**
 * Check if credentials file has insecure permissions
 *
 * @param path Path to credentials file
 * @return true if permissions are insecure (not 0600), false if secure
 */
bool ik_credentials_insecure_permissions(const char *path);

#endif // IK_CREDENTIALS_H
