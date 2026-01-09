#ifndef IK_CONFIG_H
#define IK_CONFIG_H

#include <inttypes.h>
#include <talloc.h>
#include "error.h"

typedef struct {
    char *openai_model;
    double openai_temperature;
    int32_t openai_max_completion_tokens;
    char *openai_system_message;
    char *listen_address;
    uint16_t listen_port;
    char *db_connection_string;
    int32_t max_tool_turns;
    int64_t max_output_size;
    int32_t history_size;
    char *default_provider;  // NEW: "anthropic", "openai", "google"
} ik_config_t;

res_t ik_config_load(TALLOC_CTX *ctx, const char *path, ik_config_t **out);

// Get default provider name with env var override support
const char *ik_config_get_default_provider(ik_config_t *config);

// internal helper function (exposed for testing)
res_t ik_cfg_expand_tilde(TALLOC_CTX *ctx, const char *path);

#endif // IK_CONFIG_H
