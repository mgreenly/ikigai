#ifndef IK_PP_HELPERS_H
#define IK_PP_HELPERS_H

#include "format.h"
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * Generic pretty-print helper functions for consistent formatting
 * across all ik_pp_* implementations.
 *
 * All functions are designed to be composable and respect indentation
 * for proper nesting of structures.
 */

/**
 * Print type header with address.
 *
 * Format: "TypeName @ 0x...\n"
 *
 * @param buf Format buffer
 * @param indent Indentation level (number of spaces)
 * @param type Type name (e.g., "ik_input_buffer_t", "ik_input_buffer_cursor_t")
 * @param ptr Pointer to structure
 */
void ik_pp_header(ik_format_buffer_t *buf, int32_t indent, const char *type, const void *ptr);

/**
 * Print named pointer field.
 *
 * Format: "  field_name: 0x... (or NULL)\n"
 *
 * @param buf Format buffer
 * @param indent Indentation level
 * @param name Field name
 * @param ptr Pointer value
 */
void ik_pp_pointer(ik_format_buffer_t *buf, int32_t indent, const char *name, const void *ptr);

/**
 * Print named size_t field.
 *
 * Format: "  field_name: 42\n"
 *
 * @param buf Format buffer
 * @param indent Indentation level
 * @param name Field name
 * @param value size_t value
 */
void ik_pp_size_t(ik_format_buffer_t *buf, int32_t indent, const char *name, size_t value);

/**
 * Print named int32_t field.
 *
 * Format: "  field_name: 42\n"
 *
 * @param buf Format buffer
 * @param indent Indentation level
 * @param name Field name
 * @param value int32_t value
 */
void ik_pp_int32(ik_format_buffer_t *buf, int32_t indent, const char *name, int32_t value);

/**
 * Print named uint32_t field.
 *
 * Format: "  field_name: 42\n"
 *
 * @param buf Format buffer
 * @param indent Indentation level
 * @param name Field name
 * @param value uint32_t value
 */
void ik_pp_uint32(ik_format_buffer_t *buf, int32_t indent, const char *name, uint32_t value);

/**
 * Print named string field with escaping.
 *
 * Escapes special characters: \n, \r, \t, \\, \", control chars, DEL
 *
 * Format: "  field_name: \"escaped string content\"\n"
 *
 * @param buf Format buffer
 * @param indent Indentation level
 * @param name Field name
 * @param str String pointer (can be NULL)
 * @param len String length in bytes
 */
void ik_pp_string(ik_format_buffer_t *buf, int32_t indent, const char *name, const char *str, size_t len);

/**
 * Print named boolean field.
 *
 * Format: "  field_name: true\n" or "  field_name: false\n"
 *
 * @param buf Format buffer
 * @param indent Indentation level
 * @param name Field name
 * @param value Boolean value
 */
void ik_pp_bool(ik_format_buffer_t *buf, int32_t indent, const char *name, bool value);

#endif // IK_PP_HELPERS_H
