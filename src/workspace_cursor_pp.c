/**
 * @file workspace_cursor_pp.c
 * @brief Cursor pretty-print implementation
 */

#include "workspace_cursor.h"
#include "pp_helpers.h"
#include <assert.h>

void ik_pp_cursor(const ik_cursor_t *cursor, struct ik_format_buffer_t *buf, int32_t indent)
{
    assert(cursor != NULL); /* LCOV_EXCL_BR_LINE */
    assert(buf != NULL); /* LCOV_EXCL_BR_LINE */

    // Print header with type name and address
    ik_pp_header(buf, indent, "ik_cursor_t", cursor);

    // Print fields using generic helpers
    ik_pp_size_t(buf, indent, "byte_offset", cursor->byte_offset);
    ik_pp_size_t(buf, indent, "grapheme_offset", cursor->grapheme_offset);
}
