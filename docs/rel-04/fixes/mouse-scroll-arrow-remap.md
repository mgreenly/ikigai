# Fix: Remap Arrow Keys to Scroll in Viewport Mode

## Problem

After switching to alternate scroll mode (`?1007h`), scroll wheel sends arrow key sequences. Currently, `IK_INPUT_ARROW_UP/DOWN` triggers history navigation, not viewport scrolling.

Users expect scroll wheel to scroll the viewport, not navigate history.

## Solution

When viewport is scrolled (offset > 0), arrow up/down should scroll instead of navigating history. This matches the intuitive expectation that scroll wheel scrolls content.

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/style.md

### Pre-read Docs
- docs/return_values.md

### Pre-read Source (patterns)
- src/repl_actions.c (IK_INPUT_ARROW_UP/DOWN cases)
- src/repl_actions_history.c (ik_repl_handle_arrow_up_action, ik_repl_handle_arrow_down_action)
- src/repl_actions_viewport.c (ik_repl_handle_scroll_up_action, ik_repl_handle_scroll_down_action)

### Pre-read Tests (patterns)
- tests/unit/repl/repl_actions_test.c (action handling tests)
- tests/unit/repl/repl_scroll_test.c (scroll behavior tests)

## Pre-conditions
- `make check` passes
- Fix `mouse-alternate-scroll-mode.md` completed

## Task

Modify arrow up/down handling to scroll viewport when `viewport_offset > 0`.

**Current behavior:**
- Arrow up → history previous (always)
- Arrow down → history next (always)

**New behavior:**
- If `viewport_offset > 0`: arrow up/down → scroll viewport
- If `viewport_offset == 0`: arrow up/down → history navigation (unchanged)

This makes scroll wheel naturally scroll when viewing scrollback, and fall back to history navigation when at the bottom.

## TDD Cycle

### Red
1. Add/modify tests in `tests/unit/repl/repl_actions_test.c`:
   - Arrow up with `viewport_offset > 0` → scroll up (not history)
   - Arrow down with `viewport_offset > 0` → scroll down (not history)
   - Arrow up with `viewport_offset == 0` → history previous (unchanged)
   - Arrow down with `viewport_offset == 0` → history next (unchanged)
   - Arrow down when scrolled returns to offset 0 → next arrow down triggers history
2. Run `make check` - expect test failures

### Green
1. In `src/repl_actions_history.c`, modify `ik_repl_handle_arrow_up_action()`:
   - At start: if `repl->viewport_offset > 0`, call `ik_repl_handle_scroll_up_action(repl)` and return
   - Existing history logic remains for `viewport_offset == 0`
2. Modify `ik_repl_handle_arrow_down_action()`:
   - At start: if `repl->viewport_offset > 0`, call `ik_repl_handle_scroll_down_action(repl)` and return
   - Existing history logic remains for `viewport_offset == 0`
3. Run `make check` - expect pass

### Refactor
1. Consider if the delegation pattern is clean or needs adjustment
2. Update function comments to document the conditional behavior
3. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- Scroll wheel scrolls viewport when viewing scrollback
- Scroll wheel triggers history navigation when at bottom
- Keyboard arrow keys work the same (scroll when scrolled, history when at bottom)
