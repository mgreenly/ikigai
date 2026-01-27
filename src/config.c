#include "config.h"
#include "config_defaults.h"
#include "json_allocator.h"
#include "logger.h"
#include "panic.h"
#include "paths.h"
#include "vendor/yyjson/yyjson.h"
#include "wrapper.h"

#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

static res_t parse_config_from_json(TALLOC_CTX *ctx, yyjson_val *root, ik_config_t *cfg)
{
    // Extract fields
    yyjson_val *model = yyjson_obj_get_(root, "openai_model");
    yyjson_val *temperature = yyjson_obj_get_(root, "openai_temperature");
    yyjson_val *max_completion_tokens = yyjson_obj_get_(root, "openai_max_completion_tokens");
    yyjson_val *system_message = yyjson_obj_get_(root, "openai_system_message");
    yyjson_val *address = yyjson_obj_get_(root, "listen_address");
    yyjson_val *port = yyjson_obj_get_(root, "listen_port");
    yyjson_val *db_host = yyjson_obj_get_(root, "db_host");
    yyjson_val *db_port = yyjson_obj_get_(root, "db_port");
    yyjson_val *db_name = yyjson_obj_get_(root, "db_name");
    yyjson_val *db_user = yyjson_obj_get_(root, "db_user");
    yyjson_val *max_tool_turns = yyjson_obj_get_(root, "max_tool_turns");
    yyjson_val *max_output_size = yyjson_obj_get_(root, "max_output_size");
    yyjson_val *history_size = yyjson_obj_get_(root, "history_size");
    yyjson_val *default_provider = yyjson_obj_get_(root, "default_provider");

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

    // validate db_host (optional)
    if (db_host && !yyjson_is_null(db_host) && !yyjson_is_str(db_host)) {
        return ERR(ctx, PARSE, "Invalid type for db_host");
    }

    // validate db_port (optional)
    int32_t db_port_value = IK_DEFAULT_DB_PORT;
    if (db_port && !yyjson_is_null(db_port)) {
        if (!yyjson_is_int(db_port)) {
            return ERR(ctx, PARSE, "Invalid type for db_port");
        }
        int64_t db_port_raw = yyjson_get_sint_(db_port);
        if (db_port_raw < 1 || db_port_raw > 65535) {
            return ERR(ctx, OUT_OF_RANGE, "db_port must be 1-65535, got %lld", (long long)db_port_raw);
        }
        db_port_value = (int32_t)db_port_raw;
    }

    // validate db_name (optional)
    if (db_name && !yyjson_is_null(db_name) && !yyjson_is_str(db_name)) {
        return ERR(ctx, PARSE, "Invalid type for db_name");
    }

    // validate db_user (optional)
    if (db_user && !yyjson_is_null(db_user) && !yyjson_is_str(db_user)) {
        return ERR(ctx, PARSE, "Invalid type for db_user");
    }

    // validate max_tool_turns
    if (!max_tool_turns) {
        return ERR(ctx, PARSE, "Missing max_tool_turns");
    }
    if (!yyjson_is_int(max_tool_turns)) {
        return ERR(ctx, PARSE, "Invalid type for max_tool_turns");
    }
    int64_t max_tool_turns_value = yyjson_get_sint_(max_tool_turns);
    if (max_tool_turns_value < 1 || max_tool_turns_value > 1000) {
        return ERR(ctx,
                   OUT_OF_RANGE,
                   "max_tool_turns must be 1-1000, got %lld",
                   (long long)max_tool_turns_value);
    }

    // validate max_output_size
    if (!max_output_size) {
        return ERR(ctx, PARSE, "Missing max_output_size");
    }
    if (!yyjson_is_int(max_output_size)) {
        return ERR(ctx, PARSE, "Invalid type for max_output_size");
    }
    int64_t max_output_size_value = yyjson_get_sint_(max_output_size);
    if (max_output_size_value < 1024 || max_output_size_value > 104857600) {
        return ERR(ctx,
                   OUT_OF_RANGE,
                   "max_output_size must be 1024-104857600, got %lld",
                   (long long)max_output_size_value);
    }

    // validate history_size (optional - defaults from config_defaults.h)
    int32_t history_size_value = IK_DEFAULT_HISTORY_SIZE;
    if (history_size) {
        if (!yyjson_is_int(history_size)) {
            return ERR(ctx, PARSE, "Invalid type for history_size");
        }
        int64_t history_size_raw = yyjson_get_sint_(history_size);
        if (history_size_raw < 1 || history_size_raw > INT32_MAX) {
            return ERR(ctx,
                       OUT_OF_RANGE,
                       "history_size must be 1-%d, got %lld",
                       INT32_MAX,
                       (long long)history_size_raw);
        }
        history_size_value = (int32_t)history_size_raw;
    }

    // copy values to config
    cfg->openai_model = talloc_strdup(cfg, yyjson_get_str_(model));
    cfg->openai_temperature = temperature_value;
    cfg->openai_max_completion_tokens = (int32_t)max_completion_tokens_value;
    // Only override system message from config if not already set from file
    if (!cfg->openai_system_message) {
        if (system_message && !yyjson_is_null(system_message)) {
            cfg->openai_system_message = talloc_strdup(cfg, yyjson_get_str_(system_message));
        } else {
            cfg->openai_system_message = talloc_strdup(cfg, IK_DEFAULT_OPENAI_SYSTEM_MESSAGE);
            if (!cfg->openai_system_message) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        }
    }
    cfg->listen_address = talloc_strdup(cfg, yyjson_get_str_(address));
    cfg->listen_port = port_value;

    // Copy database config fields (all optional)
    if (db_host && !yyjson_is_null(db_host)) {
        const char *db_host_str = yyjson_get_str_(db_host);
        if (db_host_str && db_host_str[0] != '\0') {
            cfg->db_host = talloc_strdup(cfg, db_host_str);
            if (!cfg->db_host) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        } else {
            cfg->db_host = talloc_strdup(cfg, IK_DEFAULT_DB_HOST);
            if (!cfg->db_host) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        }
    } else {
        cfg->db_host = talloc_strdup(cfg, IK_DEFAULT_DB_HOST);
        if (!cfg->db_host) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    }

    cfg->db_port = db_port_value;

    if (db_name && !yyjson_is_null(db_name)) {
        const char *db_name_str = yyjson_get_str_(db_name);
        if (db_name_str && db_name_str[0] != '\0') {
            cfg->db_name = talloc_strdup(cfg, db_name_str);
            if (!cfg->db_name) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        } else {
            cfg->db_name = talloc_strdup(cfg, IK_DEFAULT_DB_NAME);
            if (!cfg->db_name) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        }
    } else {
        cfg->db_name = talloc_strdup(cfg, IK_DEFAULT_DB_NAME);
        if (!cfg->db_name) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    }

    if (db_user && !yyjson_is_null(db_user)) {
        const char *db_user_str = yyjson_get_str_(db_user);
        if (db_user_str && db_user_str[0] != '\0') {
            cfg->db_user = talloc_strdup(cfg, db_user_str);
            if (!cfg->db_user) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        } else {
            cfg->db_user = talloc_strdup(cfg, IK_DEFAULT_DB_USER);
            if (!cfg->db_user) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        }
    } else {
        cfg->db_user = talloc_strdup(cfg, IK_DEFAULT_DB_USER);
        if (!cfg->db_user) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    }

    cfg->max_tool_turns = (int32_t)max_tool_turns_value;
    cfg->max_output_size = max_output_size_value;
    cfg->history_size = history_size_value;

    // parse default_provider (optional)
    if (default_provider) {
        if (!yyjson_is_str(default_provider)) {
            return ERR(ctx, PARSE, "Invalid type for default_provider");
        }
        const char *provider_str = yyjson_get_str_(default_provider);
        // Empty string treated as unset
        if (provider_str && provider_str[0] != '\0') {
            cfg->default_provider = talloc_strdup(cfg, provider_str);
        } else {
            cfg->default_provider = NULL;
        }
    } else {
        cfg->default_provider = NULL;
    }

    return OK(NULL);
}

