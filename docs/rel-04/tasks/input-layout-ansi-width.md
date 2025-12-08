# Task: Input Buffer ANSI-Aware Width Calculation

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
- src/input_buffer/layout.c (current width calculation)

### Pre-read Tests (patterns)
- tests/unit/input_buffer/layout_test.c

## Pre-conditions
- `make check` passes
- `ik_ansi_skip_csi()` exists and works
- Scrollback ANSI width task completed

## Task
Update `calculate_display_width()` in `src/input_buffer/layout.c` to skip ANSI escape sequences. This static function calculates display width for the input buffer layout system.

Note: In practice, the input buffer strips ANSI sequences (see input-strip-sgr task), so this is primarily defensive. However, consistency across all width calculations is important.

## TDD Cycle

### Red
1. Add test to `tests/unit/input_buffer/layout_test.c`:
   - Test layout with embedded SGR in input text
   - Test physical lines calculation ignores escape sequences
   - Verify display width excludes escape bytes
2. Run `make check` - expect test failures

### Green
1. Add `#include "ansi.h"` to layout.c
2. In `calculate_display_width()`, add skip logic at start of loop:
   ```c
   while (pos < len) {
       // Skip ANSI escape sequences
       size_t skip = ik_ansi_skip_csi(text, len, pos);
       if (skip > 0) {
           pos += skip;
           continue;
       }
       // ... existing UTF-8 processing
   }
   ```
3. Run `make check` - expect pass

### Refactor
1. Verify no code duplication with scrollback
2. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- Input buffer layout correctly calculates display width excluding ANSI escapes
- 100% test coverage maintained
