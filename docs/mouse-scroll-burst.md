# Mouse Scroll Burst Detection

## Overview

Mouse wheel scrolling in terminal applications is complicated because terminals don't have native mouse wheel events. Instead, when alternate scroll mode is enabled, the terminal translates wheel movements into arrow key sequences.

## Current Implementation

The arrow burst detector (`src/arrow_burst.c`) distinguishes between:
- **Keyboard arrow presses**: Single, isolated arrow events
- **Mouse wheel scrolling**: Rapid bursts of arrow events (within 15ms threshold)

### Event Flow

```
Mouse wheel notch
    → Terminal sends arrow escape sequence
    → Input parser emits IK_INPUT_ARROW_UP/DOWN
    → Burst detector buffers event
    → If rapid sequence detected → emit IK_ARROW_BURST_RESULT_SCROLL_UP/DOWN
    → ik_repl_handle_scroll_up/down_action() moves viewport by MOUSE_SCROLL_LINES
```

### Key Files

- `src/arrow_burst.c` - Burst detection state machine
- `src/arrow_burst.h` - Threshold constant `IK_ARROW_BURST_THRESHOLD_MS` (15ms)
- `src/repl_actions.c` - Intercepts arrows, routes through burst detector
- `src/repl_actions_viewport.c` - Scroll handlers, `MOUSE_SCROLL_LINES` constant

## Why MOUSE_SCROLL_LINES Must Stay at 1

A single mouse wheel movement generates **multiple** arrow events (typically 3-4 per notch, depending on the terminal and mouse settings). Each detected burst event triggers a scroll action.

```
One wheel notch → 3-4 arrow events → 3-4 scroll actions → 3-4 lines scrolled
```

If `MOUSE_SCROLL_LINES = 3`:
```
One wheel notch → 3-4 arrow events → 3-4 scroll actions → 9-12 lines scrolled
```

This makes scrolling unusably fast and jumpy.

## Future: Adjusting Scroll Speed

To increase scroll speed (lines per wheel notch), the correct approach is to modify the burst detection logic, not the lines-per-scroll constant.

### Option A: Accumulate Events in Burst Detector

Instead of emitting a scroll result for each arrow in a burst, accumulate N events before emitting:

```c
// In arrow_burst.c
#define BURST_EVENTS_PER_SCROLL 2  // Emit scroll every 2 burst events

if (detector->burst_count % BURST_EVENTS_PER_SCROLL == 0) {
    return (direction == UP) ? SCROLL_UP : SCROLL_DOWN;
} else {
    return NONE;  // Swallow this event
}
```

This reduces the number of scroll actions while keeping each action at 1 line.

### Option B: Emit Scroll with Multiplier

Add a scroll amount to the burst result, letting the handler know how many lines to scroll based on burst intensity:

```c
typedef struct {
    ik_arrow_burst_result_t type;
    size_t scroll_lines;  // 1 for slow scroll, 2-3 for fast scroll
} ik_arrow_burst_outcome_t;
```

The burst detector could increase `scroll_lines` based on how rapid/sustained the burst is.

### Option C: Time-Based Acceleration

Track the duration of continuous scrolling and accelerate over time:
- First 200ms: 1 line per event
- 200-500ms: 2 lines per event
- 500ms+: 3 lines per event

This mimics how many GUI applications handle scroll acceleration.

## Testing Considerations

Any changes to burst detection should be tested with:
1. Unit tests with injected timestamps (existing pattern in `arrow_burst_test.c`)
2. Manual testing with actual mouse wheel on different terminals
3. Verification that keyboard arrows still work for cursor movement

## References

- `docs/rel-05/tasks/arrow-burst-detector.md` - Original implementation task
- `docs/rel-05/tasks/arrow-burst-integration.md` - Integration task
- `tests/unit/arrow_burst/arrow_burst_test.c` - Unit tests with timestamp injection
