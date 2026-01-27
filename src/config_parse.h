#ifndef IK_CONFIG_PARSE_H
#define IK_CONFIG_PARSE_H

#include "config.h"
#include "error.h"
#include "vendor/yyjson/yyjson.h"

#include <talloc.h>

// Parse configuration from JSON object into config structure
res_t ik_config_parse_json(TALLOC_CTX *ctx, yyjson_val *root, ik_config_t *cfg);

// Validation helpers
res_t ik_validate_required_string(TALLOC_CTX *ctx, yyjson_val *val, const char *field_name);
res_t ik_validate_required_number(TALLOC_CTX *ctx, yyjson_val *val, const char *field_name);
res_t ik_validate_optional_string(TALLOC_CTX *ctx, yyjson_val *val, const char *field_name);
res_t ik_validate_int64_range(TALLOC_CTX *ctx,
                               yyjson_val *val,
                               const char *field_name,
                               int64_t min,
                               int64_t max,
                               int64_t *out);
char *ik_copy_optional_db_string(ik_config_t *cfg,
                                  yyjson_val *val,
                                  const char *default_val);

#endif // IK_CONFIG_PARSE_H
