/**
 * @file core.c
 * @brief Input buffer text storage implementation
 */

#include "core.h"
#include "panic.h"
#include "wrapper.h"
#include "error.h"
#include <assert.h>
#include <stdbool.h>
#include <talloc.h>

ik_input_buffer_t *ik_input_buffer_create(void *parent)
{
    ik_input_buffer_t *input_buffer = talloc_zero_(parent, sizeof(ik_input_buffer_t));
    if (input_buffer == NULL)PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    res_t res = ik_byte_array_create(input_buffer, 64);
    if (is_err(&res))PANIC("allocation failed");  // LCOV_EXCL_BR_LINE
    input_buffer->text = res.ok;

    input_buffer->cursor = ik_input_buffer_cursor_create(input_buffer);

    input_buffer->cursor_byte_offset = 0;
    input_buffer->target_column = 0;
    input_buffer->physical_lines = 0;
    input_buffer->cached_width = 0;
    input_buffer->layout_dirty = 1;  /* Layout needs initial calculation */
    return input_buffer;
}

const char *ik_input_buffer_get_text(ik_input_buffer_t *input_buffer, size_t *len_out)
{
    assert(input_buffer != NULL);     /* LCOV_EXCL_BR_LINE */
    assert(len_out != NULL);  /* LCOV_EXCL_BR_LINE */

    *len_out = ik_byte_array_size(input_buffer->text);
    return (const char *)input_buffer->text->data;
}

void ik_input_buffer_clear(ik_input_buffer_t *input_buffer)
{
    assert(input_buffer != NULL); /* LCOV_EXCL_BR_LINE */

    ik_byte_array_clear(input_buffer->text);
    input_buffer->cursor_byte_offset = 0;
    input_buffer->target_column = 0;
    ik_input_buffer_invalidate_layout(input_buffer);  /* Invalidate layout cache */

    /* Reset cursor to position 0 */
    input_buffer->cursor->byte_offset = 0;
    input_buffer->cursor->grapheme_offset = 0;
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
    assert(out != NULL); // LCOV_EXCL_BR_LINE

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

res_t ik_input_buffer_insert_codepoint(ik_input_buffer_t *input_buffer, uint32_t codepoint)
{
    assert(input_buffer != NULL); /* LCOV_EXCL_BR_LINE */

    /* Encode codepoint to UTF-8 */
    uint8_t utf8_bytes[4];
    size_t num_bytes = encode_utf8(codepoint, utf8_bytes);
    if (num_bytes == 0) {
        return ERR(input_buffer, INVALID_ARG, "Invalid Unicode codepoint");
    }

    /* Insert bytes at cursor position */
    for (size_t i = 0; i < num_bytes; i++) {
        res_t res = ik_byte_array_insert(input_buffer->text, input_buffer->cursor_byte_offset + i, utf8_bytes[i]);
        if (is_err(&res))PANIC("allocation failed");  // LCOV_EXCL_BR_LINE
    }

    /* Advance cursor by number of bytes inserted */
    input_buffer->cursor_byte_offset += num_bytes;

    /* Reset target column on text modification */
    input_buffer->target_column = 0;
    ik_input_buffer_invalidate_layout(input_buffer);  /* Invalidate layout cache */

    /* Update cursor position */
    size_t text_len;
    const char *text = ik_input_buffer_get_text(input_buffer, &text_len);
    ik_input_buffer_cursor_set_position(input_buffer->cursor, text, text_len, input_buffer->cursor_byte_offset);

    return OK(NULL);
}

res_t ik_input_buffer_insert_newline(ik_input_buffer_t *input_buffer)
{
    assert(input_buffer != NULL); /* LCOV_EXCL_BR_LINE */

    /* Insert newline byte at cursor position */
    res_t res = ik_byte_array_insert(input_buffer->text, input_buffer->cursor_byte_offset, '\n');
    if (is_err(&res))PANIC("allocation failed");  // LCOV_EXCL_BR_LINE

    /* Advance cursor by 1 byte */
    input_buffer->cursor_byte_offset += 1;

    /* Reset target column on text modification */
    input_buffer->target_column = 0;
    ik_input_buffer_invalidate_layout(input_buffer);  /* Invalidate layout cache */

    /* Update cursor position */
    size_t text_len;
    const char *text = ik_input_buffer_get_text(input_buffer, &text_len);
    ik_input_buffer_cursor_set_position(input_buffer->cursor, text, text_len, input_buffer->cursor_byte_offset);

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
    assert(data != NULL); // LCOV_EXCL_BR_LINE
    assert(cursor_pos > 0); /* LCOV_EXCL_BR_LINE */

    /* Move back at least one byte */
    size_t pos = cursor_pos - 1;

    /* Skip UTF-8 continuation bytes (10xxxxxx) */
    while (pos > 0 && (data[pos] & 0xC0) == 0x80) {
        pos--;
    }

    return pos;
}

res_t ik_input_buffer_backspace(ik_input_buffer_t *input_buffer)
{
    assert(input_buffer != NULL); /* LCOV_EXCL_BR_LINE */

    /* If cursor is at start, this is a no-op */
    if (input_buffer->cursor_byte_offset == 0) {
        return OK(NULL);
    }

    /* Find the start of the previous UTF-8 character */
    const uint8_t *data = input_buffer->text->data;
    size_t prev_char_start = find_prev_char_start(data, input_buffer->cursor_byte_offset);

    /* Delete all bytes from prev_char_start to cursor */
    size_t num_bytes_to_delete = input_buffer->cursor_byte_offset - prev_char_start;
    for (size_t i = 0; i < num_bytes_to_delete; i++) {
        ik_byte_array_delete(input_buffer->text, prev_char_start);
    }

    /* Update cursor to the start of the deleted character */
    input_buffer->cursor_byte_offset = prev_char_start;

    /* Reset target column on text modification */
    input_buffer->target_column = 0;
    ik_input_buffer_invalidate_layout(input_buffer);  /* Invalidate layout cache */

    /* Update cursor position */
    size_t text_len;
    const char *text = ik_input_buffer_get_text(input_buffer, &text_len);
    ik_input_buffer_cursor_set_position(input_buffer->cursor, text, text_len, input_buffer->cursor_byte_offset);

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
    assert(data != NULL); // LCOV_EXCL_BR_LINE

    /* If cursor at end, return same position */
    if (cursor_pos >= data_len)return cursor_pos;  // LCOV_EXCL_LINE - defensive: caller checks cursor < data_len

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
    } else char_len = 1; /* LCOV_EXCL_LINE - Invalid UTF-8 lead byte - never occurs with valid input */

    /* Return end position (clamped to data_len) */
    size_t end_pos = cursor_pos + char_len;
    return (end_pos > data_len) ? data_len : end_pos; /* LCOV_EXCL_BR_LINE - defensive: well-formed UTF-8 won't exceed buffer */
}

