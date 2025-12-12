# Task: Create Arrow Burst Detector Module

## Target

Bug Fix: Distinguish mouse wheel scroll (rapid arrow bursts) from keyboard arrow keys.

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/errors.md
- .agents/skills/patterns/state-machine.md
- .agents/skills/patterns/context-struct.md

## Pre-read Docs
- docs/memory.md (talloc patterns)
- docs/error_handling.md (res_t patterns)

## Pre-read Source (patterns)
- src/input.h (ik_input_action_type_t - IK_INPUT_ARROW_UP, IK_INPUT_ARROW_DOWN)
- src/input.c (input parsing patterns)
- src/history.h (example of simple state tracking module)
- src/history.c (example of simple state tracking module)
- tests/unit/input/input_test.c (input testing patterns)
- tests/unit/history/history_test.c (state machine testing patterns)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes

## Background

Modern terminals convert mouse wheel scroll events into arrow key escape sequences when an application is in the alternate screen buffer. This means the application receives identical escape sequences for:
- Mouse wheel scroll up → `ESC [ A` (same as arrow up)
- Keyboard arrow up → `ESC [ A`

However, mouse wheel events arrive in **rapid bursts** (multiple events within milliseconds), while keyboard presses arrive singly (with ~300ms delay before key repeat kicks in at ~30ms intervals).

**Detection strategy:** If multiple arrow events arrive within a short threshold (~15ms), treat as mouse wheel scroll. If a single arrow event arrives and no follow-up within threshold, treat as keyboard.

## Task

Create a new module `src/arrow_burst.c` and `src/arrow_burst.h` that implements a state machine to detect arrow burst patterns.

**CRITICAL:** The detector must be **time-injectable** for testing. Tests pass explicit timestamps, not real time. The module contains pure logic with no I/O or clock calls.

## Data Structures

```c
// Arrow burst detector state
typedef enum {
    IK_ARROW_BURST_IDLE,       // No arrow events pending
    IK_ARROW_BURST_BUFFERING,  // First arrow received, waiting for more or timeout
} ik_arrow_burst_state_t;

// Result of processing an event or timeout
typedef enum {
    IK_ARROW_BURST_RESULT_NONE,         // No action yet (still buffering)
    IK_ARROW_BURST_RESULT_SCROLL_UP,    // Detected scroll up burst
    IK_ARROW_BURST_RESULT_SCROLL_DOWN,  // Detected scroll down burst
    IK_ARROW_BURST_RESULT_CURSOR_UP,    // Single arrow up (keyboard)
    IK_ARROW_BURST_RESULT_CURSOR_DOWN,  // Single arrow down (keyboard)
} ik_arrow_burst_result_t;

// Detector context
typedef struct {
    ik_arrow_burst_state_t state;
    ik_input_action_type_t pending_direction;  // ARROW_UP or ARROW_DOWN
    int64_t first_event_time_ms;               // Timestamp of first event in potential burst
    int32_t burst_count;                       // Number of events in current burst
} ik_arrow_burst_detector_t;

#define IK_ARROW_BURST_THRESHOLD_MS 15
```

## API

```c
// Create detector (talloc-based)
ik_arrow_burst_detector_t *ik_arrow_burst_create(void *parent);

// Process an arrow event with explicit timestamp
// Returns result indicating what action to take (if any)
ik_arrow_burst_result_t ik_arrow_burst_process(
    ik_arrow_burst_detector_t *det,
    ik_input_action_type_t arrow_type,  // Must be ARROW_UP or ARROW_DOWN
    int64_t timestamp_ms
);

// Check if timeout has expired, return pending action if so
// Call this when no input received but time has passed
ik_arrow_burst_result_t ik_arrow_burst_check_timeout(
    ik_arrow_burst_detector_t *det,
    int64_t current_time_ms
);

// Get remaining timeout in ms (for select() timeout calculation)
// Returns -1 if no timeout pending, otherwise ms until timeout
int64_t ik_arrow_burst_get_timeout_ms(
    ik_arrow_burst_detector_t *det,
    int64_t current_time_ms
);

// Reset detector to idle state
void ik_arrow_burst_reset(ik_arrow_burst_detector_t *det);
```

## State Machine Logic

```
IDLE:
  on ARROW_UP/DOWN:
    → record direction, timestamp, burst_count=1
    → transition to BUFFERING
    → return RESULT_NONE

BUFFERING:
  on ARROW_UP/DOWN (same direction, within threshold):
    → increment burst_count
    → if burst_count >= 2: return SCROLL_UP/DOWN
    → else: return RESULT_NONE

  on ARROW_UP/DOWN (same direction, OUTSIDE threshold):
    → this is a new burst, previous was single key
    → emit CURSOR_UP/DOWN for previous
    → start new buffering for this event
    → return CURSOR_UP/DOWN

  on ARROW_UP/DOWN (DIFFERENT direction):
    → previous was single key (direction change = new input)
    → emit CURSOR for previous direction
    → start new buffering for new direction
    → return CURSOR_UP/DOWN

  on timeout (current_time > first_event_time + threshold):
    → single arrow key detected
    → transition to IDLE
    → return CURSOR_UP/DOWN
```

