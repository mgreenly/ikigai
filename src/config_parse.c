#include "config_parse.h"

#include "config_defaults.h"
#include "panic.h"
#include "wrapper.h"

#include <inttypes.h>
#include <limits.h>
#include <talloc.h>


#include "poison.h"
res_t ik_validate_required_string(TALLOC_CTX *ctx, yyjson_val *val, const char *field_name)
{
    if (!val) {
        return ERR(ctx, PARSE, "Missing %s", field_name);
    }
    if (!yyjson_is_str(val)) {
        return ERR(ctx, PARSE, "Invalid type for %s", field_name);
    }
    return OK(NULL);
}

res_t ik_validate_required_number(TALLOC_CTX *ctx, yyjson_val *val, const char *field_name)
{
    if (!val) {
        return ERR(ctx, PARSE, "Missing %s", field_name);
    }
    if (!yyjson_is_num(val)) {
        return ERR(ctx, PARSE, "Invalid type for %s", field_name);
    }
    return OK(NULL);
}

res_t ik_validate_optional_string(TALLOC_CTX *ctx, yyjson_val *val, const char *field_name)
{
    if (val && !yyjson_is_null(val) && !yyjson_is_str(val)) {
        return ERR(ctx, PARSE, "Invalid type for %s", field_name);
    }
    return OK(NULL);
}

res_t ik_validate_int64_range(TALLOC_CTX *ctx,
                               yyjson_val *val,
                               const char *field_name,
                               int64_t min,
                               int64_t max,
                               int64_t *out)
{
    if (!val) {
        return ERR(ctx, PARSE, "Missing %s", field_name);
    }
    if (!yyjson_is_int(val)) {
        return ERR(ctx, PARSE, "Invalid type for %s", field_name);
    }
    int64_t value = yyjson_get_sint_(val);
    if (value < min || value > max) {
        return ERR(ctx,
                   OUT_OF_RANGE,
                   "%s must be %lld-%lld, got %lld",
                   field_name,
                   (long long)min,
                   (long long)max,
                   (long long)value);
    }
    *out = value;
    return OK(NULL);
}

char *ik_copy_optional_db_string(ik_config_t *cfg,
                                  yyjson_val *val,
                                  const char *default_val)
{
    if (val && !yyjson_is_null(val)) {
        const char *str = yyjson_get_str_(val);
        if (str && str[0] != '\0') {
            char *result = talloc_strdup(cfg, str);
            if (!result) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
            return result;
        }
    }
    char *result = talloc_strdup(cfg, default_val);
    if (!result) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    return result;
}

res_t ik_config_parse_json(TALLOC_CTX *ctx, yyjson_val *root, ik_config_t *cfg)
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
    CHECK(ik_validate_required_string(ctx, model, "openai_model"));

    // validate openai_temperature
    CHECK(ik_validate_required_number(ctx, temperature, "openai_temperature"));
    double temperature_value = yyjson_get_real(temperature);
    if (temperature_value < 0.0 || temperature_value > 2.0) {
        return ERR(ctx, OUT_OF_RANGE, "Temperature must be 0.0-2.0, got %f", temperature_value);
    }

    // validate openai_max_completion_tokens
    int64_t max_completion_tokens_value;
    CHECK(ik_validate_int64_range(ctx,
                                   max_completion_tokens,
                                   "openai_max_completion_tokens",
                                   1,
                                   128000,
                                   &max_completion_tokens_value));

    // validate openai_system_message (optional)
    CHECK(ik_validate_optional_string(ctx, system_message, "openai_system_message"));

    // validate listen_address
    CHECK(ik_validate_required_string(ctx, address, "listen_address"));

    // validate listen_port
    int64_t port_raw;
    CHECK(ik_validate_int64_range(ctx, port, "listen_port", 1024, 65535, &port_raw));
    uint16_t port_value = (uint16_t)port_raw;

    // validate db_host (optional)
    CHECK(ik_validate_optional_string(ctx, db_host, "db_host"));

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
    CHECK(ik_validate_optional_string(ctx, db_name, "db_name"));

    // validate db_user (optional)
    CHECK(ik_validate_optional_string(ctx, db_user, "db_user"));

    // validate max_tool_turns
    int64_t max_tool_turns_value;
    CHECK(ik_validate_int64_range(ctx,
                                   max_tool_turns,
                                   "max_tool_turns",
                                   1,
                                   1000,
                                   &max_tool_turns_value));

    // validate max_output_size
    int64_t max_output_size_value;
    CHECK(ik_validate_int64_range(ctx,
                                   max_output_size,
                                   "max_output_size",
                                   1024,
                                   104857600,
                                   &max_output_size_value));

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
    cfg->db_host = ik_copy_optional_db_string(cfg, db_host, IK_DEFAULT_DB_HOST);
    cfg->db_port = db_port_value;
    cfg->db_name = ik_copy_optional_db_string(cfg, db_name, IK_DEFAULT_DB_NAME);
    cfg->db_user = ik_copy_optional_db_string(cfg, db_user, IK_DEFAULT_DB_USER);

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
