# REPL Terminal Interface

## Overview

Build a minimal REPL chatbot with a split-buffer terminal interface that will eventually talk to OpenAI. This serves as a **manual testing harness** for developing AI model modules.

## Progressive Development

This work is split into incremental phases, each building on the previous.

**Current Roadmap:**
- **Phase 0**: Clean up existing error handling + build generic `ik_array_t` utility
- **Phase 1**: Simple dynamic zone only (validate terminal, UTF-8, cursor handling)
- **Phase 2**: Add scrollback buffer and scrolling (complete terminal UI)
- **Phase 3**: OpenAI integration (make it a real chatbot)

**Current focus**: Phase 0, Task 1 (error handling cleanup)

Each phase follows strict TDD (Test-Driven Development) with 100% coverage requirement.

**Note**: This roadmap will be re-evaluated after completing each phase. We'll adjust future phases based on what we learn during implementation.

### Phase 0: Foundation - Clean Up Existing Code + Generic Array Utility

**Goal**: Clean up existing code and build reusable expandable array before REPL work.

Before building any terminal code, we need to clean up the existing error handling implementation and then build a generic expandable array utility.

#### Task 1: Error Handling Cleanup ✅ COMPLETE

Clean up existing code to properly follow the 3 modes of operation (IO operations, contract violations, pure operations) before building new features.

**Completed changes:**
1. ✅ Removed helper functions (`ik_check_null`, `ik_check_range`) - now using asserts for contract violations
2. ✅ Added ~35 missing assertions to all existing functions (config.c, protocol.c, error.h)
3. ✅ Replaced defensive NULL checks with assertions
4. ✅ Fixed `expand_tilde` to return `res_t` (IO operation doing allocation)

**Impact:** The codebase now properly follows the 3 modes of operation philosophy. All tests pass with proper separation of concerns between IO errors, contract violations, and pure operations.

#### Task 2: Generic Array Utility

**See [docs/array.md](array.md) for complete design.**

After error handling cleanup, build the generic expandable array utility that will be used throughout the REPL implementation.

**Key components:**
1. Generic `ik_array_t` implementation (element_size configurable)
2. Common textbook operations (append, insert, delete, get, set)
3. Talloc-based memory management
4. Growth by doubling capacity
5. Full test coverage via TDD

**Typed wrappers to create:**
- `ik_byte_array_t` - For dynamic zone text (UTF-8 bytes)
- `ik_line_array_t` - For scrollback buffer (line pointers)

**Development approach**: Strict TDD red/green cycle with 100% coverage.

**Why second?** Both dynamic zone text and scrollback buffer need expandable storage. Building this utility on a clean foundation ensures consistency.

### Phase 1: Simple Dynamic Zone (First Baby Steps)

**Goal**: Get basic terminal and UTF-8 handling working without scrollback complexity.

Build minimal interactive terminal with just a dynamic zone. No scrollback buffer yet - keep it simple to validate fundamentals.

**Features:**
1. Terminal setup (raw mode, alternate screen)
2. Single dynamic zone (can fill entire screen via wrapping)
3. Text input (typing characters into `ik_byte_array_t`)
4. UTF-8 and grapheme cluster handling via libutf8proc
5. Cursor tracking (dual offset: byte + grapheme)
6. Cursor movement (arrow keys left/right through graphemes, up/down through wrapped lines)
7. Text editing (insert at cursor, backspace/delete)
8. Enter inserts newline (no submission yet)
9. Basic rendering with vterm (compose + blit)
10. Exit on Ctrl+C

**What we validate:**
- Terminal raw mode and alternate screen
- vterm rendering pipeline
- UTF-8/grapheme handling is correct (emoji, combining chars)
- Cursor position tracking (byte offset ↔ grapheme offset)
- Text insertion/deletion at arbitrary positions
- Multi-line text via wrapping
- Clean terminal restoration on exit

**What we defer:**
- Scrollback buffer (comes in Phase 2)
- Viewport scrolling (comes in Phase 2)
- Separator line (comes in Phase 2)
- Mouse wheel input (comes in Phase 2)
- Line submission to history (comes in Phase 2)

**Development approach**: Strict TDD with 100% coverage.

### Phase 2: Add Scrollback and Scrolling

**Goal**: Implement the full continuous buffer model.

Once Phase 1 validates the fundamentals, add the continuous buffer with scrollback.

