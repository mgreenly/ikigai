#include "pp_helpers.h"
#include <assert.h>
#include <stdio.h>

void ik_pp_header(ik_format_buffer_t *buf, int32_t indent, const char *type, const void *ptr)
{
    assert(buf != NULL); /* LCOV_EXCL_BR_LINE */
    assert(type != NULL); /* LCOV_EXCL_BR_LINE */

    ik_format_indent(buf, indent);
    ik_format_appendf(buf, "%s @ %p\n", type, ptr);
}

void ik_pp_pointer(ik_format_buffer_t *buf, int32_t indent, const char *name, const void *ptr)
{
    assert(buf != NULL); /* LCOV_EXCL_BR_LINE */
    assert(name != NULL); /* LCOV_EXCL_BR_LINE */

    ik_format_indent(buf, indent);
    if (ptr == NULL) {
        ik_format_appendf(buf, "%s: NULL\n", name);
    } else {
        ik_format_appendf(buf, "%s: %p\n", name, ptr);
    }
}

void ik_pp_size_t(ik_format_buffer_t *buf, int32_t indent, const char *name, size_t value)
{
    assert(buf != NULL); /* LCOV_EXCL_BR_LINE */
    assert(name != NULL); /* LCOV_EXCL_BR_LINE */

    ik_format_indent(buf, indent);
    ik_format_appendf(buf, "%s: %zu\n", name, value);
}

void ik_pp_int32(ik_format_buffer_t *buf, int32_t indent, const char *name, int32_t value)
{
    assert(buf != NULL); /* LCOV_EXCL_BR_LINE */
    assert(name != NULL); /* LCOV_EXCL_BR_LINE */

    ik_format_indent(buf, indent);
    ik_format_appendf(buf, "%s: %" PRId32 "\n", name, value);
}

void ik_pp_uint32(ik_format_buffer_t *buf, int32_t indent, const char *name, uint32_t value)
{
    assert(buf != NULL); /* LCOV_EXCL_BR_LINE */
    assert(name != NULL); /* LCOV_EXCL_BR_LINE */

    ik_format_indent(buf, indent);
    ik_format_appendf(buf, "%s: %" PRIu32 "\n", name, value);
}

void ik_pp_string(ik_format_buffer_t *buf, int32_t indent, const char *name, const char *str, size_t len)
{
    assert(buf != NULL); /* LCOV_EXCL_BR_LINE */
    assert(name != NULL); /* LCOV_EXCL_BR_LINE */

    ik_format_indent(buf, indent);
    ik_format_appendf(buf, "%s: ", name);

    if (str == NULL) {
        ik_format_append(buf, "NULL\n");
        return;
    }

    ik_format_append(buf, "\"");

    // Escape special characters
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)str[i];
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
        } else if (c < 32 || c == 127) {
            // Control characters and DEL
            ik_format_appendf(buf, "\\x%02x", c);
        } else {
            // Regular character
            ik_format_appendf(buf, "%c", c);
        }
    }

    ik_format_append(buf, "\"\n");
}

void ik_pp_bool(ik_format_buffer_t *buf, int32_t indent, const char *name, bool value)
{
    assert(buf != NULL); /* LCOV_EXCL_BR_LINE */
    assert(name != NULL); /* LCOV_EXCL_BR_LINE */

    ik_format_indent(buf, indent);
    ik_format_appendf(buf, "%s: %s\n", name, value ? "true" : "false");
}
