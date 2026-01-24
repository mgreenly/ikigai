// Text processing utilities for rendering
#include "render_text.h"

#include <assert.h>

// Count newlines in text
size_t ik_render_count_newlines(const char *text, size_t text_len)
{
    assert(text != NULL || text_len == 0); // LCOV_EXCL_BR_LINE

    size_t count = 0;
    for (size_t i = 0; i < text_len; i++) {
        if (text[i] == '\n') {
            count++;
        }
    }
    return count;
}

// Copy text to buffer, converting \n to \r\n
// Returns number of bytes written to dest
size_t ik_render_copy_text_with_crlf(char *dest, const char *src, size_t src_len)
{
    assert(dest != NULL);             // LCOV_EXCL_BR_LINE
    assert(src != NULL || src_len == 0); // LCOV_EXCL_BR_LINE

    size_t offset = 0;
    for (size_t i = 0; i < src_len; i++) {
        if (src[i] == '\n') {
            dest[offset++] = '\r';
            dest[offset++] = '\n';
        } else {
            dest[offset++] = src[i];
        }
    }
    return offset;
}
