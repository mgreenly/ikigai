# Task: Parse SGR Mouse Escape Sequences

## Target
Feature: Mouse wheel scrolling

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/style.md
- .agents/skills/patterns/state-machine.md

### Pre-read Docs
- docs/return_values.md

### Pre-read Source (patterns)
- src/input.h (ik_input_parser_t state machine struct, action types)
- src/input.c (ik_input_parse_byte, parse_escape_sequence - existing CSI handling)

### Pre-read Tests (patterns)
- tests/unit/input/input_test.c (escape sequence parsing tests)

## Pre-conditions
- `make check` passes
- Task `mouse-scroll-action-types.md` completed (IK_INPUT_SCROLL_UP/DOWN exist)

## Task
Extend the input parser state machine to recognize SGR mouse scroll events.

**SGR Mouse Format:**
```
ESC [ < button ; x ; y M    (button press)
ESC [ < button ; x ; y m    (button release)
```

**Scroll wheel button codes:**
- 64 = scroll up
- 65 = scroll down

**Parsing strategy:**
1. After `ESC [`, if next byte is `<`, enter SGR mouse parsing mode
2. Parse digits until `;` (this is the button code)
3. Skip x coordinate (digits until `;`)
4. Skip y coordinate (digits until `M` or `m`)
5. If button is 64, emit `IK_INPUT_SCROLL_UP`
6. If button is 65, emit `IK_INPUT_SCROLL_DOWN`
7. All other button codes emit `IK_INPUT_UNKNOWN` (ignore clicks for now)

The existing `esc_buf` (16 bytes) is sufficient for mouse sequences.

## TDD Cycle

### Red
1. Add tests in `tests/unit/input/input_test.c`:
   - Scroll up: feed `"\x1b[<64;10;20M"` byte-by-byte, expect `IK_INPUT_SCROLL_UP`
   - Scroll down: feed `"\x1b[<65;10;20m"` byte-by-byte, expect `IK_INPUT_SCROLL_DOWN`
   - Click (button 0): feed `"\x1b[<0;10;20M"`, expect `IK_INPUT_UNKNOWN`
   - Partial sequence: verify parser buffers bytes correctly
   - Large coordinates: `"\x1b[<64;999;999M"` still parses correctly
2. Add stub handling in `parse_escape_sequence()` that returns `IK_INPUT_UNKNOWN` for `<`
3. Run `make check` - expect test failures (scroll actions not recognized)

### Green
1. Extend `parse_escape_sequence()` in input.c:
   - When `esc_buf[0] == '<'`, enter SGR mouse parsing
   - Parse button code (digits before first `;`)
   - Skip x and y coordinates
   - On `M` or `m`, check button code:
     - 64 → return `IK_INPUT_SCROLL_UP`
     - 65 → return `IK_INPUT_SCROLL_DOWN`
     - other → return `IK_INPUT_UNKNOWN`
2. Run `make check` - expect pass

### Refactor
1. Ensure buffer overflow protection (sequence length limits)
2. Handle malformed sequences gracefully (return UNKNOWN, reset state)
3. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- Parser recognizes SGR scroll up/down sequences
- Other mouse events (clicks) are ignored (UNKNOWN)
- Malformed sequences don't crash parser
