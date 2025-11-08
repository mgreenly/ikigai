/**
 * @file dynzone.h
 * @brief Dynamic zone text buffer for REPL input area
 *
 * Provides a text buffer for the dynamic zone (input area) of the REPL.
 * Uses ik_byte_array_t for UTF-8 text storage and tracks cursor position.
 */

#ifndef IKIGAI_DYNZONE_H
#define IKIGAI_DYNZONE_H

#include "byte_array.h"
#include "error.h"
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Dynamic zone context
 *
 * Represents the editable text buffer in the REPL's dynamic zone.
 * Stores UTF-8 text and tracks the cursor position in bytes.
 */
typedef struct ik_dynzone_t {
    ik_byte_array_t *text;       /**< UTF-8 text buffer */
    size_t cursor_byte_offset;   /**< Cursor position (byte offset) */
} ik_dynzone_t;

/**
 * @brief Create a new dynamic zone
 *
 * @param parent Talloc parent context
 * @param zone_out Pointer to receive allocated zone
 * @return RES_OK on success, RES_ERR on failure
 */
res_t ik_dynzone_create(void *parent, ik_dynzone_t **zone_out);

/**
 * @brief Insert a Unicode codepoint at the cursor position
 *
 * Encodes the codepoint to UTF-8 and inserts at cursor_byte_offset.
 * Advances the cursor after the inserted bytes.
 *
 * @param zone Dynamic zone
 * @param codepoint Unicode codepoint to insert
 * @return RES_OK on success, RES_ERR on failure
 */
res_t ik_dynzone_insert_codepoint(ik_dynzone_t *zone, uint32_t codepoint);

/**
 * @brief Insert a newline character at the cursor position
 *
 * Inserts '\n' at cursor_byte_offset and advances cursor.
 *
 * @param zone Dynamic zone
 * @return RES_OK on success, RES_ERR on failure
 */
res_t ik_dynzone_insert_newline(ik_dynzone_t *zone);

/**
 * @brief Delete the character before the cursor (backspace)
 *
 * Finds the start of the previous UTF-8 character and deletes it.
 * Moves cursor to the start of the deleted character.
 * No-op if cursor is at the start of the buffer.
 *
 * @param zone Dynamic zone
 * @return RES_OK on success, RES_ERR on failure
 */
res_t ik_dynzone_backspace(ik_dynzone_t *zone);

/**
 * @brief Delete the character at the cursor position
 *
 * Finds the end of the current UTF-8 character and deletes it.
 * Cursor position remains unchanged.
 * No-op if cursor is at the end of the buffer.
 *
 * @param zone Dynamic zone
 * @return RES_OK on success, RES_ERR on failure
 */
res_t ik_dynzone_delete(ik_dynzone_t *zone);

/**
 * @brief Get the text buffer contents
 *
 * Returns a pointer to the internal text buffer and its length.
 * The returned pointer is valid until the next modification.
 *
 * @param zone Dynamic zone
 * @param text_out Pointer to receive text buffer
 * @param len_out Pointer to receive text length in bytes
 * @return RES_OK on success, RES_ERR on failure
 */
res_t ik_dynzone_get_text(ik_dynzone_t *zone, char **text_out, size_t *len_out);

/**
 * @brief Clear the dynamic zone
 *
 * Removes all text and resets cursor to position 0.
 *
 * @param zone Dynamic zone
 */
void ik_dynzone_clear(ik_dynzone_t *zone);

#endif /* IKIGAI_DYNZONE_H */
