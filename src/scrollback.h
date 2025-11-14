#ifndef IK_SCROLLBACK_H
#define IK_SCROLLBACK_H

#include <inttypes.h>
#include <stddef.h>

#include "error.h"

// Layout metadata for a single line in scrollback
typedef struct {
    size_t display_width;    // Visual width (grapheme clusters)
    size_t physical_lines;   // Number of wrapped lines at cached width
} ik_line_layout_t;

// Scrollback buffer: stores historical output with layout caching
// Design: Separate hot (layouts) and cold (text) data for cache locality
typedef struct ik_scrollback_t {
    char *text_buffer;              // Packed text storage
    size_t *text_offsets;           // Offset of each line in text_buffer
    size_t *text_lengths;           // Byte length of each line
    ik_line_layout_t *layouts;      // Pre-computed layout metadata (hot)
    size_t count;                   // Number of lines stored
    size_t capacity;                // Allocated capacity for arrays
    int32_t cached_width;           // Terminal width for cached layouts
    size_t total_physical_lines;    // Sum of all physical_lines
    size_t buffer_used;             // Bytes used in text_buffer
    size_t buffer_capacity;         // Allocated size of text_buffer
} ik_scrollback_t;

// Create a new scrollback buffer
// parent: talloc context for allocation
// terminal_width: initial terminal width for layout calculation
// scrollback_out: output pointer for created scrollback
// Returns: OK on success, ERR on allocation failure
res_t ik_scrollback_create(void *parent, int32_t terminal_width,
                            ik_scrollback_t **scrollback_out);

// Append a line to the scrollback buffer
// scrollback: the scrollback buffer
// text: the text to append (may contain UTF-8)
// length: byte length of the text
// Returns: OK on success, ERR on allocation failure
res_t ik_scrollback_append_line(ik_scrollback_t *scrollback, const char *text,
                                 size_t length);

// Ensure layout cache is current for given terminal width
// scrollback: the scrollback buffer
// terminal_width: current terminal width
// If width matches cached_width, does nothing (O(1))
// If width differs, recalculates all physical_lines (O(n) arithmetic)
void ik_scrollback_ensure_layout(ik_scrollback_t *scrollback,
                                  int32_t terminal_width);

#endif  // IK_SCROLLBACK_H
