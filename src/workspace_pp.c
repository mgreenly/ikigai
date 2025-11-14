/**
 * @file workspace_pp.c
 * @brief Workspace pretty-print implementation
 */

#include "workspace.h"
#include "workspace_cursor.h"
#include "format.h"
#include "byte_array.h"
#include "error.h"
#include <assert.h>
#include <inttypes.h>

/**
 * @brief Escape a string for display (convert newlines to \n, etc.)
 *
 * @param str String to escape
 * @param len Length of string in bytes
 * @param buf Format buffer to append to
 */
static void escape_string_to_buffer(const char *str, size_t len, ik_format_buffer_t *buf)
{
    for (size_t i = 0; i < len; i++) {
        char c = str[i];
        if (c == '\n') {
            ik_format_append(buf, "\\n");
        } else if (c == '\r') {
            ik_format_append(buf, "\\r");
        } else if (c == '\t') {
            ik_format_append(buf, "\\t");
        } else if (c == '\\') {
            ik_format_append(buf, "\\\\");
        } else if (c == '"') {
            ik_format_append(buf, "\\\"");
        } else if ((uint8_t)c < 32 || (uint8_t)c == 127) {
            /* Control characters - show as hex */
            ik_format_appendf(buf, "\\x%02x", (uint8_t)c);
        } else {
            /* Regular character (including UTF-8 bytes) */
            ik_format_appendf(buf, "%c", c);
        }
    }
}

void ik_pp_workspace(const ik_workspace_t *workspace, ik_format_buffer_t *buf, int32_t indent)
{
    assert(workspace != NULL); /* LCOV_EXCL_BR_LINE */
    assert(buf != NULL); /* LCOV_EXCL_BR_LINE */

    /* Print header with workspace address */
    ik_format_indent(buf, indent);
    ik_format_appendf(buf, "ik_workspace_t @ %p:\n", (const void *)workspace);

    /* Get text buffer */
    char *text = NULL;
    size_t text_len = 0;
    if (workspace->text != NULL) { /* LCOV_EXCL_BR_LINE - defensive: text always allocated in create */
        text = (char *)workspace->text->data;
        text_len = ik_byte_array_size(workspace->text);
    }

    /* Print text length */
    ik_format_indent(buf, indent);
    ik_format_appendf(buf, "  text length: %zu\n", text_len);

    /* Get cursor position */
    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    if (workspace->cursor != NULL) { /* LCOV_EXCL_BR_LINE - defensive: cursor always allocated in create */
        ik_cursor_get_position(workspace->cursor, &byte_offset, &grapheme_offset);
    }

    /* Print cursor positions */
    ik_format_indent(buf, indent);
    ik_format_appendf(buf, "  cursor byte: %zu\n", byte_offset);

    ik_format_indent(buf, indent);
    ik_format_appendf(buf, "  cursor grapheme: %zu\n", grapheme_offset);

    /* Print target column */
    ik_format_indent(buf, indent);
    ik_format_appendf(buf, "  target_column: %zu\n", workspace->target_column);

    /* Print text content (with escaping) */
    if (text_len > 0 && text != NULL) { /* LCOV_EXCL_BR_LINE - defensive: text always non-NULL when text_len > 0 */
        ik_format_indent(buf, indent);
        ik_format_append(buf, "  text: \"");
        escape_string_to_buffer(text, text_len, buf);
        ik_format_append(buf, "\"\n");
    }
}
