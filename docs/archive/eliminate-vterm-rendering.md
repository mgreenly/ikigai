# Eliminate libvterm: Viewport and Rendering Details

This document describes viewport calculation, rendering algorithms, and performance analysis.

### Viewport Calculation and Rendering

```c
// Find which logical line contains a given physical row
size_t find_logical_line_at_physical_row(ik_scrollback_t *sb, size_t target_row)
{
    size_t current_row = 0;

    for (size_t i = 0; i < sb->count; i++) {
        if (current_row + sb->layouts[i].physical_lines > target_row) {
            return i;  // This logical line contains target_row
        }
        current_row += sb->layouts[i].physical_lines;
    }

    return sb->count;  // Past end
}

// Render a frame to the terminal
res_t render_frame(ik_repl_ctx_t *ctx)
{
    // Ensure all layout caches are valid
    scrollback_ensure_layout(ctx->scrollback, ctx->screen_cols);
    workspace_ensure_layout(ctx->workspace, ctx->screen_cols);

    // Calculate total content size
    size_t total_physical =
        ctx->scrollback->total_physical_lines +  // Cached
        1 +                                       // Separator
        ctx->workspace->physical_lines;           // Cached

    // Calculate viewport window (which physical lines to show)
    size_t view_end = total_physical - ctx->scroll_offset;
    size_t view_start = (view_end > ctx->screen_rows)
        ? view_end - ctx->screen_rows
        : 0;

    // Build frame buffer (single write for no flicker)
    char *framebuf = talloc_array(ctx, char, 64 * 1024);  // 64KB buffer
    size_t pos = 0;

    // Home cursor
    pos += sprintf(framebuf + pos, "\x1b[H");

    // Render visible portion of scrollback
    size_t current_physical_line = 0;
    size_t logical_line = 0;

    while (logical_line < ctx->scrollback->count &&
           current_physical_line < view_end) {
        ik_line_layout_t *layout = &ctx->scrollback->layouts[logical_line];

        if (current_physical_line + layout->physical_lines > view_start) {
            // This line is at least partially visible
            size_t offset = ctx->scrollback->text_offsets[logical_line];
            size_t length = ctx->scrollback->text_lengths[logical_line];
            memcpy(framebuf + pos, ctx->scrollback->text_buffer + offset, length);
            pos += length;
            framebuf[pos++] = '\n';
        }

        current_physical_line += layout->physical_lines;
        logical_line++;
    }

    // Render separator (if visible)
    if (current_physical_line >= view_start &&
        current_physical_line < view_end) {
        pos += sprintf(framebuf + pos, "─────────────\n");
    }
    current_physical_line++;

    // Render dynamic zone (if visible)
    if (current_physical_line < view_end) {
        memcpy(framebuf + pos,
               ctx->workspace->text->data,
               ctx->workspace->text->len);
        pos += ctx->workspace->text->len;
    }

    // Calculate cursor screen position
    ik_cursor_screen_pos_t cursor_pos;
    calculate_cursor_screen_position(
        ctx->workspace->text->data,
        ctx->workspace->text->len,
        ctx->workspace->cursor->byte_offset,
        ctx->screen_cols,
        &cursor_pos
    );

    // Position cursor (adjust for scrollback offset)
    size_t cursor_absolute_row =
        ctx->scrollback->total_physical_lines + 1 + cursor_pos.screen_row;

    if (cursor_absolute_row >= view_start && cursor_absolute_row < view_end) {
        size_t cursor_viewport_row = cursor_absolute_row - view_start;
        pos += sprintf(framebuf + pos, "\x1b[%zu;%dH",
                      cursor_viewport_row + 1,
                      cursor_pos.screen_col + 1);
    }

    // Single write to terminal (no flicker)
    ssize_t written = write(ctx->term->tty_fd, framebuf, pos);
    talloc_free(framebuf);

    if (written != (ssize_t)pos) {
        return ERR(ctx, IO, "Failed to write frame to terminal");
    }

    return OK(ctx);
}
```

