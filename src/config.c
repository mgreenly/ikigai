#include "config.h"
#include "json_allocator.h"
#include "logger.h"
#include "panic.h"
#include "vendor/yyjson/yyjson.h"
#include "wrapper.h"

#include <errno.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

res_t expand_tilde(TALLOC_CTX *ctx, const char *path)
{
    assert(ctx != NULL); // LCOV_EXCL_BR_LINE
    assert(path != NULL); // LCOV_EXCL_BR_LINE

    // check if path needs tilde expansion
    if (path[0] != '~') {
        char *result = talloc_strdup_(ctx, path);
        if (result == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        return OK(result);
    }

    // if the path starts with tilde but $HOME is not set error
    const char *home = getenv("HOME");
    if (!home) {
        return ERR(ctx, INVALID_ARG, "HOME not set, cannot expand ~");
    }

    // return $HOME expanded path
    char *result = talloc_asprintf_(ctx, "%s%s", home, path + 1);
    if (result == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    return OK(result);
}

// Create default config file with default values
static res_t create_default_config(TALLOC_CTX *ctx, const char *path)
{
    assert(ctx != NULL); // LCOV_EXCL_BR_LINE
    assert(path != NULL); // LCOV_EXCL_BR_LINE

    // Extract directory from path
    char *path_copy = talloc_strdup(ctx, path);
    if (path_copy == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Create directory if it doesn't exist
    char *dir = dirname(path_copy);
    struct stat st;
    if (posix_stat_(dir, &st) != 0) {
        if (posix_mkdir_(dir, 0755) != 0) {
            return ERR(ctx, IO, "Failed to create directory %s: %s", dir, strerror(errno));
        }
    }

    // create the json document with talloc allocator
    yyjson_alc allocator = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&allocator);
    if (doc == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // create a mutable root document
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    if (root == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    yyjson_mut_doc_set_root(doc, root);

    // add the objects
    yyjson_mut_obj_add_str(doc, root, "openai_api_key", "YOUR_API_KEY_HERE");
    yyjson_mut_obj_add_str(doc, root, "openai_model", "gpt-5-mini");
    yyjson_mut_obj_add_real(doc, root, "openai_temperature", 1.0);
    yyjson_mut_obj_add_int(doc, root, "openai_max_completion_tokens", 4096);
    yyjson_mut_obj_add_null(doc, root, "openai_system_message");
    yyjson_mut_obj_add_str(doc, root, "listen_address", "127.0.0.1");
    yyjson_mut_obj_add_int(doc, root, "listen_port", 1984);

    // Write to file with pretty printing
    yyjson_write_err write_err;
    if (!yyjson_mut_write_file_(path, doc, YYJSON_WRITE_PRETTY, &allocator, &write_err)) {
        return ERR(ctx, IO, "Failed to write config file %s: %s", path, write_err.msg);
    }

    // no cleanup required talloc frees everything when ctx is freed
    return OK(NULL);
}

res_t ik_cfg_load(TALLOC_CTX *ctx, const char *path)
{
    assert(ctx != NULL); // LCOV_EXCL_BR_LINE
    assert(path != NULL); // LCOV_EXCL_BR_LINE

    // expand tilde in path
    char *expanded_path = TRY(expand_tilde(ctx, path));

    // check if file exists
    struct stat st;
    if (posix_stat_(expanded_path, &st) != 0) {
        // File doesn't exist, create default config
        res_t create_result = create_default_config(ctx, expanded_path);
        if (create_result.is_err) {
            return create_result;
        }
    }

    // load and parse config file using yyjson with talloc allocator
    yyjson_alc allocator = ik_make_talloc_allocator(ctx);
    yyjson_read_err read_err;
    yyjson_doc *doc = yyjson_read_file_(expanded_path, 0, &allocator, &read_err);
    if (!doc) {
        return ERR(ctx, PARSE, "Failed to parse JSON: %s", read_err.msg);
    }

    yyjson_val *root = yyjson_doc_get_root_(doc);
    if (!root || !yyjson_is_obj(root)) {
        return ERR(ctx, PARSE, "JSON root is not an object");
    }

    // Allocate config structure
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    if (cfg == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Extract fields
    yyjson_val *api_key = yyjson_obj_get_(root, "openai_api_key");
    yyjson_val *model = yyjson_obj_get_(root, "openai_model");
    yyjson_val *temperature = yyjson_obj_get_(root, "openai_temperature");
    yyjson_val *max_completion_tokens = yyjson_obj_get_(root, "openai_max_completion_tokens");
    yyjson_val *system_message = yyjson_obj_get_(root, "openai_system_message");
    yyjson_val *address = yyjson_obj_get_(root, "listen_address");
    yyjson_val *port = yyjson_obj_get_(root, "listen_port");

    // validate openai_api_key
    if (!api_key) {
        return ERR(ctx, PARSE, "Missing openai_api_key");
    }
    if (!yyjson_is_str(api_key)) {
        return ERR(ctx, PARSE, "Invalid type for openai_api_key");
    }

    // validate openai_model
    if (!model) {
        return ERR(ctx, PARSE, "Missing openai_model");
    }
    if (!yyjson_is_str(model)) {
        return ERR(ctx, PARSE, "Invalid type for openai_model");
    }

    // validate openai_temperature
    if (!temperature) {
        return ERR(ctx, PARSE, "Missing openai_temperature");
    }
    if (!yyjson_is_num(temperature)) {
        return ERR(ctx, PARSE, "Invalid type for openai_temperature");
    }
    double temperature_value = yyjson_get_real(temperature);
    if (temperature_value < 0.0 || temperature_value > 2.0) {
        return ERR(ctx, OUT_OF_RANGE, "Temperature must be 0.0-2.0, got %f", temperature_value);
    }

    // validate openai_max_completion_tokens
    if (!max_completion_tokens) {
        return ERR(ctx, PARSE, "Missing openai_max_completion_tokens");
    }
    if (!yyjson_is_int(max_completion_tokens)) {
        return ERR(ctx, PARSE, "Invalid type for openai_max_completion_tokens");
    }
    int64_t max_completion_tokens_value = yyjson_get_sint_(max_completion_tokens);
    if (max_completion_tokens_value < 1 || max_completion_tokens_value > 128000) {
        return ERR(ctx,
                   OUT_OF_RANGE,
                   "Max completion tokens must be 1-128000, got %lld",
                   (long long)max_completion_tokens_value);
    }

    // validate openai_system_message (optional)
    if (system_message && !yyjson_is_null(system_message) && !yyjson_is_str(system_message)) {
        return ERR(ctx, PARSE, "Invalid type for openai_system_message");
    }

    // validate listen_address
    if (!address) {
        return ERR(ctx, PARSE, "Missing listen_address");
    }
    if (!yyjson_is_str(address)) {
        return ERR(ctx, PARSE, "Invalid type for listen_address");
    }

    // validate listen_port
    if (!port) {
        return ERR(ctx, PARSE, "Missing listen_port");
    }
    if (!yyjson_is_int(port)) {
        return ERR(ctx, PARSE, "Invalid type for listen_port");
    }

    // extract port value and validate range
    int64_t port_raw = yyjson_get_sint_(port);
    if (port_raw < 1024 || port_raw > 65535) {
        return ERR(ctx, OUT_OF_RANGE, "Port must be 1024-65535, got %lld", (long long)port_raw);
    }
    uint16_t port_value = (uint16_t)port_raw;

    // copy values to config
    cfg->openai_api_key = talloc_strdup(cfg, yyjson_get_str_(api_key));
    cfg->openai_model = talloc_strdup(cfg, yyjson_get_str_(model));
    cfg->openai_temperature = temperature_value;
    cfg->openai_max_completion_tokens = (int32_t)max_completion_tokens_value;
    if (system_message && !yyjson_is_null(system_message)) {
        cfg->openai_system_message = talloc_strdup(cfg, yyjson_get_str_(system_message));
    } else {
        cfg->openai_system_message = NULL;
    }
    cfg->listen_address = talloc_strdup(cfg, yyjson_get_str_(address));
    cfg->listen_port = port_value;

    // no cleanup required talloc frees everything when ctx is freed
    return OK(cfg);
}
