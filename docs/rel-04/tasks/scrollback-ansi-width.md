# Task: Scrollback ANSI-Aware Width Calculation

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
- src/ansi.c (skip implementation)
- src/scrollback.c (current width calculation, lines 125-213)

### Pre-read Tests (patterns)
- tests/unit/scrollback/scrollback_test.c
- tests/unit/ansi/skip_test.c

## Pre-conditions
- `make check` passes
- `ik_ansi_skip_csi()` exists and works

## Task
Update `ik_scrollback_append_line()` to skip ANSI escape sequences when calculating display width. Currently the function iterates UTF-8 codepoints to calculate line widths for wrapping. It must now also skip CSI sequences so they don't contribute to display width.

The two loops in `ik_scrollback_append_line()` that calculate width (lines 125-169 and 188-213) need to call `ik_ansi_skip_csi()` before processing each byte.

## TDD Cycle

### Red
1. Add test to `tests/unit/scrollback/scrollback_test.c`:
   - Test append line with embedded SGR: `"\x1b[38;5;242mhello\x1b[0m"` (5 display chars)
   - Test append line with SGR at start: `"\x1b[0mworld"` (5 display chars)
   - Test append line with SGR at end: `"test\x1b[0m"` (4 display chars)
   - Test append line with multiple SGRs: `"\x1b[1m\x1b[38;5;242mbold gray\x1b[0m"` (9 display chars)
   - Test physical lines calculation with colors (verify wrapping still works)
   - Verify `layouts[n].display_width` matches visible characters only
2. Run `make check` - expect test failures (widths include escape bytes)

### Green
1. Add `#include "ansi.h"` to scrollback.c
2. In first width loop (lines 125-169), after checking for newline:
   ```c
   // Skip ANSI escape sequences
   size_t skip = ik_ansi_skip_csi(text, length, pos);
   if (skip > 0) {
       pos += skip;
       continue;
   }
   ```
3. In second width loop (lines 188-213), add same skip logic
4. Run `make check` - expect pass

### Refactor
1. Consider extracting common width calculation helper (defer if complexity low)
2. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- Scrollback correctly calculates display width excluding ANSI escapes
- Line wrapping works correctly with colored text
- 100% test coverage maintained
