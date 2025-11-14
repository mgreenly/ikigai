#include "scrollback.h"

#include <assert.h>
#include <string.h>
#include <talloc.h>
#include <utf8proc.h>

#include "error.h"
#include "wrapper.h"

// Initial capacities for scrollback arrays
#define INITIAL_LINE_CAPACITY 16
#define INITIAL_BUFFER_CAPACITY 1024

res_t ik_scrollback_create(void *parent, int32_t terminal_width,
                            ik_scrollback_t **scrollback_out)
{
    // Validate preconditions
    assert(parent != NULL);          // LCOV_EXCL_BR_LINE
    assert(terminal_width > 0);      // LCOV_EXCL_BR_LINE
    assert(scrollback_out != NULL);  // LCOV_EXCL_BR_LINE

    // Allocate scrollback structure
    ik_scrollback_t *sb = ik_talloc_zero_wrapper(parent, sizeof(ik_scrollback_t));
    if (sb == NULL) {
        return ERR(parent, OOM, "Failed to allocate scrollback");
    }

    // Allocate line metadata arrays
    sb->text_offsets =
        ik_talloc_array_wrapper(sb, sizeof(size_t), INITIAL_LINE_CAPACITY);
    if (sb->text_offsets == NULL) {
        talloc_free(sb);
        return ERR(parent, OOM, "Failed to allocate text_offsets");
    }

    sb->text_lengths =
        ik_talloc_array_wrapper(sb, sizeof(size_t), INITIAL_LINE_CAPACITY);
    if (sb->text_lengths == NULL) {
        talloc_free(sb);
        return ERR(parent, OOM, "Failed to allocate text_lengths");
    }

    sb->layouts = ik_talloc_array_wrapper(sb, sizeof(ik_line_layout_t),
                                          INITIAL_LINE_CAPACITY);
    if (sb->layouts == NULL) {
        talloc_free(sb);
        return ERR(parent, OOM, "Failed to allocate layouts");
    }

    // Allocate text buffer
    sb->text_buffer = ik_talloc_array_wrapper(sb, sizeof(char), INITIAL_BUFFER_CAPACITY);
    if (sb->text_buffer == NULL) {
        talloc_free(sb);
        return ERR(parent, OOM, "Failed to allocate text_buffer");
    }

    // Initialize fields
    sb->count = 0;
    sb->capacity = INITIAL_LINE_CAPACITY;
    sb->cached_width = terminal_width;
    sb->total_physical_lines = 0;
    sb->buffer_used = 0;
    sb->buffer_capacity = INITIAL_BUFFER_CAPACITY;

    *scrollback_out = sb;
    return OK(sb);
}

res_t ik_scrollback_append_line(ik_scrollback_t *scrollback, const char *text,
                                 size_t length)
{
    assert(scrollback != NULL);  // LCOV_EXCL_BR_LINE
    assert(text != NULL);        // LCOV_EXCL_BR_LINE

    // Check if we need to grow the line arrays
    if (scrollback->count >= scrollback->capacity) {
        size_t new_capacity = scrollback->capacity * 2;
        TALLOC_CTX *ctx = talloc_parent(scrollback);

        size_t *new_offsets = ik_talloc_realloc_wrapper(
            ctx, scrollback->text_offsets, sizeof(size_t) * new_capacity);
        if (new_offsets == NULL) {
            return ERR(ctx, OOM, "Failed to grow text_offsets");
        }
        scrollback->text_offsets = new_offsets;

        size_t *new_lengths = ik_talloc_realloc_wrapper(
            ctx, scrollback->text_lengths, sizeof(size_t) * new_capacity);
        if (new_lengths == NULL) {
            return ERR(ctx, OOM, "Failed to grow text_lengths");
        }
        scrollback->text_lengths = new_lengths;

        ik_line_layout_t *new_layouts = ik_talloc_realloc_wrapper(
            ctx, scrollback->layouts, sizeof(ik_line_layout_t) * new_capacity);
        if (new_layouts == NULL) {
            return ERR(ctx, OOM, "Failed to grow layouts");
        }
        scrollback->layouts = new_layouts;

        scrollback->capacity = new_capacity;
    }

    // Check if we need to grow the text buffer
    if (scrollback->buffer_used + length > scrollback->buffer_capacity) {
        size_t new_buffer_capacity = scrollback->buffer_capacity * 2;
        while (new_buffer_capacity < scrollback->buffer_used + length) {
            new_buffer_capacity *= 2;
        }

        TALLOC_CTX *ctx = talloc_parent(scrollback);
        char *new_buffer =
            ik_talloc_realloc_wrapper(ctx, scrollback->text_buffer, new_buffer_capacity);
        if (new_buffer == NULL) {
            return ERR(ctx, OOM, "Failed to grow text_buffer");
        }
        scrollback->text_buffer = new_buffer;
        scrollback->buffer_capacity = new_buffer_capacity;
    }

    // Store text in buffer
    size_t offset = scrollback->buffer_used;
    if (length > 0) {
        memcpy(scrollback->text_buffer + offset, text, length);
    }

    // Calculate display width by iterating through UTF-8 characters
    size_t display_width = 0;
    size_t pos = 0;
    while (pos < length) {
        utf8proc_int32_t codepoint;
        utf8proc_ssize_t bytes =
            utf8proc_iterate((const utf8proc_uint8_t *)(text + pos), (utf8proc_ssize_t)(length - pos), &codepoint);

        if (bytes < 0) {
            // Invalid UTF-8 sequence - skip 1 byte
            pos++;
            display_width++;
            continue;
        }

        int32_t width = utf8proc_charwidth(codepoint);
        if (width < 0) {  // LCOV_EXCL_BR_LINE
            width = 0;    // LCOV_EXCL_LINE - utf8proc never returns negative in practice
        }
        display_width += (size_t)width;
        pos += (size_t)bytes;
    }

    // Calculate physical lines (ceiling division)
    size_t physical_lines = 1;  // Empty line still takes 1 physical line
    if (display_width > 0) {
        physical_lines = (display_width + (size_t)scrollback->cached_width - 1) /
                         (size_t)scrollback->cached_width;
    }

    // Store metadata
    scrollback->text_offsets[scrollback->count] = offset;
    scrollback->text_lengths[scrollback->count] = length;
    scrollback->layouts[scrollback->count].display_width = display_width;
    scrollback->layouts[scrollback->count].physical_lines = physical_lines;

    // Update counters
    scrollback->count++;
    scrollback->buffer_used += length;
    scrollback->total_physical_lines += physical_lines;

    return OK(scrollback);
}

void ik_scrollback_ensure_layout(ik_scrollback_t *scrollback,
                                  int32_t terminal_width)
{
    assert(scrollback != NULL);  // LCOV_EXCL_BR_LINE
    assert(terminal_width > 0);  // LCOV_EXCL_BR_LINE

    // If width hasn't changed, nothing to do
    if (terminal_width == scrollback->cached_width) {
        return;
    }

    // Recalculate physical_lines for all lines
    size_t new_total = 0;
    for (size_t i = 0; i < scrollback->count; i++) {
        size_t display_width = scrollback->layouts[i].display_width;

        // Calculate physical lines (ceiling division)
        size_t physical_lines = 1;  // Empty line still takes 1 physical line
        if (display_width > 0) {
            physical_lines = (display_width + (size_t)terminal_width - 1) /
                             (size_t)terminal_width;
        }

        scrollback->layouts[i].physical_lines = physical_lines;
        new_total += physical_lines;
    }

    // Update cached state
    scrollback->cached_width = terminal_width;
    scrollback->total_physical_lines = new_total;
}
