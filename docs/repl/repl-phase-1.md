# REPL Terminal - Phase 1: Simple Dynamic Zone

[← Back to REPL Terminal Overview](README.md)

**Status**: 🔄 IN PROGRESS - Core components complete, REPL event loop pending

**Goal**: Get basic terminal and UTF-8 handling working without scrollback complexity.

Build minimal interactive terminal with just a dynamic zone. No scrollback buffer yet - keep it simple to validate fundamentals.

## Implementation Status

### ✅ Complete Components

1. **Terminal setup** - src/terminal.h, src/terminal.c
   - ✅ Raw mode configuration
   - ✅ Alternate screen buffer
   - ✅ Terminal size detection

2. **Text input parsing** - src/input.h, src/input.c
   - ✅ UTF-8 byte sequence decoding
   - ✅ Escape sequence parsing
   - ✅ Action mapping (char, arrows, backspace, delete, newline, Ctrl+C)

3. **Cursor tracking** - src/cursor.h, src/cursor.c
   - ✅ Dual offset tracking (byte + grapheme)
   - ✅ Grapheme cluster detection via libutf8proc
   - ✅ Left/right movement by grapheme

4. **Workspace** - src/workspace.h, src/workspace.c
   - ✅ UTF-8 text buffer using `ik_byte_array_t`
   - ✅ Insert codepoint at cursor
   - ✅ Insert newline
   - ✅ Backspace/delete operations
   - ✅ Cursor movement (left/right)

5. **Rendering** - src/render.h, src/render.c
   - ✅ libvterm integration
   - ✅ Virtual terminal composition
   - ✅ Blit to actual terminal
   - ✅ Cursor positioning

### 🔄 In Progress - Task 6: REPL Event Loop (src/repl.h, src/repl.c)

**Completed (Steps 1-3)**:
- ✅ Context initialization (term, render, workspace, input parser)
- ✅ Cleanup and terminal restoration
- ✅ Comprehensive mocking for testing without TTY
- ✅ 100% test coverage for init/cleanup

**Remaining work (Steps 4-8)**:
- ❌ Step 4: Render frame helper (`ik_repl_render_frame()`)
  - Clear render context, get workspace text, write to render
  - Calculate cursor screen position (accounting for wrapping)
  - Set cursor and blit to screen
- ❌ Step 5: Process input action helper (`ik_repl_process_action()`)
  - Handle all action types (char, newline, backspace, delete, arrows, Ctrl+C)
  - Up/down arrows deferred (no-op for Phase 1)
- ❌ Step 6: Main event loop (`ik_repl_run()` - currently a stub)
  - Initial render, read loop, parse bytes, process actions, re-render
- ❌ Step 7: Main entry point
  - Refactor src/main.c to use REPL context
- ❌ Step 8: Final demo and polish
  - Manual testing checklist, cleanup, formatting

**Reference**: See tasks.md lines 49-123 for complete task breakdown

## What we validate

- Terminal raw mode and alternate screen
- vterm rendering pipeline
- UTF-8/grapheme handling is correct (emoji, combining chars)
- Cursor position tracking (byte offset ↔ grapheme offset)
- Text insertion/deletion at arbitrary positions
- Multi-line text via wrapping
- Clean terminal restoration on exit

## What we defer

- Scrollback buffer (comes in Phase 2)
- Viewport scrolling (comes in Phase 2)
- Separator line (comes in Phase 2)
- Mouse wheel input (comes in Phase 2)
- Line submission to history (comes in Phase 2)

## Development approach

Strict TDD with 100% coverage.
