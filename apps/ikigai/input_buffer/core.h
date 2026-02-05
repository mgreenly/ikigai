/**
 * @file core.h
 * @brief Input buffer text storage for REPL input area
 *
 * Provides a text buffer for the input buffer (input area) of the REPL.
 * Uses ik_byte_array_t for UTF-8 text storage and tracks cursor position.
 */

#ifndef IKIGAI_INPUT_BUFFER_CORE_H
#define IKIGAI_INPUT_BUFFER_CORE_H

#include "apps/ikigai/byte_array.h"
#include "apps/ikigai/input_buffer/cursor.h"
#include "shared/error.h"

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Input buffer context
 *
 * Represents the editable text buffer in the REPL's input buffer.
 * Stores UTF-8 text and tracks the cursor position using grapheme-aware cursor.
 */
typedef struct ik_input_buffer_t {
    ik_byte_array_t *text;       /**< UTF-8 text buffer */
    ik_input_buffer_cursor_t *cursor;         /**< Cursor position (byte and grapheme offsets) */
    size_t cursor_byte_offset;   /**< Legacy byte offset - deprecated, use cursor instead */
    size_t target_column;        /**< Preserved column position for multi-line navigation (0 = use current) */
    size_t physical_lines;       /**< Cached number of physical lines (accounting for wrapping) */
    int32_t cached_width;        /**< Terminal width used for last layout calculation */
    int32_t layout_dirty;        /**< Flag: 1 = layout needs recalculation, 0 = cache valid */
} ik_input_buffer_t;

/**
 * @brief Create a new input buffer
 *
 * @param parent Talloc parent context
 * @return Pointer to new input buffer (never NULL - PANICs on OOM)
 */
ik_input_buffer_t *ik_input_buffer_create(void *parent);

/**
 * @brief Get the text buffer contents
 *
 * Returns a pointer to the internal text buffer and its length.
 * The returned pointer is valid until the next modification.
 *
 * @param input_bufferWorkspace
 * @param len_out Pointer to receive text length in bytes
 * @return Pointer to text buffer (may be NULL if empty)
 */
const char *ik_input_buffer_get_text(ik_input_buffer_t *input_buffer, size_t *len_out);

/**
 * @brief Clear the input buffer
 *
 * Removes all text and resets cursor to position 0.
 *
 * @param input_bufferWorkspace
 */
void ik_input_buffer_clear(ik_input_buffer_t *input_buffer);

/**
 * @brief Replace entire input buffer with new text
 *
 * Clears the existing buffer and sets it to the new text.
 * Resets cursor to position 0.
 *
 * @param input_buffer Workspace
 * @param text New text to set (UTF-8 encoded)
 * @param text_len Length of new text in bytes
 * @return RES_OK on success, RES_ERR on failure
 */
res_t ik_input_buffer_set_text(ik_input_buffer_t *input_buffer, const char *text, size_t text_len);

/**
 * @brief Insert a Unicode codepoint at the cursor position
 *
 * Encodes the codepoint to UTF-8 and inserts it at the current cursor position.
 * Advances the cursor by the number of bytes inserted.
 *
 * @param input_bufferWorkspace
 * @param codepoint Unicode codepoint to insert (U+0000 to U+10FFFF)
 * @return RES_OK on success, RES_ERR on failure
 */
res_t ik_input_buffer_insert_codepoint(ik_input_buffer_t *input_buffer, uint32_t codepoint);

/**
 * @brief Insert a newline at the cursor position
 *
 * Inserts a newline character ('\n') at the current cursor position.
 * Advances the cursor by 1 byte.
 *
 * @param input_bufferWorkspace
 * @return RES_OK on success, RES_ERR on failure
 */
res_t ik_input_buffer_insert_newline(ik_input_buffer_t *input_buffer);

/**
 * @brief Delete the character before the cursor (backspace)
 *
 * Deletes the previous UTF-8 character (grapheme cluster) before the cursor.
 * Moves the cursor backward by the number of bytes deleted.
 * If cursor is at position 0, this is a no-op.
 *
 * @param input_bufferWorkspace
 * @return RES_OK on success, RES_ERR on failure
 */
res_t ik_input_buffer_backspace(ik_input_buffer_t *input_buffer);

/**
 * @brief Delete the character after the cursor (delete key)
 *
 * Deletes the UTF-8 character at the cursor position.
 * The cursor position stays the same.
 * If cursor is at end of text, this is a no-op.
 *
 * @param input_bufferWorkspace
 * @return RES_OK on success, RES_ERR on failure
 */
res_t ik_input_buffer_delete(ik_input_buffer_t *input_buffer);

/**
 * @brief Move cursor left by one grapheme cluster
 *
 * Moves the cursor backward by one grapheme cluster.
 * If cursor is at start, this is a no-op.
 *
 * @param input_bufferWorkspace
 * @return RES_OK on success, RES_ERR on failure
 */
res_t ik_input_buffer_cursor_left(ik_input_buffer_t *input_buffer);

/**
 * @brief Move cursor right by one grapheme cluster
 *
 * Moves the cursor forward by one grapheme cluster.
 * If cursor is at end, this is a no-op.
 *
 * @param input_bufferWorkspace
 * @return RES_OK on success, RES_ERR on failure
 */
res_t ik_input_buffer_cursor_right(ik_input_buffer_t *input_buffer);