res_t ik_input_buffer_delete(ik_input_buffer_t *input_buffer)
{
    assert(input_buffer != NULL); /* LCOV_EXCL_BR_LINE */

    size_t text_len = ik_byte_array_size(input_buffer->text);

    /* If cursor is at end, this is a no-op */
    if (input_buffer->cursor_byte_offset >= text_len) {
        return OK(NULL);
    }

    /* Find the end of the current UTF-8 character */
    const uint8_t *data = input_buffer->text->data;
    size_t next_char_end = find_next_char_end(data, text_len, input_buffer->cursor_byte_offset);

    /* Delete all bytes from cursor to next_char_end */
    size_t num_bytes_to_delete = next_char_end - input_buffer->cursor_byte_offset;
    for (size_t i = 0; i < num_bytes_to_delete; i++) {
        ik_byte_array_delete(input_buffer->text, input_buffer->cursor_byte_offset);
    }

    /* Cursor stays at same position (deleted forward, not backward) */

    /* Reset target column on text modification */
    input_buffer->target_column = 0;
    ik_input_buffer_invalidate_layout(input_buffer);  /* Invalidate layout cache */

    /* Update cursor position */
    const char *text = ik_input_buffer_get_text(input_buffer, &text_len);
    ik_input_buffer_cursor_set_position(input_buffer->cursor, text, text_len, input_buffer->cursor_byte_offset);

    return OK(NULL);
}

res_t ik_input_buffer_cursor_left(ik_input_buffer_t *input_buffer)
{
    assert(input_buffer != NULL); /* LCOV_EXCL_BR_LINE */

    /* If already at start, no-op */
    if (input_buffer->cursor->byte_offset == 0) {
        return OK(NULL);
    }

    size_t text_len;
    const char *text = ik_input_buffer_get_text(input_buffer, &text_len);

    // Defensive check: text can be NULL (lazy allocation), but cursor > 0 implies text exists
    if (text == NULL)return OK(NULL);  // LCOV_EXCL_LINE - defensive: cursor > 0 implies text != NULL

    ik_input_buffer_cursor_move_left(input_buffer->cursor, text, text_len);

    /* Update legacy cursor_byte_offset for backward compatibility */
    input_buffer->cursor_byte_offset = input_buffer->cursor->byte_offset;

    /* Reset target column on horizontal movement */
    input_buffer->target_column = 0;

    return OK(NULL);
}

res_t ik_input_buffer_cursor_right(ik_input_buffer_t *input_buffer)
{
    assert(input_buffer != NULL); /* LCOV_EXCL_BR_LINE */

    size_t text_len;
    const char *text = ik_input_buffer_get_text(input_buffer, &text_len);

    /* If at end or empty text, no-op */
    if (text == NULL || input_buffer->cursor->byte_offset >= text_len) { /* LCOV_EXCL_BR_LINE - defensive: text NULL only when cursor at 0 */
        return OK(NULL);
    }

    ik_input_buffer_cursor_move_right(input_buffer->cursor, text, text_len);

    /* Update legacy cursor_byte_offset for backward compatibility */
    input_buffer->cursor_byte_offset = input_buffer->cursor->byte_offset;

    /* Reset target column on horizontal movement */
    input_buffer->target_column = 0;

    return OK(NULL);
}

