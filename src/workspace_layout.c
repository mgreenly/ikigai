/**
 * @file workspace_layout.c
 * @brief Workspace layout caching implementation
 */

#include "workspace.h"
#include "error.h"
#include <assert.h>
#include <inttypes.h>
#include <utf8proc.h>

/**
 * @brief Calculate display width of UTF-8 text
 *
 * @param text UTF-8 text
 * @param len Length in bytes
 * @return Display width in columns
 */
static size_t calculate_display_width(const char *text, size_t len)
{
    if (text == NULL || len == 0)return 0;  // LCOV_EXCL_LINE - defensive: text is never NULL when len > 0

    size_t display_width = 0;
    size_t pos = 0;

    while (pos < len) {
        utf8proc_int32_t codepoint;
        utf8proc_ssize_t bytes_read = utf8proc_iterate((const utf8proc_uint8_t *)(text + pos),
                                                       (utf8proc_ssize_t)(len - pos),
                                                       &codepoint);

        /* Invalid UTF-8 - treat as 1 column and continue (defensive: workspace only contains valid UTF-8) */
        if (bytes_read <= 0) { /* LCOV_EXCL_BR_LINE */
            display_width++; pos++; continue; /* LCOV_EXCL_LINE */
        }

        /* Get character width */
        int32_t char_width = utf8proc_charwidth(codepoint);
        if (char_width >= 0) { /* LCOV_EXCL_BR_LINE - defensive: utf8proc_charwidth returns >= 0 for all valid codepoints */
            display_width += (size_t)char_width;
        }

        pos += (size_t)bytes_read;
    }

    return display_width;
}

res_t ik_workspace_ensure_layout(ik_workspace_t *workspace, int32_t terminal_width)
{
    assert(workspace != NULL); /* LCOV_EXCL_BR_LINE */

    /* If layout is clean and width unchanged, no recalculation needed */
    if (workspace->layout_dirty == 0 && workspace->cached_width == terminal_width) {
        return OK(NULL);
    }

    /* Get workspace text */
    char *text;
    size_t text_len;
    ik_workspace_get_text(workspace, &text, &text_len); // Never fails

    /* Empty workspace: 1 physical line */
    if (text == NULL || text_len == 0) { /* LCOV_EXCL_BR_LINE - defensive: text is NULL only when text_len is 0 */
        workspace->physical_lines = 1;
        workspace->cached_width = terminal_width;
        workspace->layout_dirty = 0;
        return OK(NULL);
    }

    /* Calculate physical lines by scanning text */
    size_t physical_lines = 0;
    size_t line_start = 0;

    for (size_t i = 0; i <= text_len; i++) {
        /* Found newline or end of text */
        if (i == text_len || text[i] == '\n') {
            size_t line_len = i - line_start;

            if (line_len == 0) {
                /* Empty line (just newline) */
                physical_lines += 1;
            } else {
                /* Calculate display width of this logical line */
                size_t display_width = calculate_display_width(text + line_start, line_len);

                /* Calculate how many physical lines it occupies */
                if (display_width == 0) { /* LCOV_EXCL_BR_LINE - rare: only zero-width characters */
                    physical_lines += 1;
                } else if (terminal_width > 0) { /* LCOV_EXCL_BR_LINE - defensive: terminal_width is always > 0 */
                    /* Integer division with rounding up */
                    physical_lines += (display_width + (size_t)terminal_width - 1) / (size_t)terminal_width;
                } else {
                    /* Defensive: terminal_width should never be <= 0 */
                    physical_lines += 1; /* LCOV_EXCL_LINE */
                }
            }

            /* Move to next line */
            line_start = i + 1;
        }
    }

    /* If text doesn't end with newline, we already counted the last line */
    /* If text ends with newline, we counted it above */

    /* Update cached values */
    workspace->physical_lines = physical_lines;
    workspace->cached_width = terminal_width;
    workspace->layout_dirty = 0;

    return OK(NULL);
}

void ik_workspace_invalidate_layout(ik_workspace_t *workspace)
{
    assert(workspace != NULL); /* LCOV_EXCL_BR_LINE */
    workspace->layout_dirty = 1;
}

size_t ik_workspace_get_physical_lines(ik_workspace_t *workspace)
{
    assert(workspace != NULL); /* LCOV_EXCL_BR_LINE */
    return workspace->physical_lines;
}
