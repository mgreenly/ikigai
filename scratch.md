# Mouse Scroll Bug Investigation

## Architecture

### Terminal Mode
- Mode 1007 (`\x1b[?1007h`) - Alternate scroll mode
- Converts mouse wheel events to arrow key escape sequences
- Required: without it, no scroll events at all

### Event Flow
```
Mouse wheel notch
  → Terminal sends arrow escape sequences (3 events per notch, rapid)
  → Input parser emits IK_INPUT_ARROW_UP/DOWN
  → Scroll accumulator classifies (wheel vs keyboard)
  → If wheel: SCROLL_UP/DOWN → scroll handler → viewport_offset changes
  → If keyboard: PASS → arrow handler → cursor movement only
```

### Scroll Accumulator Algorithm
```c
Constants:
  ACCUMULATOR_MAX = 15
  ACCUMULATOR_DRAIN = 5
  KEYBOARD_THRESHOLD_MS = 15

On arrow event:
  elapsed = current_time - previous_time

  if (elapsed > 15ms):
    return PASS  // keyboard arrow, let through

  accumulator -= 5
  if (accumulator < 1):
    accumulator = 15  // reset
    return SCROLL_UP/DOWN  // wheel detected

  return NONE  // swallow, still accumulating
```

- 3 rapid events (< 15ms apart) → drains 15 to 0 → emits 1 SCROLL
- Slow events (> 15ms apart) → PASS (keyboard)

### Event Types (must be distinct)
| Event | Source | Handler | Action |
|-------|--------|---------|--------|
| SCROLL_UP/DOWN | Wheel (via accumulator) | scroll handler | viewport_offset ± 1 |
| PASS → ARROW_UP/DOWN | Keyboard | arrow handler | cursor movement |
| Ctrl+P/N | Keyboard | history handler | history navigation |

## The Bug

### Symptom
- Scrolling works when input buffer is visible on screen
- Scrolling stops when input buffer scrolls off screen
- "Accumulated" scrolls must be "undone" before viewport moves again
- Affects both fast and slow scrolling

### Key Observation
Something changes when input buffer visibility changes. The issue is NOT in the accumulator (it emits correctly). The issue is in how events are HANDLED after emission.

## Files Involved

- `src/terminal.c` - Mode 1007 enable/disable
- `src/input.c` - Parses escape sequences to IK_INPUT_* events
- `src/scroll_accumulator.c` - Classifies arrows as wheel vs keyboard
- `src/repl_actions.c` - Routes events to handlers (lines 49-86)
- `src/repl_actions_viewport.c` - Scroll handlers (scroll_up/down_action)
- `src/repl_actions_history.c` - Arrow handlers (cursor movement)
- `src/repl_viewport.c` - Viewport calculation, rendering

## Changes Made (may have broken things)

1. Changed accumulator result enum:
   - Removed: ARROW_UP, ARROW_DOWN
   - Added: PASS
   - Now: SCROLL_UP, SCROLL_DOWN, NONE, PASS

2. Changed accumulator logic:
   - `elapsed > 15ms` → returns PASS (was returning ARROW_UP/DOWN)

3. Changed repl_actions.c intercept:
   - PASS now breaks and falls through to main switch
   - Main switch handles IK_INPUT_ARROW_UP/DOWN via arrow handlers

4. Simplified arrow handlers:
   - Removed: viewport_offset check that converted arrows to scroll
   - Removed: history navigation on arrow keys
   - Now: only cursor movement + completion navigation

## Viewport Offset Clamping

In `repl_viewport.c` rendering (lines 49-54):
```c
size_t offset = repl->viewport_offset;
if (offset > max_offset) {
    offset = max_offset;  // LOCAL variable clamped, not repl->viewport_offset
}
```

This clamps a LOCAL copy. If viewport_offset exceeds max_offset, the display is clamped but the actual value isn't. This could cause the "accumulation" symptom.

But scroll_up_action DOES clamp:
```c
repl->viewport_offset = (new_offset > max_offset) ? max_offset : new_offset;
```

## Debug Logging Added

In repl_actions.c - logs to file (not stderr):
- Which result accumulator returns
- viewport_offset and max_offset values
- Whether offset changes

## Unknowns

1. Why does behavior change when input buffer scrolls off screen?
2. Are scroll events reaching the scroll handler when scrolled up?
3. Is viewport_offset actually changing?
4. Is max_offset calculated correctly in all cases?
