// @file multiline.c
// @brief Input buffer multi-line navigation implementation

#include "core.h"
#include "error.h"
#include "panic.h"
#include <assert.h>

// @brief Find the start of the current line (position after previous newline, or 0)
//
// @param text Text buffer
// @param cursor_pos Current cursor position
// @return Position of current line start
static size_t find_line_start(const char *text, size_t cursor_pos)
{
    assert(text != NULL); // LCOV_EXCL_BR_LINE

    if (cursor_pos == 0) {
        return 0;
    }

    // Scan backward to find newline
    size_t pos = cursor_pos;
    while (pos > 0 && text[pos - 1] != '\n') {
        pos--;
    }

    return pos;
}

// @brief Find the end of the current line (position of newline, or end of text)
//
// @param text Text buffer
// @param text_len Length of text
// @param cursor_pos Current cursor position
// @return Position of current line end (newline position or text_len)
static size_t find_line_end(const char *text, size_t text_len, size_t cursor_pos)
{
    assert(text != NULL); // LCOV_EXCL_BR_LINE

    // Scan forward to find newline or end
    size_t pos = cursor_pos;
    while (pos < text_len && text[pos] != '\n') {
        pos++;
    }

    return pos;
}

// @brief Count grapheme clusters in a substring
//
// Precondition: text must contain valid UTF-8. This is enforced by
// ik_input_buffer_insert_codepoint() and ik_input_buffer_insert_newline(),
// which are the only operations that modify input buffer text.
//
// Note: If invalid UTF-8 is encountered, the program will abort() with
// a diagnostic message, as this indicates a precondition violation.
//
// @param text Start of substring
// @param len Length of substring in bytes
// @return Number of grapheme clusters
static size_t count_graphemes(const char *text, size_t len)
{
    assert(text != NULL); // LCOV_EXCL_BR_LINE

    if (len == 0) {
        return 0;
    }

    // Create temporary cursor to count graphemes
    size_t grapheme_count = 0;
    size_t byte_pos = 0;

    while (byte_pos < len) {
        uint8_t first_byte = (uint8_t)text[byte_pos];
        size_t char_len;

        if ((first_byte & 0x80) == 0) {
            char_len = 1; // ASCII
        } else if ((first_byte & 0xE0) == 0xC0) {
            char_len = 2;
        } else if ((first_byte & 0xF0) == 0xE0) {
            char_len = 3;
        } else if ((first_byte & 0xF8) == 0xF0) { // LCOV_EXCL_BR_LINE
            char_len = 4;
        } else {
            PANIC("invalid UTF-8 in input buffer text");  // LCOV_EXCL_LINE
        }

        byte_pos += char_len;
        grapheme_count++;
    }

    return grapheme_count;
}

// @brief Find byte position of Nth grapheme within a substring
//
// Precondition: text must contain valid UTF-8. This is enforced by
// ik_input_buffer_insert_codepoint() and ik_input_buffer_insert_newline(),
// which are the only operations that modify input buffer text.
//
// Note: If invalid UTF-8 is encountered, the program will abort() with
// a diagnostic message, as this indicates a precondition violation.
//
// @param text Start of substring
// @param len Length of substring in bytes
// @param target_grapheme Target grapheme index (0-based)
// @return Byte offset of target grapheme (or len if past end)
static size_t grapheme_to_byte_offset(const char *text, size_t len, size_t target_grapheme)
{
    assert(text != NULL); // LCOV_EXCL_BR_LINE

    if (len == 0 || target_grapheme == 0) {
        return 0;
    }

    size_t grapheme_count = 0;
    size_t byte_pos = 0;

    while (byte_pos < len && grapheme_count < target_grapheme) {
        uint8_t first_byte = (uint8_t)text[byte_pos];
        size_t char_len;

        if ((first_byte & 0x80) == 0) {
            char_len = 1; // ASCII
        } else if ((first_byte & 0xE0) == 0xC0) {
            char_len = 2;
        } else if ((first_byte & 0xF0) == 0xE0) {
            char_len = 3;
        } else if ((first_byte & 0xF8) == 0xF0) { // LCOV_EXCL_BR_LINE
            char_len = 4;
        } else {
            PANIC("invalid UTF-8 in input buffer text");  // LCOV_EXCL_LINE
        }

        byte_pos += char_len;
        grapheme_count++;
    }

    return byte_pos;
}

