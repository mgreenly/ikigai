# Task: Rewrite Mouse Scroll Detection with Accumulator

## Target

Bug Fix: Replace arrow burst detector with simpler accumulator-based scroll detection.

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/errors.md

## Pre-read Docs
- docs/mouse-scroll-burst.md (background on current implementation)
- docs/memory.md (talloc patterns)

## Pre-read Source
- src/arrow_burst.c (current implementation to replace)
- src/arrow_burst.h (current API)
- src/repl_actions.c (integration point, lines 49-79)
- src/repl_actions_viewport.c (scroll handlers, MOUSE_SCROLL_LINES constant)
- tests/unit/arrow_burst/arrow_burst_test.c (existing tests to replace)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes

## Background

The current arrow burst detector uses a state machine with IDLE/BUFFERING states and timeout-based detection. This is overly complex. A simpler accumulator-based approach cleanly separates mouse wheel detection from keyboard arrows.

## Algorithm

The new algorithm uses a **token bucket** style accumulator:

```
Constants:
  ACCUMULATOR_MAX = 15 (ms)
  ACCUMULATOR_DRAIN = 5 (ms)
  KEYBOARD_THRESHOLD = 15 (ms)
  SCROLL_LINES = 1 (lines per scroll event)

State:
  previous_time: int64_t (timestamp of last key event)
  accumulator: int64_t (starts at ACCUMULATOR_MAX)

On every key event:
  elapsed = current_time - previous_time
  previous_time = current_time

  if (arrow up/down):
    if elapsed > KEYBOARD_THRESHOLD:
      emit ARROW (definitely keyboard, slow enough)
    else:
      accumulator -= ACCUMULATOR_DRAIN
      if accumulator < 1:
        accumulator = ACCUMULATOR_MAX
        emit SCROLL
      // else: swallow (rapid arrow, waiting to confirm scroll)
  else:
    accumulator = min(ACCUMULATOR_MAX, accumulator + elapsed)
```

### Key Insights

1. **Elapsed > 15ms**: Arrow is definitely keyboard (or key repeat at ~30ms). Emit as cursor movement immediately. Accumulator untouched.

2. **Elapsed <= 15ms**: Arrow is probably mouse wheel. Drain accumulator. If depleted, emit scroll.

3. **Non-arrow keys**: Refill the accumulator (typing between arrows resets detection).

4. **No timeout needed**: The elapsed time check handles everything. No buffering state, no deferred actions.

### Behavior Traces

**Mouse wheel (3ms between events):**
```
Arrow 1 (elapsed=3ms):  3<=15, acc=15-5=10, no action (swallow)
Arrow 2 (elapsed=3ms):  3<=15, acc=10-5=5,  no action (swallow)
Arrow 3 (elapsed=3ms):  3<=15, acc=5-5=0,   SCROLL! reset acc=15
Arrow 4 (elapsed=3ms):  3<=15, acc=15-5=10, no action (swallow)
Arrow 5 (elapsed=3ms):  3<=15, acc=10-5=5,  no action (swallow)
Arrow 6 (elapsed=3ms):  3<=15, acc=5-5=0,   SCROLL! reset acc=15
```
Result: 1 scroll per 3 rapid arrows. No cursor movement leaks.

**Keyboard arrow (500ms between presses):**
```
Arrow 1 (elapsed=500ms): 500>15, emit ARROW (cursor moves)
Arrow 2 (elapsed=500ms): 500>15, emit ARROW (cursor moves)
Arrow 3 (elapsed=500ms): 500>15, emit ARROW (cursor moves)
```
Result: Every press moves cursor. Accumulator never drains.

**Key repeat (~33ms):**
```
Arrow 1 (elapsed=33ms): 33>15, emit ARROW
Arrow 2 (elapsed=33ms): 33>15, emit ARROW
Arrow 3 (elapsed=33ms): 33>15, emit ARROW
```
Result: Key repeat is slower than 15ms, so all arrows emit as cursor.

**Mixed: type then scroll:**
```
Type 'h' (elapsed=100ms): acc = min(15, 10+100) = 15
Arrow 1 (elapsed=3ms):    3<=15, acc=15-5=10, swallow
Arrow 2 (elapsed=3ms):    3<=15, acc=10-5=5, swallow
Arrow 3 (elapsed=3ms):    3<=15, acc=5-5=0, SCROLL!
```

## Data Structures

Replace the current `ik_arrow_burst_detector_t` with a simpler struct:

