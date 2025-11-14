#ifndef IKIGAI_RENDER_H
#define IKIGAI_RENDER_H

#include "error.h"
#include "scrollback.h"
#include <inttypes.h>
#include <stddef.h>

typedef struct ik_render_ctx_t {
    int32_t rows;      // Terminal height
    int32_t cols;      // Terminal width
    int32_t tty_fd;    // Terminal file descriptor
} ik_render_ctx_t;

// Create render context
res_t ik_render_create(void *parent, int32_t rows, int32_t cols, int32_t tty_fd, ik_render_ctx_t **ctx_out);

// Render workspace to terminal (text + cursor positioning)
res_t ik_render_workspace(ik_render_ctx_t *ctx, const char *text, size_t text_len, size_t cursor_byte_offset);

// Render scrollback lines to terminal (Phase 4)
res_t ik_render_scrollback(ik_render_ctx_t *ctx,
                           ik_scrollback_t *scrollback,
                           size_t start_line,
                           size_t line_count,
                           int32_t *rows_used_out);

#endif /* IKIGAI_RENDER_H */
