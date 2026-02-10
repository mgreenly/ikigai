#ifndef IK_CONFIG_H
#define IK_CONFIG_H

#include <inttypes.h>
#include <talloc.h>
#include "shared/error.h"
#include "apps/ikigai/paths.h"

typedef struct {
    char *openai_model;
    double openai_temperature;
    int32_t openai_max_completion_tokens;
    char *openai_system_message;
    char *listen_address;
    uint16_t listen_port;
    char *db_host;
    int32_t db_port;
    char *db_name;
    char *db_user;
    int32_t max_tool_turns;
    int64_t max_output_size;
    int32_t history_size;
    char *default_provider;  // NEW: "anthropic", "openai", "google"
} ik_config_t;

res_t ik_config_load(TALLOC_CTX *ctx, ik_paths_t *paths, ik_config_t **out);

#endif // IK_CONFIG_H
