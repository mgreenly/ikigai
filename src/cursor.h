/**
 * @file cursor.h
 * @brief Cursor position tracking with grapheme cluster support
 *
 * Tracks cursor position in both byte offset and grapheme offset.
 * Uses libutf8proc for proper grapheme cluster detection.
 */

#ifndef IKIGAI_CURSOR_H
#define IKIGAI_CURSOR_H

#include "error.h"
#include <stddef.h>

/**
 * @brief Cursor context
 *
 * Tracks cursor position in UTF-8 text using dual representation:
 * - byte_offset: Position in bytes (for text operations)
 * - grapheme_offset: Position in grapheme clusters (for display/movement)
 */
typedef struct ik_cursor_t {
    size_t byte_offset;      /**< Cursor position in bytes */
    size_t grapheme_offset;  /**< Cursor position in grapheme clusters */
} ik_cursor_t;

/**
 * @brief Create a new cursor
 *
 * @param parent Talloc parent context
 * @param cursor_out Pointer to receive allocated cursor
 * @return RES_OK on success, RES_ERR on failure
 */
res_t ik_cursor_create(void *parent, ik_cursor_t **cursor_out);

/**
 * @brief Set cursor position by byte offset
 *
 * Sets the cursor to the given byte offset and recalculates the
 * grapheme offset by counting grapheme clusters from the start.
 *
 * @param cursor Cursor
 * @param text Text buffer
 * @param text_len Length of text in bytes
 * @param byte_offset Byte position to set cursor to
 * @return RES_OK on success, RES_ERR on failure
 */
res_t ik_cursor_set_position(ik_cursor_t *cursor, const char *text, size_t text_len, size_t byte_offset);

/**
 * @brief Move cursor left by one grapheme cluster
 *
 * Moves the cursor backward by one grapheme cluster.
 * Updates both byte_offset and grapheme_offset.
 * If cursor is at start (byte 0), this is a no-op.
 *
 * @param cursor Cursor
 * @param text Text buffer
 * @param text_len Length of text in bytes
 * @return RES_OK on success, RES_ERR on failure
 */
res_t ik_cursor_move_left(ik_cursor_t *cursor, const char *text, size_t text_len);

/**
 * @brief Move cursor right by one grapheme cluster
 *
 * Moves the cursor forward by one grapheme cluster.
 * Updates both byte_offset and grapheme_offset.
 * If cursor is at end, this is a no-op.
 *
 * @param cursor Cursor
 * @param text Text buffer
 * @param text_len Length of text in bytes
 * @return RES_OK on success, RES_ERR on failure
 */
res_t ik_cursor_move_right(ik_cursor_t *cursor, const char *text, size_t text_len);

/**
 * @brief Get cursor position
 *
 * @param cursor Cursor
 * @param byte_offset_out Pointer to receive byte offset
 * @param grapheme_offset_out Pointer to receive grapheme offset
 * @return RES_OK on success, RES_ERR on failure
 */
res_t ik_cursor_get_position(ik_cursor_t *cursor, size_t *byte_offset_out, size_t *grapheme_offset_out);

#endif /* IKIGAI_CURSOR_H */