res_t ik_input_buffer_get_cursor_position(ik_input_buffer_t *input_buffer, size_t *byte_out, size_t *grapheme_out)
{
    assert(input_buffer != NULL); /* LCOV_EXCL_BR_LINE */
    assert(byte_out != NULL); /* LCOV_EXCL_BR_LINE */
    assert(grapheme_out != NULL); /* LCOV_EXCL_BR_LINE */

    ik_input_buffer_cursor_get_position(input_buffer->cursor, byte_out, grapheme_out);
    return OK(NULL);
}

/**
 * @brief Check if a byte is a word character (alphanumeric or UTF-8 multi-byte)
 *
 * Word characters are: a-z, A-Z, 0-9, and UTF-8 multi-byte characters.
 * Non-word characters are: space, newline, punctuation, other ASCII.
 *
 * @param byte Byte to check
 * @return true if word character, false otherwise
 */
static bool is_word_char(uint8_t byte)
{
    /* Alphanumeric: 0-9, A-Z, a-z */
    if ((byte >= '0' && byte <= '9') || (byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z')) {
        return true;
    }

    /* UTF-8 multi-byte characters (start bytes 0xC0-0xF7) */
    if (byte >= 0xC0) {
        return true;
    }

    return false;
}

/**
 * @brief Check if byte is whitespace
 *
 * @param byte Byte to check
 * @return true if whitespace, false otherwise
 */
static bool is_whitespace(uint8_t byte)
{
    return (byte == ' ' || byte == '\t' || byte == '\n' || byte == '\r');
}

/**
 * @brief Character class for word boundary detection
 */
typedef enum {
    CHAR_CLASS_WORD,        /* Alphanumeric or UTF-8 multibyte */
    CHAR_CLASS_WHITESPACE,  /* Space, tab, newline, etc. */
    CHAR_CLASS_PUNCTUATION  /* Everything else */
} char_class_t;

/**
 * @brief Get character class for a byte
 *
 * @param byte Byte to classify
 * @return Character class
 */
static char_class_t get_char_class(uint8_t byte)
{
    if (is_word_char(byte)) {
        return CHAR_CLASS_WORD;
    } else if (is_whitespace(byte)) {
        return CHAR_CLASS_WHITESPACE;
    } else {
        return CHAR_CLASS_PUNCTUATION;
    }
}

res_t ik_input_buffer_delete_word_backward(ik_input_buffer_t *input_buffer)
{
    assert(input_buffer != NULL); /* LCOV_EXCL_BR_LINE */

    /* If cursor is at start, this is a no-op */
    if (input_buffer->cursor_byte_offset == 0) {
        return OK(NULL);
    }

    const uint8_t *data = input_buffer->text->data;
    size_t pos = input_buffer->cursor_byte_offset;

    /* Step 1: Skip trailing whitespace (always skip whitespace first) */
    while (pos > 0) {
        size_t prev_pos = find_prev_char_start(data, pos);
        uint8_t byte = data[prev_pos];

        if (is_whitespace(byte)) {
            pos = prev_pos;
        } else {
            break;
        }
    }

    /* If we only had whitespace, we're done */
    if (pos == 0) {
        goto delete_range;
    }

    /* Step 2: Determine the character class at current position */
    size_t prev_pos = find_prev_char_start(data, pos);
    char_class_t target_class = get_char_class(data[prev_pos]);

    /* Step 3: Delete backward through characters of the same class */
    while (pos > 0) {
        prev_pos = find_prev_char_start(data, pos);
        char_class_t current_class = get_char_class(data[prev_pos]);

        if (current_class == target_class) {
            pos = prev_pos;
        } else {
            break;
        }
    }

delete_range:
    /* Delete from pos to cursor */
    size_t num_bytes_to_delete = input_buffer->cursor_byte_offset - pos;
    for (size_t i = 0; i < num_bytes_to_delete; i++) {
        ik_byte_array_delete(input_buffer->text, pos);
    }

    /* Update cursor */
    input_buffer->cursor_byte_offset = pos;
    size_t text_len;
    const char *text = ik_input_buffer_get_text(input_buffer, &text_len);
    ik_input_buffer_cursor_set_position(input_buffer->cursor, text, text_len, input_buffer->cursor_byte_offset);

    /* Reset target column on text modification */
    input_buffer->target_column = 0;
    ik_input_buffer_invalidate_layout(input_buffer);  /* Invalidate layout cache */

    return OK(NULL);
}
