/**
 * @file render_cursor.h
 * @brief Cursor screen position calculation for rendering
 */

#ifndef IKIGAI_RENDER_CURSOR_H
#define IKIGAI_RENDER_CURSOR_H

#include "shared/error.h"
#include <inttypes.h>
#include <stddef.h>

// Cursor screen position (row, col) for rendering
typedef struct {
    int32_t screen_row;
    int32_t screen_col;
} cursor_screen_pos_t;

/**
 * Calculate cursor screen position from byte offset
 *
 * Internal function for testing - calculates where cursor should appear on screen
 * accounting for UTF-8 character widths and line wrapping
 *
 * @param ctx Context for error handling
 * @param text Text to analyze
 * @param text_len Length of text in bytes
 * @param cursor_byte_offset Cursor position in bytes
 * @param terminal_width Terminal width in columns
 * @param pos_out Output screen position
 * @return OK or error
 */
res_t calculate_cursor_screen_position(void *ctx,
                                       const char *text,
                                       size_t text_len,
                                       size_t cursor_byte_offset,
                                       int32_t terminal_width,
                                       cursor_screen_pos_t *pos_out);

#endif // IKIGAI_RENDER_CURSOR_H
