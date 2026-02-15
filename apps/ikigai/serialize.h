#pragma once

#include "shared/error.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <talloc.h>

// Serialize framebuffer byte array to JSON string
// Returns JSON string allocated on ctx
res_t ik_serialize_framebuffer(TALLOC_CTX *ctx,
                                const uint8_t *framebuffer,
                                size_t framebuffer_len,
                                int32_t rows,
                                int32_t cols,
                                int32_t cursor_row,
                                int32_t cursor_col,
                                bool cursor_visible);
