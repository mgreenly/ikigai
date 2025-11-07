# Phase 1a: Minimal Standalone Client

## Overview

Build a minimal REPL chatbot with a split-buffer terminal interface that will eventually talk to OpenAI. This serves as a **manual testing harness** for developing AI model modules.

## Progressive Development

Phase 1a is split into incremental steps:

### Step 1: Split-Buffer REPL Terminal (Current Target)

Build the terminal interface mechanics without any LLM integration.

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
   - **Mouse wheel**: Scroll up/down through history
   - **Arrow keys**: Up/down scrolling
   - **Page Up/Down**: Jump by screen height
   - **Bounds**:
     - Can't scroll above first/oldest line
     - Can't scroll past bottom (where dynamic zone is fully visible)
   - **Dynamic zone behavior**:
     - Scrolls off bottom of viewport when viewing old history
     - When visible, entire dynamic zone stays on screen (not cut off at top)

5. **Config Integration**
   - Load config via existing `ik_cfg_load()`
   - Initial config only needs basic settings (API key path for later)

#### Architecture

**Mental Model**: The terminal is a viewport into a scrollback buffer with two conceptual zones:

```
┌─────────────────────────────────────┐
│  Terminal Screen (80x24 viewport)   │
│                                      │
│  ╔═══════════════════════════════╗  │
│  ║ Upper: Window into scrollback ║  │
│  ║ [line 500]                    ║  │ ← Viewing lines 500-520
│  ║ [line 501]                    ║  │   of scrollback buffer
│  ║ [line 502]                    ║  │
│  ║ ...                           ║  │
│  ║ [line 520]                    ║  │
│  ╠═══════════════════════════════╣  │
│  ║ ───────────────────────────── ║  │ ← Separator
│  ╠═══════════════════════════════╣  │
│  ║ Lower: Dynamic Zone           ║  │
│  ║ > type here█                  ║  │ ← Always shows "now"
│  ╚═══════════════════════════════╝  │
└─────────────────────────────────────┘
```

**Key behaviors**:
- When scrolled up: upper window slides through scrollback, dynamic zone disappears off bottom
- When at bottom (default): upper window shows recent history, dynamic zone visible at bottom
- Dynamic zone only visible when `scroll_offset == 0`

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
   - Enter (submit line)
   - Backspace (edit)
   - Arrow Up/Down (scroll history)
   - Page Up/Down (jump by screen)

2. **Before Step 1 complete** - Add mouse wheel:
   - Enable mouse tracking mode (`\x1b[?1000h` + `\x1b[?1006h`)
   - Parse mouse wheel events from escape sequences
   - Adjust scroll_offset and re-render

3. **Future enhancement** - Kitty keyboard protocol:
   - Unambiguous encoding for all key combos
   - Distinguishes ESC vs Alt
   - Optional, with graceful fallback

**Cursor Management**:
Two distinct cursor concepts:
1. **Logical cursor** - Position in input buffer (byte offset)
   - Tracked in input buffer data structure
   - Updated by editing operations (insert, backspace, arrow left/right)

2. **Screen cursor** - Visual position on terminal
   - Managed by vterm during rendering
   - Calculated from scroll position + layout + logical cursor

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

