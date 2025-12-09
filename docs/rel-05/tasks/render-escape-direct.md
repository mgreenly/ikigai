# Task: Direct Escape Sequence Generation Without snprintf

## Target
Render Performance Optimization - Eliminate format string parsing overhead

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/tdd.md

## Pre-read Docs
- docs/memory.md (talloc ownership)

## Pre-read Source (patterns)
- src/repl_viewport.c (line 275-281 - talloc_asprintf_ for cursor positioning)
- src/render.c (any escape sequence generation)

## Pre-read Tests (patterns)
- tests/unit/render/*.c (render test patterns)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- Cursor positioning uses `talloc_asprintf_()` with format strings
- render-buffer-reuse.md task is complete (pre-allocated cursor buffer exists)

## Task
Replace `snprintf`/`talloc_asprintf_` calls for escape sequence generation with direct integer-to-string conversion. Format string parsing has overhead that can be avoided for simple numeric sequences.

Current approach:
```c
char *cursor_seq = talloc_asprintf_(ctx, "\x1b[%d;%dH", row, col);
```

New approach:
```c
// Direct construction into pre-allocated buffer
size_t len = ik_escape_cursor_pos(buf, sizeof(buf), row, col);
```

## TDD Cycle

### Red
1. Create `src/escape.h` with fast escape sequence builders:
   ```c
   #pragma once

   #include <stddef.h>
   #include <stdint.h>

   // Build cursor position escape sequence: \x1b[row;colH
   // Returns number of bytes written (not including null terminator)
   size_t ik_escape_cursor_pos(char *buf, size_t capacity,
                               uint16_t row, uint16_t col);

   // Build clear screen sequence: \x1b[2J
   size_t ik_escape_clear_screen(char *buf, size_t capacity);

   // Build cursor home sequence: \x1b[H
   size_t ik_escape_cursor_home(char *buf, size_t capacity);

   // Fast integer to string (for internal use, but exposed for testing)
   // Returns pointer to start of number in buf (writes backwards from end)
   char *ik_itoa_fast(char *buf_end, uint16_t value);
   ```

2. Create `tests/unit/render/escape_test.c`:
   - Test cursor position with row=1, col=1 produces `\x1b[1;1H`
   - Test cursor position with row=999, col=999 produces correct sequence
   - Test cursor position with row=1, col=80 produces `\x1b[1;80H`
   - Test clear screen produces `\x1b[2J`
   - Test cursor home produces `\x1b[H`
   - Test `ik_itoa_fast` with 0, 1, 10, 100, 999, 65535

3. Run `make check` - expect failures

### Green
1. Create `src/escape.c`:
   ```c
   #include "escape.h"

   char *ik_itoa_fast(char *buf_end, uint16_t value)
   {
       char *p = buf_end;
       do {
           *--p = '0' + (value % 10);
           value /= 10;
       } while (value > 0);
       return p;
   }

   size_t ik_escape_cursor_pos(char *buf, size_t capacity,
                               uint16_t row, uint16_t col)
   {
       // Max: \x1b[65535;65535H = 2 + 5 + 1 + 5 + 1 = 14 bytes + null
       char temp[16];
       char *p = temp + sizeof(temp);

       *--p = 'H';

       char *col_str = ik_itoa_fast(p, col);
       p = col_str;

       *--p = ';';

       char *row_str = ik_itoa_fast(p, row);
       p = row_str;

       *--p = '[';
       *--p = '\x1b';

       size_t len = (temp + sizeof(temp)) - p - 1;  // -1 for 'H' already counted
       // ... copy to buf
   }
   ```

2. Update `src/repl_viewport.c` to use `ik_escape_cursor_pos()` instead of `talloc_asprintf_()`

3. Run `make check` - expect pass

### Refactor
1. Profile to verify performance improvement
2. Consider additional escape sequences if used frequently
3. Run `make lint` - verify clean

## Post-conditions
- Working tree is clean (all changes committed)
- `make check` passes
- `src/escape.h` and `src/escape.c` exist with fast escape builders
- No `snprintf`/`talloc_asprintf_` in hot render path for escape sequences
- Same escape sequences produced (behavioral equivalence)
