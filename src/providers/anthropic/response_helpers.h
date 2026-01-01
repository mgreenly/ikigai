#pragma once

#include "error.h"
#include "providers/provider_types.h"
#include "vendor/yyjson/yyjson.h"

#include <talloc.h>

// Parse usage statistics from JSON
void ik_anthropic_parse_usage(yyjson_val *usage_obj, ik_usage_t *out_usage);
