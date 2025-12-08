# Task: Render Cursor ANSI-Aware Position Calculation

## Target
Infrastructure: ANSI color support (Phase 2 - Width Calculation)

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/naming.md

### Pre-read Docs
- docs/ansi-color.md

### Pre-read Source (patterns)
- src/ansi.h (skip function)
- src/render_cursor.c (current cursor calculation)

### Pre-read Tests (patterns)
- tests/unit/render/cursor_test.c

## Pre-conditions
- `make check` passes
- `ik_ansi_skip_csi()` exists and works
- Scrollback and input buffer ANSI width tasks completed

## Task
Update `calculate_cursor_screen_position()` in `src/render_cursor.c` to skip ANSI escape sequences when calculating cursor position. The cursor position must reflect visible character positions, not byte positions including escape sequences.

## TDD Cycle

### Red
1. Add test to `tests/unit/render/cursor_test.c`:
   - Test cursor position after SGR: `"\x1b[0mhello"` cursor at byte 4 = screen col 0
   - Test cursor position with SGR prefix: `"\x1b[38;5;242mtext"` cursor at byte 11 = screen col 0
   - Test cursor in middle of colored text
   - Test cursor after multiple escape sequences
2. Run `make check` - expect test failures

### Green
1. Add `#include "ansi.h"` to render_cursor.c
2. In `calculate_cursor_screen_position()`, after newline check and before UTF-8 decode:
   ```c
   // Skip ANSI escape sequences
   size_t skip = ik_ansi_skip_csi(text, text_len, pos);
   if (skip > 0) {
       pos += skip;
       continue;
   }
   ```
3. Run `make check` - expect pass

### Refactor
1. Verify cursor correctly tracks through colored text
2. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- Cursor position calculation correctly skips ANSI escapes
- Cursor appears at correct visual position in colored text
- 100% test coverage maintained
