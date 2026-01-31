/**
 * @file scrollback_utils.c
 * @brief Utility functions for scrollback text analysis
 */

#include "scrollback_utils.h"

#include "ansi.h"
#include "output_style.h"
#include "panic.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <talloc.h>
#include <utf8proc.h>


#include "poison.h"
size_t ik_scrollback_calculate_display_width(const char *text, size_t length)
{
    size_t display_width = 0;
    size_t pos = 0;

    while (pos < length) {
        // Skip ANSI escape sequences
        size_t skip = ik_ansi_skip_csi(text, length, pos);
        if (skip > 0) {
            pos += skip;
            continue;
        }

        utf8proc_int32_t cp;
        utf8proc_ssize_t bytes = utf8proc_iterate(
            (const utf8proc_uint8_t *)(text + pos),
            (utf8proc_ssize_t)(length - pos),
            &cp);

        if (bytes <= 0) {
            display_width++;
            pos++;
            continue;
        }

        if (cp == '\n') {
            pos += (size_t)bytes;
            continue;
        }

        int32_t width = utf8proc_charwidth(cp);
        if (width > 0) {
            display_width += (size_t)width;
        }

        pos += (size_t)bytes;
    }

    return display_width;
}

size_t ik_scrollback_count_newlines(const char *text, size_t length)
{
    size_t newline_count = 0;
    size_t pos = 0;

    while (pos < length) {
        // Skip ANSI escape sequences
        size_t skip = ik_ansi_skip_csi(text, length, pos);
        if (skip > 0) {
            pos += skip;
            continue;
        }

        // Decode UTF-8 codepoint
        utf8proc_int32_t cp;
        utf8proc_ssize_t bytes = utf8proc_iterate(
            (const utf8proc_uint8_t *)(text + pos),
            (utf8proc_ssize_t)(length - pos),
            &cp);

        if (bytes <= 0) {
            pos++;
            continue;
        }

        if (cp == '\n') {
            newline_count++;
        }

        pos += (size_t)bytes;
    }

    return newline_count;
}

char *ik_scrollback_trim_trailing(void *parent, const char *text, size_t length)
{
    assert(parent != NULL);  // LCOV_EXCL_BR_LINE

    if (text == NULL || length == 0) {
        char *result = talloc_strdup(parent, "");
        if (result == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        return result;
    }

    // Find last non-whitespace character
    size_t end = length;
    while (end > 0) {
        char c = text[end - 1];
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            break;
        }
        end--;
    }

    char *result = talloc_strndup(parent, text, end);
    if (result == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    return result;
}

char *ik_scrollback_format_warning(void *parent, const char *text)
{
    assert(parent != NULL);  // LCOV_EXCL_BR_LINE
    assert(text != NULL);  // LCOV_EXCL_BR_LINE

    const char *prefix = ik_output_prefix(IK_OUTPUT_WARNING);
    int32_t color_code = ik_output_color(IK_OUTPUT_WARNING);

    // If colors are disabled or color is default, return text with prefix only
    if (!ik_ansi_colors_enabled() || color_code < 0) {
        char *result = talloc_asprintf(parent, "%s %s", prefix, text);
        if (result == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        return result;
    }

    // Format with color: prefix + color + text + reset
    uint8_t color = (uint8_t)color_code;
    char color_seq[16];
    ik_ansi_fg_256(color_seq, sizeof(color_seq), color);

    char *result = talloc_asprintf(parent, "%s%s %s%s", color_seq, prefix, text, IK_ANSI_RESET);
    if (result == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    return result;
}
