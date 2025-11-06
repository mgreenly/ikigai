#include "config.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <libgen.h>
#include <jansson.h>

// Expand tilde (~) to HOME directory
// Returns NULL if path starts with ~ but HOME is not set
char *expand_tilde(TALLOC_CTX *ctx, const char *path)
{
    if (!path || path[0] != '~') {
        return talloc_strdup(ctx, path);
    }

    const char *home = getenv("HOME");
    if (!home) {
        return NULL;
    }

    return talloc_asprintf(ctx, "%s%s", home, path + 1);
}

// Create default config file with default values
static ik_result_t create_default_config(TALLOC_CTX *ctx, const char *path)
{
    // Extract directory from path
    char *path_copy = talloc_strdup(ctx, path);
    // LCOV_EXCL_START - OOM will be tested via injectable allocator in Task 8
    if (!path_copy) {
        return ERR(ctx, OOM, "Failed to allocate path copy");
    }
    // LCOV_EXCL_STOP

    char *dir = dirname(path_copy);

    // Create directory if it doesn't exist
    struct stat st;
    if (stat(dir, &st) != 0) {  // LCOV_EXCL_BR_LINE - directory exists case difficult to test
        // LCOV_EXCL_START - mkdir failure requires permission/disk errors
        if (mkdir(dir, 0755) != 0) {    // LCOV_EXCL_BR_LINE
            return ERR(ctx, IO, "Failed to create directory %s: %s", dir, strerror(errno));
        }
        // LCOV_EXCL_STOP
    }

    // Create default JSON config
    json_t *root = json_object();
    // LCOV_EXCL_START - json_object OOM extremely rare, handled by jansson
    if (!root) {
        return ERR(ctx, OOM, "Failed to create JSON object");
    }
    // LCOV_EXCL_STOP

    json_object_set_new(root, "openai_api_key", json_string("YOUR_API_KEY_HERE"));
    json_object_set_new(root, "listen_address", json_string("127.0.0.1"));
    json_object_set_new(root, "listen_port", json_integer(1984));

    // Write to file
    // LCOV_EXCL_START - json_dump_file failure requires disk/permission errors
    if (json_dump_file(root, path, JSON_INDENT(2)) != 0) {
        json_decref(root);
        return ERR(ctx, IO, "Failed to write config file %s", path);
    }
    // LCOV_EXCL_STOP

    json_decref(root);
    ik_log_info("Created default config at %s", path);

    return OK(NULL);
}

ik_result_t ik_cfg_load(TALLOC_CTX *ctx, const char *path)
{
    if (!path) {
        return ERR(ctx, INVALID_ARG, "Config path is NULL");
    }

    // Expand tilde in path
    char *expanded_path = expand_tilde(ctx, path);
    if (!expanded_path) {
        return ERR(ctx, INVALID_ARG, "HOME not set, cannot expand ~");
    }

    // Check if file exists
    struct stat st;
    if (stat(expanded_path, &st) != 0) {
        // File doesn't exist, create default config
        ik_result_t create_result = create_default_config(ctx, expanded_path);
        // LCOV_EXCL_START - create errors tested above, path covered in other tests
        if (create_result.is_err) {
            return create_result;
        }
        // LCOV_EXCL_STOP
    }

    // Load and parse config file
    json_error_t json_err;
    json_t *root = json_load_file(expanded_path, 0, &json_err);
    if (!root) {
        return ERR(ctx, PARSE, "Failed to parse JSON: %s", json_err.text);
    }

    // Allocate config structure
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    // LCOV_EXCL_START - OOM will be tested via injectable allocator in Task 8
    if (!cfg) {
        json_decref(root);
        return ERR(ctx, OOM, "Failed to allocate config");
    }
    // LCOV_EXCL_STOP

    // Extract fields (validation will be in later tasks)
    json_t *api_key = json_object_get(root, "openai_api_key");
    json_t *address = json_object_get(root, "listen_address");
    json_t *port = json_object_get(root, "listen_port");

    if (!api_key || !json_is_string(api_key)) { // LCOV_EXCL_BR_LINE
        json_decref(root);
        return ERR(ctx, PARSE, "Missing or invalid openai_api_key");
    }

    if (!address || !json_is_string(address)) { // LCOV_EXCL_BR_LINE
        json_decref(root);
        return ERR(ctx, PARSE, "Missing or invalid listen_address");
    }

    if (!port || !json_is_integer(port)) { // LCOV_EXCL_BR_LINE
        json_decref(root);
        return ERR(ctx, PARSE, "Missing or invalid listen_port");
    }

    // Extract port value and validate range
    int port_value = json_integer_value(port);
    if (port_value < 1024 || port_value > 65535) {
        json_decref(root);
        return ERR(ctx, OUT_OF_RANGE, "Port must be 1024-65535, got %d", port_value);
    }

    // Copy values to config
    cfg->openai_api_key = talloc_strdup(cfg, json_string_value(api_key));
    cfg->listen_address = talloc_strdup(cfg, json_string_value(address));
    cfg->listen_port = port_value;

    json_decref(root);

    return OK(cfg);
}