```c
// Scroll accumulator for mouse wheel detection
typedef struct {
    int64_t previous_time_ms;  // Timestamp of last key event
    int64_t accumulator;       // Token bucket (drains on rapid arrows)
} ik_scroll_accumulator_t;

// Result of processing an arrow event
typedef enum {
    IK_SCROLL_RESULT_NONE,        // Swallowed (rapid arrow, not yet scroll)
    IK_SCROLL_RESULT_SCROLL_UP,   // Emit scroll up
    IK_SCROLL_RESULT_SCROLL_DOWN, // Emit scroll down
    IK_SCROLL_RESULT_ARROW_UP,    // Emit cursor up (keyboard)
    IK_SCROLL_RESULT_ARROW_DOWN,  // Emit cursor down (keyboard)
} ik_scroll_result_t;

#define IK_SCROLL_ACCUMULATOR_MAX 15
#define IK_SCROLL_ACCUMULATOR_DRAIN 5
#define IK_SCROLL_KEYBOARD_THRESHOLD_MS 15
```

## API

```c
// Create accumulator (talloc-based)
ik_scroll_accumulator_t *ik_scroll_accumulator_create(void *parent);

// Process an arrow event with explicit timestamp
// Returns what action to take (scroll, arrow, or none)
ik_scroll_result_t ik_scroll_accumulator_process_arrow(
    ik_scroll_accumulator_t *acc,
    ik_input_action_type_t arrow_type,  // Must be ARROW_UP or ARROW_DOWN
    int64_t timestamp_ms
);

// Process a non-arrow event (refills accumulator)
void ik_scroll_accumulator_process_other(
    ik_scroll_accumulator_t *acc,
    int64_t timestamp_ms
);

// Reset to initial state
void ik_scroll_accumulator_reset(ik_scroll_accumulator_t *acc);
```

Note: No `check_timeout()` or `get_timeout_ms()` needed - the algorithm is synchronous.

## TDD Cycle

### Red

Create `tests/unit/scroll_accumulator/scroll_accumulator_test.c`:

```c
// Test 1: Slow arrow (keyboard) emits cursor
START_TEST(test_slow_arrow_emits_cursor)
{
    ik_scroll_accumulator_t *acc = ik_scroll_accumulator_create(ctx);

    // Arrow after 500ms (way above 15ms threshold)
    ik_scroll_result_t r = ik_scroll_accumulator_process_arrow(
        acc, IK_INPUT_ARROW_UP, 500);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_ARROW_UP);
}
END_TEST

// Test 2: Three rapid arrows emit one scroll
START_TEST(test_three_rapid_arrows_scroll)
{
    ik_scroll_accumulator_t *acc = ik_scroll_accumulator_create(ctx);

    // Simulate rapid mouse wheel (3ms apart)
    // First needs a baseline time
    ik_scroll_result_t r;

    r = ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 0);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_NONE);  // elapsed=0, swallow

    r = ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 3);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_NONE);  // acc=10, swallow

    r = ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 6);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_SCROLL_UP);  // acc=5-5=0, scroll!
}
END_TEST

// Test 3: Scroll down direction
START_TEST(test_scroll_down)
{
    ik_scroll_accumulator_t *acc = ik_scroll_accumulator_create(ctx);

    ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_DOWN, 0);
    ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_DOWN, 3);
    ik_scroll_result_t r = ik_scroll_accumulator_process_arrow(
        acc, IK_INPUT_ARROW_DOWN, 6);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_SCROLL_DOWN);
}
END_TEST

// Test 4: Accumulator resets after scroll
START_TEST(test_accumulator_resets_after_scroll)
{
    ik_scroll_accumulator_t *acc = ik_scroll_accumulator_create(ctx);

    // Drain to scroll
    ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 0);
    ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 3);
    ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 6);  // scroll

    // Next 3 should also scroll (accumulator reset to 15)
    ik_scroll_result_t r;
    r = ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 9);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_NONE);  // acc=10

    r = ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 12);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_NONE);  // acc=5

    r = ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 15);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_SCROLL_UP);  // scroll!
}
END_TEST

// Test 5: Non-arrow key refills accumulator
START_TEST(test_non_arrow_refills)
{
    ik_scroll_accumulator_t *acc = ik_scroll_accumulator_create(ctx);

    // Start draining
    ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 0);
    ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 3);  // acc=10

    // Type a character after 50ms
    ik_scroll_accumulator_process_other(acc, 53);  // acc = min(15, 10+50) = 15

    // Now need 3 more rapid arrows to scroll
    ik_scroll_result_t r;
    r = ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 56);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_NONE);  // acc=10

    r = ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 59);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_NONE);  // acc=5

    r = ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 62);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_SCROLL_UP);
}
END_TEST

// Test 6: Key repeat (33ms) always emits cursor
START_TEST(test_key_repeat_emits_cursor)
{
    ik_scroll_accumulator_t *acc = ik_scroll_accumulator_create(ctx);

    // Simulate held arrow key at 30Hz (33ms)
    ik_scroll_result_t r;
    r = ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 0);
    // First event elapsed=0, but let's say initial previous_time is -100
    // Actually need to think about initialization...

    r = ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 33);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_ARROW_UP);

    r = ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 66);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_ARROW_UP);

    r = ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 99);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_ARROW_UP);
}
END_TEST

// Test 7: First event handling (previous_time not set)
START_TEST(test_first_event)
{
    ik_scroll_accumulator_t *acc = ik_scroll_accumulator_create(ctx);

    // Very first arrow event - elapsed is large (from init time of 0)
    ik_scroll_result_t r = ik_scroll_accumulator_process_arrow(
        acc, IK_INPUT_ARROW_UP, 1000);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_ARROW_UP);
}
END_TEST

// Test 8: Direction change - both emit appropriately
START_TEST(test_direction_change)
{
    ik_scroll_accumulator_t *acc = ik_scroll_accumulator_create(ctx);

    // Rapid up arrows
    ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 0);
    ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 3);
    ik_scroll_result_t r = ik_scroll_accumulator_process_arrow(
        acc, IK_INPUT_ARROW_UP, 6);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_SCROLL_UP);

    // Rapid down arrows (accumulator was reset)
    ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_DOWN, 9);
    ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_DOWN, 12);
    r = ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_DOWN, 15);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_SCROLL_DOWN);
}
END_TEST

// Test 9: Reset clears state
START_TEST(test_reset)
{
    ik_scroll_accumulator_t *acc = ik_scroll_accumulator_create(ctx);

    // Drain partially
    ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 0);
    ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 3);

    // Reset
    ik_scroll_accumulator_reset(acc);

    // Should need 3 arrows again
    ik_scroll_result_t r;
    r = ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 100);
    // After reset, elapsed from 0 to 100 is large, so emit cursor
    ck_assert_int_eq(r, IK_SCROLL_RESULT_ARROW_UP);
}
END_TEST

// Test 10: Exactly at threshold (15ms) - should emit cursor
START_TEST(test_at_threshold_emits_cursor)
{
    ik_scroll_accumulator_t *acc = ik_scroll_accumulator_create(ctx);

    // Set baseline
    ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 0);

    // Exactly 15ms later - at threshold, NOT rapid
    // elapsed > KEYBOARD_THRESHOLD means > 15, so 15 is not > 15
    // This is <= 15, so it drains. Let's verify behavior.
    // If elapsed == 15: not > 15, so drains accumulator
    ik_scroll_result_t r = ik_scroll_accumulator_process_arrow(
        acc, IK_INPUT_ARROW_UP, 15);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_NONE);  // acc=10, swallow
}
END_TEST

// Test 11: Just above threshold (16ms) - should emit cursor
START_TEST(test_above_threshold_emits_cursor)
{
    ik_scroll_accumulator_t *acc = ik_scroll_accumulator_create(ctx);

    ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 0);

    // 16ms later - above threshold
    ik_scroll_result_t r = ik_scroll_accumulator_process_arrow(
        acc, IK_INPUT_ARROW_UP, 16);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_ARROW_UP);
}
END_TEST
```