res_t ik_input_buffer_cursor_up(ik_input_buffer_t *input_buffer)
{
    assert(input_buffer != NULL); // LCOV_EXCL_BR_LINE

    size_t text_len;
    const char *text = ik_input_buffer_get_text(input_buffer, &text_len);

    // Empty input buffer - no-op
    if (text == NULL || text_len == 0) {
        return OK(NULL);
    }

    size_t cursor_pos = input_buffer->cursor->byte_offset;

    // Find current line start
    size_t current_line_start = find_line_start(text, cursor_pos);

    // If already on first line, no-op
    if (current_line_start == 0) {
        return OK(NULL);
    }

    // Calculate column position within current line (in graphemes)
    size_t column_graphemes = count_graphemes(text + current_line_start, cursor_pos - current_line_start);

    // Save target column for future vertical movements (if not already set)
    if (input_buffer->target_column == 0) {
        input_buffer->target_column = column_graphemes;
    }

    // Find previous line start (position after newline before current line)
    // current_line_start - 1 is the newline at end of previous line
    size_t prev_line_start = find_line_start(text, current_line_start - 1);

    // Find previous line end (the newline at current_line_start - 1)
    size_t prev_line_end = current_line_start - 1;

    // Calculate previous line length in graphemes
    size_t prev_line_graphemes = count_graphemes(text + prev_line_start, prev_line_end - prev_line_start);

    // Use saved target column, or current column if just starting vertical movement
    size_t desired_column = (input_buffer->target_column > 0) ? input_buffer->target_column : column_graphemes;

    // Position cursor at target column, or at line end if line is shorter
    size_t target_column = (desired_column <= prev_line_graphemes) ? desired_column : prev_line_graphemes;
    size_t new_pos = prev_line_start + grapheme_to_byte_offset(text + prev_line_start,
                                                               prev_line_end - prev_line_start,
                                                               target_column);

    // Update cursor position
    input_buffer->cursor_byte_offset = new_pos;
    ik_input_buffer_cursor_set_position(input_buffer->cursor, text, text_len, new_pos);

    return OK(NULL);
}

res_t ik_input_buffer_cursor_down(ik_input_buffer_t *input_buffer)
{
    assert(input_buffer != NULL); // LCOV_EXCL_BR_LINE

    size_t text_len;
    const char *text = ik_input_buffer_get_text(input_buffer, &text_len);

    // Empty input buffer - no-op
    if (text == NULL || text_len == 0) {
        return OK(NULL);
    }

    size_t cursor_pos = input_buffer->cursor->byte_offset;

    // Find current line start and end
    size_t current_line_start = find_line_start(text, cursor_pos);
    size_t current_line_end = find_line_end(text, text_len, cursor_pos);

    // If already on last line (no newline after current line), no-op
    if (current_line_end >= text_len) {
        return OK(NULL);
    }

    // Calculate column position within current line (in graphemes)
    size_t column_graphemes = count_graphemes(text + current_line_start, cursor_pos - current_line_start);

    // Save target column for future vertical movements (if not already set)
    if (input_buffer->target_column == 0) {
        input_buffer->target_column = column_graphemes;
    }

    // Next line starts after the newline
    size_t next_line_start = current_line_end + 1;

    // Find next line end
    size_t next_line_end = find_line_end(text, text_len, next_line_start);

    // Calculate next line length in graphemes
    size_t next_line_graphemes = count_graphemes(text + next_line_start, next_line_end - next_line_start);

    // Use saved target column, or current column if just starting vertical movement
    size_t desired_column = (input_buffer->target_column > 0) ? input_buffer->target_column : column_graphemes;

    // Position cursor at target column, or at line end if line is shorter
    size_t target_column = (desired_column <= next_line_graphemes) ? desired_column : next_line_graphemes;
    size_t new_pos = next_line_start + grapheme_to_byte_offset(text + next_line_start,
                                                               next_line_end - next_line_start,
                                                               target_column);

    // Update cursor position
    input_buffer->cursor_byte_offset = new_pos;
    ik_input_buffer_cursor_set_position(input_buffer->cursor, text, text_len, new_pos);

    return OK(NULL);
}

res_t ik_input_buffer_cursor_to_line_start(ik_input_buffer_t *input_buffer)
{
    assert(input_buffer != NULL); // LCOV_EXCL_BR_LINE

    size_t text_len;
    const char *text = ik_input_buffer_get_text(input_buffer, &text_len);

    // Empty input buffer - no-op
    if (text == NULL || text_len == 0) {
        return OK(NULL);
    }

    size_t cursor_pos = input_buffer->cursor->byte_offset;

    // Find current line start
    size_t line_start = find_line_start(text, cursor_pos);

    // If already at line start, no-op
    if (cursor_pos == line_start) {
        return OK(NULL);
    }

    // Update cursor position to line start
    input_buffer->cursor_byte_offset = line_start;
    ik_input_buffer_cursor_set_position(input_buffer->cursor, text, text_len, line_start);

    // Reset target column on horizontal movement
    input_buffer->target_column = 0;

    return OK(NULL);
}

