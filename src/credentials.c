#include "credentials.h"
#include "json_allocator.h"
#include "panic.h"
#include "vendor/yyjson/yyjson.h"
#include "wrapper.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// Helper to expand tilde in path
static res_t expand_tilde(TALLOC_CTX *ctx, const char *path, char **out_path)
{
    assert(ctx != NULL); // LCOV_EXCL_BR_LINE
    assert(path != NULL); // LCOV_EXCL_BR_LINE
    assert(out_path != NULL); // LCOV_EXCL_BR_LINE

    // If path doesn't start with tilde, return as-is
    if (path[0] != '~') {
        char *result = talloc_strdup_(ctx, path);
        if (result == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        *out_path = result;
        return OK(NULL);
    }

    // Get HOME environment variable
    const char *home = getenv("HOME");
    if (!home) {
        return ERR(ctx, INVALID_ARG, "HOME not set, cannot expand ~");
    }

    // Expand tilde to HOME
    char *result = talloc_asprintf_(ctx, "%s%s", home, path + 1);
    if (result == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    *out_path = result;
    return OK(NULL);
}

// Helper to get environment variable value (returns NULL if empty or unset)
static const char *get_env_nonempty(const char *name)
{
    assert(name != NULL); // LCOV_EXCL_BR_LINE
    const char *value = getenv(name);
    if (value && value[0] != '\0') {
        return value;
    }
    return NULL;
}

// Helper to load credentials from JSON file
static res_t load_from_file(TALLOC_CTX *ctx, const char *path, ik_credentials_t *creds)
{
    assert(ctx != NULL); // LCOV_EXCL_BR_LINE
    assert(path != NULL); // LCOV_EXCL_BR_LINE
    assert(creds != NULL); // LCOV_EXCL_BR_LINE

    // Check if file exists
    struct stat st;
    if (posix_stat_(path, &st) != 0) {
        // File doesn't exist - this is OK, return empty credentials
        return OK(NULL);
    }

    // Read and parse JSON file
    yyjson_alc allocator = ik_make_talloc_allocator(ctx);
    yyjson_read_err read_err;
    yyjson_doc *doc = yyjson_read_file_(path, 0, &allocator, &read_err);
    if (!doc) {
        return ERR(ctx, PARSE, "Failed to parse JSON: %s", read_err.msg);
    }

    yyjson_val *root = yyjson_doc_get_root_(doc);
    if (!root || !yyjson_is_obj(root)) {
        return ERR(ctx, PARSE, "JSON root is not an object");
    }

    // Extract provider credentials
    yyjson_val *openai_obj = yyjson_obj_get_(root, "openai");
    if (openai_obj && yyjson_is_obj(openai_obj)) {
        yyjson_val *api_key = yyjson_obj_get_(openai_obj, "api_key");
        if (api_key && yyjson_is_str(api_key)) {
            const char *key_str = yyjson_get_str_(api_key);
            if (key_str && key_str[0] != '\0') {
                creds->openai_api_key = talloc_strdup(creds, key_str);
                if (creds->openai_api_key == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
            }
        }
    }

    yyjson_val *anthropic_obj = yyjson_obj_get_(root, "anthropic");
    if (anthropic_obj && yyjson_is_obj(anthropic_obj)) {
        yyjson_val *api_key = yyjson_obj_get_(anthropic_obj, "api_key");
        if (api_key && yyjson_is_str(api_key)) {
            const char *key_str = yyjson_get_str_(api_key);
            if (key_str && key_str[0] != '\0') {
                creds->anthropic_api_key = talloc_strdup(creds, key_str);
                if (creds->anthropic_api_key == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
            }
        }
    }

    yyjson_val *google_obj = yyjson_obj_get_(root, "google");
    if (google_obj && yyjson_is_obj(google_obj)) {
        yyjson_val *api_key = yyjson_obj_get_(google_obj, "api_key");
        if (api_key && yyjson_is_str(api_key)) {
            const char *key_str = yyjson_get_str_(api_key);
            if (key_str && key_str[0] != '\0') {
                creds->google_api_key = talloc_strdup(creds, key_str);
                if (creds->google_api_key == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
            }
        }
    }

    return OK(NULL);
}

res_t ik_credentials_load(TALLOC_CTX *ctx, const char *path, ik_credentials_t **out_creds)
{
    assert(ctx != NULL); // LCOV_EXCL_BR_LINE
    assert(out_creds != NULL); // LCOV_EXCL_BR_LINE

    // Use default path if none provided
    const char *creds_path = path;
    char *expanded_path = NULL;
    if (!creds_path) {
        // Check for IKIGAI_CONFIG_DIR environment variable
        const char *config_dir = getenv("IKIGAI_CONFIG_DIR");
        if (config_dir) {
            creds_path = talloc_asprintf(ctx, "%s/credentials.json", config_dir);
            if (creds_path == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        } else {
            creds_path = "~/.config/ikigai/credentials.json";
        }
    }

    // Expand tilde in path
    res_t expand_result = expand_tilde(ctx, creds_path, &expanded_path);
    if (is_err(&expand_result)) {
        return expand_result;
    }

    // Allocate credentials structure
    ik_credentials_t *creds = talloc_zero(ctx, ik_credentials_t);
    if (creds == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Check for insecure permissions and warn
    if (ik_credentials_insecure_permissions(expanded_path)) {
        fprintf(stderr, "Warning: credentials file %s has insecure permissions (should be 0600)\n", expanded_path);
    }

    // Load from file first (errors are warnings, env vars take priority)
    res_t load_result = load_from_file(ctx, expanded_path, creds);
    if (is_err(&load_result)) {
        // Warn but continue - env vars can still provide credentials
        fprintf(stderr, "Warning: %s\n", load_result.err->msg);
    }

    // Override with environment variables (higher precedence)
    const char *env_openai = get_env_nonempty("OPENAI_API_KEY");
    if (env_openai) {
        // Free file value if present
        if (creds->openai_api_key) {
            talloc_free(creds->openai_api_key);
        }
        creds->openai_api_key = talloc_strdup(creds, env_openai);
        if (creds->openai_api_key == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    }

    const char *env_anthropic = get_env_nonempty("ANTHROPIC_API_KEY");
    if (env_anthropic) {
        if (creds->anthropic_api_key) {
            talloc_free(creds->anthropic_api_key);
        }
        creds->anthropic_api_key = talloc_strdup(creds, env_anthropic);
        if (creds->anthropic_api_key == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    }

    const char *env_google = get_env_nonempty("GOOGLE_API_KEY");
    if (env_google) {
        if (creds->google_api_key) {
            talloc_free(creds->google_api_key);
        }
        creds->google_api_key = talloc_strdup(creds, env_google);
        if (creds->google_api_key == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    }

    *out_creds = creds;
    return OK(creds);
}

const char *ik_credentials_get(const ik_credentials_t *creds, const char *provider)
{
    assert(creds != NULL); // LCOV_EXCL_BR_LINE
    assert(provider != NULL); // LCOV_EXCL_BR_LINE

    if (strcmp(provider, "openai") == 0) {
        return creds->openai_api_key;
    } else if (strcmp(provider, "anthropic") == 0) {
        return creds->anthropic_api_key;
    } else if (strcmp(provider, "google") == 0) {
        return creds->google_api_key;
    }

    return NULL;
}

bool ik_credentials_insecure_permissions(const char *path)
{
    assert(path != NULL); // LCOV_EXCL_BR_LINE

    struct stat st;
    if (posix_stat_(path, &st) != 0) {
        // File doesn't exist - not insecure
        return false;
    }

    // Check if permissions are exactly 0600 (owner read/write only)
    mode_t perms = st.st_mode & 0777;
    return perms != 0600;
}