### Green

1. Create `src/scroll_accumulator.h` with types and declarations
2. Create `src/scroll_accumulator.c` with implementation
3. Update Makefile (add new source and test)
4. Run `make check` - new tests pass

### Refactor - Integration

1. Update `src/repl.h`:
   - Change `ik_arrow_burst_detector_t *arrow_detector` to `ik_scroll_accumulator_t *scroll_acc`

2. Update `src/repl_init.c`:
   - Change `ik_arrow_burst_create()` to `ik_scroll_accumulator_create()`

3. Update `src/repl_actions.c` (lines 49-79):
   - Replace burst detector logic with accumulator logic
   - Call `ik_scroll_accumulator_process_arrow()` for arrows
   - Call `ik_scroll_accumulator_process_other()` for other keys

4. Update `src/repl_event_handlers.c`:
   - Remove timeout handling for burst detector (no longer needed)

5. Remove old module:
   - Delete `src/arrow_burst.c` and `src/arrow_burst.h`
   - Delete `tests/unit/arrow_burst/`
   - Update Makefile

6. Update existing tests that reference arrow_detector

### Verify

1. `make check` - all tests pass
2. `make lint` - complexity checks pass
3. `make coverage` - 100% coverage
4. Manual test: mouse wheel scrolls, keyboard arrows move cursor

## Post-conditions
- Working tree is clean (all changes committed)

- `make check` passes
- Old `arrow_burst` module removed
- New `scroll_accumulator` module in place
- REPL integration updated
- No timeout handling needed (simpler event loop)
- Manual verification of mouse scroll behavior

## Notes

The constants can be tuned:
- `ACCUMULATOR_MAX = 15`: Higher = more rapid arrows needed per scroll
- `ACCUMULATOR_DRAIN = 5`: Higher = fewer rapid arrows needed per scroll
- `KEYBOARD_THRESHOLD_MS = 15`: Lower = more sensitive to keyboard vs mouse
- `SCROLL_LINES = 1`: Lines scrolled per scroll event (in repl_actions_viewport.c)

Current tuning: 3 rapid arrows (< 15ms apart each) = 1 scroll line.
