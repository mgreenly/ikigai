# Task: Enable Mouse Reporting in Terminal

## Target
Feature: Mouse wheel scrolling

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/style.md
- .agents/skills/testability.md

### Pre-read Docs
- docs/return_values.md
- docs/memory.md

### Pre-read Source (patterns)
- src/terminal.h (ik_term_ctx_t struct)
- src/terminal.c (ik_term_init, ik_term_cleanup - init sends alternate screen escape, cleanup restores)
- src/wrapper.h (posix_write_ wrapper for testability)

### Pre-read Tests (patterns)
- tests/unit/terminal/terminal_test.c (terminal init/cleanup tests, mock patterns)

## Pre-conditions
- `make check` passes

## Task
Enable SGR mouse reporting when entering raw mode, disable when exiting.

**SGR Mouse Protocol:**
- Enable: `\x1b[?1000h` (mouse button tracking) + `\x1b[?1006h` (SGR extended mode)
- Disable: `\x1b[?1000l` + `\x1b[?1006l`

These sequences should be sent during `ik_term_init()` (after alternate screen) and `ik_term_cleanup()` (before leaving alternate screen).

## TDD Cycle

### Red
1. Add/modify tests in `tests/unit/terminal/terminal_test.c`:
   - Verify `ik_term_init()` writes mouse enable sequences to the terminal
   - Verify `ik_term_cleanup()` writes mouse disable sequences
   - Use mock `posix_write_` to capture output and verify sequences present
2. Run `make check` - expect test failure (sequences not written)

### Green
1. In `ik_term_init()` (terminal.c), after sending alternate screen escape:
   - Add write of `"\x1b[?1000h\x1b[?1006h"` to enable SGR mouse reporting
2. In `ik_term_cleanup()` (terminal.c), before sending alternate screen exit:
   - Add write of `"\x1b[?1000l\x1b[?1006l"` to disable mouse reporting
3. Run `make check` - expect pass

### Refactor
1. Consider defining the escape sequences as constants for clarity
2. Ensure cleanup always disables mouse even if other operations fail
3. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- Terminal enables SGR mouse mode on init
- Terminal disables SGR mouse mode on cleanup
- Mouse events will now be sent by terminal (parser handles them in next task)
