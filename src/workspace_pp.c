/**
 * @file workspace_pp.c
 * @brief Workspace pretty-print implementation
 */

#include "workspace.h"
#include "workspace_cursor.h"
#include "format.h"
#include "byte_array.h"
#include "pp_helpers.h"
#include "error.h"
#include <assert.h>
#include <inttypes.h>

void ik_pp_workspace(const ik_workspace_t *workspace, ik_format_buffer_t *buf, int32_t indent)
{
    assert(workspace != NULL); /* LCOV_EXCL_BR_LINE */
    assert(buf != NULL); /* LCOV_EXCL_BR_LINE */

    /* Print header with workspace address using generic helper */
    ik_pp_header(buf, indent, "ik_workspace_t", workspace);

    /* Get text buffer */
    char *text = NULL;
    size_t text_len = 0;
    if (workspace->text != NULL) { /* LCOV_EXCL_BR_LINE - defensive: text always allocated in create */
        text = (char *)workspace->text->data;
        text_len = ik_byte_array_size(workspace->text);
    }

    /* Print text length using generic helper */
    ik_pp_size_t(buf, indent + 2, "text_len", text_len);

    /* Print cursor recursively using ik_pp_cursor (nested structure) */
    if (workspace->cursor != NULL) { /* LCOV_EXCL_BR_LINE - defensive: cursor always allocated in create */
        ik_pp_cursor(workspace->cursor, buf, indent + 2);
    }

    /* Print target column using generic helper */
    ik_pp_size_t(buf, indent + 2, "target_column", workspace->target_column);

    /* Print text content using generic helper (with escaping) */
    if (text_len > 0 && text != NULL) { /* LCOV_EXCL_BR_LINE - defensive: text always non-NULL when text_len > 0 */
        ik_pp_string(buf, indent + 2, "text", text, text_len);
    }
}
