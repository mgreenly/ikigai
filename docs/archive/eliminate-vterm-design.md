# Eliminate libvterm: Design Overview

This document describes the high-level architecture and data structures for direct terminal rendering.

## Proposed Solution

### Architecture Overview

The direct rendering approach manages two types of content with different performance characteristics:

1. **Scrollback buffer**: Immutable history (hundreds/thousands of logical lines)
2. **Dynamic zone (workspace)**: Editable input area (typically < 100 lines, max ~128)

Both require wrapping calculations, but we optimize by caching physical line counts and only recalculating on terminal resize or content changes.

### Core Data Structures

```c
// Layout metadata (hot data - accessed during reflow and rendering)
typedef struct {
    size_t display_width;          // Pre-computed: sum of all charwidths (never changes)
    size_t physical_lines;         // Cached: wraps to N lines at cached_width
} ik_line_layout_t;

// Scrollback buffer with separated hot/cold data
typedef struct {
    // Cold data: text content (only accessed during rendering)
    char *text_buffer;             // All line text in one contiguous buffer
    size_t *text_offsets;          // Where each line starts in text_buffer
    size_t *text_lengths;          // Length in bytes of each line

    // Hot data: layout metadata (accessed during reflow and rendering)
    ik_line_layout_t *layouts;     // Parallel array of layout info

    size_t count;                  // Number of lines
    size_t capacity;               // Allocated capacity
    int cached_width;              // Terminal width for cached physical_lines
    size_t total_physical_lines;   // Cached: sum of all physical_lines
    size_t buffer_used;            // Bytes used in text_buffer
    size_t buffer_capacity;        // Total buffer capacity
} ik_scrollback_t;

// Dynamic zone (workspace) with layout cache
typedef struct {
    ik_byte_array_t *text;         // UTF-8 bytes (can contain \n)
    ik_cursor_t *cursor;           // Byte + grapheme offsets

    // Layout cache (invalidated on text change or resize)
    size_t physical_lines;         // Cached: total wrapped lines
    int cached_width;              // Width this is valid for
    bool layout_dirty;             // Need to recalculate
} ik_workspace_t;

// Screen position for cursor
typedef struct {
    int screen_row;                // Physical row (0-based)
    int screen_col;                // Physical column (0-based)
} ik_cursor_screen_pos_t;
```

**Key design decisions:**

1. **No newline characters stored**: Each array entry represents one logical line. The newline is implicit in the array structure. All newline types (LF, CR, CRLF, etc.) are handled the same - they signal line boundaries but aren't preserved in storage.

2. **Separated storage**: Hot data (layout metadata) is separate from cold data (text content) for better cache locality during reflow operations.

3. **Pre-computed display width**: The sum of all character display widths is computed once when the line is created and never changes. This makes reflow O(1) per line.

### Caching Strategy

The key insight: **Pre-compute display width once, reflow becomes pure arithmetic**.

| Component | What to Cache | When to Compute |
|-----------|---------------|-----------------|
| **Scrollback display_width** | Sum of charwidths per line | Once when line created (never changes) |
| **Scrollback physical_lines** | Wrapped line count | Terminal resize: `display_width / terminal_width` |
| **Scrollback total** | `total_physical_lines` | Terminal resize: sum all physical_lines |
| **Workspace layout** | `physical_lines` total | Terminal resize or text edit |
| **Cursor position** | Don't cache | Calculate on-demand (cheap: O(cursor_offset)) |

**Why this works:**
- **Line creation**: Scan UTF-8 once to compute `display_width` - one-time cost
- **Terminal resize**: O(1) arithmetic per line (`display_width / terminal_width`) - no UTF-8 scanning
- **Text edit in workspace**: Set `layout_dirty = true`, recalculate once per frame
- **Rendering**: Use cached values, no recalculation needed

**Performance improvement:**
- Old approach: Resize requires full UTF-8 scan of all lines (50,000 chars)
- New approach: Resize is just division (1000 lines × 1 division = ~1000 ops)
- **50× faster reflow**

### Core Calculation Functions

```c
// Calculate display width of a logical line (sum of all charwidths)
// This is computed ONCE when the line is created
size_t calculate_display_width(const char *text, size_t text_len)
{
    size_t width = 0;
    size_t pos = 0;

    while (pos < text_len) {
        // Decode UTF-8
        utf8proc_int32_t cp;
        utf8proc_ssize_t bytes = utf8proc_iterate(
            (const uint8_t *)(text + pos),
            text_len - pos,
            &cp
        );

        if (bytes <= 0) break;  // Invalid UTF-8

        // Get display width (1 for ASCII, 2 for CJK, 0 for combining)
        int char_width = utf8proc_charwidth(cp);
        width += char_width;
        pos += bytes;
    }

    return width;
}

// Calculate how many physical lines a logical line wraps to
// This is O(1) arithmetic using pre-computed display_width
size_t calculate_physical_lines(size_t display_width, int terminal_width)
{
    if (display_width == 0) {
        return 1;  // Empty lines take 1 physical line
    }

    // Integer division with ceiling
    return (display_width + terminal_width - 1) / terminal_width;
}

// Calculate cursor screen position (row, col) from byte offset
res_t calculate_cursor_screen_position(
    const char *text,
    size_t text_len,
    size_t cursor_byte_offset,
    int terminal_width,
    ik_cursor_screen_pos_t *pos_out
) {
    int row = 0;
    int col = 0;
    size_t pos = 0;

    // Iterate through text up to cursor position
    while (pos < cursor_byte_offset) {
        // Handle newlines
        if (text[pos] == '\n') {
            row++;
            col = 0;
            pos++;
            continue;
        }

        // Decode UTF-8 codepoint
        utf8proc_int32_t cp;
        utf8proc_ssize_t bytes = utf8proc_iterate(
            (const uint8_t *)(text + pos),
            text_len - pos,
            &cp
        );

        if (bytes <= 0) {
            return ERR(NULL, INVALID_ARG, "Invalid UTF-8");
        }

        // Get display width (accounts for wide chars like CJK)
        int width = utf8proc_charwidth(cp);

        // Check for line wrap
        if (col + width > terminal_width) {
            row++;
            col = 0;
        }

        col += width;
        pos += bytes;
    }

    pos_out->screen_row = row;
    pos_out->screen_col = col;
    return OK(NULL);
}
```