## TDD Cycle

### Red

Create `tests/unit/arrow_burst/arrow_burst_test.c`:

```c
// Test 1: Single arrow up, then timeout → CURSOR_UP
START_TEST(test_single_arrow_up_timeout)
{
    ik_arrow_burst_detector_t *det = ik_arrow_burst_create(ctx);

    // First arrow at T=0
    ik_arrow_burst_result_t result = ik_arrow_burst_process(det, IK_INPUT_ARROW_UP, 0);
    ck_assert_int_eq(result, IK_ARROW_BURST_RESULT_NONE);  // Buffering

    // Timeout at T=20 (past 15ms threshold)
    result = ik_arrow_burst_check_timeout(det, 20);
    ck_assert_int_eq(result, IK_ARROW_BURST_RESULT_CURSOR_UP);
}
END_TEST

// Test 2: Two rapid arrows → SCROLL
START_TEST(test_two_rapid_arrows_scroll)
{
    ik_arrow_burst_detector_t *det = ik_arrow_burst_create(ctx);

    // First arrow at T=0
    ik_arrow_burst_result_t result = ik_arrow_burst_process(det, IK_INPUT_ARROW_UP, 0);
    ck_assert_int_eq(result, IK_ARROW_BURST_RESULT_NONE);

    // Second arrow at T=5 (within 15ms)
    result = ik_arrow_burst_process(det, IK_INPUT_ARROW_UP, 5);
    ck_assert_int_eq(result, IK_ARROW_BURST_RESULT_SCROLL_UP);
}
END_TEST

// Test 3: Two slow arrows → two CURSOR moves
START_TEST(test_two_slow_arrows_cursor)
{
    ik_arrow_burst_detector_t *det = ik_arrow_burst_create(ctx);

    // First arrow at T=0
    ik_arrow_burst_result_t result = ik_arrow_burst_process(det, IK_INPUT_ARROW_UP, 0);
    ck_assert_int_eq(result, IK_ARROW_BURST_RESULT_NONE);

    // Timeout at T=20
    result = ik_arrow_burst_check_timeout(det, 20);
    ck_assert_int_eq(result, IK_ARROW_BURST_RESULT_CURSOR_UP);

    // Second arrow at T=100 (new event)
    result = ik_arrow_burst_process(det, IK_INPUT_ARROW_UP, 100);
    ck_assert_int_eq(result, IK_ARROW_BURST_RESULT_NONE);

    // Timeout at T=120
    result = ik_arrow_burst_check_timeout(det, 120);
    ck_assert_int_eq(result, IK_ARROW_BURST_RESULT_CURSOR_UP);
}
END_TEST

// Test 4: Burst of 5 arrows → multiple SCROLL results
START_TEST(test_burst_of_five)
{
    ik_arrow_burst_detector_t *det = ik_arrow_burst_create(ctx);

    ik_arrow_burst_result_t r;
    r = ik_arrow_burst_process(det, IK_INPUT_ARROW_DOWN, 0);
    ck_assert_int_eq(r, IK_ARROW_BURST_RESULT_NONE);

    r = ik_arrow_burst_process(det, IK_INPUT_ARROW_DOWN, 3);
    ck_assert_int_eq(r, IK_ARROW_BURST_RESULT_SCROLL_DOWN);

    r = ik_arrow_burst_process(det, IK_INPUT_ARROW_DOWN, 6);
    ck_assert_int_eq(r, IK_ARROW_BURST_RESULT_SCROLL_DOWN);

    r = ik_arrow_burst_process(det, IK_INPUT_ARROW_DOWN, 9);
    ck_assert_int_eq(r, IK_ARROW_BURST_RESULT_SCROLL_DOWN);

    r = ik_arrow_burst_process(det, IK_INPUT_ARROW_DOWN, 12);
    ck_assert_int_eq(r, IK_ARROW_BURST_RESULT_SCROLL_DOWN);
}
END_TEST

// Test 5: Direction change mid-buffer → emit cursor for first
START_TEST(test_direction_change)
{
    ik_arrow_burst_detector_t *det = ik_arrow_burst_create(ctx);

    // Arrow up at T=0
    ik_arrow_burst_result_t r = ik_arrow_burst_process(det, IK_INPUT_ARROW_UP, 0);
    ck_assert_int_eq(r, IK_ARROW_BURST_RESULT_NONE);

    // Arrow DOWN at T=5 (different direction)
    r = ik_arrow_burst_process(det, IK_INPUT_ARROW_DOWN, 5);
    ck_assert_int_eq(r, IK_ARROW_BURST_RESULT_CURSOR_UP);  // Emit previous

    // Now buffering down, timeout
    r = ik_arrow_burst_check_timeout(det, 25);
    ck_assert_int_eq(r, IK_ARROW_BURST_RESULT_CURSOR_DOWN);
}
END_TEST

// Test 6: get_timeout_ms returns correct remaining time
START_TEST(test_get_timeout)
{
    ik_arrow_burst_detector_t *det = ik_arrow_burst_create(ctx);

    // No event → no timeout
    int64_t timeout = ik_arrow_burst_get_timeout_ms(det, 0);
    ck_assert_int_eq(timeout, -1);

    // Event at T=0 → timeout at T=15
    ik_arrow_burst_process(det, IK_INPUT_ARROW_UP, 0);

    timeout = ik_arrow_burst_get_timeout_ms(det, 0);
    ck_assert_int_eq(timeout, 15);

    timeout = ik_arrow_burst_get_timeout_ms(det, 10);
    ck_assert_int_eq(timeout, 5);

    timeout = ik_arrow_burst_get_timeout_ms(det, 15);
    ck_assert_int_eq(timeout, 0);

    timeout = ik_arrow_burst_get_timeout_ms(det, 20);
    ck_assert_int_eq(timeout, 0);  // Already expired
}
END_TEST

// Test 7: Arrow outside threshold starts new buffer
START_TEST(test_outside_threshold_new_buffer)
{
    ik_arrow_burst_detector_t *det = ik_arrow_burst_create(ctx);

    // Arrow at T=0
    ik_arrow_burst_result_t r = ik_arrow_burst_process(det, IK_INPUT_ARROW_UP, 0);
    ck_assert_int_eq(r, IK_ARROW_BURST_RESULT_NONE);

    // Arrow at T=50 (outside threshold, same direction)
    // Should emit CURSOR_UP for previous and start new buffering
    r = ik_arrow_burst_process(det, IK_INPUT_ARROW_UP, 50);
    ck_assert_int_eq(r, IK_ARROW_BURST_RESULT_CURSOR_UP);

    // Now in BUFFERING for new event, timeout
    r = ik_arrow_burst_check_timeout(det, 70);
    ck_assert_int_eq(r, IK_ARROW_BURST_RESULT_CURSOR_UP);
}
END_TEST

// Test 8: Reset clears state
START_TEST(test_reset)
{
    ik_arrow_burst_detector_t *det = ik_arrow_burst_create(ctx);

    ik_arrow_burst_process(det, IK_INPUT_ARROW_UP, 0);
    ik_arrow_burst_reset(det);

    // Should be idle, no timeout
    int64_t timeout = ik_arrow_burst_get_timeout_ms(det, 10);
    ck_assert_int_eq(timeout, -1);
}
END_TEST

// Test 9: Non-arrow input is assertion failure (or ignored)
// Depending on design choice - document behavior

// Test 10: Scroll continues to emit for each additional burst event
START_TEST(test_continued_scroll_burst)
{
    ik_arrow_burst_detector_t *det = ik_arrow_burst_create(ctx);

    // Start burst
    ik_arrow_burst_process(det, IK_INPUT_ARROW_UP, 0);
    ik_arrow_burst_result_t r = ik_arrow_burst_process(det, IK_INPUT_ARROW_UP, 3);
    ck_assert_int_eq(r, IK_ARROW_BURST_RESULT_SCROLL_UP);

    // Burst continues - each new rapid event should scroll
    r = ik_arrow_burst_process(det, IK_INPUT_ARROW_UP, 6);
    ck_assert_int_eq(r, IK_ARROW_BURST_RESULT_SCROLL_UP);

    // Pause (T=50, outside threshold)
    // This should NOT emit cursor - we're in scroll mode
    // Wait for timeout or handle differently?
    // After burst ends, no cursor emit - just return to idle
}
END_TEST
```

