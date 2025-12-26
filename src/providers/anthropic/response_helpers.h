#pragma once

#include "error.h"
#include "providers/provider_types.h"
#include "vendor/yyjson/yyjson.h"

#include <talloc.h>

// Parse content block array from JSON
res_t ik_anthropic_parse_content_blocks(TALLOC_CTX *ctx, yyjson_val *content_arr,
                                         ik_content_block_t **out_blocks, size_t *out_count);

// Parse usage statistics from JSON
void ik_anthropic_parse_usage(yyjson_val *usage_obj, ik_usage_t *out_usage);
