# Fix: Switch to Alternate Scroll Mode

## Problem

Current mouse mode (`?1000h?1006h`) captures ALL mouse events, preventing:
- Text selection in terminal (ghostty)
- Right-click context menus

## Solution

Replace button tracking mode with alternate scroll mode (`?1007h`):
- Scroll wheel → arrow key sequences (handled by terminal)
- Clicks NOT captured → text selection and menus work
- No coordinate data needed

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/style.md

### Pre-read Docs
- docs/return_values.md

### Pre-read Source (patterns)
- src/terminal.h (ik_term_ctx_t struct)
- src/terminal.c (ESC_MOUSE_ENABLE, ESC_MOUSE_DISABLE constants, ik_term_init, ik_term_cleanup)

### Pre-read Tests (patterns)
- tests/unit/terminal/terminal_test.c (mouse enable/disable verification)

## Pre-conditions
- `make check` passes

## Task

Change terminal mouse mode from button tracking (`?1000h?1006h`) to alternate scroll (`?1007h`).

**Escape sequence changes:**
- Enable: `\x1b[?1007h` (8 bytes) instead of `\x1b[?1000h\x1b[?1006h` (16 bytes)
- Disable: `\x1b[?1007l` (8 bytes) instead of `\x1b[?1000l\x1b[?1006l` (16 bytes)

**Behavior change:**
With `?1007h`, scroll wheel sends arrow key sequences (`ESC [ A` / `ESC [ B`) when in alternate screen. The input parser already handles these as `IK_INPUT_ARROW_UP` / `IK_INPUT_ARROW_DOWN`.

## TDD Cycle

### Red
1. Update tests in `tests/unit/terminal/terminal_test.c`:
   - Change expected enable sequence from `"\x1b[?1000h\x1b[?1006h"` to `"\x1b[?1007h"`
   - Change expected disable sequence from `"\x1b[?1000l\x1b[?1006l"` to `"\x1b[?1007l"`
   - Update write length expectations (16 → 8 bytes)
2. Run `make check` - expect test failure (old sequences still written)

### Green
1. In `src/terminal.c`:
   - Change `ESC_MOUSE_ENABLE` from `"\x1b[?1000h\x1b[?1006h"` to `"\x1b[?1007h"`
   - Change `ESC_MOUSE_DISABLE` from `"\x1b[?1000l\x1b[?1006l"` to `"\x1b[?1007l"`
   - Update `posix_write_` length arguments from 16 to 8
2. Run `make check` - expect pass

### Refactor
1. Update comments to reflect alternate scroll mode instead of SGR mouse
2. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- Terminal uses alternate scroll mode (`?1007h`)
- Text selection works in terminal
- Right-click menus work in terminal
- Scroll wheel sends arrow key events (next task handles behavior)
