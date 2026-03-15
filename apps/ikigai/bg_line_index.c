/**
 * @file bg_line_index.c
 * @brief Line index for background process output streams
 */

#include "apps/ikigai/bg_line_index.h"

#include <assert.h>
#include <inttypes.h>
#include <talloc.h>

#include "shared/panic.h"
#include "shared/poison.h"

#define BG_LINE_INDEX_INITIAL_CAPACITY 256

/* ================================================================
 * Lifecycle
 * ================================================================ */

bg_line_index_t *bg_line_index_create(TALLOC_CTX *ctx)
{
    bg_line_index_t *idx = talloc_zero(ctx, bg_line_index_t);
    if (idx == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    idx->offsets = talloc_array(idx, off_t, BG_LINE_INDEX_INITIAL_CAPACITY);
    if (idx->offsets == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    idx->capacity = BG_LINE_INDEX_INITIAL_CAPACITY;
    idx->count    = 0;
    idx->cursor   = 0;

    return idx;
}

void bg_line_index_destroy(bg_line_index_t *idx)
{
    if (idx == NULL) return;
    talloc_free(idx);
}

/* ================================================================
 * Append
 * ================================================================ */

void bg_line_index_append(bg_line_index_t *idx, const uint8_t *data,
                          size_t len)
{
    assert(idx != NULL); // LCOV_EXCL_BR_LINE

    if (data == NULL || len == 0) return;

    for (size_t i = 0; i < len; i++) {
        if (data[i] == '\n') {
            if (idx->count >= idx->capacity) {
                int64_t new_cap = idx->capacity * 2;
                off_t *new_offsets = talloc_realloc(idx, idx->offsets,
                                                    off_t,
                                                    (unsigned int)new_cap);
                if (new_offsets == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
                idx->offsets  = new_offsets;
                idx->capacity = new_cap;
            }
            idx->offsets[idx->count] = idx->cursor + (off_t)i;
            idx->count++;
        }
    }

    idx->cursor += (off_t)len;
}

/* ================================================================
 * Query
 * ================================================================ */

res_t bg_line_index_get_range(const bg_line_index_t *idx,
                              int64_t start_line, int64_t end_line,
                              off_t *out_offset, size_t *out_length)
{
    if (idx == NULL || out_offset == NULL || out_length == NULL) {
        TALLOC_CTX *tmp = talloc_new(NULL);
        res_t r = ERR(tmp, INVALID_ARG, "NULL argument");
        return r;
    }

    if (start_line < 1 || end_line < start_line || end_line > idx->count) {
        TALLOC_CTX *tmp = talloc_new(NULL);
        res_t r = ERR(tmp, OUT_OF_RANGE,
                      "line range [%" PRId64 ", %" PRId64 "] invalid"
                      " (count=%" PRId64 ")",
                      start_line, end_line, idx->count);
        return r;
    }

    off_t start_off = (start_line == 1) ? 0
                                        : idx->offsets[start_line - 2] + 1;
    off_t end_byte  = idx->offsets[end_line - 1]; /* inclusive newline */

    *out_offset = start_off;
    *out_length = (size_t)(end_byte - start_off + 1);

    return OK(NULL);
}

int64_t bg_line_index_count(const bg_line_index_t *idx)
{
    assert(idx != NULL); // LCOV_EXCL_BR_LINE
    return idx->count;
}
