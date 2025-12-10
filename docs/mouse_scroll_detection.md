# Mouse Scroll Detection via Terminal Mode 1007h

This document describes how ikigai distinguishes mouse wheel scrolls from keyboard arrow keys using terminal escape sequences and timing-based burst detection.

## Problem Statement

Terminal applications receive keyboard and mouse input as byte sequences. When using terminal mode 1007h (Alternate Scroll Mode), mouse wheel scrolls are converted to arrow key sequences—identical to pressing the up/down arrow keys. We need to distinguish between:

- **Mouse wheel scroll**: User rotates wheel one notch
- **Keyboard arrow**: User presses up/down arrow key

## Solution Overview

Two key insights enable detection:

1. **Terminals send multiple arrows per wheel notch** (3-10 depending on terminal)
2. **These arrows arrive in rapid bursts** (sub-millisecond spacing)

Keyboard arrows, even when held for repeat, have ~30ms intervals. By detecting "rapid bursts of 2+ arrows", we reliably classify mouse wheel vs keyboard.

## Terminal Setup

### Entering Alternate Buffer Mode

On startup, send these escape sequences to the terminal:

```
ESC[?1049h    # Enter alternate screen buffer
ESC[?1007h    # Enable alternate scroll mode (mouse wheel → arrows)
ESC[?25l      # Hide cursor (optional, for cleaner rendering)
```

**Mode 1007h behavior**: Mouse wheel scrolls generate arrow key escape sequences:
- Scroll up → `ESC[A` (same as up arrow key)
- Scroll down → `ESC[B` (same as down arrow key)

**Why 1007h instead of 1006h/1000h (SGR mouse tracking)?** Mode 1007h does NOT capture mouse clicks, allowing the parent terminal to retain text selection, right-click menus, and other mouse functionality.

### Exiting Alternate Buffer Mode

On cleanup, send only:

```
ESC[?1007l    # Disable alternate scroll mode
ESC[?1049l    # Exit alternate screen buffer
```

**Critical**: Do NOT send `ESC[?25h` (show cursor) or `ESC[0m` (reset attributes) during cleanup. Some terminals (notably Ghostty) corrupt their 1007h state when receiving these sequences, reducing arrows-per-scroll from 3 to 1.

## Arrows Per Scroll by Terminal

| Terminal       | Arrows per wheel notch |
|----------------|------------------------|
| Ghostty        | 3                      |
| gnome-terminal | 3                      |
| foot           | 3                      |
| xterm          | 5                      |
| Kitty          | 10                     |

All tested terminals send N > 1 arrows per scroll. The detection algorithm relies on this property.

## Detection State Machine

```
┌──────────────────────────────────────────────────────────────┐
│                                                              │
│    ┌──────┐   arrow    ┌─────────┐   arrow     ┌───────────┐│
│    │      │ ─────────► │         │ ──────────► │           ││
│    │ IDLE │            │ WAITING │  (≤20ms)    │ ABSORBING ││
│    │      │ ◄───────── │         │  emit WHEEL │           ││
│    └──────┘   timeout  └─────────┘             └───────────┘│
│        ▲      (emit      │    ▲                   │    │    │
│        │       ARROW)    │    │                   │    │    │
│        │                 │    │ arrow (>20ms)     │    │    │
│        │                 │    │ emit ARROW        │    │    │
│        │                 │    │ stay WAITING      │    │    │
│        │                 │    └───────────────────┘    │    │
│        │                 │                             │    │
│        │                 │    timeout                  │    │
│        │                 │    (no output)              │    │
│        └─────────────────┴─────────────────────────────┘    │
│                                                              │
│    arrow (>20ms from ABSORBING): transition to WAITING       │
│    arrow (≤20ms in ABSORBING): absorb, reset timer           │
└──────────────────────────────────────────────────────────────┘
```

### States

| State     | Meaning                                           |
|-----------|---------------------------------------------------|
| IDLE      | No pending arrow event                            |
| WAITING   | First arrow received, waiting to classify         |
| ABSORBING | Burst detected (wheel), absorbing remaining arrows|

### Transitions

| From      | Event              | Condition | To        | Output       |
|-----------|--------------------|-----------|-----------|--------------|
| IDLE      | arrow              | —         | WAITING   | none         |
| WAITING   | arrow              | ≤20ms     | ABSORBING | WHEEL        |
| WAITING   | arrow              | >20ms     | WAITING   | ARROW        |
| WAITING   | timeout            | >20ms     | IDLE      | ARROW        |
| ABSORBING | arrow              | ≤20ms     | ABSORBING | none (absorb)|
| ABSORBING | arrow              | >20ms     | WAITING   | none         |
| ABSORBING | timeout            | >20ms     | IDLE      | none         |

### Key Properties

