/**
 * @file workspace.h
 * @brief Workspace text buffer for REPL input area
 *
 * Provides a text buffer for the workspace (input area) of the REPL.
 * Uses ik_byte_array_t for UTF-8 text storage and tracks cursor position.
 */

#ifndef IKIGAI_WORKSPACE_H
#define IKIGAI_WORKSPACE_H

#include "byte_array.h"
#include "cursor.h"
#include "error.h"
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Workspace context
 *
 * Represents the editable text buffer in the REPL's workspace.
 * Stores UTF-8 text and tracks the cursor position using grapheme-aware cursor.
 */
typedef struct ik_workspace_t {
    ik_byte_array_t *text;       /**< UTF-8 text buffer */
    ik_cursor_t *cursor;         /**< Cursor position (byte and grapheme offsets) */
    size_t cursor_byte_offset;   /**< Legacy byte offset - deprecated, use cursor instead */
} ik_workspace_t;

/**
 * @brief Create a new workspace
 *
 * @param parent Talloc parent context
 * @param workspace_out Pointer to receive allocated workspace
 * @return RES_OK on success, RES_ERR on failure
 */
res_t ik_workspace_create(void *parent, ik_workspace_t **workspace_out);

/**
 * @brief Get the text buffer contents
 *
 * Returns a pointer to the internal text buffer and its length.
 * The returned pointer is valid until the next modification.
 *
 * @param workspace Workspace
 * @param text_out Pointer to receive text buffer
 * @param len_out Pointer to receive text length in bytes
 * @return RES_OK on success, RES_ERR on failure
 */
res_t ik_workspace_get_text(ik_workspace_t *workspace, char **text_out, size_t *len_out);

/**
 * @brief Clear the workspace
 *
 * Removes all text and resets cursor to position 0.
 *
 * @param workspace Workspace
 */
void ik_workspace_clear(ik_workspace_t *workspace);

/**
 * @brief Insert a Unicode codepoint at the cursor position
 *
 * Encodes the codepoint to UTF-8 and inserts it at the current cursor position.
 * Advances the cursor by the number of bytes inserted.
 *
 * @param workspace Workspace
 * @param codepoint Unicode codepoint to insert (U+0000 to U+10FFFF)
 * @return RES_OK on success, RES_ERR on failure
 */
res_t ik_workspace_insert_codepoint(ik_workspace_t *workspace, uint32_t codepoint);

/**
 * @brief Insert a newline at the cursor position
 *
 * Inserts a newline character ('\n') at the current cursor position.
 * Advances the cursor by 1 byte.
 *
 * @param workspace Workspace
 * @return RES_OK on success, RES_ERR on failure
 */
res_t ik_workspace_insert_newline(ik_workspace_t *workspace);

/**
 * @brief Delete the character before the cursor (backspace)
 *
 * Deletes the previous UTF-8 character (grapheme cluster) before the cursor.
 * Moves the cursor backward by the number of bytes deleted.
 * If cursor is at position 0, this is a no-op.
 *
 * @param workspace Workspace
 * @return RES_OK on success, RES_ERR on failure
 */
res_t ik_workspace_backspace(ik_workspace_t *workspace);

/**
 * @brief Delete the character after the cursor (delete key)
 *
 * Deletes the UTF-8 character at the cursor position.
 * The cursor position stays the same.
 * If cursor is at end of text, this is a no-op.
 *
 * @param workspace Workspace
 * @return RES_OK on success, RES_ERR on failure
 */
res_t ik_workspace_delete(ik_workspace_t *workspace);

/**
 * @brief Move cursor left by one grapheme cluster
 *
 * Moves the cursor backward by one grapheme cluster.
 * If cursor is at start, this is a no-op.
 *
 * @param workspace Workspace
 * @return RES_OK on success, RES_ERR on failure
 */
res_t ik_workspace_cursor_left(ik_workspace_t *workspace);

/**
 * @brief Move cursor right by one grapheme cluster
 *
 * Moves the cursor forward by one grapheme cluster.
 * If cursor is at end, this is a no-op.
 *
 * @param workspace Workspace
 * @return RES_OK on success, RES_ERR on failure
 */
res_t ik_workspace_cursor_right(ik_workspace_t *workspace);

/**
 * @brief Get cursor position
 *
 * Returns the cursor position in both byte offset and grapheme offset.
 *
 * @param workspace Workspace
 * @param byte_out Pointer to receive byte offset
 * @param grapheme_out Pointer to receive grapheme offset
 * @return RES_OK on success, RES_ERR on failure
 */
res_t ik_workspace_get_cursor_position(ik_workspace_t *workspace, size_t *byte_out, size_t *grapheme_out);

/**
 * @brief Move cursor up by one line
 *
 * Moves the cursor up to the previous line, attempting to preserve column position.
 * If cursor is on the first line, this is a no-op.
 *
 * @param workspace Workspace
 * @return RES_OK on success, RES_ERR on failure
 */
res_t ik_workspace_cursor_up(ik_workspace_t *workspace);

/**
 * @brief Move cursor down by one line
 *
 * Moves the cursor down to the next line, attempting to preserve column position.
 * If cursor is on the last line, this is a no-op.
 *
 * @param workspace Workspace
 * @return RES_OK on success, RES_ERR on failure
 */
res_t ik_workspace_cursor_down(ik_workspace_t *workspace);

/**
 * @brief Move cursor to the start of the current line (Ctrl+A)
 *
 * Moves the cursor to the beginning of the current line.
 * If cursor is already at the line start, this is a no-op.
 *
 * @param workspace Workspace
 * @return RES_OK on success, RES_ERR on failure
 */
res_t ik_workspace_cursor_to_line_start(ik_workspace_t *workspace);

#endif /* IKIGAI_WORKSPACE_H */
