# REPL Terminal - Phase 2: Implementation Details

[← Back to Phase 2 Overview](repl-phase-2.md)

This document contains detailed implementation specifications for Phase 2.

## Input Flow

All keyboard and mouse input flows through the terminal emulator (Ghostty/Kitty):
- Terminal receives OS-level events
- Encodes them as byte sequences (regular chars or escape sequences)
- Writes to app's stdin
- App reads in raw mode and processes

## Progressive Input Implementation

1. **Initial REPL** - Basic reliable keys only:
   - Regular typing (letters, numbers, punctuation)
   - Enter (submit line to scrollback)
   - Backspace (delete character)
   - Arrow keys (cursor movement within dynamic zone)
     - Left/Right: move horizontally within line
     - Up/Down: move vertically between lines in dynamic zone
     - Up at top of dynamic zone: no-op
     - Down at bottom: no-op

2. **Before completing current work** - Add scrolling:
   - Mouse wheel scrolling:
     - Enable mouse tracking mode (`\x1b[?1000h` + `\x1b[?1006h`)
     - Parse mouse wheel events from escape sequences
     - Adjust scroll position and re-render
   - Page Up/Down: scroll by larger increments

3. **Future enhancement** - Kitty keyboard protocol:
   - Unambiguous encoding for all key combos
   - Distinguishes ESC vs Alt
   - Optional, with graceful fallback

## Cursor Management

Two distinct cursor concepts:

1. **Logical cursor** - Position in dynamic zone text
   - Tracked with dual representation for efficiency:
     - `cursor_byte_offset` - Byte position in UTF-8 string (for text operations)
     - `cursor_grapheme_offset` - Grapheme cluster count (for movement)
   - Updated by editing operations (insert, backspace, arrow keys)
   - Arrow left/right: move by one grapheme cluster
     - Use libutf8proc to find grapheme boundaries
     - Update both byte and grapheme offsets
   - Arrow up/down: move vertically between wrapped lines
     - Calculate which wrapped line cursor is on
     - Move to previous/next wrapped line, preserve horizontal position

2. **Screen cursor** - Visual position on terminal
   - Managed by vterm during rendering
   - Calculated from scroll position + layout + logical cursor
   - May be off-screen when scrolled up viewing history

## Snap-back behavior

- When typing any key (including Enter) while cursor is off-screen
- Viewport scrolls to position cursor line at bottom of screen
- Ensures user can see what they're typing
- Happens before processing the keystroke

## Scrollback Line Format

Variable-length logical lines (NOT fixed-width grid):
- Each line stores semantic content: "user: hello" or "ai: long response..."
- Lines can be any length (UTF-8 strings)
- Render module handles wrapping to current terminal width
- **Terminal resize**: Just re-render with new dimensions, no buffer modification needed
- Simpler than fixed-width approach (no reflow logic required)

## Line Processing Pipeline

When user presses Enter, the line dispatcher examines the input and decides action:

1. **Very first draft** - Simple flow:
   - All entered lines → append to scrollback buffer
   - Ctrl+C → exit program (simplest escape hatch)

