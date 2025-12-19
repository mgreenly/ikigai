#ifndef IKIGAI_RENDER_H
#define IKIGAI_RENDER_H

#include "error.h"
#include "scrollback.h"
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct ik_render_ctx_t {
    int32_t rows;      // Terminal height
    int32_t cols;      // Terminal width
    int32_t tty_fd;    // Terminal file descriptor
} ik_render_ctx_t;

// Create render context
res_t ik_render_create(TALLOC_CTX *ctx, int32_t rows, int32_t cols, int32_t tty_fd, ik_render_ctx_t **render_ctx_out);

// Render input buffer to terminal (text + cursor positioning)
res_t ik_render_input_buffer(ik_render_ctx_t *ctx, const char *text, size_t text_len, size_t cursor_byte_offset);

// Render scrollback lines to terminal (Phase 4)
res_t ik_render_scrollback(ik_render_ctx_t *ctx,
                           ik_scrollback_t *scrollback,
                           size_t start_line,
                           size_t line_count,
                           int32_t *rows_used_out);

// Render combined scrollback + input buffer in single atomic write (Phase 4 Task 4.4)
// render_separator and render_input_buffer control visibility (unified document model)
res_t ik_render_combined(ik_render_ctx_t *ctx,
                         ik_scrollback_t *scrollback,
                         size_t scrollback_start_line,
                         size_t scrollback_line_count,
                         const char *input_text,
                         size_t input_text_len,
                         size_t input_cursor_offset,
                         bool render_separator,
                         bool render_input_buffer);

#endif /* IKIGAI_RENDER_H */
