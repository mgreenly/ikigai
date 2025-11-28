/**
 * @file cursor.h
 * @brief Internal cursor module for input buffer
 *
 * INTERNAL USE ONLY: This module is input buffer-private.
 * Precondition: All text passed to cursor functions must be valid UTF-8.
 * This is guaranteed by input buffer's insert operations.
 *
 * Tracks cursor position in both byte offset and grapheme offset.
 * Uses libutf8proc for proper grapheme cluster detection.
 */

#ifndef IKIGAI_INPUT_BUFFER_CURSOR_H
#define IKIGAI_INPUT_BUFFER_CURSOR_H

#include "error.h"
#include <stddef.h>

// Forward declaration for pretty-print functions
struct ik_format_buffer_t;

/**
 * @brief Cursor context
 *
 * Tracks cursor position in UTF-8 text using dual representation:
 * - byte_offset: Position in bytes (for text operations)
 * - grapheme_offset: Position in grapheme clusters (for display/movement)
 */
typedef struct ik_input_buffer_cursor_t {
    size_t byte_offset;      /**< Cursor position in bytes */
    size_t grapheme_offset;  /**< Cursor position in grapheme clusters */
} ik_input_buffer_cursor_t;

/**
 * @brief Create a new cursor
 *
 * @param parent Talloc parent context
 * @return Allocated cursor (never fails - OOM PANICs)
 */
ik_input_buffer_cursor_t *ik_input_buffer_cursor_create(void *parent);

/**
 * @brief Set cursor position by byte offset
 *
 * Sets the cursor to the given byte offset and recalculates the
 * grapheme offset by counting grapheme clusters from the start.
 *
 * Precondition: text must be valid UTF-8 (guaranteed by input buffer)
 *
 * @param cursor Cursor
 * @param text Text buffer
 * @param text_len Length of text in bytes
 * @param byte_offset Byte position to set cursor to
 */
void ik_input_buffer_cursor_set_position(ik_input_buffer_cursor_t *cursor, const char *text, size_t text_len, size_t byte_offset);

/**
 * @brief Move cursor left by one grapheme cluster
 *
 * Moves the cursor backward by one grapheme cluster.
 * Updates both byte_offset and grapheme_offset.
 * If cursor is at start (byte 0), this is a no-op.
 *
 * Precondition: text must be valid UTF-8 (guaranteed by input buffer)
 *
 * @param cursor Cursor
 * @param text Text buffer
 * @param text_len Length of text in bytes
 */
void ik_input_buffer_cursor_move_left(ik_input_buffer_cursor_t *cursor, const char *text, size_t text_len);

/**
 * @brief Move cursor right by one grapheme cluster
 *
 * Moves the cursor forward by one grapheme cluster.
 * Updates both byte_offset and grapheme_offset.
 * If cursor is at end, this is a no-op.
 *
 * Precondition: text must be valid UTF-8 (guaranteed by input buffer)
 *
 * @param cursor Cursor
 * @param text Text buffer
 * @param text_len Length of text in bytes
 */
void ik_input_buffer_cursor_move_right(ik_input_buffer_cursor_t *cursor, const char *text, size_t text_len);

/**
 * @brief Get cursor position
 *
 * @param cursor Cursor
 * @param byte_offset_out Pointer to receive byte offset
 * @param grapheme_offset_out Pointer to receive grapheme offset
 */
void ik_input_buffer_cursor_get_position(ik_input_buffer_cursor_t *cursor, size_t *byte_offset_out, size_t *grapheme_offset_out);

/**
 * @brief Pretty-print cursor structure
 *
 * @param cursor Cursor to pretty-print
 * @param buf Format buffer to append output to
 * @param indent Indentation level (number of spaces)
 */
void ik_pp_input_buffer_cursor(const ik_input_buffer_cursor_t *cursor, struct ik_format_buffer_t *buf, int32_t indent);

#endif /* IKIGAI_INPUT_BUFFER_CURSOR_H */
