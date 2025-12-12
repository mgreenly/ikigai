# Task: Fix Mouse Scroll To Scroll Scrollback Not Input History

## Target

User Story: docs/rel-05/user-stories/49-mouse-scroll-scrollback-not-history.md

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/errors.md

## Pre-read Docs
- docs/rel-05/user-stories/49-mouse-scroll-scrollback-not-history.md

## Pre-read Source (patterns)
- src/input.c (input event parsing, mouse event handling)
- src/repl_event_handlers.c (event dispatch to actions)
- src/repl_actions.c (action handlers for scrolling)
- src/repl_actions_history.c (input history navigation)
- src/scrollback.c (scrollback viewport scrolling)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- Keyboard input history navigation works (Up/Down arrows)
- Bug present: mouse wheel scrolls input history instead of scrollback viewport

## Task

Fix the mouse scroll event routing so mouse wheel scrolls the scrollback viewport, not the input history.

**Root cause:** When input history feature was added, mouse scroll events were incorrectly mapped to input history navigation actions instead of scrollback viewport scrolling actions.

**Solution:** Route mouse scroll events to scrollback viewport scrolling, ensure input history is only navigated via keyboard.

## TDD Cycle

### Red

1. Create test in `tests/unit/repl/mouse_scroll_test.c` (or add to existing mouse/scroll tests):
   - Setup REPL with scrollback content (multiple lines)
   - Setup input buffer with text: "current input"
   - Setup input history with previous entries: ["old command 1", "old command 2"]
   - Simulate mouse wheel up event
   - Verify scrollback viewport position changed (scrolled up)
   - Verify input buffer content unchanged (still "current input")
   - Verify input history position unchanged (not navigated)

2. Add test for mouse wheel down:
   - Similar setup
   - Simulate mouse wheel down event
   - Verify scrollback scrolled down
   - Verify input buffer unchanged

3. Run `make check` - expect test to FAIL (mouse changes input history)

### Green

1. Locate mouse event handling in `src/input.c`:
   - Find where mouse scroll events are parsed
   - Identify event codes for scroll up/down

2. Locate event dispatch in `src/repl_event_handlers.c`:
   - Find where mouse scroll events are routed to actions
   - Current (WRONG) routing probably goes to history navigation

3. Change routing from input history to scrollback scroll:
   ```c
   // BEFORE (WRONG):
   case INPUT_EVENT_MOUSE_SCROLL_UP:
       return ik_repl_history_previous(repl);  // WRONG - don't touch history

   case INPUT_EVENT_MOUSE_SCROLL_DOWN:
       return ik_repl_history_next(repl);  // WRONG

   // AFTER (CORRECT):
   case INPUT_EVENT_MOUSE_SCROLL_UP:
       return ik_repl_scroll_up(repl, scroll_lines);  // Scroll viewport

   case INPUT_EVENT_MOUSE_SCROLL_DOWN:
       return ik_repl_scroll_down(repl, scroll_lines);  // Scroll viewport
   ```

4. Verify scrollback scroll functions exist:
   - `ik_repl_scroll_up()` / `ik_scrollback_scroll_up()`
   - `ik_repl_scroll_down()` / `ik_scrollback_scroll_down()`
   - These should be same functions used by Page Up/Page Down

5. Ensure input history navigation is ONLY reachable via keyboard:
   - Up Arrow → `ik_repl_history_previous()`
   - Down Arrow → `ik_repl_history_next()`
   - No mouse events should route to history functions

6. Run `make check` - expect PASS

### Verify

1. Run `make check` - all tests pass

2. Run `make lint` - complexity checks pass

3. Manual test:
   - Run `bin/ikigai`
   - Submit several messages to create scrollback
   - Type some text in input (don't submit)
   - Scroll mouse wheel up
   - Verify scrollback scrolls up, input text unchanged
   - Scroll mouse wheel down
   - Verify scrollback scrolls down, input text unchanged
   - Use Up arrow key
   - Verify input history navigation works (input replaced with previous command)
   - Use Down arrow key
   - Verify input history navigation works (input replaced with next/current)

## Post-conditions

- `make check` passes
- Mouse wheel scrolls scrollback viewport
- Mouse wheel does NOT navigate input history
- Input buffer content unchanged during mouse scrolling
- Working tree is clean (all changes committed)
- Keyboard Up/Down still navigate input history correctly
- Test exists verifying mouse scroll behavior
