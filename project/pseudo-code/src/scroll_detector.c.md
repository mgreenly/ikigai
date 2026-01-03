## Overview

The scroll detector module distinguishes between mouse wheel scroll events and keyboard arrow key presses by analyzing the timing pattern of arrow input events. When arrow keys arrive in rapid succession (within a configurable burst threshold), they're interpreted as mouse wheel events. Single arrows or arrows with pauses between them are treated as keyboard input.

## Code

```
state machine with three states: IDLE, WAITING, ABSORBING

function create_detector(parent):
    allocate new detector structure with parent context
    initialize state to IDLE
    initialize timer_start to 0
    set burst_threshold to default milliseconds
    return detector

function process_arrow(detector, arrow_direction, timestamp):
    elapsed_time = timestamp - detector.timer_start

    switch detector.state:

        case IDLE:
            transition to WAITING state
            record the arrow direction
            start timer with current timestamp
            return NONE (no output yet)

        case WAITING:
            if elapsed_time <= burst_threshold:
                // Second arrow arrived quickly - this is a scroll wheel event!
                output SCROLL_UP or SCROLL_DOWN based on recorded direction
                transition to ABSORBING state to consume remaining burst
                restart timer
                return SCROLL result

            else:
                // Timer expired waiting for second arrow - first was just a keyboard arrow
                output ARROW_UP or ARROW_DOWN for the pending direction
                reset pending to new incoming arrow
                restart timer for new potential burst
                return ARROW result

        case ABSORBING:
            if elapsed_time <= burst_threshold:
                // Another arrow in the burst - consume it
                restart timer
                return ABSORBED (silently discard)

            else:
                // Burst timeout expired - prepare for new burst
                transition to WAITING state
                record new arrow direction
                restart timer
                return NONE

function check_timeout(detector, timestamp):
    if detector is IDLE:
        return NONE (nothing pending)

    elapsed_time = timestamp - detector.timer_start

    if elapsed_time <= burst_threshold:
        return NONE (not yet expired)

    // Timer expired - flush whatever is pending
    if detector is WAITING:
        output the pending arrow (ARROW_UP or ARROW_DOWN)
        return to IDLE
        return ARROW result

    else if detector is ABSORBING:
        // Burst finished, no more output needed
        return to IDLE
        return NONE

function get_timeout_needed(detector, timestamp):
    if detector is IDLE:
        return -1 (no timeout needed)

    elapsed_time = timestamp - detector.timer_start
    remaining = burst_threshold - elapsed_time

    if remaining < 0:
        return 0 (already expired)
    else:
        return remaining (milliseconds until expiration)

function flush_pending(detector):
    if detector is WAITING:
        // Immediately emit the pending arrow without waiting for timeout
        output ARROW_UP or ARROW_DOWN
        return to IDLE
        return ARROW result

    else:
        // IDLE or ABSORBING - nothing to flush
        return to IDLE
        return NONE

function reset(detector):
    return to IDLE state
    clear timer
```
