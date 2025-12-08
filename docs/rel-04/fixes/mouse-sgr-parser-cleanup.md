# Fix: Remove SGR Mouse Parser

## Problem

After switching to alternate scroll mode (`?1007h`), the SGR mouse parser in `input.c` is dead code. It parses `ESC [ < button ; x ; y M/m` sequences that are no longer sent.

## Solution

Remove the SGR mouse parsing code to reduce complexity and eliminate dead code.

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/style.md

### Pre-read Docs
- docs/return_values.md

### Pre-read Source (patterns)
- src/input.c (parse_sgr_mouse_sequence function, calls to it in parse_escape_sequence)
- src/input.h (IK_INPUT_SCROLL_UP, IK_INPUT_SCROLL_DOWN - check if still used elsewhere)

### Pre-read Tests (patterns)
- tests/unit/input/input_test.c (SGR mouse parsing tests)
- tests/unit/input/scroll_test.c (scroll-specific tests if exists)

## Pre-conditions
- `make check` passes
- Fix `mouse-alternate-scroll-mode.md` completed
- Fix `mouse-scroll-arrow-remap.md` completed

## Task

Remove the SGR mouse sequence parser since it's no longer needed.

**Code to remove:**
1. `parse_sgr_mouse_sequence()` function in `input.c`
2. Call to `parse_sgr_mouse_sequence()` in `parse_escape_sequence()`
3. Tests for SGR mouse parsing in `tests/unit/input/`

**Code to keep:**
- `IK_INPUT_SCROLL_UP` / `IK_INPUT_SCROLL_DOWN` enum values (may be useful for future)
- OR remove them if truly unused (check all references first)

## TDD Cycle

### Red
1. Search for all uses of `IK_INPUT_SCROLL_UP` and `IK_INPUT_SCROLL_DOWN`:
   - If used only in SGR parser and tests → remove enum values too
   - If used elsewhere → keep enum values
2. Remove SGR mouse tests from `tests/unit/input/`:
   - Tests feeding `"\x1b[<64;10;20M"` etc.
3. Run `make check` - expect failures (tests reference removed code)

### Green
1. Remove `parse_sgr_mouse_sequence()` function from `input.c`
2. Remove call to `parse_sgr_mouse_sequence()` from `parse_escape_sequence()`
3. If enum values unused elsewhere, remove `IK_INPUT_SCROLL_UP` / `IK_INPUT_SCROLL_DOWN` from `input.h`
4. Remove corresponding cases from `repl_actions.c` if enum values removed
5. Run `make check` - expect pass

### Refactor
1. Verify no dead code remains
2. Run `make check` - verify still green
3. Run `make lint` - verify no warnings

## Post-conditions
- `make check` passes
- `make lint` passes
- No SGR mouse parsing code remains
- Input parser is simpler
