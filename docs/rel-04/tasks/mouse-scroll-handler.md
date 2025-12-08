# Task: Handle Mouse Scroll Actions

## Target
Feature: Mouse wheel scrolling

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/style.md

### Pre-read Docs
- docs/return_values.md

### Pre-read Source (patterns)
- src/repl.h (ik_repl_ctx_t with viewport_offset field)
- src/repl_actions.c (handle_page_up_action_, IK_INPUT_PAGE_DOWN case - viewport scrolling logic)
- src/repl_viewport.c (ik_repl_calculate_viewport - how offset is used)

### Pre-read Tests (patterns)
- tests/unit/repl/repl_viewport_test.c (viewport calculation tests)
- tests/unit/repl/repl_actions_test.c (action handling tests if exists)

## Pre-conditions
- `make check` passes
- Task `mouse-scroll-action-types.md` completed
- Task `mouse-sgr-parse.md` completed

## Task
Handle `IK_INPUT_SCROLL_UP` and `IK_INPUT_SCROLL_DOWN` actions in the REPL action processor.

**Behavior:**
- Scroll up: increase `viewport_offset` by `MOUSE_SCROLL_LINES` (clamp to max)
- Scroll down: decrease `viewport_offset` by `MOUSE_SCROLL_LINES` (clamp to 0)

**Constant:**
Define `MOUSE_SCROLL_LINES` as 1 (configurable for future adjustment).

This is similar to Page Up/Down handling but scrolls by a fixed small amount instead of a full screen.

## TDD Cycle

### Red
1. Add tests in appropriate test file (create `tests/unit/repl/repl_scroll_test.c` if needed):
   - `IK_INPUT_SCROLL_UP` increases `viewport_offset` by 1
   - `IK_INPUT_SCROLL_DOWN` decreases `viewport_offset` by 1
   - Scroll up clamps at max offset (can't scroll past top)
   - Scroll down clamps at 0 (can't scroll past bottom)
   - Scroll down when already at 0 stays at 0
   - Scroll actions don't affect input buffer content
2. Run `make check` - expect failures (actions not handled)

### Green
1. In `src/repl_actions.c`:
   - Define `#define MOUSE_SCROLL_LINES 1` at top of file
   - Add `case IK_INPUT_SCROLL_UP:` in `ik_repl_process_action()`:
     - Calculate max_offset (same logic as page up)
     - `new_offset = viewport_offset + MOUSE_SCROLL_LINES`
     - Clamp to max_offset
   - Add `case IK_INPUT_SCROLL_DOWN:`:
     - If `viewport_offset >= MOUSE_SCROLL_LINES`, subtract
     - Else set to 0
2. Run `make check` - expect pass

### Refactor
1. Consider extracting shared clamping logic with Page Up/Down
2. Ensure MOUSE_SCROLL_LINES is easy to find and modify
3. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- Mouse scroll adjusts viewport by 1 line
- Proper clamping at both ends
- `MOUSE_SCROLL_LINES` constant defined for future adjustment
