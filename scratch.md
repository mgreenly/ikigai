# Mouse Wheel Detection - Current State

## What We're Doing
Detecting mouse wheel scroll events vs keyboard arrow presses in terminal mode 1007h (alternate scroll mode).

## How 1007h Mode Works
- Mouse wheel events are converted to arrow escape sequences (ESC [ A / ESC [ B)
- Wheel events arrive as rapid pairs (~5ms apart)
- Keyboard arrows arrive as single events (human speed)
- Detection strategy: buffer first arrow, wait 10ms for second, decide based on timing

## Timer Detection Implementation Status

### Terminal Mode (WORKING)
`src/terminal.c:16` - Using 1007h mode (alternate scroll):
```c
#define ESC_MOUSE_ENABLE "\x1b[?1007h"  // Converts wheel -> arrows
```

### Detection Module (RENAMED, LOGIC EXISTS)
Just renamed `scroll_detumulator` -> `scroll_detector` because the name was confusing.

The module implements timer-based detection:
- `ik_scroll_detector_process_arrow()` - Called when arrow arrives
  - First arrow: buffer it, return NONE (triggers select timeout setup)
  - Second arrow within 10ms: emit SCROLL_UP/DOWN, re-buffer current
  - Second arrow after 10ms: emit ARROW_UP/DOWN, re-buffer current
- `ik_scroll_detector_check_timeout()` - Called when select() times out (10ms expired)
  - Flushes buffered arrow as ARROW_UP/DOWN
- `ik_scroll_detector_get_timeout_ms()` - Tells select() when to wake up
  - Returns remaining time until flush (0-10ms)

### Select Loop Integration (WORKING)
`src/repl_event_handlers.c:59` - Includes scroll timeout in select() calculation
`src/repl.c:86` - Calls check_timeout() when select() expires

### Current Problem
**Logs show ONLY arrow events, NO mousewheel scroll events.**

This means:
1. Either mousewheel isn't sending two arrows in 1007h mode, OR
2. The second arrow isn't arriving within the 10ms window, OR
3. The re-buffering logic (lines 53-55 in scroll_detector.c) is wrong

## Key Timing Values
- Burst threshold: 10ms (`IK_SCROLL_BURST_THRESHOLD_MS`)
- Expected mousewheel spacing: ~5ms
- Margin: 5ms buffer should be enough

## Debug Logging (ADDED)
`src/scroll_detector.c` has debug logging via `ik_log_debug_json()`:
- Logs MOUSE_WHEEL detection (rapid burst)
- Logs ARROW detection (timeout or slow_followup)

**Important:** Uses `ik_log_debug_json()` NOT `ik_log_debug()` because stdout is invisible in alternate buffer mode.

## Next Steps
1. Verify mousewheel actually sends two arrow sequences in 1007h mode
2. Check if both sequences are being parsed (input.c parser state)
3. Verify timing - are they arriving within 10ms?
4. Investigate re-buffering logic - should it clear pending instead?

## Key Files
- `src/scroll_detector.{c,h}` - Timer detection implementation
- `src/terminal.c` - 1007h mode setup
- `src/repl_event_handlers.c` - select() timeout calculation
- `src/repl.c` - timeout handling
- `src/repl_actions.c:59` - Routes arrows through detector
- `src/input.c` - Escape sequence parser

## Log Location
`.ikigai/logs/current/log` - tail this with grep for scroll_detect events
