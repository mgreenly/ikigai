# Task: Increase Mouse Scroll Lines to 3

## Target

Input Handling: Increase mouse wheel scroll amount from 1 line to 3 lines per scroll event.

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/scm.md

## Pre-read Source
- src/repl_actions_viewport.c (MOUSE_SCROLL_LINES constant, scroll handlers)
- tests/unit/repl/repl_scroll_test.c (tests verifying scroll behavior)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes

## Background

Mouse wheel scrolling currently moves the viewport by 1 line per scroll event. This feels slow compared to typical terminal emulator behavior. Increasing to 3 lines per scroll provides a more natural scrolling experience.

The `MOUSE_SCROLL_LINES` constant is used only for viewport offset calculations in `ik_repl_handle_scroll_up_action()` and `ik_repl_handle_scroll_down_action()`. It is not used for any other purpose.

## Task

Change `MOUSE_SCROLL_LINES` from 1 to 3.

## TDD Cycle

### Red

Update test assertions in `tests/unit/repl/repl_scroll_test.c`:

1. `test_scroll_up_increases_offset`: Change expected offset from 6 to 8 (5+3)
2. `test_scroll_down_decreases_offset`: Change expected offset from 4 to 2 (5-3)
3. `test_scroll_up_empty_input_buffer`: Change expected offset from 6 to 8 (5+3)

Run `make check` - these tests should fail.

### Green

Change the constant in `src/repl_actions_viewport.c`:
```c
#define MOUSE_SCROLL_LINES 3
```

Run `make check` - tests should pass.

### Verify

1. `make check` - all tests pass
2. `make lint` - no issues

## Post-conditions
- Working tree is clean (all changes committed)
- `make check` passes
- `MOUSE_SCROLL_LINES` is 3
