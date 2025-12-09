# Task: Deferred Arrow Event Detection for Mouse Scroll

## Target

Bug Fix: Replace timing-based scroll detection with deferred event detection to correctly distinguish mouse wheel from keyboard arrows.

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/errors.md

## Pre-read Docs
- docs/mouse-scroll-burst.md (background on scroll detection)
- docs/memory.md (talloc patterns)

## Pre-read Source
- src/scroll_accumulator.c (current implementation)
- src/scroll_accumulator.h (current API)
- src/repl_actions.c (integration point, lines 49-112)
- src/repl_event_handlers.c (event loop, select timeout calculation)

## Pre-conditions
- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes

## Background

### The Problem

The current scroll accumulator algorithm fails because:

1. **Terminal sends escape sequences byte-by-byte**: Arrow up = ESC `[` `A` (3 bytes)
2. **First complete arrow always has huge elapsed time**: The first arrow event after idle sees `elapsed = now - 0` (billions of ms), triggering PASS (keyboard)
3. **Mouse wheel sends 2 arrow events per notch** (in this terminal), but the first is misclassified

### Root Cause Discovery (via logging)

Testing revealed:
- Mouse wheel notch sends 2 `ARROW_UP` events ~0-1ms apart
- First event always shows `elapsed_ms` in billions (from epoch 0)
- Algorithm returns PASS for first event, NONE for second
- No SCROLL is ever emitted

### The Core Insight

We cannot distinguish mouse wheel from keyboard on the **first** event. We must **wait** to see if a second rapid event follows:
- Second event within ~10ms → mouse wheel → emit SCROLL
- No second event (timeout) → keyboard → emit ARROW

## Algorithm: Deferred Detection

```
State:
  pending_arrow: bool (is an arrow event buffered?)
  pending_direction: UP or DOWN
  pending_time_ms: int64_t (when the pending event arrived)

On arrow event:
  now = current_time_ms

  if pending_arrow:
    elapsed = now - pending_time_ms
    if elapsed <= 10ms:
      // Rapid follow-up = mouse wheel burst
      emit SCROLL(pending_direction)
      pending_arrow = false
      // Note: current event starts a new potential burst
      pending_arrow = true
      pending_direction = current_direction
      pending_time_ms = now
    else:
      // Slow follow-up = keyboard
      emit ARROW(pending_direction)
      // Current event becomes new pending
      pending_direction = current_direction
      pending_time_ms = now
  else:
    // First arrow after idle - buffer it
    pending_arrow = true
    pending_direction = current_direction
    pending_time_ms = now

On timeout (select returns with no input):
  if pending_arrow and (now - pending_time_ms > 10ms):
    emit ARROW(pending_direction)
    pending_arrow = false

On non-arrow event:
  if pending_arrow:
    emit ARROW(pending_direction)
    pending_arrow = false
  // Then process the non-arrow event normally
```

### Behavior Traces

**Mouse wheel (2 events, 1ms apart):**
```
Arrow 1 (t=0):     pending=false → buffer it, pending=true
Arrow 2 (t=1):     pending=true, elapsed=1ms ≤ 10ms → emit SCROLL, buffer Arrow 2
[no more events]
Timeout (t=15):    pending=true, elapsed=14ms > 10ms → emit ARROW
```
Result: 1 SCROLL emitted (correct for mouse wheel)

**Single keyboard press:**
```
Arrow 1 (t=0):     pending=false → buffer it, pending=true
[no more events]
Timeout (t=15):    pending=true, elapsed=15ms > 10ms → emit ARROW
```
Result: 1 ARROW emitted after 10-15ms delay (correct for keyboard)

**Keyboard hold (30ms repeat rate):**
```
Arrow 1 (t=0):     pending=false → buffer it
Arrow 2 (t=30):    pending=true, elapsed=30ms > 10ms → emit ARROW, buffer Arrow 2
Arrow 3 (t=60):    pending=true, elapsed=30ms > 10ms → emit ARROW, buffer Arrow 3
Timeout (t=75):    emit ARROW
```
Result: ARROWs emitted with ~30ms delay each (correct for key repeat)

**Rapid mouse scrolling (continuous):**
```
Arrow 1 (t=0):     buffer
Arrow 2 (t=1):     elapsed=1ms → SCROLL, buffer
Arrow 3 (t=50):    elapsed=49ms → ARROW (new notch started), buffer
Arrow 4 (t=51):    elapsed=1ms → SCROLL, buffer
...
```
Result: 1 SCROLL per wheel notch

## Data Structures

Update `ik_scroll_accumulator_t`:

