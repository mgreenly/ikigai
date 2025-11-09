/**
 * @file cursor.c
 * @brief Cursor position tracking implementation
 */

#include "cursor.h"
#include "wrapper.h"
#include <assert.h>
#include <string.h>
#include <talloc.h>

res_t ik_cursor_create(void *parent, ik_cursor_t **cursor_out)
{
    assert(parent != NULL);      /* LCOV_EXCL_BR_LINE */
    assert(cursor_out != NULL);  /* LCOV_EXCL_BR_LINE */

    ik_cursor_t *cursor = ik_talloc_zero_wrapper(parent, sizeof(ik_cursor_t));
    if (cursor == NULL) {
        return ERR(parent, OOM, "Failed to allocate cursor");
    }

    // Both offsets initialize to 0 via talloc_zero_wrapper (via memset)
    *cursor_out = cursor;
    return OK(cursor);
}