res_t ik_input_buffer_cursor_to_line_end(ik_input_buffer_t *input_buffer)
{
    assert(input_buffer != NULL); // LCOV_EXCL_BR_LINE

    size_t text_len;
    const char *text = ik_input_buffer_get_text(input_buffer, &text_len);

    // Empty input buffer - no-op
    if (text == NULL || text_len == 0) {
        return OK(NULL);
    }

    size_t cursor_pos = input_buffer->cursor->byte_offset;

    // Find current line end (position of \n or text_len)
    size_t line_end = find_line_end(text, text_len, cursor_pos);

    // If already at line end, no-op
    if (cursor_pos == line_end) {
        return OK(NULL);
    }

    // Update cursor position to line end
    input_buffer->cursor_byte_offset = line_end;
    ik_input_buffer_cursor_set_position(input_buffer->cursor, text, text_len, line_end);

    // Reset target column on horizontal movement
    input_buffer->target_column = 0;

    return OK(NULL);
}

res_t ik_input_buffer_kill_to_line_end(ik_input_buffer_t *input_buffer)
{
    assert(input_buffer != NULL); // LCOV_EXCL_BR_LINE

    size_t text_len;
    const char *text = ik_input_buffer_get_text(input_buffer, &text_len);

    // Empty input buffer - no-op
    if (text == NULL || text_len == 0) {
        return OK(NULL);
    }

    size_t cursor_pos = input_buffer->cursor->byte_offset;

    // Find current line end (position of \n or text_len)
    size_t line_end = find_line_end(text, text_len, cursor_pos);

    // If already at line end, no-op
    if (cursor_pos >= line_end) {
        return OK(NULL);
    }

    // Delete bytes from cursor to line end
    size_t num_bytes_to_delete = line_end - cursor_pos;
    for (size_t i = 0; i < num_bytes_to_delete; i++) {
        ik_byte_array_delete(input_buffer->text, cursor_pos);
    }

    // Update cursor (text changed, need to resync cursor object)
    text = ik_input_buffer_get_text(input_buffer, &text_len); // Never fails
    input_buffer->cursor_byte_offset = cursor_pos;
    ik_input_buffer_cursor_set_position(input_buffer->cursor, text, text_len, cursor_pos);

    // Reset target column on text modification
    input_buffer->target_column = 0;

    // Invalidate layout cache
    ik_input_buffer_invalidate_layout(input_buffer);

    return OK(NULL);
}

res_t ik_input_buffer_kill_line(ik_input_buffer_t *input_buffer)
{
    assert(input_buffer != NULL); // LCOV_EXCL_BR_LINE

    size_t text_len;
    const char *text = ik_input_buffer_get_text(input_buffer, &text_len);

    // Empty input buffer - no-op
    if (text == NULL || text_len == 0) {
        return OK(NULL);
    }

    size_t cursor_pos = input_buffer->cursor->byte_offset;

    // Find current line boundaries
    size_t line_start = find_line_start(text, cursor_pos);
    size_t line_end = find_line_end(text, text_len, cursor_pos);

    // Include the newline in deletion if present
    // Note: find_line_end() guarantees text[line_end] == '\n' when line_end < text_len
    size_t delete_end = line_end;
    if (line_end < text_len) {
        delete_end = line_end + 1;
    }

    // Delete entire line from line_start to delete_end
    size_t num_bytes_to_delete = delete_end - line_start;
    for (size_t i = 0; i < num_bytes_to_delete; i++) {
        ik_byte_array_delete(input_buffer->text, line_start);
    }

    // Position cursor at line_start (where the deleted line was)
    text = ik_input_buffer_get_text(input_buffer, &text_len); // Never fails

    // Ensure cursor position doesn't exceed new text length
    size_t new_cursor_pos = (line_start <= text_len) ? line_start : text_len;
    input_buffer->cursor_byte_offset = new_cursor_pos;
    ik_input_buffer_cursor_set_position(input_buffer->cursor, text, text_len, new_cursor_pos);

    // Reset target column on text modification
    input_buffer->target_column = 0;

    // Invalidate layout cache
    ik_input_buffer_invalidate_layout(input_buffer);

    return OK(NULL);
}