**New features:**
1. Scrollback buffer (`ik_line_array_t`)
2. Separator line
3. Continuous buffer model (scrollback + separator + dynamic zone)
4. Viewport scrolling (mouse wheel, Page Up/Down)
5. Snap-back behavior (typing when scrolled up)
6. Enter submits line to scrollback
7. Dynamic zone scrolling (when taller than screen)

**See "Split-Buffer REPL Terminal" section below for complete design.**

### Split-Buffer REPL Terminal (Phase 2 Design)

This describes the final design with scrollback buffer. Most features are deferred to Phase 2.

**Goal**: A working REPL with scrollback buffer and dynamic input zone.

#### Features

1. **Alternate Screen Mode**
   - App uses alternate screen buffer on launch
   - Clean terminal on exit (no history pollution)

2. **Split Buffer Layout**
   ```
   [scrollback line 1 - oldest]
   [scrollback line 2]
   [scrollback line 3]
   ...
   [scrollback line N - most recent]
   ─────────────────────────────────
   > user input here█
   ```
   - **Scrollback zone** (top): Immutable conversation history
   - **ASCII separator**: Visual boundary (e.g., `─────────`)
   - **Dynamic zone** (bottom): Editable prompt line

3. **REPL Behavior**
   - Type text in prompt
   - Press Enter → text moves to scrollback buffer
   - New empty prompt appears in dynamic zone

4. **Scrolling**
   - **Mouse wheel**: Scroll up/down through buffer
   - **Page Up/Down**: Scroll by larger increments (e.g., screen height)
   - **Bounds**:
     - Can't scroll above first/oldest line
     - Can't scroll past bottom (last line of dynamic zone)
   - **Dynamic zone behavior**:
     - As you scroll up, dynamic zone disappears off bottom line-by-line
     - Can scroll to any position where dynamic zone is partially or fully visible
     - Separator line scrolls with dynamic zone (not fixed)

5. **Config Integration**
   - Load config via existing `ik_cfg_load()`
   - Initial config only needs basic settings (API key path for later)

#### Architecture

**Mental Model**: The terminal is a viewport into one continuous buffer:

```
┌─────────────────────────────────────┐
│  Terminal Screen (80x24 viewport)   │
│                                      │
│  ╔═══════════════════════════════╗  │
│  ║ Scrollback line 500           ║  │ ← Viewport showing
│  ║ Scrollback line 501           ║  │   lines 500-523 of
│  ║ ...                           ║  │   continuous buffer
│  ║ Scrollback line 520           ║  │
│  ║ ───────────────────────────── ║  │ ← Separator (part of buffer)
│  ║ Dynamic zone line 1           ║  │
│  ║ Dynamic zone line 2           ║  │ ← Multi-line input area
│  ║ Dynamic zone line 3█          ║  │   (variable height)
│  ╚═══════════════════════════════╝  │
└─────────────────────────────────────┘

        Continuous Buffer:
        [scrollback lines...]
        [separator line]
        [dynamic zone lines...]
```

**Key behaviors**:
- One continuous buffer: scrollback + separator + dynamic zone
- Viewport scrolls through this buffer via mouse wheel or Page Up/Down
- Dynamic zone height varies based on text content (wrapping)
- As you scroll up, dynamic zone + separator disappear off bottom line-by-line
- Cursor always positioned within dynamic zone (even when scrolled off-screen)
- When you type with cursor off-screen, viewport snaps to show cursor at bottom

**Implementation Strategy**: Use libvterm for ALL rendering (one coherent system), but maintain our own scrollback buffer for control.

#### Detailed Design Decisions

**Input Flow**:
All keyboard and mouse input flows through the terminal emulator (Ghostty/Kitty):
- Terminal receives OS-level events
- Encodes them as byte sequences (regular chars or escape sequences)
- Writes to app's stdin
- App reads in raw mode and processes

**Progressive Input Implementation**:
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

**Cursor Management**:
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

**Snap-back behavior**:
- When typing any key (including Enter) while cursor is off-screen
- Viewport scrolls to position cursor line at bottom of screen
- Ensures user can see what they're typing
- Happens before processing the keystroke

