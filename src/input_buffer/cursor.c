/**
 * @file cursor.c
 * @brief Cursor position tracking implementation
 */

#include "cursor.h"
#include "panic.h"
#include "wrapper.h"
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <talloc.h>
#include <utf8proc.h>

ik_input_buffer_cursor_t *ik_input_buffer_cursor_create(void *parent)
{
    assert(parent != NULL);  /* LCOV_EXCL_BR_LINE */

    ik_input_buffer_cursor_t *cursor = talloc_zero_(parent, sizeof(ik_input_buffer_cursor_t));
    if (cursor == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Both offsets initialize to 0 via talloc_zero_wrapper (via memset)
    return cursor;
}

void ik_input_buffer_cursor_set_position(ik_input_buffer_cursor_t *cursor,
                                         const char *text,
                                         size_t text_len,
                                         size_t byte_offset)
{
    assert(cursor != NULL);         /* LCOV_EXCL_BR_LINE */
    assert(text != NULL);           /* LCOV_EXCL_BR_LINE */
    assert(byte_offset <= text_len); /* LCOV_EXCL_BR_LINE */

    // Set byte offset
    cursor->byte_offset = byte_offset;

    // Count grapheme clusters from start to byte_offset
    size_t grapheme_count = 0;
    size_t pos = 0;
    utf8proc_int32_t prev_codepoint = -1;

    while (pos < byte_offset) {
        utf8proc_int32_t codepoint;
        utf8proc_ssize_t bytes_read = utf8proc_iterate((const utf8proc_uint8_t *)(text + pos),
                                                       (utf8proc_ssize_t)(text_len - pos),
                                                       &codepoint);

        assert(bytes_read > 0); /* LCOV_EXCL_BR_LINE - UTF-8 validity guaranteed by input buffer */

        // Check if this is a grapheme break
        // A grapheme break occurs when we transition from one grapheme cluster to another
        if (prev_codepoint == -1 || utf8proc_grapheme_break(prev_codepoint, codepoint)) {
            grapheme_count++;
        }

        prev_codepoint = codepoint;
        pos += (size_t)bytes_read;
    }

    cursor->grapheme_offset = grapheme_count;
}

void ik_input_buffer_cursor_move_left(ik_input_buffer_cursor_t *cursor, const char *text, size_t text_len)
{
    assert(cursor != NULL);  /* LCOV_EXCL_BR_LINE */
    assert(text != NULL);    /* LCOV_EXCL_BR_LINE */

    // If cursor is at start, this is a no-op
    if (cursor->byte_offset == 0) {
        return;
    }

    // Scan through text and find grapheme boundaries
    // We need to find the last grapheme boundary that is before cursor->byte_offset
    size_t last_boundary_byte = 0;
    size_t grapheme_count = 0;
    size_t pos = 0;
    utf8proc_int32_t prev_codepoint = -1;

    while (pos < text_len) {
        // Decode one codepoint
        utf8proc_int32_t codepoint;
        utf8proc_ssize_t bytes_read = utf8proc_iterate((const utf8proc_uint8_t *)(text + pos),
                                                       (utf8proc_ssize_t)(text_len - pos),
                                                       &codepoint);

        assert(bytes_read > 0); /* LCOV_EXCL_BR_LINE - UTF-8 validity guaranteed by input buffer */

        // Check if this starts a new grapheme cluster
        if (prev_codepoint == -1 || utf8proc_grapheme_break(prev_codepoint, codepoint)) {
            // This is a grapheme boundary
            if (pos < cursor->byte_offset) {
                // This boundary is before cursor, save it
                last_boundary_byte = pos;
                grapheme_count++;
            } else {
                // We've reached or passed cursor position
                break;
            }
        }

        prev_codepoint = codepoint;
        pos += (size_t)bytes_read;
    }

    // Move cursor to the last grapheme boundary before current position
    cursor->byte_offset = last_boundary_byte;
    cursor->grapheme_offset = grapheme_count > 0 ? grapheme_count - 1 : 0;  /* LCOV_EXCL_BR_LINE */
}

void ik_input_buffer_cursor_move_right(ik_input_buffer_cursor_t *cursor, const char *text, size_t text_len)
{
    assert(cursor != NULL);  /* LCOV_EXCL_BR_LINE */
    assert(text != NULL);    /* LCOV_EXCL_BR_LINE */

    // If cursor is at end, this is a no-op
    if (cursor->byte_offset == text_len) {
        return;
    }

    // Scan through text starting from current position to find next grapheme boundary
    size_t pos = cursor->byte_offset;
    utf8proc_int32_t prev_codepoint = -1;
    bool found_next_boundary = false;
    size_t next_boundary_byte = text_len;

    while (pos < text_len) {
        // Decode one codepoint
        utf8proc_int32_t codepoint;
        utf8proc_ssize_t bytes_read = utf8proc_iterate((const utf8proc_uint8_t *)(text + pos),
                                                       (utf8proc_ssize_t)(text_len - pos),
                                                       &codepoint);

        assert(bytes_read > 0); /* LCOV_EXCL_BR_LINE - UTF-8 validity guaranteed by input buffer */

        pos += (size_t)bytes_read;

        // Check if this starts a new grapheme cluster
        if (prev_codepoint != -1 && utf8proc_grapheme_break(prev_codepoint, codepoint)) {
            // We found the next grapheme boundary
            next_boundary_byte = pos - (size_t)bytes_read;
            found_next_boundary = true;
            break;
        }

        prev_codepoint = codepoint;
    }

    // If we didn't find a grapheme break, the next boundary is at the end of text
    if (!found_next_boundary) {
        next_boundary_byte = pos;
    }

    // Move cursor to next grapheme boundary
    cursor->byte_offset = next_boundary_byte;
    cursor->grapheme_offset++;
}

void ik_input_buffer_cursor_get_position(ik_input_buffer_cursor_t *cursor,
                                         size_t *byte_offset_out,
                                         size_t *grapheme_offset_out)
{
    assert(cursor != NULL);              /* LCOV_EXCL_BR_LINE */
    assert(byte_offset_out != NULL);     /* LCOV_EXCL_BR_LINE */
    assert(grapheme_offset_out != NULL); /* LCOV_EXCL_BR_LINE */

    *byte_offset_out = cursor->byte_offset;
    *grapheme_offset_out = cursor->grapheme_offset;
}
