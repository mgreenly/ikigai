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
#include "error.h"
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Workspace context
 *
 * Represents the editable text buffer in the REPL's workspace.
 * Stores UTF-8 text and tracks the cursor position in bytes.
 */
typedef struct ik_workspace_t {
    ik_byte_array_t *text;       /**< UTF-8 text buffer */
    size_t cursor_byte_offset;   /**< Cursor position (byte offset) */
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
 * @brief Insert a Unicode codepoint at the cursor position
 *
 * Encodes the codepoint to UTF-8 and inserts at cursor_byte_offset.
 * Advances the cursor after the inserted bytes.
 *
 * @param workspace Workspace
 * @param codepoint Unicode codepoint to insert
 * @return RES_OK on success, RES_ERR on failure
 */
res_t ik_workspace_insert_codepoint(ik_workspace_t *workspace, uint32_t codepoint);

/**
 * @brief Insert a newline character at the cursor position
 *
 * Inserts '\n' at cursor_byte_offset and advances cursor.
 *
 * @param workspace Workspace
 * @return RES_OK on success, RES_ERR on failure
 */
res_t ik_workspace_insert_newline(ik_workspace_t *workspace);

/**
 * @brief Delete the character before the cursor (backspace)
 *
 * Finds the start of the previous UTF-8 character and deletes it.
 * Moves cursor to the start of the deleted character.
 * No-op if cursor is at the start of the buffer.
 *
 * @param workspace Workspace
 * @return RES_OK on success, RES_ERR on failure
 */
res_t ik_workspace_backspace(ik_workspace_t *workspace);

/**
 * @brief Delete the character at the cursor position
 *
 * Finds the end of the current UTF-8 character and deletes it.
 * Cursor position remains unchanged.
 * No-op if cursor is at the end of the buffer.
 *
 * @param workspace Workspace
 * @return RES_OK on success, RES_ERR on failure
 */
res_t ik_workspace_delete(ik_workspace_t *workspace);

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

#endif /* IKIGAI_WORKSPACE_H */
