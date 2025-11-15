#include "config.h"
#include "logger.h"
#include "panic.h"
#include "wrapper.h"
#include "json_allocator.h"
#include "vendor/yyjson/yyjson.h"
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <libgen.h>

// NOTE: yyjson is used for config parsing with talloc integration.
// All JSON memory is managed by talloc - no manual cleanup needed.
// See docs/jansson_to_yyjson_proposal.md for migration details.

// Expand tilde (~) to HOME directory
// Returns error if path starts with ~ but HOME is not set
res_t expand_tilde(TALLOC_CTX *ctx, const char *path)
{
    assert(ctx != NULL); // LCOV_EXCL_BR_LINE
    assert(path != NULL); // LCOV_EXCL_BR_LINE
    if (path[0] != '~') {
        char *result = ik_talloc_strdup_wrapper(ctx, path);
        if (result == NULL)PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        return OK(result);
    }

    const char *home = getenv("HOME");
    if (!home) {
        return ERR(ctx, INVALID_ARG, "HOME not set, cannot expand ~");
    }

    char *result = ik_talloc_asprintf_wrapper(ctx, "%s%s", home, path + 1);
    if (result == NULL)PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    return OK(result);
}

// Create default config file with default values
static res_t create_default_config(TALLOC_CTX *ctx, const char *path)
{
    assert(ctx != NULL); // LCOV_EXCL_BR_LINE
    assert(path != NULL); // LCOV_EXCL_BR_LINE
    // Extract directory from path
    char *path_copy = talloc_strdup(ctx, path);
    if (path_copy == NULL)PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    char *dir = dirname(path_copy);

    // Create directory if it doesn't exist
    struct stat st;
    if (ik_stat_wrapper(dir, &st) != 0) {
        if (ik_mkdir_wrapper(dir, 0755) != 0) {
            return ERR(ctx, IO, "Failed to create directory %s: %s", dir, strerror(errno));
        }
    }

    // Create default JSON config using yyjson with talloc allocator
    yyjson_alc alc = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&alc);
    if (doc == NULL)PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *root = yyjson_mut_obj(doc);
    if (root == NULL)PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    yyjson_mut_doc_set_root(doc, root);

    yyjson_mut_obj_add_str(doc, root, "openai_api_key", "YOUR_API_KEY_HERE");
    yyjson_mut_obj_add_str(doc, root, "listen_address", "127.0.0.1");
    yyjson_mut_obj_add_int(doc, root, "listen_port", 1984);

    // Write to file with pretty printing
    yyjson_write_err write_err;
    if (!ik_yyjson_mut_write_file_wrapper(path, doc, YYJSON_WRITE_PRETTY, &alc, &write_err)) {
        return ERR(ctx, IO, "Failed to write config file %s: %s", path, write_err.msg);
    }

    // Note: No manual cleanup needed! talloc frees everything when ctx is freed
    ik_log_info("Created default config at %s", path);

    return OK(NULL);
}

res_t ik_cfg_load(TALLOC_CTX *ctx, const char *path)
{
    assert(ctx != NULL); // LCOV_EXCL_BR_LINE
    assert(path != NULL); // LCOV_EXCL_BR_LINE
    // Expand tilde in path
    char *expanded_path = TRY(expand_tilde(ctx, path));

    // Check if file exists
    struct stat st;
    if (ik_stat_wrapper(expanded_path, &st) != 0) {
        // File doesn't exist, create default config
        res_t create_result = create_default_config(ctx, expanded_path);
        if (create_result.is_err) {
            return create_result;
        }
    }

    // Load and parse config file using yyjson with talloc allocator
    yyjson_alc alc = ik_make_talloc_allocator(ctx);
    yyjson_read_err read_err;
    yyjson_doc *doc = ik_yyjson_read_file_wrapper(expanded_path, 0, &alc, &read_err);
    if (!doc) {
        return ERR(ctx, PARSE, "Failed to parse JSON: %s", read_err.msg);
    }

    yyjson_val *root = ik_yyjson_doc_get_root_wrapper(doc);
    if (!root || !yyjson_is_obj(root)) {
        return ERR(ctx, PARSE, "JSON root is not an object");
    }

    // Allocate config structure
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    if (cfg == NULL)PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Extract fields
    yyjson_val *api_key = ik_yyjson_obj_get_wrapper(root, "openai_api_key");
    yyjson_val *address = ik_yyjson_obj_get_wrapper(root, "listen_address");
    yyjson_val *port = ik_yyjson_obj_get_wrapper(root, "listen_port");

    // Validate openai_api_key
    if (!api_key) {
        return ERR(ctx, PARSE, "Missing openai_api_key");
    }
    if (!yyjson_is_str(api_key)) {
        return ERR(ctx, PARSE, "Invalid type for openai_api_key");
    }

    // Validate listen_address
    if (!address) {
        return ERR(ctx, PARSE, "Missing listen_address");
    }
    if (!yyjson_is_str(address)) {
        return ERR(ctx, PARSE, "Invalid type for listen_address");
    }

    // Validate listen_port
    if (!port) {
        return ERR(ctx, PARSE, "Missing listen_port");
    }
    if (!yyjson_is_int(port)) {
        return ERR(ctx, PARSE, "Invalid type for listen_port");
    }

    // Extract port value and validate range
    int64_t port_raw = ik_yyjson_get_sint_wrapper(port);
    if (port_raw < 1024 || port_raw > 65535) {
        return ERR(ctx, OUT_OF_RANGE, "Port must be 1024-65535, got %lld", (long long)port_raw);
    }
    uint16_t port_value = (uint16_t)port_raw;

    // Copy values to config
    cfg->openai_api_key = talloc_strdup(cfg, ik_yyjson_get_str_wrapper(api_key));
    cfg->listen_address = talloc_strdup(cfg, ik_yyjson_get_str_wrapper(address));
    cfg->listen_port = port_value;

    // Note: No manual cleanup needed! talloc frees doc when ctx is freed
    return OK(cfg);
}
