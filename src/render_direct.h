#ifndef IKIGAI_RENDER_DIRECT_H
#define IKIGAI_RENDER_DIRECT_H

#include "error.h"
#include <inttypes.h>
#include <stddef.h>

typedef struct ik_render_direct_ctx_t {
    int32_t rows;      // Terminal height
    int32_t cols;      // Terminal width
    int32_t tty_fd;    // Terminal file descriptor
} ik_render_direct_ctx_t;

// Create render context
res_t ik_render_direct_create(void *parent, int32_t rows, int32_t cols, int32_t tty_fd,
                              ik_render_direct_ctx_t **ctx_out);

// Render workspace to terminal (text + cursor positioning)
res_t ik_render_direct_workspace(ik_render_direct_ctx_t *ctx,
                                 const char *text,
                                 size_t text_len,
                                 size_t cursor_byte_offset);

#endif /* IKIGAI_RENDER_DIRECT_H */