1. **One wheel notch = one WHEEL event** regardless of terminal arrow count
2. **Single keyboard press = one ARROW event** (after 20ms timeout)
3. **Keyboard repeat = multiple ARROW events** (each 30ms+ apart)
4. **No spurious events** from variable terminal behavior

## Event Loop Integration

Pseudocode for the main event loop:

```
detector = ScrollDetector(threshold_ms=20)

loop:
    # Calculate timeout for select()
    timeout = detector.get_timeout_ms(now())
    if timeout < 0:
        timeout = OTHER_TIMEOUTS  # spinner, polling, etc.
    else:
        timeout = min(timeout, OTHER_TIMEOUTS)

    ready = select(fds=[tty_fd, ...], timeout=timeout)

    if ready == 0:  # timeout
        result = detector.check_timeout(now())
        if result == ARROW_UP or result == ARROW_DOWN:
            handle_arrow(result)
        continue

    if tty_fd is readable:
        byte = read(tty_fd, 1)
        action = parse_input(byte)  # may buffer for escape sequences

        if action == ARROW_UP or action == ARROW_DOWN:
            result = detector.process_arrow(action, now())
            switch result:
                case SCROLL_UP:   handle_scroll(UP)
                case SCROLL_DOWN: handle_scroll(DOWN)
                case ARROW_UP:    handle_arrow(UP)
                case ARROW_DOWN:  handle_arrow(DOWN)
                case NONE:        pass  # buffered, waiting
        else:
            # Non-arrow input: flush any pending arrow first
            flushed = detector.flush()
            if flushed != NONE:
                handle_arrow(flushed)
            handle_action(action)
```

## Scroll Detector Implementation

Pseudocode for the detector:

```
struct ScrollDetector:
    state: IDLE | WAITING | ABSORBING
    pending_dir: UP | DOWN
    timer_start_ms: int64
    threshold_ms: int64 = 20

func process_arrow(dir, timestamp_ms) -> Result:
    elapsed = timestamp_ms - timer_start_ms

    switch state:
        case IDLE:
            state = WAITING
            pending_dir = dir
            timer_start_ms = timestamp_ms
            return NONE

        case WAITING:
            if elapsed <= threshold_ms:
                # Burst detected - mouse wheel!
                result = SCROLL_UP if pending_dir == UP else SCROLL_DOWN
                state = ABSORBING
                timer_start_ms = timestamp_ms  # reset for absorb window
                return result
            else:
                # Slow follow-up - keyboard
                result = ARROW_UP if pending_dir == UP else ARROW_DOWN
                pending_dir = dir
                timer_start_ms = timestamp_ms
                # Stay in WAITING
                return result

        case ABSORBING:
            if elapsed <= threshold_ms:
                # More burst arrows - absorb them
                timer_start_ms = timestamp_ms  # extend absorb window
                return NONE
            else:
                # Burst ended, new arrow starting
                state = WAITING
                pending_dir = dir
                timer_start_ms = timestamp_ms
                return NONE

func check_timeout(timestamp_ms) -> Result:
    if state == IDLE:
        return NONE

    elapsed = timestamp_ms - timer_start_ms
    if elapsed <= threshold_ms:
        return NONE

    if state == WAITING:
        # Single arrow - keyboard
        result = ARROW_UP if pending_dir == UP else ARROW_DOWN
        state = IDLE
        return result

    # ABSORBING - burst complete
    state = IDLE
    return NONE

func get_timeout_ms(timestamp_ms) -> int64:
    if state == IDLE:
        return -1  # no timeout needed

    elapsed = timestamp_ms - timer_start_ms
    remaining = threshold_ms - elapsed
    return max(0, remaining)

func flush() -> Result:
    if state == WAITING:
        result = ARROW_UP if pending_dir == UP else ARROW_DOWN
        state = IDLE
        return result

    # IDLE or ABSORBING - nothing to flush
    state = IDLE
    return NONE
```

## Timing Considerations

- **Burst threshold: 20ms** - Wide enough to catch all burst arrows, narrow enough to not conflict with keyboard repeat
- **Keyboard repeat rate: ~30ms** - Typical repeat interval when holding a key
- **Mouse wheel burst spacing: <1ms** - Arrows arrive nearly simultaneously

The 20ms threshold provides a safe margin: mouse bursts complete in <5ms, keyboard repeat is >30ms.

## Testing

The detection can be verified by monitoring logs:

```bash
tail -f .ikigai/logs/current/log | jq -c 'select(.event | test("scroll|arrow"))'
```

Expected log events:
- `arrow_arrival`: Raw arrow input with state and timing
- `scroll_state`: State machine transitions
- `scroll_detect`: Final classification (MOUSE_WHEEL or ARROW)
- `scroll_absorb`: Arrows absorbed in ABSORBING state