Run `make check` - expect tests to FAIL (module doesn't exist yet).

### Green

1. Create `src/arrow_burst.h` with types and function declarations

2. Create `src/arrow_burst.c` with implementation:
   - `ik_arrow_burst_create()` - talloc allocate, initialize to IDLE
   - `ik_arrow_burst_process()` - implement state machine
   - `ik_arrow_burst_check_timeout()` - check if timeout expired
   - `ik_arrow_burst_get_timeout_ms()` - calculate remaining time
   - `ik_arrow_burst_reset()` - reset to IDLE

3. Add to Makefile (src objects and test)

4. Run `make check` - all tests should pass

### Verify

1. Run `make check` - all tests pass
2. Run `make lint` - complexity checks pass
3. Run `make coverage` - new module has 100% coverage

## Post-conditions

- `make check` passes
- New module `src/arrow_burst.c` and `src/arrow_burst.h` exist
- Comprehensive unit tests in `tests/unit/arrow_burst/arrow_burst_test.c`
- Module is pure logic, no I/O, no clock calls
- All timestamps injected for testability
- 100% test coverage on new module
- Working tree is clean (all changes committed)

## Notes

This module is intentionally isolated from the REPL. Integration happens in the next task. This separation ensures the burst detection logic is thoroughly tested in isolation before wiring it into the event loop.

The 15ms threshold is defined as a constant and can be tuned if needed.
