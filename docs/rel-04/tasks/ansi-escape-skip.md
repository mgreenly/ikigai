# Task: ANSI Escape Sequence Skip Function

## Target
Infrastructure: ANSI color support (Phase 1 - Foundation)

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/naming.md
- .agents/skills/errors.md

### Pre-read Docs
- docs/ansi-color.md
- docs/naming.md

### Pre-read Source (patterns)
- src/scrollback.c (UTF-8 iteration pattern, lines 125-213)

### Pre-read Tests (patterns)
- tests/unit/scrollback/scrollback_test.c

## Pre-conditions
- `make check` passes

## Task
Create `src/ansi.h` and `src/ansi.c` with a function to skip ANSI CSI escape sequences during text iteration. This is the foundation for color support - width calculations must skip non-printing escape sequences.

Core API:
```c
// Skip ANSI CSI escape sequence if present at position
// Returns number of bytes to skip (0 if not an escape sequence)
// CSI format: ESC [ <params> <terminal_byte>
// Terminal bytes are 0x40-0x7E (uppercase letters, @, etc.)
size_t ik_ansi_skip_csi(const char *text, size_t len, size_t pos);
```

CSI sequence structure:
- Starts with `\x1b[` (ESC + '[')
- Parameter bytes: 0x30-0x3F (digits, semicolons, etc.)
- Intermediate bytes: 0x20-0x2F (optional)
- Terminal byte: 0x40-0x7E (letter that ends sequence, e.g., 'm' for SGR)

## TDD Cycle

### Red
1. Create `src/ansi.h`:
   - Add `size_t ik_ansi_skip_csi(const char *text, size_t len, size_t pos);`
2. Create `tests/unit/ansi/skip_test.c`:
   - Test returns 0 for regular text (no escape)
   - Test returns 0 for partial ESC (just `\x1b`)
   - Test returns 0 for ESC without '[' (e.g., `\x1bO`)
   - Test skips simple SGR: `\x1b[0m` (4 bytes)
   - Test skips 256-color foreground: `\x1b[38;5;242m` (11 bytes)
   - Test skips 256-color background: `\x1b[48;5;249m` (11 bytes)
   - Test skips combined: `\x1b[38;5;242;1m` (bold + color)
   - Test handles sequence at end of buffer
   - Test handles incomplete sequence (no terminal byte)
3. Create stub `src/ansi.c` that returns 0
4. Update Makefile to build ansi.c and test
5. Run `make check` - expect test failures

### Green
1. Implement `ik_ansi_skip_csi()`:
   - Check if `text[pos]` is ESC (`\x1b`)
   - Check if next byte is `[`
   - Scan parameter bytes (0x30-0x3F) and intermediate bytes (0x20-0x2F)
   - Check for terminal byte (0x40-0x7E)
   - Return total bytes consumed (or 0 if not valid CSI)
2. Run `make check` - expect pass

### Refactor
1. Verify naming follows `ik_ansi_*` convention
2. Add inline documentation
3. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `src/ansi.h` and `src/ansi.c` exist
- `ik_ansi_skip_csi()` correctly identifies and measures CSI sequences
- 100% test coverage for new code
