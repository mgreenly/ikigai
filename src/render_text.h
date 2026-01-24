// Text processing utilities for rendering
#ifndef IK_RENDER_TEXT_H
#define IK_RENDER_TEXT_H

#include <inttypes.h>
#include <stddef.h>

// Count newlines in text
size_t ik_render_count_newlines(const char *text, size_t text_len);

// Copy text to buffer, converting \n to \r\n
// Returns number of bytes written to dest
size_t ik_render_copy_text_with_crlf(char *dest, const char *src, size_t src_len);

#endif // IK_RENDER_TEXT_H
