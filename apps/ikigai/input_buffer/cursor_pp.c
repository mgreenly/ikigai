/**
 * @file cursor_pp.c
 * @brief Cursor pretty-print implementation
 */

#include "apps/ikigai/input_buffer/cursor.h"
#include "apps/ikigai/pp_helpers.h"

#include <assert.h>


#include "shared/poison.h"
void ik_pp_input_buffer_cursor(const ik_input_buffer_cursor_t *cursor, struct ik_format_buffer_t *buf, int32_t indent)
{
    assert(cursor != NULL); /* LCOV_EXCL_BR_LINE */
    assert(buf != NULL); /* LCOV_EXCL_BR_LINE */

    // Print header with type name and address
    ik_pp_header(buf, indent, "ik_input_buffer_cursor_t", cursor);

    // Print fields using generic helpers (indented 2 more than header)
    ik_pp_size_t(buf, indent + 2, "byte_offset", cursor->byte_offset);
    ik_pp_size_t(buf, indent + 2, "grapheme_offset", cursor->grapheme_offset);
}