2. **Later in Phase 1a** - Add command processor:
   - Lines starting with `/` → dispatch to command handler
   - `/exit` → cleanup and exit program (doesn't go to scrollback)
   - Regular lines (not starting with `/`) → append to scrollback buffer
   - Leaves room for future commands: `/clear`, `/help`, etc.

3. **Future (Step 2 with AI)** - Enhanced dispatcher:
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
    size_t scroll_offset;        // 0 = at bottom, >0 = scrolled up

    // Dynamic zone (current input)
    char *dynamic_text;          // Current line being edited
    size_t cursor_pos;           // Cursor position in dynamic_text

    // vterm for rendering entire screen
    VTerm *vterm;                // Full screen size (cols x rows)
    VTermScreen *vscreen;

    // Screen dimensions
    int screen_rows;             // e.g., 24
    int screen_cols;             // e.g., 80
    int dynamic_rows;            // Rows for dynamic zone (e.g., 2)
} ik_term_ctx_t;
```

#### Rendering Pipeline

**Compose → Blit → Display**

```c
void render_frame(ik_term_ctx_t *ctx) {
    // 1. COMPOSE: Build what should be visible in vterm
    vterm_screen_reset(ctx->vscreen, 1);  // Clear vterm

    int scrollback_rows = ctx->screen_rows - ctx->dynamic_rows - 1; // -1 for separator

    // Calculate which scrollback lines to show
    size_t view_end = ctx->scrollback_count - ctx->scroll_offset;
    size_t view_start = (view_end > scrollback_rows) ? view_end - scrollback_rows : 0;

    // Write scrollback lines to vterm
    for (size_t i = view_start; i < view_end; i++) {
        vterm_input_write(ctx->vterm, ctx->scrollback[i], strlen(ctx->scrollback[i]));
        vterm_input_write(ctx->vterm, "\n", 1);
    }

    // If at bottom (scroll_offset == 0), show dynamic zone
    if (ctx->scroll_offset == 0) {
        vterm_input_write(ctx->vterm, "─────────────\n", ...);
        vterm_input_write(ctx->vterm, "> ", 2);
        vterm_input_write(ctx->vterm, ctx->dynamic_text, strlen(ctx->dynamic_text));
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
- `ik_buffer_scroll_up/down()` - Adjust scroll_offset with bounds
- `render_frame()` - Compose vterm content based on scroll position
- `blit_vterm_to_screen()` - Copy vterm to real terminal
- `process_input()` - Handle keys, update state, trigger render

**Main loop structure**:
```c
while (!quit) {
    // Read input from real terminal (raw mode)
    char buf[128];
    ssize_t n = read(tty_fd, buf, sizeof(buf));

    // Process input (update scrollback, dynamic_text, scroll_offset)
    for (ssize_t i = 0; i < n; i++) {
        process_input_byte(ctx, buf[i]);
    }

    // Render frame (compose vterm + blit to screen)
    render_frame(ctx);
}
```

**Dependencies**:
- libvterm (terminal emulation and UTF-8 handling)
- talloc (memory management)
- ik_cfg module (config loading)

**UTF-8 handling**:
- vterm handles UTF-8 internally (just write UTF-8 bytes to vterm)
- Scrollback stores UTF-8 strings as-is
- Blit converts vterm UTF-32 cells back to UTF-8 for terminal

#### What This Validates

- Alternate screen terminal behavior
- vterm as unified rendering system
- Manual scrollback control with viewport window
- Scroll mechanics and bounds (dynamic zone visibility)
- Memory management patterns for dynamic buffers
- Single-write blit pattern (no flicker)
- Foundation for adding streaming AI responses

---

### Step 2: OpenAI Integration (Future)

Add direct OpenAI client library integration to make this a working chatbot.

**Features to add**:
- OpenAI API client (streaming)
- Send prompt on Enter, display response in scrollback
- Spinner in dynamic zone while waiting for response
- Stream chunks into scrollback as they arrive

**Not in scope for Step 1** - focus on terminal mechanics first.

---

## Testing Strategy

Manual testing only for Step 1:
1. Launch app, verify alternate screen
2. Type lines, press Enter, verify they move to scrollback
3. Fill screen with lines, verify scrolling works
4. Test scroll bounds (top/bottom)
5. Test mouse wheel scrolling
6. Exit app, verify terminal restored cleanly

Unit tests would be valuable for buffer management logic but not essential for this exploratory phase.

---

## Notes

- **No server connection** - this is a standalone client
- **No persistence** - conversation lost on exit
- **Minimal UI** - just text, no colors/formatting (can add later)
- Purpose is rapid iteration on AI module patterns with manual testing
