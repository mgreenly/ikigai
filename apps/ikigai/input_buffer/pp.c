/**
 * @file pp.c
 * @brief Input buffer pretty-print implementation
 */

#include "apps/ikigai/byte_array.h"
#include "apps/ikigai/format.h"
#include "apps/ikigai/input_buffer/core.h"
#include "apps/ikigai/input_buffer/cursor.h"
#include "apps/ikigai/pp_helpers.h"
#include "shared/error.h"

#include <assert.h>
#include <inttypes.h>


#include "shared/poison.h"
void ik_pp_input_buffer(const ik_input_buffer_t *input_buffer, ik_format_buffer_t *buf, int32_t indent)
{
    assert(input_buffer != NULL); /* LCOV_EXCL_BR_LINE */
    assert(buf != NULL); /* LCOV_EXCL_BR_LINE */

    /* Print header with input buffer address using generic helper */
    ik_pp_header(buf, indent, "ik_input_buffer_t", input_buffer);

    /* Get text buffer */
    char *text = NULL;
    size_t text_len = 0;
    if (input_buffer->text != NULL) { /* LCOV_EXCL_BR_LINE - defensive: text always allocated in create */
        text = (char *)input_buffer->text->data;
        text_len = ik_byte_array_size(input_buffer->text);
    }

    /* Print text length using generic helper */
    ik_pp_size_t(buf, indent + 2, "text_len", text_len);

    /* Print cursor recursively using ik_pp_input_buffer_cursor (nested structure) */
    if (input_buffer->cursor != NULL) { /* LCOV_EXCL_BR_LINE - defensive: cursor always allocated in create */
        ik_pp_input_buffer_cursor(input_buffer->cursor, buf, indent + 2);
    }

    /* Print target column using generic helper */
    ik_pp_size_t(buf, indent + 2, "target_column", input_buffer->target_column);

    /* Print text content using generic helper (with escaping) */
    if (text_len > 0 && text != NULL) { /* LCOV_EXCL_BR_LINE - defensive: text always non-NULL when text_len > 0 */
        ik_pp_string(buf, indent + 2, "text", text, text_len);
    }
}