**Scrollback Line Format**:
Variable-length logical lines (NOT fixed-width grid):
- Each line stores semantic content: "user: hello" or "ai: long response..."
- Lines can be any length (UTF-8 strings)
- Render module handles wrapping to current terminal width
- **Terminal resize**: Just re-render with new dimensions, no buffer modification needed
- Simpler than fixed-width approach (no reflow logic required)

**Line Processing Pipeline**:
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

**Module Organization**:
- **repl module** - Main context, owns all data structures, provides init/cleanup/run
- **terminal module** - Raw mode, alternate screen, termios state
- **buffer module** - Scrollback buffer and input buffer management
- **render module** - vterm ownership, compose and blit operations
- **input module** - Process raw byte sequences, dispatch to buffer/render
- **expandable array utility** - Generic reusable array (for scrollback and future needs)

**Memory Strategy**:
- Use talloc for all allocations (consistent with existing codebase)
- Main context owns everything via talloc hierarchy
- Cleanup is automatic with talloc_free()
- Expandable arrays use talloc_realloc() for growth

#### Data Structures

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

#### Rendering Pipeline

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

#### Implementation Components

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

**Dependencies**:
- libvterm (terminal emulation and rendering)
- libutf8proc (UTF-8 text processing and grapheme cluster detection)
- talloc (memory management)
- ik_cfg module (config loading)

**UTF-8 and Grapheme Handling**:
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

#### What This Validates

- Alternate screen terminal behavior
- vterm as unified rendering system
- Continuous buffer model (scrollback + separator + dynamic zone)
- Viewport scrolling through variable-height content
- Multi-line dynamic zone with cursor movement
- Snap-back behavior when typing while scrolled
- Mouse wheel and Page Up/Down scrolling
- Scroll bounds with partially visible dynamic zone
- Memory management patterns for dynamic buffers
- Single-write blit pattern (no flicker)
- Foundation for adding streaming AI responses

### Phase 3: OpenAI Integration (Future)

**Goal**: Make this a working AI chatbot.

Add OpenAI API client integration once terminal mechanics are solid.

**Features:**
- OpenAI API client library (streaming)
- Send dynamic zone content as prompt on Enter
- Display AI response in scrollback
- Spinner/status indicator while waiting for response
- Stream response chunks into scrollback as they arrive
- Error handling for API failures

**Not in scope for Phases 0-2** - focus on terminal mechanics first.

---

## Testing Strategy

**Development approach**: Strict TDD (Test-Driven Development)

All code is developed using TDD red/green cycle:
1. **Red**: Write failing test first
2. **Green**: Write minimal code to pass test
3. **Verify**: Run `make check`, `make lint`, `make coverage`
4. **Coverage requirement**: 100% line, function, and branch coverage

**Manual testing**: Performed AFTER TDD is complete for each feature to validate full integration.

### Manual Test Plan (Phase 2 - Full UI)

Once Phase 2 is complete and all TDD tests pass, validate the complete system manually:

1. **Launch and basic operation**:
   - Launch app, verify alternate screen
   - Type lines, press Enter, verify they move to scrollback
   - Exit app, verify terminal restored cleanly

2. **UTF-8 and grapheme handling**:
   - Type emoji (🎉, 👨‍👩‍👧‍👦), verify they display correctly
   - Type combining characters (e + ´ = é)
   - Use left/right arrows to move through multi-byte characters
   - Verify cursor moves by whole grapheme clusters (not bytes)
   - Insert text in middle of emoji sequence, verify no corruption
   - Backspace over multi-byte characters, verify they delete as units

3. **Multi-line dynamic zone**:
   - Type multi-line text, verify wrapping works correctly
   - Use arrow keys to move cursor around in multi-line text
   - Verify up/down arrows move between wrapped lines

4. **Scrolling behavior**:
   - Fill screen with lines, use mouse wheel to scroll up through history
   - Verify dynamic zone + separator disappear line-by-line as you scroll up
   - While scrolled up (dynamic zone off-screen), type a character
   - Verify viewport snaps back to show cursor at bottom
   - Test scroll bounds: can't scroll above first line or past bottom

5. **Advanced features**:
   - Test Page Up/Down scrolling
   - Type very long text in dynamic zone (taller than screen)
   - Verify internal scrolling within dynamic zone works

---

## Notes

- **No server connection** - this is a standalone client
- **No persistence** - conversation lost on exit
- **Minimal UI** - just text, no colors/formatting (can add later)
- Purpose is rapid iteration on AI module patterns with manual testing