```c
typedef struct {
    // Deferred detection state
    bool pending;                      // Is an arrow event buffered?
    ik_input_action_type_t pending_dir; // ARROW_UP or ARROW_DOWN
    int64_t pending_time_ms;           // When pending event arrived

    // Constants (could be #defines, but struct allows testing)
    int64_t burst_threshold_ms;        // Max time between events to be a burst (10ms)
} ik_scroll_accumulator_t;

// Result of processing an arrow event
typedef enum {
    IK_SCROLL_RESULT_NONE,        // Event buffered, waiting for more
    IK_SCROLL_RESULT_SCROLL_UP,   // Emit scroll up (mouse wheel detected)
    IK_SCROLL_RESULT_SCROLL_DOWN, // Emit scroll down (mouse wheel detected)
    IK_SCROLL_RESULT_ARROW_UP,    // Emit arrow up (keyboard detected)
    IK_SCROLL_RESULT_ARROW_DOWN,  // Emit arrow down (keyboard detected)
} ik_scroll_result_t;

#define IK_SCROLL_BURST_THRESHOLD_MS 10
```

## API

```c
// Create accumulator (talloc-based)
ik_scroll_accumulator_t *ik_scroll_accumulator_create(void *parent);

// Process an arrow event
// May return NONE (buffered), SCROLL_*, or ARROW_*
ik_scroll_result_t ik_scroll_accumulator_process_arrow(
    ik_scroll_accumulator_t *acc,
    ik_input_action_type_t arrow_type,  // ARROW_UP or ARROW_DOWN
    int64_t timestamp_ms
);

// Check if timeout expired and flush pending event
// Called from event loop when select() times out
// Returns ARROW_* if pending event flushed, NONE otherwise
ik_scroll_result_t ik_scroll_accumulator_check_timeout(
    ik_scroll_accumulator_t *acc,
    int64_t timestamp_ms
);

// Get timeout for select() (returns -1 if no pending, else ms until flush)
int64_t ik_scroll_accumulator_get_timeout_ms(
    ik_scroll_accumulator_t *acc,
    int64_t timestamp_ms
);

// Flush pending event immediately (for non-arrow input)
// Returns ARROW_* if pending event flushed, NONE otherwise
ik_scroll_result_t ik_scroll_accumulator_flush(ik_scroll_accumulator_t *acc);

// Reset to initial state
void ik_scroll_accumulator_reset(ik_scroll_accumulator_t *acc);
```

## TDD Cycle

### Red

Create/update `tests/unit/scroll_accumulator/scroll_accumulator_test.c`:

```c
// Test 1: First arrow is buffered (returns NONE)
START_TEST(test_first_arrow_buffered)
{
    ik_scroll_accumulator_t *acc = ik_scroll_accumulator_create(ctx);

    ik_scroll_result_t r = ik_scroll_accumulator_process_arrow(
        acc, IK_INPUT_ARROW_UP, 1000);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_NONE);
}
END_TEST

// Test 2: Rapid second arrow emits SCROLL
START_TEST(test_rapid_second_arrow_emits_scroll)
{
    ik_scroll_accumulator_t *acc = ik_scroll_accumulator_create(ctx);

    ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 1000);
    ik_scroll_result_t r = ik_scroll_accumulator_process_arrow(
        acc, IK_INPUT_ARROW_UP, 1001);  // 1ms later
    ck_assert_int_eq(r, IK_SCROLL_RESULT_SCROLL_UP);
}
END_TEST

// Test 3: Slow second arrow emits ARROW for first, buffers second
START_TEST(test_slow_second_arrow_emits_arrow)
{
    ik_scroll_accumulator_t *acc = ik_scroll_accumulator_create(ctx);

    ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 1000);
    ik_scroll_result_t r = ik_scroll_accumulator_process_arrow(
        acc, IK_INPUT_ARROW_UP, 1030);  // 30ms later
    ck_assert_int_eq(r, IK_SCROLL_RESULT_ARROW_UP);
}
END_TEST

// Test 4: Timeout flushes pending as ARROW
START_TEST(test_timeout_flushes_arrow)
{
    ik_scroll_accumulator_t *acc = ik_scroll_accumulator_create(ctx);

    ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 1000);
    ik_scroll_result_t r = ik_scroll_accumulator_check_timeout(acc, 1015);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_ARROW_UP);
}
END_TEST

// Test 5: Timeout before threshold returns NONE
START_TEST(test_timeout_before_threshold_returns_none)
{
    ik_scroll_accumulator_t *acc = ik_scroll_accumulator_create(ctx);

    ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 1000);
    ik_scroll_result_t r = ik_scroll_accumulator_check_timeout(acc, 1005);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_NONE);
}
END_TEST

// Test 6: get_timeout_ms returns correct value
START_TEST(test_get_timeout_ms)
{
    ik_scroll_accumulator_t *acc = ik_scroll_accumulator_create(ctx);

    // No pending - returns -1
    int64_t t = ik_scroll_accumulator_get_timeout_ms(acc, 1000);
    ck_assert_int_eq(t, -1);

    // With pending at t=1000, check at t=1003 - should return 7ms
    ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 1000);
    t = ik_scroll_accumulator_get_timeout_ms(acc, 1003);
    ck_assert_int_eq(t, 7);

    // At t=1015 - already expired, return 0
    t = ik_scroll_accumulator_get_timeout_ms(acc, 1015);
    ck_assert_int_eq(t, 0);
}
END_TEST

// Test 7: flush() emits pending ARROW
START_TEST(test_flush_emits_arrow)
{
    ik_scroll_accumulator_t *acc = ik_scroll_accumulator_create(ctx);

    ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_DOWN, 1000);
    ik_scroll_result_t r = ik_scroll_accumulator_flush(acc);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_ARROW_DOWN);

    // Second flush returns NONE
    r = ik_scroll_accumulator_flush(acc);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_NONE);
}
END_TEST

// Test 8: Scroll direction preserved
START_TEST(test_scroll_direction)
{
    ik_scroll_accumulator_t *acc = ik_scroll_accumulator_create(ctx);

    ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_DOWN, 1000);
    ik_scroll_result_t r = ik_scroll_accumulator_process_arrow(
        acc, IK_INPUT_ARROW_DOWN, 1001);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_SCROLL_DOWN);
}
END_TEST

// Test 9: Mixed directions - each burst independent
START_TEST(test_mixed_directions)
{
    ik_scroll_accumulator_t *acc = ik_scroll_accumulator_create(ctx);

    // Up burst
    ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 1000);
    ik_scroll_result_t r = ik_scroll_accumulator_process_arrow(
        acc, IK_INPUT_ARROW_UP, 1001);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_SCROLL_UP);

    // Wait, then down burst
    ik_scroll_accumulator_check_timeout(acc, 1050);  // Flush any pending
    ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_DOWN, 1100);
    r = ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_DOWN, 1101);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_SCROLL_DOWN);
}
END_TEST

// Test 10: Reset clears pending
START_TEST(test_reset_clears_pending)
{
    ik_scroll_accumulator_t *acc = ik_scroll_accumulator_create(ctx);

    ik_scroll_accumulator_process_arrow(acc, IK_INPUT_ARROW_UP, 1000);
    ik_scroll_accumulator_reset(acc);

    // Timeout should return NONE (nothing pending)
    ik_scroll_result_t r = ik_scroll_accumulator_check_timeout(acc, 1020);
    ck_assert_int_eq(r, IK_SCROLL_RESULT_NONE);
}
END_TEST
```