### Cache Management Functions

```c
// Ensure scrollback cache is valid for current terminal width
void scrollback_ensure_layout(ik_scrollback_t *sb, int terminal_width)
{
    if (sb->cached_width == terminal_width) {
        return;  // Cache valid
    }

    // Recalculate all lines using pre-computed display_width (O(1) per line)
    sb->total_physical_lines = 0;

    for (size_t i = 0; i < sb->count; i++) {
        // Pure arithmetic - no UTF-8 scanning needed!
        sb->layouts[i].physical_lines = calculate_physical_lines(
            sb->layouts[i].display_width,
            terminal_width
        );
        sb->total_physical_lines += sb->layouts[i].physical_lines;
    }

    sb->cached_width = terminal_width;
}

// Append new line to scrollback
res_t scrollback_append_line(ik_scrollback_t *sb, const char *text, size_t len)
{
    // Grow arrays if needed
    if (sb->count >= sb->capacity) {
        size_t new_cap = sb->capacity * 2;

        // Grow layout array
        ik_line_layout_t *new_layouts = talloc_realloc(
            sb, sb->layouts, ik_line_layout_t, new_cap
        );
        if (!new_layouts) {
            return ERR(sb, OOM, "Failed to grow layout array");
        }
        sb->layouts = new_layouts;

        // Grow offset/length arrays
        size_t *new_offsets = talloc_realloc(sb, sb->text_offsets, size_t, new_cap);
        size_t *new_lengths = talloc_realloc(sb, sb->text_lengths, size_t, new_cap);
        if (!new_offsets || !new_lengths) {
            return ERR(sb, OOM, "Failed to grow offset/length arrays");
        }
        sb->text_offsets = new_offsets;
        sb->text_lengths = new_lengths;
        sb->capacity = new_cap;
    }

    // Grow text buffer if needed
    if (sb->buffer_used + len > sb->buffer_capacity) {
        size_t new_cap = sb->buffer_capacity * 2;
        while (new_cap < sb->buffer_used + len) {
            new_cap *= 2;
        }
        char *new_buffer = talloc_realloc(sb, sb->text_buffer, char, new_cap);
        if (!new_buffer) {
            return ERR(sb, OOM, "Failed to grow text buffer");
        }
        sb->text_buffer = new_buffer;
        sb->buffer_capacity = new_cap;
    }

    // Store text in contiguous buffer
    sb->text_offsets[sb->count] = sb->buffer_used;
    sb->text_lengths[sb->count] = len;
    memcpy(sb->text_buffer + sb->buffer_used, text, len);
    sb->buffer_used += len;

    // Calculate display width (one-time cost)
    sb->layouts[sb->count].display_width = calculate_display_width(text, len);

    // Calculate physical lines at current width
    sb->layouts[sb->count].physical_lines = calculate_physical_lines(
        sb->layouts[sb->count].display_width,
        sb->cached_width
    );

    // Update totals
    sb->total_physical_lines += sb->layouts[sb->count].physical_lines;
    sb->count++;

    return OK(sb);
}

// Ensure workspace cache is valid for current terminal width
void workspace_ensure_layout(ik_workspace_t *ws, int terminal_width)
{
    if (!ws->layout_dirty && ws->cached_width == terminal_width) {
        return;  // Cache valid
    }

    // Recalculate (happens on resize or after text edits)
    // Note: Workspace needs full scan because it's mutable and may contain \n
    size_t lines = 1;
    size_t col = 0;
    size_t pos = 0;

    while (pos < ws->text->len) {
        if (ws->text->data[pos] == '\n') {
            lines++;
            col = 0;
            pos++;
            continue;
        }

        utf8proc_int32_t cp;
        utf8proc_ssize_t bytes = utf8proc_iterate(
            (const uint8_t *)(ws->text->data + pos),
            ws->text->len - pos,
            &cp
        );

        if (bytes <= 0) break;

        int width = utf8proc_charwidth(cp);
        if (col + width > terminal_width) {
            lines++;
            col = 0;
        }

        col += width;
        pos += bytes;
    }

    ws->physical_lines = lines;
    ws->cached_width = terminal_width;
    ws->layout_dirty = false;
}

// Invalidate workspace layout cache (call on any text edit)
void workspace_invalidate_layout(ik_workspace_t *ws)
{
    ws->layout_dirty = true;
}
```

### Viewport Calculation and Rendering

## Continued

See [Viewport and Rendering Details](eliminate-vterm-rendering.md) for viewport calculation and rendering implementation details.
