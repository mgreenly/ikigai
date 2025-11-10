/**
 * @file workspace.c
 * @brief Workspace text buffer implementation
 */

#include "workspace.h"
#include "wrapper.h"
#include "error.h"
#include <assert.h>
#include <talloc.h>

res_t ik_workspace_create(void *parent, ik_workspace_t **workspace_out)
{
    assert(workspace_out != NULL); /* LCOV_EXCL_BR_LINE */

    ik_workspace_t *workspace = ik_talloc_zero_wrapper(parent, sizeof(ik_workspace_t));
    if (workspace == NULL) {
        return ERR(parent, OOM, "Failed to allocate workspace");
    }

    res_t res = ik_byte_array_create(workspace, 64);
    if (is_err(&res)) {
        talloc_free(workspace);
        return res;
    }
    workspace->text = res.ok;

    res = ik_cursor_create(workspace, &workspace->cursor);
    if (is_err(&res)) {
        talloc_free(workspace);
        return res;
    }

    workspace->cursor_byte_offset = 0;
    *workspace_out = workspace;
    return OK(workspace);
}

res_t ik_workspace_get_text(ik_workspace_t *workspace, char **text_out, size_t *len_out)
{
    assert(workspace != NULL);     /* LCOV_EXCL_BR_LINE */
    assert(text_out != NULL); /* LCOV_EXCL_BR_LINE */
    assert(len_out != NULL);  /* LCOV_EXCL_BR_LINE */

    *text_out = (char *)workspace->text->data;
    *len_out = ik_byte_array_size(workspace->text);
    return OK(NULL);
}

void ik_workspace_clear(ik_workspace_t *workspace)
{
    assert(workspace != NULL); /* LCOV_EXCL_BR_LINE */

    ik_byte_array_clear(workspace->text);
    workspace->cursor_byte_offset = 0;

    /* Reset cursor to position 0 */
    workspace->cursor->byte_offset = 0;
    workspace->cursor->grapheme_offset = 0;
}

/**
 * @brief Encode a Unicode codepoint to UTF-8
 *
 * @param codepoint Unicode codepoint to encode
 * @param out Output buffer (must have at least 4 bytes)
 * @return Number of bytes written (1-4), or 0 on invalid codepoint
 */
