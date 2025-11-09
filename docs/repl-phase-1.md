# REPL Terminal - Phase 1: Simple Dynamic Zone

[← Back to REPL Terminal Overview](repl-terminal.md)

**Goal**: Get basic terminal and UTF-8 handling working without scrollback complexity.

Build minimal interactive terminal with just a dynamic zone. No scrollback buffer yet - keep it simple to validate fundamentals.

## Features

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
