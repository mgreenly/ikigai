/**
 * @file bg_line_index.h
 * @brief Line index for background process output streams
 *
 * Tracks newline byte offsets in an append-only PTY output stream,
 * enabling O(1) random-access line range queries via pread().
 *
 * Single-threaded. All operations must be called from the main thread.
 */

#ifndef IK_BG_LINE_INDEX_H
#define IK_BG_LINE_INDEX_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <talloc.h>

#include "shared/error.h"

/**
 * Line index: in-memory array of newline byte offsets.
 *
 * offsets[i] is the absolute byte offset (from stream start) of the
 * i-th '\n'. Lines are 1-indexed in the public API.
 *
 * A "line" is a sequence of bytes terminated by '\n'. Only complete
 * lines (those with a terminating '\n') are queryable via get_range.
 */
typedef struct bg_line_index {
    off_t   *offsets;  /* absolute byte offset of each '\n' */
    int64_t  count;    /* number of newlines recorded        */
    int64_t  capacity; /* allocated slots in offsets         */
    off_t    cursor;   /* total bytes appended so far        */
} bg_line_index_t;

/**
 * Create a line index.
 *
 * @param ctx  Talloc parent. The returned struct and its offsets array
 *             are allocated as children of ctx.
 * @return     Allocated index. Never NULL; panics on OOM.
 */
bg_line_index_t *bg_line_index_create(TALLOC_CTX *ctx);

/**
 * Destroy a line index.
 *
 * Frees the struct and its internal offsets array. Safe to call with NULL.
 *
 * @param idx  Index to destroy (may be NULL).
 */
void bg_line_index_destroy(bg_line_index_t *idx);

/**
 * Append a chunk of output and record any newlines found.
 *
 * Scans data[0..len-1] for '\n', recording their absolute stream
 * byte offsets (cursor + local index). Advances the internal cursor
 * by len. Grows the offsets array automatically as needed.
 *
 * No-op if data is NULL or len is 0.
 *
 * @param idx   Line index (must not be NULL).
 * @param data  Bytes to scan.
 * @param len   Number of bytes in data.
 */
void bg_line_index_append(bg_line_index_t *idx, const uint8_t *data,
                          size_t len);

/**
 * Get byte offset and length for a line range.
 *
 * Lines are 1-indexed and the range is inclusive on both ends. A
 * "line" is a sequence of bytes terminated by '\n'. Only complete
 * lines are queryable; partial content after the last newline is not.
 *
 * @param idx         Line index (must not be NULL).
 * @param start_line  First line (1-indexed, inclusive).
 * @param end_line    Last line (1-indexed, inclusive, >= start_line).
 * @param out_offset  Set to the absolute byte offset of the first byte
 *                    of start_line on success.
 * @param out_length  Set to the total byte count from start_line through
 *                    end_line (inclusive of the '\n' terminating end_line)
 *                    on success.
 * @return            OK on success.
 *                    ERR_INVALID_ARG  if idx, out_offset, or out_length is NULL.
 *                    ERR_OUT_OF_RANGE if start_line < 1, end_line > count,
 *                                     or start_line > end_line.
 */
res_t bg_line_index_get_range(const bg_line_index_t *idx,
                              int64_t start_line, int64_t end_line,
                              off_t *out_offset, size_t *out_length);

/**
 * Return the total number of complete lines indexed.
 *
 * A complete line is one terminated by '\n'. Partial content after
 * the last newline is not counted.
 *
 * @param idx  Line index (must not be NULL).
 * @return     Number of complete lines.
 */
int64_t bg_line_index_count(const bg_line_index_t *idx);

#endif /* IK_BG_LINE_INDEX_H */