### Green

1. Update `src/scroll_accumulator.h` with new struct and API
2. Rewrite `src/scroll_accumulator.c` with deferred detection logic
3. Run `make check` - new tests pass

### Refactor - Integration

1. Update `src/repl_event_handlers.c`:
   - In `calculate_select_timeout_ms()`, add scroll accumulator timeout:
     ```c
     int64_t scroll_timeout_ms = ik_scroll_accumulator_get_timeout_ms(
         repl->scroll_acc, now_ms);
     ```
   - After `select()` returns, check scroll timeout:
     ```c
     ik_scroll_result_t scroll_result = ik_scroll_accumulator_check_timeout(
         repl->scroll_acc, now_ms);
     if (scroll_result != IK_SCROLL_RESULT_NONE) {
         handle_scroll_result(repl, scroll_result);
     }
     ```

2. Update `src/repl_actions.c`:
   - In scroll accumulator handling, process the result
   - For non-arrow events, call `ik_scroll_accumulator_flush()` first

3. Create helper to handle scroll results:
   ```c
   static res_t handle_scroll_result(ik_repl_ctx_t *repl, ik_scroll_result_t result)
   {
       switch (result) {
           case IK_SCROLL_RESULT_SCROLL_UP:
               return ik_repl_handle_scroll_up_action(repl);
           case IK_SCROLL_RESULT_SCROLL_DOWN:
               return ik_repl_handle_scroll_down_action(repl);
           case IK_SCROLL_RESULT_ARROW_UP:
               return ik_repl_handle_arrow_up_action(repl);
           case IK_SCROLL_RESULT_ARROW_DOWN:
               return ik_repl_handle_arrow_down_action(repl);
           default:
               return OK(NULL);
       }
   }
   ```

### Verify

1. `make check` - all tests pass
2. `make lint` - complexity checks pass
3. Manual test:
   - Mouse wheel scrolls viewport (no cursor movement)
   - Keyboard arrows move cursor (with ~10ms latency)
   - Key repeat works smoothly

## Post-conditions
- Working tree is clean (all changes committed)

- `make check` passes
- Mouse wheel correctly emits SCROLL events
- Keyboard arrows correctly emit ARROW events (after brief delay)
- No spurious cursor movements on mouse wheel
- Event loop integrates scroll timeout with existing timeouts

## Notes

### Latency Trade-off

Keyboard arrow presses will have ~10ms latency before taking effect. This is imperceptible to humans but necessary for correct detection.

### Tuning

The `IK_SCROLL_BURST_THRESHOLD_MS` constant (10ms) can be tuned:
- Lower (5ms): Faster keyboard response, may miss slow mouse wheels
- Higher (20ms): More reliable mouse detection, noticeable keyboard latency

### Future Enhancement

If latency becomes an issue, could add heuristic: if cursor is at document edge (can't move further), emit SCROLL immediately without waiting.

## Scope Limitation

This task only fixes event emission (SCROLL vs ARROW). Any bugs in how those events are handled by `ik_repl_handle_scroll_up_action()` etc. are out of scope and should be addressed in separate tasks.