2. **Later in current work** - Add command processor:
   - Lines starting with `/` → dispatch to command handler
   - `/exit` → cleanup and exit program (doesn't go to scrollback)
   - Regular lines (not starting with `/`) → append to scrollback buffer
   - Leaves room for future commands: `/clear`, `/help`, etc.

3. **Future (with AI integration)** - Enhanced dispatcher:
   - `/ask <question>` → send to AI, wait for response, append to scrollback
   - Regular lines → context for AI conversation
   - Commands → special actions

**Dispatcher responsibilities**:
- Parse line to determine type (command vs regular text)
- Route to appropriate handler (command processor, buffer append, AI sender)
- Return action to main loop (continue, exit, etc.)

## Module Organization

- **repl module** - Main context, owns all data structures, provides init/cleanup/run
- **terminal module** - Raw mode, alternate screen, termios state
- **buffer module** - Scrollback buffer and input buffer management
- **render module** - vterm ownership, compose and blit operations
- **input module** - Process raw byte sequences, dispatch to buffer/render
- **expandable array utility** - Generic reusable array (for scrollback and future needs)

## Memory Strategy

- Use talloc for all allocations (consistent with existing codebase)
- Main context owns everything via talloc hierarchy
- Cleanup is automatic with talloc_free()
- Expandable arrays use talloc_realloc() for growth

## Data Structures

```c
typedef struct {
    // Real terminal state
    int tty_fd;
    struct termios orig_termios;

    // Scrollback buffer (manual management)
    char **scrollback;           // talloc array of strings (1000s of lines)
    size_t scrollback_count;     // Total lines in buffer
    size_t scroll_line_offset;   // Lines scrolled up from bottom (0 = at bottom)

    // Dynamic zone (current input - multi-line)
    char *dynamic_text;          // Current text being edited (UTF-8 bytes, may contain newlines)
    size_t cursor_byte_offset;   // Byte position in dynamic_text (for insertion/deletion)
    size_t cursor_grapheme_offset; // Grapheme cluster count from start (for arrow key movement)
    size_t dynamic_scroll_offset; // Internal scroll offset within dynamic zone (if taller than screen)

    // vterm for rendering entire screen
    VTerm *vterm;                // Full screen size (cols x rows)
    VTermScreen *vscreen;

    // Screen dimensions
    int screen_rows;             // e.g., 24
    int screen_cols;             // e.g., 80
} ik_term_ctx_t;
```

## Rendering Pipeline

**Compose → Blit → Display**

```c
void render_frame(ik_term_ctx_t *ctx) {
    // 1. COMPOSE: Build what should be visible in vterm
    vterm_screen_reset(ctx->vscreen, 1);  // Clear vterm

    // Calculate total buffer height (lines that could be rendered)
    size_t dynamic_lines = count_wrapped_lines(ctx->dynamic_text, ctx->screen_cols);
    size_t total_buffer_lines = ctx->scrollback_count + 1 + dynamic_lines; // +1 for separator

    // Determine which line range to show based on scroll_line_offset
    // When scroll_line_offset == 0, we show the bottom of the buffer
    size_t view_end = total_buffer_lines - ctx->scroll_line_offset;
    size_t view_start = (view_end > ctx->screen_rows) ? view_end - ctx->screen_rows : 0;

    // Render visible portion of continuous buffer
    size_t current_line = view_start;
    size_t vterm_row = 0;

    // Render scrollback lines (if visible)
    while (current_line < ctx->scrollback_count && vterm_row < ctx->screen_rows) {
        if (current_line >= view_start) {
            vterm_input_write(ctx->vterm, ctx->scrollback[current_line],
                             strlen(ctx->scrollback[current_line]));
            vterm_input_write(ctx->vterm, "\n", 1);
            vterm_row++;
        }
        current_line++;
    }

    // Render separator line (if visible)
    if (current_line == ctx->scrollback_count && vterm_row < ctx->screen_rows) {
        vterm_input_write(ctx->vterm, "─────────────\n", ...);
        vterm_row++;
        current_line++;
    }

    // Render dynamic zone lines (if visible)
    if (current_line > ctx->scrollback_count && vterm_row < ctx->screen_rows) {
        size_t dynamic_start_line = current_line - ctx->scrollback_count - 1;
        render_dynamic_zone_portion(ctx, dynamic_start_line,
                                   ctx->screen_rows - vterm_row);
    }

    // 2. BLIT: Copy vterm cells to real terminal (single write)
    blit_vterm_to_screen(ctx);
}

void blit_vterm_to_screen(ik_term_ctx_t *ctx) {
    // Build entire frame in memory buffer
    char *framebuf = talloc_array(NULL, char, ctx->screen_rows * ctx->screen_cols * 4);
    size_t pos = 0;

    // Home cursor (NO clear - just overwrite)
    pos += sprintf(framebuf + pos, "\033[H");

    // Copy all vterm cells
    for (int row = 0; row < ctx->screen_rows; row++) {
        for (int col = 0; col < ctx->screen_cols; col++) {
            VTermScreenCell cell;
            vterm_screen_get_cell(ctx->vscreen, (VTermPos){row, col}, &cell);
            pos += encode_utf8(cell.chars[0], framebuf + pos);
        }
        framebuf[pos++] = '\n';
    }

    // Position cursor
    VTermPos cursorpos;
    vterm_state_get_cursorpos(vterm_obtain_state(ctx->vterm), &cursorpos);
    pos += sprintf(framebuf + pos, "\033[%d;%dH", cursorpos.row + 1, cursorpos.col + 1);

    // Single write to terminal (no flicker)
    write(STDOUT_FILENO, framebuf, pos);

    talloc_free(framebuf);
}
```

**Performance characteristics**:
- No screen clear needed (just overwrite cells)
- Single `write()` syscall (no flicker)
- ~2-3ms per frame for 80x24 screen
- Smooth at even 30fps (33ms budget)

## Implementation Components

**New modules/files**:
- `src/client/terminal.c` - Terminal initialization, raw mode, alternate screen
- `src/client/render.c` - Rendering pipeline (compose + blit)
- `src/client/input.c` - Input handling (keys, mouse, scrolling)
- `src/client/buffer.c` - Scrollback buffer management
- `src/client/main.c` - Client entry point and main loop

**Key functions**:
- `ik_term_init()` - Enter raw mode, alternate screen, create vterm
- `ik_term_cleanup()` - Restore terminal state
- `ik_buffer_append_line()` - Add line to scrollback
- `scroll_viewport()` - Adjust scroll_line_offset with bounds (mouse wheel, Page Up/Down)
- `move_cursor()` - Move cursor within dynamic zone (arrow keys)
- `snap_to_cursor()` - Scroll viewport to show cursor at bottom (if off-screen)
- `count_wrapped_lines()` - Calculate how many screen lines dynamic zone text uses
- `render_frame()` - Compose vterm content based on scroll position
- `blit_vterm_to_screen()` - Copy vterm to real terminal
- `process_input()` - Handle keys, update state, trigger render

**Main loop structure**:
```c
while (!quit) {
    // Read input from real terminal (raw mode)
    char buf[128];
    ssize_t n = read(tty_fd, buf, sizeof(buf));

    // Process input
    for (ssize_t i = 0; i < n; i++) {
        InputAction action = process_input_byte(ctx, buf[i]);

        switch (action.type) {
            case ACTION_TYPING:
                // Snap viewport to show cursor if off-screen
                snap_to_cursor(ctx);
                // Insert character into dynamic zone
                insert_char(ctx, action.ch);
                break;

            case ACTION_CURSOR_MOVE:
                // Move cursor within dynamic zone (arrow keys)
                move_cursor(ctx, action.direction);
                break;

            case ACTION_SCROLL:
                // Scroll viewport (mouse wheel, Page Up/Down)
                scroll_viewport(ctx, action.lines);
                break;

            case ACTION_SUBMIT:
                // Snap back, then submit dynamic zone to scrollback
                snap_to_cursor(ctx);
                submit_line(ctx);
                break;
        }
    }

    // Render frame (compose vterm + blit to screen)
    render_frame(ctx);
}
```

## Dependencies

- libvterm (terminal emulation and rendering)
- libutf8proc (UTF-8 text processing and grapheme cluster detection)
- talloc (memory management)
- ik_cfg module (config loading)

## UTF-8 and Grapheme Handling

- **Library**: Use **libutf8proc** for UTF-8 text processing and grapheme cluster detection
- **Rendering**: vterm handles UTF-8 internally (just write UTF-8 bytes to vterm)
- **Storage**: Scrollback and dynamic zone store UTF-8 strings as-is (byte arrays)
- **Cursor Position Tracking**: Maintain both in dynamic zone:
  - `cursor_byte_offset` - Byte position in UTF-8 string (for insertion/deletion)
  - `cursor_grapheme_offset` - Grapheme cluster count from start (for arrow key movement)
  - Keep both in sync when text is modified
- **Arrow Key Movement**:
  - Left/Right: Move by one grapheme cluster (not byte, not codepoint)
  - Use libutf8proc to find grapheme boundaries
  - Convert grapheme offset ↔ byte offset as needed
- **Text Insertion**:
  - Convert cursor_grapheme_offset → cursor_byte_offset
  - Insert new bytes at cursor_byte_offset
  - Shift remaining bytes forward
  - Recalculate grapheme count
- **Why Graphemes**: Handles multi-byte characters (emoji, combining characters like é = e + ´) correctly
- **Blit**: vterm converts UTF-32 cells back to UTF-8 for terminal output