static size_t encode_utf8(uint32_t codepoint, uint8_t *out)
{
    if (codepoint <= 0x7F) {
        /* 1-byte sequence: 0xxxxxxx */
        out[0] = (uint8_t)codepoint;
        return 1;
    } else if (codepoint <= 0x7FF) {
        /* 2-byte sequence: 110xxxxx 10xxxxxx */
        out[0] = (uint8_t)(0xC0 | (codepoint >> 6));
        out[1] = (uint8_t)(0x80 | (codepoint & 0x3F));
        return 2;
    } else if (codepoint <= 0xFFFF) {
        /* 3-byte sequence: 1110xxxx 10xxxxxx 10xxxxxx */
        out[0] = (uint8_t)(0xE0 | (codepoint >> 12));
        out[1] = (uint8_t)(0x80 | ((codepoint >> 6) & 0x3F));
        out[2] = (uint8_t)(0x80 | (codepoint & 0x3F));
        return 3;
    } else if (codepoint <= 0x10FFFF) {
        /* 4-byte sequence: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
        out[0] = (uint8_t)(0xF0 | (codepoint >> 18));
        out[1] = (uint8_t)(0x80 | ((codepoint >> 12) & 0x3F));
        out[2] = (uint8_t)(0x80 | ((codepoint >> 6) & 0x3F));
        out[3] = (uint8_t)(0x80 | (codepoint & 0x3F));
        return 4;
    }
    return 0; /* Invalid codepoint */
}

res_t ik_workspace_insert_codepoint(ik_workspace_t *workspace, uint32_t codepoint)
{
    assert(workspace != NULL); /* LCOV_EXCL_BR_LINE */

    /* Encode codepoint to UTF-8 */
    uint8_t utf8_bytes[4];
    size_t num_bytes = encode_utf8(codepoint, utf8_bytes);
    if (num_bytes == 0) {
        return ERR(workspace, INVALID_ARG, "Invalid Unicode codepoint");
    }

    /* Insert bytes at cursor position */
    for (size_t i = 0; i < num_bytes; i++) {
        res_t res = ik_byte_array_insert(workspace->text, workspace->cursor_byte_offset + i, utf8_bytes[i]);
        if (is_err(&res)) {
            return res;
        }
    }

    /* Advance cursor by number of bytes inserted */
    workspace->cursor_byte_offset += num_bytes;

    /* Update cursor position */
    char *text;
    size_t text_len;
    ik_workspace_get_text(workspace, &text, &text_len); // Never fails
    ik_cursor_set_position(workspace->cursor, text, text_len, workspace->cursor_byte_offset);

    return OK(NULL);
}

res_t ik_workspace_insert_newline(ik_workspace_t *workspace)
{
    assert(workspace != NULL); /* LCOV_EXCL_BR_LINE */

    /* Insert newline byte at cursor position */
    res_t res = ik_byte_array_insert(workspace->text, workspace->cursor_byte_offset, '\n');
    if (is_err(&res)) {
        return res;
    }

    /* Advance cursor by 1 byte */
    workspace->cursor_byte_offset += 1;

    /* Update cursor position */
    char *text;
    size_t text_len;
    ik_workspace_get_text(workspace, &text, &text_len); // Never fails
    ik_cursor_set_position(workspace->cursor, text, text_len, workspace->cursor_byte_offset);

    return OK(NULL);
}

/**
 * @brief Find the start of the previous UTF-8 character before the cursor
 *
 * @param data Text buffer
 * @param cursor_pos Current cursor position (must be > 0)
 * @return Position of the start of the previous UTF-8 character
 */
static size_t find_prev_char_start(const uint8_t *data, size_t cursor_pos)
{
    assert(cursor_pos > 0); /* LCOV_EXCL_BR_LINE */

    /* Move back at least one byte */
    size_t pos = cursor_pos - 1;

    /* Skip UTF-8 continuation bytes (10xxxxxx) */
    while (pos > 0 && (data[pos] & 0xC0) == 0x80) {
        pos--;
    }

    return pos;
}

res_t ik_workspace_backspace(ik_workspace_t *workspace)
{
    assert(workspace != NULL); /* LCOV_EXCL_BR_LINE */

    /* If cursor is at start, this is a no-op */
    if (workspace->cursor_byte_offset == 0) {
        return OK(NULL);
    }

    /* Find the start of the previous UTF-8 character */
    const uint8_t *data = workspace->text->data;
    size_t prev_char_start = find_prev_char_start(data, workspace->cursor_byte_offset);

    /* Delete all bytes from prev_char_start to cursor */
    size_t num_bytes_to_delete = workspace->cursor_byte_offset - prev_char_start;
    for (size_t i = 0; i < num_bytes_to_delete; i++) {
        ik_byte_array_delete(workspace->text, prev_char_start);
    }

    /* Update cursor to the start of the deleted character */
    workspace->cursor_byte_offset = prev_char_start;

    /* Update cursor position */
    char *text;
    size_t text_len;
    ik_workspace_get_text(workspace, &text, &text_len); // Never fails
    ik_cursor_set_position(workspace->cursor, text, text_len, workspace->cursor_byte_offset);

    return OK(NULL);
}

/**
 * @brief Find the end of the current UTF-8 character at the cursor
 *
 * @param data Text buffer
 * @param data_len Length of text buffer
 * @param cursor_pos Current cursor position
 * @return Position of the end of the current UTF-8 character (one past last byte)
 */
static size_t find_next_char_end(const uint8_t *data, size_t data_len, size_t cursor_pos)
{
    /* If cursor at end, return same position */
    if (cursor_pos >= data_len) { /* LCOV_EXCL_BR_LINE */
        return cursor_pos; /* LCOV_EXCL_LINE - defensive: caller checks cursor < data_len */
    }

    /* Determine the length of the UTF-8 character at cursor_pos */
    uint8_t first_byte = data[cursor_pos];
    size_t char_len;

    if ((first_byte & 0x80) == 0) {
        /* ASCII: 0xxxxxxx */
        char_len = 1;
    } else if ((first_byte & 0xE0) == 0xC0) {
        /* 2-byte: 110xxxxx */
        char_len = 2;
    } else if ((first_byte & 0xF0) == 0xE0) {
        /* 3-byte: 1110xxxx */
        char_len = 3;
    } else if ((first_byte & 0xF8) == 0xF0) { /* LCOV_EXCL_BR_LINE */
        /* 4-byte: 11110xxx */
        char_len = 4;
    } else {
        /* LCOV_EXCL_START - Invalid UTF-8 lead byte - never occurs with valid input */
        /* Invalid UTF-8 lead byte - treat as 1 byte */
        char_len = 1;
        /* LCOV_EXCL_STOP */
    }

    /* Return end position (clamped to data_len) */
    size_t end_pos = cursor_pos + char_len;
    return (end_pos > data_len) ? data_len : end_pos; /* LCOV_EXCL_BR_LINE - defensive: well-formed UTF-8 won't exceed buffer */
}

res_t ik_workspace_delete(ik_workspace_t *workspace)
{
    assert(workspace != NULL); /* LCOV_EXCL_BR_LINE */

    size_t text_len = ik_byte_array_size(workspace->text);

    /* If cursor is at end, this is a no-op */
    if (workspace->cursor_byte_offset >= text_len) {
        return OK(NULL);
    }

    /* Find the end of the current UTF-8 character */
    const uint8_t *data = workspace->text->data;
    size_t next_char_end = find_next_char_end(data, text_len, workspace->cursor_byte_offset);

    /* Delete all bytes from cursor to next_char_end */
    size_t num_bytes_to_delete = next_char_end - workspace->cursor_byte_offset;
    for (size_t i = 0; i < num_bytes_to_delete; i++) {
        ik_byte_array_delete(workspace->text, workspace->cursor_byte_offset);
    }

    /* Cursor stays at same position (deleted forward, not backward) */

    /* Update cursor position */
    char *text;
    ik_workspace_get_text(workspace, &text, &text_len); // Never fails
    ik_cursor_set_position(workspace->cursor, text, text_len, workspace->cursor_byte_offset);

    return OK(NULL);
}

res_t ik_workspace_cursor_left(ik_workspace_t *workspace)
{
    assert(workspace != NULL); /* LCOV_EXCL_BR_LINE */

    /* If already at start, no-op */
    if (workspace->cursor->byte_offset == 0) {
        return OK(NULL);
    }

    char *text;
    size_t text_len;
    ik_workspace_get_text(workspace, &text, &text_len); // Never fails

    // Defensive check: text can be NULL (lazy allocation), but cursor > 0 implies text exists
    if (text == NULL) { /* LCOV_EXCL_BR_LINE - defensive: cursor > 0 implies text != NULL */
        return OK(NULL); /* LCOV_EXCL_LINE */
    }

    ik_cursor_move_left(workspace->cursor, text, text_len);

    /* Update legacy cursor_byte_offset for backward compatibility */
    workspace->cursor_byte_offset = workspace->cursor->byte_offset;

    return OK(NULL);
}

res_t ik_workspace_cursor_right(ik_workspace_t *workspace)
{
    assert(workspace != NULL); /* LCOV_EXCL_BR_LINE */

    char *text;
    size_t text_len;
    ik_workspace_get_text(workspace, &text, &text_len); // Never fails

    /* If at end or empty text, no-op */
    if (text == NULL || workspace->cursor->byte_offset >= text_len) { /* LCOV_EXCL_BR_LINE - defensive: text NULL only when cursor at 0 */
        return OK(NULL);
    }

    ik_cursor_move_right(workspace->cursor, text, text_len);

    /* Update legacy cursor_byte_offset for backward compatibility */
    workspace->cursor_byte_offset = workspace->cursor->byte_offset;

    return OK(NULL);
}

res_t ik_workspace_get_cursor_position(ik_workspace_t *workspace, size_t *byte_out, size_t *grapheme_out)
{
    assert(workspace != NULL); /* LCOV_EXCL_BR_LINE */
    assert(byte_out != NULL); /* LCOV_EXCL_BR_LINE */
    assert(grapheme_out != NULL); /* LCOV_EXCL_BR_LINE */

    ik_cursor_get_position(workspace->cursor, byte_out, grapheme_out);
    return OK(NULL);
}