res_t ik_config_load(TALLOC_CTX *ctx, ik_paths_t *paths, ik_config_t **out)
{
    assert(ctx != NULL); // LCOV_EXCL_BR_LINE
    assert(paths != NULL); // LCOV_EXCL_BR_LINE
    assert(out != NULL); // LCOV_EXCL_BR_LINE

    // Get config directory from paths module
    const char *config_dir = ik_paths_get_config_dir(paths);

    // Build config file path
    char *config_path = talloc_asprintf(ctx, "%s/config.json", config_dir);
    if (!config_path) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Allocate config structure
    ik_config_t *cfg = talloc_zero(ctx, ik_config_t);
    if (cfg == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Try loading system prompt from file first (priority: file > config > default)
    const char *data_dir = ik_paths_get_data_dir(paths);
    char *system_prompt_path = talloc_asprintf(ctx, "%s/prompts/system.md", data_dir);
    if (!system_prompt_path) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    struct stat system_prompt_st;
    if (posix_stat_(system_prompt_path, &system_prompt_st) == 0) {
        // File exists - validate size
        if (system_prompt_st.st_size == 0) {
            return ERR(ctx, IO, "System prompt file is empty: %s", system_prompt_path);
        }
        if (system_prompt_st.st_size > 1024) {
            return ERR(ctx, IO, "System prompt file exceeds 1KB limit: %s (%lld bytes)",
                      system_prompt_path, (long long)system_prompt_st.st_size);
        }

        // Read file contents
        FILE *fp = fopen_(system_prompt_path, "r");
        if (!fp) {
            return ERR(ctx, IO, "Failed to open system prompt file: %s (%s)",
                      system_prompt_path, strerror(errno));
        }

        char *buffer = talloc_size(cfg, (size_t)system_prompt_st.st_size + 1);
        if (!buffer) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

        size_t bytes_read = fread_(buffer, 1, (size_t)system_prompt_st.st_size, fp);
        fclose_(fp);

        if (bytes_read != (size_t)system_prompt_st.st_size) {
            return ERR(ctx, IO, "Failed to read system prompt file: %s", system_prompt_path);
        }

        buffer[bytes_read] = '\0';
        cfg->openai_system_message = buffer;
    }

    // check if file exists
    struct stat st;
    if (posix_stat_(config_path, &st) != 0) {
        // File doesn't exist, use compiled defaults
        cfg->openai_model = talloc_strdup(cfg, IK_DEFAULT_OPENAI_MODEL);
        cfg->openai_temperature = IK_DEFAULT_OPENAI_TEMPERATURE;
        cfg->openai_max_completion_tokens = IK_DEFAULT_OPENAI_MAX_COMPLETION_TOKENS;
        // Use default system message if not loaded from file
        if (!cfg->openai_system_message) {
            cfg->openai_system_message = talloc_strdup(cfg, IK_DEFAULT_OPENAI_SYSTEM_MESSAGE);
            if (!cfg->openai_system_message) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        }
        cfg->listen_address = talloc_strdup(cfg, IK_DEFAULT_LISTEN_ADDRESS);
        cfg->listen_port = IK_DEFAULT_LISTEN_PORT;
        cfg->db_host = talloc_strdup(cfg, IK_DEFAULT_DB_HOST);
        cfg->db_port = IK_DEFAULT_DB_PORT;
        cfg->db_name = talloc_strdup(cfg, IK_DEFAULT_DB_NAME);
        cfg->db_user = talloc_strdup(cfg, IK_DEFAULT_DB_USER);
        cfg->max_tool_turns = IK_DEFAULT_MAX_TOOL_TURNS;
        cfg->max_output_size = IK_DEFAULT_MAX_OUTPUT_SIZE;
        cfg->history_size = IK_DEFAULT_HISTORY_SIZE;
        cfg->default_provider = NULL;

        if (cfg->openai_model == NULL || cfg->listen_address == NULL || cfg->db_host == NULL ||
            cfg->db_name == NULL || cfg->db_user == NULL) {
            PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        }

        // Apply environment variable overrides for database config
        const char *env_db_host = getenv("IKIGAI_DB_HOST");
        if (env_db_host && env_db_host[0] != '\0') {
            talloc_free(cfg->db_host);
            cfg->db_host = talloc_strdup(cfg, env_db_host);
            if (!cfg->db_host) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        }

        const char *env_db_port = getenv("IKIGAI_DB_PORT");
        if (env_db_port && env_db_port[0] != '\0') {
            char *endptr = NULL;
            long port_val = strtol(env_db_port, &endptr, 10);
            if (endptr != env_db_port && *endptr == '\0' && port_val >= 1 && port_val <= 65535) {
                cfg->db_port = (int32_t)port_val;
            }
        }

        const char *env_db_name = getenv("IKIGAI_DB_NAME");
        if (env_db_name && env_db_name[0] != '\0') {
            talloc_free(cfg->db_name);
            cfg->db_name = talloc_strdup(cfg, env_db_name);
            if (!cfg->db_name) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        }

        const char *env_db_user = getenv("IKIGAI_DB_USER");
        if (env_db_user && env_db_user[0] != '\0') {
            talloc_free(cfg->db_user);
            cfg->db_user = talloc_strdup(cfg, env_db_user);
            if (!cfg->db_user) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        }

        *out = cfg;
        return OK(NULL);
    }

    // load and parse config file using yyjson with talloc allocator
    yyjson_alc allocator = ik_make_talloc_allocator(ctx);
    yyjson_read_err read_err;
    yyjson_doc *doc = yyjson_read_file_(config_path, 0, &allocator, &read_err);
    if (!doc) {
        return ERR(ctx, PARSE, "Failed to parse JSON: %s", read_err.msg);
    }

    yyjson_val *root = yyjson_doc_get_root_(doc);
    if (!root || !yyjson_is_obj(root)) {
        return ERR(ctx, PARSE, "JSON root is not an object");
    }

    // Parse and validate JSON into config structure
    res_t result = parse_config_from_json(ctx, root, cfg);
    if (is_err(&result)) {
        return result;
    }

    // Apply environment variable overrides for database config
    const char *env_db_host = getenv("IKIGAI_DB_HOST");
    if (env_db_host && env_db_host[0] != '\0') {
        talloc_free(cfg->db_host);
        cfg->db_host = talloc_strdup(cfg, env_db_host);
        if (!cfg->db_host) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    }

    const char *env_db_port = getenv("IKIGAI_DB_PORT");
    if (env_db_port && env_db_port[0] != '\0') {
        char *endptr = NULL;
        long port_val = strtol(env_db_port, &endptr, 10);
        if (endptr != env_db_port && *endptr == '\0' && port_val >= 1 && port_val <= 65535) {
            cfg->db_port = (int32_t)port_val;
        }
    }

    const char *env_db_name = getenv("IKIGAI_DB_NAME");
    if (env_db_name && env_db_name[0] != '\0') {
        talloc_free(cfg->db_name);
        cfg->db_name = talloc_strdup(cfg, env_db_name);
        if (!cfg->db_name) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    }

    const char *env_db_user = getenv("IKIGAI_DB_USER");
    if (env_db_user && env_db_user[0] != '\0') {
        talloc_free(cfg->db_user);
        cfg->db_user = talloc_strdup(cfg, env_db_user);
        if (!cfg->db_user) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    }

    *out = cfg;
    return OK(NULL);
}

const char *ik_config_get_default_provider(ik_config_t *config)
{
    assert(config != NULL); // LCOV_EXCL_BR_LINE

    // Check environment variable first
    const char *env_provider = getenv("IKIGAI_DEFAULT_PROVIDER");
    if (env_provider && env_provider[0] != '\0') {
        return env_provider;
    }

    // Use config file value
    if (config->default_provider && config->default_provider[0] != '\0') {
        return config->default_provider;
    }

    // Fall back to compiled default
    return IK_DEFAULT_PROVIDER;
}