**Total writes**: 1 (entire frame in single write) vs. hundreds with vterm

### Why Caching Works

The key insight is that **display width is immutable, reflow is pure arithmetic**:

**Line creation**: One-time UTF-8 scan
- Calculate display width: O(k) where k = line length
- UTF-8 decode + charwidth lookup for each codepoint
- Stored forever, never recalculated

**Terminal resize**: Rare event (user manually resizes)
- Recalculate all scrollback lines: O(n) where n = number of logical lines
- **But each recalculation is O(1)**: just `display_width / terminal_width`
- No UTF-8 scanning needed!
- For 1000 lines: 1000 integer divisions (~microseconds)

**Text edit in workspace**: Frequent (every keystroke)
- Mark `layout_dirty = true`: O(1)
- Recalculate once per frame (before rendering): O(m) where m = workspace text length
- Only one recalculation per frame, not per keystroke

**Scrollback append**: Infrequent (user submits line)
- Calculate display width: O(k) where k = new line length (one-time cost)
- Calculate physical lines: O(1) arithmetic
- Update running total: O(1)

**Rendering**: Very frequent (every frame, ~30-60 FPS)
- Use cached `total_physical_lines`: O(1)
- Use cached `physical_lines` per line: O(1)
- No recalculation needed

### Performance Analysis

**Without pre-computed display_width** (naive approach):
```c
// Every resize: O(n * m) where n = lines, m = avg line length
for each resize:
    for each logical line:
        scan UTF-8, decode, call utf8proc_charwidth()  // Expensive
```

**With pre-computed display_width** (proposed approach):
```c
// On line creation: O(m) - one time
calculate_display_width()  // Scan UTF-8 once

// On resize: O(n) - just arithmetic!
for each logical line:
    physical_lines = display_width / terminal_width  // O(1) division
```

**Typical REPL scenario**:
- 1000 scrollback lines, 50 chars average = 50,000 chars
- 256 bytes workspace text (typical), up to 4K max
- 24 rows × 80 cols terminal
- 30 FPS rendering

**Resize performance:**
- **Without pre-computation**: 50,000 chars × 50ns (UTF-8 decode + charwidth) = 2.5ms
- **With pre-computation**: 1000 lines × 2ns (integer division) = 2μs
- **1000× faster resize** (2.5ms → 2μs)

**Memory overhead for pre-computation:**
- Per line: +8 bytes (display_width field)
- For 1000 lines: +8 KB
- Negligible compared to text storage

**Cache locality improvement:**
- Hot data (layouts): 16 bytes × 1000 = 16 KB (fits in L1 cache)
- Cold data (text): Only accessed during rendering, not during reflow
- Reflow iterates tight array of layouts, excellent cache performance

### Memory Overhead

The separated storage with pre-computed display_width adds minimal memory:

```c
// Per scrollback line metadata: 16 bytes
sizeof(ik_line_layout_t) {
    size_t display_width;    // 8 bytes
    size_t physical_lines;   // 8 bytes
}

// Per scrollback line storage: 16 bytes
size_t text_offset;          // 8 bytes
size_t text_length;          // 8 bytes
// Plus actual text in contiguous buffer

// Per scrollback buffer: ~32 bytes overhead
sizeof(int cached_width)           // 4 bytes
sizeof(size_t total_physical_lines) // 8 bytes
sizeof(size_t buffer_used)         // 8 bytes
sizeof(size_t buffer_capacity)     // 8 bytes
// + padding

// Per workspace: +16 bytes
sizeof(size_t physical_lines)      // 8 bytes
sizeof(int cached_width)           // 4 bytes
sizeof(bool layout_dirty)          // 1 byte
// + padding
```

For 1000 scrollback lines:
- Metadata arrays: 32 KB (layouts + offsets + lengths)
- Text storage: Same as before (contiguous buffer)
- Buffer overhead: ~32 bytes

**Total overhead**: ~32 KB for 1000 lines - negligible compared to text storage

**Benefit**: Hot data (layouts) is tightly packed for excellent cache performance during reflow

---

