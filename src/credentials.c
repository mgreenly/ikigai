#include "credentials.h"

#include "json_allocator.h"
#include "panic.h"
#include "wrapper_json.h"
#include "wrapper_posix.h"
#include "wrapper_stdlib.h"
#include "wrapper_talloc.h"

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
    const char *home = getenv_("HOME");
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
    const char *value = getenv_(name);
    if (value && value[0] != '\0') {
        return value;
    }
    return NULL;
}

// Helper to load a single credential from flat JSON structure
static void load_credential_field(yyjson_val *root, const char *key, ik_credentials_t *creds, char **field)
{
    assert(root != NULL); // LCOV_EXCL_BR_LINE
    assert(key != NULL); // LCOV_EXCL_BR_LINE
    assert(creds != NULL); // LCOV_EXCL_BR_LINE
    assert(field != NULL); // LCOV_EXCL_BR_LINE

    yyjson_val *val = yyjson_obj_get_(root, key);
    if (val && yyjson_is_str(val)) {
        const char *str = yyjson_get_str_(val);
        if (str && str[0] != '\0') {
            *field = talloc_strdup(creds, str);
            if (*field == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        }
    }
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

    // Load all credentials from flat JSON structure
    load_credential_field(root, "OPENAI_API_KEY", creds, &creds->openai_api_key);
    load_credential_field(root, "ANTHROPIC_API_KEY", creds, &creds->anthropic_api_key);
    load_credential_field(root, "GOOGLE_API_KEY", creds, &creds->google_api_key);
    load_credential_field(root, "BRAVE_API_KEY", creds, &creds->brave_api_key);
    load_credential_field(root, "GOOGLE_SEARCH_API_KEY", creds, &creds->google_search_api_key);
    load_credential_field(root, "GOOGLE_SEARCH_ENGINE_ID", creds, &creds->google_search_engine_id);
    load_credential_field(root, "NTFY_API_KEY", creds, &creds->ntfy_api_key);
    load_credential_field(root, "NTFY_TOPIC", creds, &creds->ntfy_topic);
    load_credential_field(root, "IKIGAI_DB_PASS", creds, &creds->db_pass);

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
        const char *config_dir = getenv_("IKIGAI_CONFIG_DIR");
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

    // Load from file first (errors are warnings, env vars take priority)
    res_t load_result = load_from_file(ctx, expanded_path, creds);
    (void)load_result;  // Warn but continue - env vars can still provide credentials

    // Override with environment variables (higher precedence)
    const char *env_openai = get_env_nonempty("OPENAI_API_KEY");
    if (env_openai) {
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

    const char *env_brave = get_env_nonempty("BRAVE_API_KEY");
    if (env_brave) {
        if (creds->brave_api_key) {
            talloc_free(creds->brave_api_key);
        }
        creds->brave_api_key = talloc_strdup(creds, env_brave);
        if (creds->brave_api_key == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    }

    const char *env_google_search = get_env_nonempty("GOOGLE_SEARCH_API_KEY");
    if (env_google_search) {
        if (creds->google_search_api_key) {
            talloc_free(creds->google_search_api_key);
        }
        creds->google_search_api_key = talloc_strdup(creds, env_google_search);
        if (creds->google_search_api_key == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    }

    const char *env_google_engine = get_env_nonempty("GOOGLE_SEARCH_ENGINE_ID");
    if (env_google_engine) {
        if (creds->google_search_engine_id) {
            talloc_free(creds->google_search_engine_id);
        }
        creds->google_search_engine_id = talloc_strdup(creds, env_google_engine);
        if (creds->google_search_engine_id == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    }

    const char *env_ntfy_key = get_env_nonempty("NTFY_API_KEY");
    if (env_ntfy_key) {
        if (creds->ntfy_api_key) {
            talloc_free(creds->ntfy_api_key);
        }
        creds->ntfy_api_key = talloc_strdup(creds, env_ntfy_key);
        if (creds->ntfy_api_key == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    }

    const char *env_ntfy_topic = get_env_nonempty("NTFY_TOPIC");
    if (env_ntfy_topic) {
        if (creds->ntfy_topic) {
            talloc_free(creds->ntfy_topic);
        }
        creds->ntfy_topic = talloc_strdup(creds, env_ntfy_topic);
        if (creds->ntfy_topic == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    }

    const char *env_db_pass = get_env_nonempty("IKIGAI_DB_PASS");
    if (env_db_pass) {
        if (creds->db_pass) {
            talloc_free(creds->db_pass);
        }
        creds->db_pass = talloc_strdup(creds, env_db_pass);
        if (creds->db_pass == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    }

    *out_creds = creds;
    return OK(creds);
}

const char *ik_credentials_get(const ik_credentials_t *creds, const char *env_var_name)
{
    assert(creds != NULL); // LCOV_EXCL_BR_LINE
    assert(env_var_name != NULL); // LCOV_EXCL_BR_LINE

    if (strcmp(env_var_name, "OPENAI_API_KEY") == 0) {
        return creds->openai_api_key;
    } else if (strcmp(env_var_name, "ANTHROPIC_API_KEY") == 0) {
        return creds->anthropic_api_key;
    } else if (strcmp(env_var_name, "GOOGLE_API_KEY") == 0) {
        return creds->google_api_key;
    } else if (strcmp(env_var_name, "BRAVE_API_KEY") == 0) {
        return creds->brave_api_key;
    } else if (strcmp(env_var_name, "GOOGLE_SEARCH_API_KEY") == 0) {
        return creds->google_search_api_key;
    } else if (strcmp(env_var_name, "GOOGLE_SEARCH_ENGINE_ID") == 0) {
        return creds->google_search_engine_id;
    } else if (strcmp(env_var_name, "NTFY_API_KEY") == 0) {
        return creds->ntfy_api_key;
    } else if (strcmp(env_var_name, "NTFY_TOPIC") == 0) {
        return creds->ntfy_topic;
    } else if (strcmp(env_var_name, "IKIGAI_DB_PASS") == 0) {
        return creds->db_pass;
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
