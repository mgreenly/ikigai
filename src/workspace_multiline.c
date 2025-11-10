/**
 * @file workspace_multiline.c
 * @brief Workspace multi-line navigation implementation
 */

#include "workspace.h"
#include "error.h"
#include <assert.h>

/**
 * @brief Find the start of the current line (position after previous newline, or 0)
 *
 * @param text Text buffer
 * @param cursor_pos Current cursor position
 * @return Position of current line start
 */
static size_t find_line_start(const char *text, size_t cursor_pos)
{
    assert(text != NULL); /* LCOV_EXCL_BR_LINE */

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

/**
 * @brief Find the end of the current line (position of newline, or end of text)
 *
 * @param text Text buffer
 * @param text_len Length of text
 * @param cursor_pos Current cursor position
 * @return Position of current line end (newline position or text_len)
 */
static size_t find_line_end(const char *text, size_t text_len, size_t cursor_pos)
{
    assert(text != NULL); /* LCOV_EXCL_BR_LINE */

    // Scan forward to find newline or end
    size_t pos = cursor_pos;
    while (pos < text_len && text[pos] != '\n') {
        pos++;
    }

    return pos;
}

/**
 * @brief Count grapheme clusters in a substring
 *
 * @param text Start of substring
 * @param len Length of substring in bytes
 * @return Number of grapheme clusters
 */
static size_t count_graphemes(const char *text, size_t len)
{
    assert(text != NULL); /* LCOV_EXCL_BR_LINE */

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
        } else if ((first_byte & 0xF8) == 0xF0) { /* LCOV_EXCL_BR_LINE */
            char_len = 4;
        } else {
            char_len = 1; // Invalid, treat as 1 byte /* LCOV_EXCL_LINE */
        }

        byte_pos += char_len;
        grapheme_count++;
    }

    return grapheme_count;
}

/**
 * @brief Find byte position of Nth grapheme within a substring
 *
 * @param text Start of substring
 * @param len Length of substring in bytes
 * @param target_grapheme Target grapheme index (0-based)
 * @return Byte offset of target grapheme (or len if past end)
 */
static size_t grapheme_to_byte_offset(const char *text, size_t len, size_t target_grapheme)
{
    assert(text != NULL); /* LCOV_EXCL_BR_LINE */

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
        } else if ((first_byte & 0xF8) == 0xF0) { /* LCOV_EXCL_BR_LINE */
            char_len = 4;
        } else {
            char_len = 1; // Invalid, treat as 1 byte /* LCOV_EXCL_LINE */
        }

        byte_pos += char_len;
        grapheme_count++;
    }

    return byte_pos;
}

res_t ik_workspace_cursor_up(ik_workspace_t *workspace)
{
    assert(workspace != NULL); /* LCOV_EXCL_BR_LINE */

    char *text;
    size_t text_len;
    ik_workspace_get_text(workspace, &text, &text_len); // Never fails

    size_t cursor_pos = workspace->cursor->byte_offset;

    // Find current line start
    size_t current_line_start = find_line_start(text, cursor_pos);

    // If already on first line, no-op
    if (current_line_start == 0) {
        return OK(NULL);
    }

    // Calculate column position within current line (in graphemes)
    size_t column_graphemes = count_graphemes(text + current_line_start, cursor_pos - current_line_start);

    // Find previous line start (position after newline before current line)
    // current_line_start - 1 is the newline at end of previous line
    size_t prev_line_start = find_line_start(text, current_line_start - 1);

    // Find previous line end (the newline at current_line_start - 1)
    size_t prev_line_end = current_line_start - 1;

    // Calculate previous line length in graphemes
    size_t prev_line_graphemes = count_graphemes(text + prev_line_start, prev_line_end - prev_line_start);

    // Position cursor at same column, or at line end if line is shorter
    size_t target_column = (column_graphemes <= prev_line_graphemes) ? column_graphemes : prev_line_graphemes;
    size_t new_pos = prev_line_start + grapheme_to_byte_offset(text + prev_line_start,
                                                               prev_line_end - prev_line_start,
                                                               target_column);

    // Update cursor position
    workspace->cursor_byte_offset = new_pos;
    res_t res = ik_cursor_set_position(workspace->cursor, text, text_len, new_pos);
    if (is_err(&res)) { /* LCOV_EXCL_BR_LINE - defensive: text is always valid UTF-8 */
        return res; /* LCOV_EXCL_LINE */
    }

    return OK(NULL);
}

res_t ik_workspace_cursor_down(ik_workspace_t *workspace)
{
    assert(workspace != NULL); /* LCOV_EXCL_BR_LINE */

    char *text;
    size_t text_len;
    ik_workspace_get_text(workspace, &text, &text_len); // Never fails

    size_t cursor_pos = workspace->cursor->byte_offset;

    // Find current line start and end
    size_t current_line_start = find_line_start(text, cursor_pos);
    size_t current_line_end = find_line_end(text, text_len, cursor_pos);

    // If already on last line (no newline after current line), no-op
    if (current_line_end >= text_len) {
        return OK(NULL);
    }

    // Calculate column position within current line (in graphemes)
    size_t column_graphemes = count_graphemes(text + current_line_start, cursor_pos - current_line_start);

    // Next line starts after the newline
    size_t next_line_start = current_line_end + 1;

    // Find next line end
    size_t next_line_end = find_line_end(text, text_len, next_line_start);

    // Calculate next line length in graphemes
    size_t next_line_graphemes = count_graphemes(text + next_line_start, next_line_end - next_line_start);

    // Position cursor at same column, or at line end if line is shorter
    size_t target_column = (column_graphemes <= next_line_graphemes) ? column_graphemes : next_line_graphemes;
    size_t new_pos = next_line_start + grapheme_to_byte_offset(text + next_line_start,
                                                               next_line_end - next_line_start,
                                                               target_column);

    // Update cursor position
    workspace->cursor_byte_offset = new_pos;
    res_t res = ik_cursor_set_position(workspace->cursor, text, text_len, new_pos);
    if (is_err(&res)) { /* LCOV_EXCL_BR_LINE - defensive: text is always valid UTF-8 */
        return res; /* LCOV_EXCL_LINE */
    }

    return OK(NULL);
}

res_t ik_workspace_cursor_to_line_start(ik_workspace_t *workspace)
{
    assert(workspace != NULL); /* LCOV_EXCL_BR_LINE */

    char *text;
    size_t text_len;
    ik_workspace_get_text(workspace, &text, &text_len); // Never fails

    size_t cursor_pos = workspace->cursor->byte_offset;

    // Find current line start
    size_t line_start = find_line_start(text, cursor_pos);

    // If already at line start, no-op
    if (cursor_pos == line_start) {
        return OK(NULL);
    }

    // Update cursor position to line start
    workspace->cursor_byte_offset = line_start;
    res_t res = ik_cursor_set_position(workspace->cursor, text, text_len, line_start);
    assert(is_ok(&res)); /* LCOV_EXCL_BR_LINE - Should never fail: text is valid UTF-8, position is valid */

    return OK(NULL);
}