/**
 * @brief Get cursor position
 *
 * Returns the cursor position in both byte offset and grapheme offset.
 *
 * @param input_bufferWorkspace
 * @param byte_out Pointer to receive byte offset
 * @param grapheme_out Pointer to receive grapheme offset
 * @return RES_OK on success, RES_ERR on failure
 */
res_t ik_input_buffer_get_cursor_position(ik_input_buffer_t *input_buffer, size_t *byte_out, size_t *grapheme_out);

/**
 * @brief Move cursor up by one line
 *
 * Moves the cursor up to the previous line, attempting to preserve column position.
 * If cursor is on the first line, this is a no-op.
 *
 * @param input_bufferWorkspace
 * @return RES_OK on success, RES_ERR on failure
 */
res_t ik_input_buffer_cursor_up(ik_input_buffer_t *input_buffer);

/**
 * @brief Move cursor down by one line
 *
 * Moves the cursor down to the next line, attempting to preserve column position.
 * If cursor is on the last line, this is a no-op.
 *
 * @param input_bufferWorkspace
 * @return RES_OK on success, RES_ERR on failure
 */
res_t ik_input_buffer_cursor_down(ik_input_buffer_t *input_buffer);

/**
 * @brief Move cursor to the start of the current line (Ctrl+A)
 *
 * Moves the cursor to the beginning of the current line.
 * If cursor is already at the line start, this is a no-op.
 *
 * @param input_bufferWorkspace
 * @return RES_OK on success, RES_ERR on failure
 */
res_t ik_input_buffer_cursor_to_line_start(ik_input_buffer_t *input_buffer);

/**
 * @brief Move cursor to the end of the current line (Ctrl+E)
 *
 * Moves the cursor to the end of the current line (before the newline if present).
 * If cursor is already at the line end, this is a no-op.
 *
 * @param input_bufferWorkspace
 * @return RES_OK on success, RES_ERR on failure
 */
res_t ik_input_buffer_cursor_to_line_end(ik_input_buffer_t *input_buffer);

/**
 * @brief Kill (delete) text from cursor to end of current line (Ctrl+K)
 *
 * Deletes all text from the current cursor position to the end of the line.
 * Does not delete the newline character if present. Cursor position unchanged.
 * If cursor is already at line end, this is a no-op.
 *
 * @param input_bufferWorkspace
 * @return RES_OK on success, RES_ERR on failure
 */
res_t ik_input_buffer_kill_to_line_end(ik_input_buffer_t *input_buffer);

/**
 * @brief Kill (delete) the entire current line (Ctrl+U)
 *
 * Deletes the entire current line from start to end, including the newline.
 * Positions the cursor at the start of the next line (or at the position
 * where the line was deleted if it was the last line).
 * If the line is empty (just a newline), deletes the newline.
 *
 * @param input_bufferWorkspace
 * @return RES_OK on success, RES_ERR on failure
 */
res_t ik_input_buffer_kill_line(ik_input_buffer_t *input_buffer);

/**
 * @brief Delete word backward (Ctrl+W)
 *
 * Deletes the word before the cursor, stopping at whitespace, punctuation,
 * or newline boundaries. Skips trailing whitespace before deleting the word.
 * If cursor is at start, this is a no-op.
 *
 * @param input_bufferWorkspace
 * @return RES_OK on success, RES_ERR on failure
 */
res_t ik_input_buffer_delete_word_backward(ik_input_buffer_t *input_buffer);

/**
 * @brief Ensure input buffer layout is calculated for given terminal width
 *
 * Calculates the number of physical lines the input buffer occupies, accounting
 * for text wrapping at the terminal width. Uses cached value if terminal
 * width hasn't changed and layout is not dirty.
 *
 * @param input_bufferWorkspace
 * @param terminal_width Terminal width in columns
 */
void ik_input_buffer_ensure_layout(ik_input_buffer_t *input_buffer, int32_t terminal_width);

/**
 * @brief Invalidate the cached layout
 *
 * Marks the layout cache as dirty, forcing recalculation on next ensure_layout call.
 * Called automatically by text modification functions.
 *
 * @param input_bufferWorkspace
 */
void ik_input_buffer_invalidate_layout(ik_input_buffer_t *input_buffer);

/**
 * @brief Get the number of physical lines
 *
 * Returns the cached number of physical lines. If layout has not been
 * calculated yet, returns 0. Call ensure_layout first to get accurate value.
 *
 * @param input_bufferWorkspace
 * @return Number of physical lines (0 if not calculated)
 */
size_t ik_input_buffer_get_physical_lines(ik_input_buffer_t *input_buffer);

// Forward declaration
typedef struct ik_format_buffer_t ik_format_buffer_t;

/**
 * @brief Pretty-print input buffer internal state
 *
 * Outputs input buffer debug information including:
 * - Memory address
 * - Text buffer length and content
 * - Cursor position (byte and grapheme offsets)
 * - Target column for multi-line navigation
 *
 * Thread-safety: Input buffer is read-only (const).
 * The format buffer must be thread-local.
 *
 * @param input_buffer Input buffer to inspect (read-only)
 * @param buf Format buffer to append output to
 * @param indent Indentation level (number of spaces)
 */
void ik_pp_input_buffer(const ik_input_buffer_t *input_buffer, ik_format_buffer_t *buf, int32_t indent);

#endif /* IKIGAI_INPUT_BUFFER_CORE_H */
