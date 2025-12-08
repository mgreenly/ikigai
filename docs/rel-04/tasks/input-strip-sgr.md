# Task: Strip SGR Sequences from Pasted Input

## Target
Infrastructure: ANSI color support (Phase 3 - Input Handling)

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
- src/input.c (escape sequence handling, parse_escape_sequence function)
- src/input.h (parser struct with esc_buf)

### Pre-read Tests (patterns)
- tests/unit/input/parser_test.c

## Pre-conditions
- `make check` passes
- `ik_ansi_skip_csi()` exists and works
- Width calculation tasks completed

## Task
Modify the input parser to recognize and discard SGR (Select Graphic Rendition) sequences from pasted input. SGR sequences are CSI sequences that terminate with 'm'. When users paste colored text from terminals, the color codes should be stripped.

Current behavior: Unknown escape sequences are discarded after timeout or buffer overflow.
New behavior: Recognize SGR sequences (terminate with 'm') and discard them immediately.

The existing `parse_escape_sequence()` function in input.c handles CSI sequences. Add detection for SGR terminal byte 'm' and discard the sequence.

## TDD Cycle

### Red
1. Add tests to `tests/unit/input/parser_test.c`:
   - Test pasting `"\x1b[0m"` produces no output action
   - Test pasting `"\x1b[38;5;242m"` produces no output action
   - Test pasting `"\x1b[38;5;242mhello"` produces only "hello" chars
   - Test pasting `"before\x1b[0mafter"` produces "beforeafter"
   - Test non-SGR escapes still work (arrow keys, etc.)
2. Run `make check` - expect test failures (SGR sequences not recognized)

### Green
1. In `parse_escape_sequence()`, add SGR detection after existing checks:
   ```c
   // SGR sequences terminate with 'm' - discard silently
   // These come from pasted colored text
   if (byte == 'm' && parser->esc_len >= 1 && parser->esc_buf[0] == '[') {
       reset_escape_state(parser);
       action_out->type = IK_ACTION_NONE;
       return;
   }
   ```
2. Place this check before the "unknown sequence" fallback
3. Run `make check` - expect pass

### Refactor
1. Consider if other CSI sequences should be explicitly handled
2. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- Pasting colored text strips color codes
- Regular escape sequences (arrows, etc.) still work
- 100% test coverage maintained
