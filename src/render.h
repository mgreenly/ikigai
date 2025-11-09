/**
 * @file render.h
 * @brief Rendering module using libvterm for terminal output
 *
 * Provides rendering functionality using libvterm to compose and display
 * text content on the terminal screen.
 */

#ifndef IKIGAI_RENDER_H
#define IKIGAI_RENDER_H

#include "error.h"
#include <inttypes.h>
#include <stddef.h>

/**
 * @brief Render context
 *
 * Represents the rendering state using a virtual terminal (libvterm).
 * Manages the composition of text content and blitting to the actual terminal.
 */
typedef struct ik_render_ctx_t {
    void *vterm;       /**< VTerm handle (opaque) */
    void *vscreen;     /**< VTermScreen handle (opaque) */
    int32_t rows;      /**< Screen height in rows */
    int32_t cols;      /**< Screen width in columns */
} ik_render_ctx_t;

/**
 * @brief Create a new render context
 *
 * @param parent Talloc parent context
 * @param rows Screen height in rows
 * @param cols Screen width in columns
 * @param render_out Pointer to receive allocated render context
 * @return RES_OK on success, RES_ERR on failure
 */
res_t ik_render_create(void *parent, int32_t rows, int32_t cols, ik_render_ctx_t **render_out);

/**
 * @brief Clear the render context
 *
 * Resets the virtual terminal to blank state.
 *
 * @param render Render context
 */
void ik_render_clear(ik_render_ctx_t *render);

/**
 * @brief Write text to the render context
 *
 * Writes UTF-8 text to the virtual terminal. The vterm handles
 * wrapping and cursor advancement automatically.
 *
 * @param render Render context
 * @param text UTF-8 text to write
 * @param len Length of text in bytes
 * @return RES_OK on success, RES_ERR on failure
 */
res_t ik_render_write_text(ik_render_ctx_t *render, const char *text, size_t len);

/**
 * @brief Set cursor position in the render context
 *
 * Sets the cursor position in the virtual terminal.
 *
 * @param render Render context
 * @param row Row position (0-based)
 * @param col Column position (0-based)
 * @return RES_OK on success, RES_ERR on failure
 */
res_t ik_render_set_cursor(ik_render_ctx_t *render, int32_t row, int32_t col);

/**
 * @brief Blit the render context to the terminal
 *
 * Renders the current state of the virtual terminal to the actual
 * terminal file descriptor in a single write operation.
 *
 * @param render Render context
 * @param tty_fd Terminal file descriptor
 * @return RES_OK on success, RES_ERR on failure
 */
res_t ik_render_blit(ik_render_ctx_t *render, int32_t tty_fd);

#endif /* IKIGAI_RENDER_H */
