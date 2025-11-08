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
