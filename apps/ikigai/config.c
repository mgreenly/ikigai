#include "apps/ikigai/config.h"

#include "apps/ikigai/config_defaults.h"
#include "apps/ikigai/config_env.h"
#include "apps/ikigai/config_parse.h"
#include "shared/json_allocator.h"
#include "shared/logger.h"
#include "shared/panic.h"
#include "apps/ikigai/paths.h"
#include "vendor/yyjson/yyjson.h"
#include "shared/wrapper.h"

#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>


#include "shared/poison.h"
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

        if (cfg->openai_model == NULL || cfg->listen_address == NULL || cfg->db_host == NULL || // LCOV_EXCL_BR_LINE
            cfg->db_name == NULL || cfg->db_user == NULL) { // LCOV_EXCL_BR_LINE
            PANIC("Out of memory"); // LCOV_EXCL_LINE
        }

        // Apply environment variable overrides for database config
        ik_config_apply_env_overrides(cfg);

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
    res_t result = ik_config_parse_json(ctx, root, cfg);
    if (is_err(&result)) {
        return result;
    }

    // Apply environment variable overrides for database config
    ik_config_apply_env_overrides(cfg);

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
