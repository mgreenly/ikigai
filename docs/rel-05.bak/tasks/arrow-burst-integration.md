# Task: Integrate Arrow Burst Detector into REPL Event Loop

## Target

Bug Fix: Wire arrow burst detector into REPL so mouse wheel scrolls buffer while arrow keys move cursor.

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/naming.md
- .agents/skills/style.md
- .agents/skills/errors.md

## Pre-read Docs
- docs/memory.md (talloc patterns)
- docs/error_handling.md (res_t patterns)

## Pre-read Source (patterns)
- src/arrow_burst.h (the detector API - created in previous task)
- src/arrow_burst.c (the detector implementation)
- src/repl.h (ik_repl_ctx_t structure)
- src/repl.c (REPL initialization)
- src/repl_event_handlers.h (event loop functions)
- src/repl_event_handlers.c (handle_terminal_input, calculate_select_timeout_ms)
- src/repl_actions.c (ik_repl_process_action - action dispatch)
- src/repl_actions_history.c (current arrow up/down handlers)
- src/repl_actions_viewport.c (scroll handlers)
- src/input_buffer/core.h (cursor movement functions)
- tests/unit/repl/repl_actions_test.c (action testing patterns)

## Pre-conditions

- Working tree is clean (`git status --porcelain` returns empty)
- `make check` passes
- Arrow burst detector module exists (`src/arrow_burst.c`, `src/arrow_burst.h`)
- Arrow burst detector has passing unit tests

## Background

The arrow burst detector distinguishes rapid arrow bursts (mouse wheel) from single arrow presses (keyboard). This task integrates it into the REPL event loop so:

- **Rapid arrow bursts (mouse wheel)** → scroll the viewport
- **Single arrow presses (keyboard)** → move cursor in input buffer

## Task

1. Add arrow burst detector to REPL context
2. Modify event loop timeout calculation to include burst timeout
3. Intercept arrow events before normal processing
4. Route burst results to scroll vs cursor actions
5. Add Ctrl+P/N for explicit history navigation

## Implementation Details

### 1. Add Detector to REPL Context

In `src/repl.h`:
```c
#include "arrow_burst.h"

struct ik_repl_ctx_t {
    // ... existing fields ...
    ik_arrow_burst_detector_t *arrow_detector;
};
```

In `src/repl.c` (ik_repl_init or similar):
```c
repl->arrow_detector = ik_arrow_burst_create(repl);
```

### 2. Modify Timeout Calculation

In `src/repl_event_handlers.c`:
```c
long calculate_select_timeout_ms(ik_repl_ctx_t *repl, long curl_timeout_ms)
{
    // ... existing timeout logic ...

    // Add arrow burst timeout
    int64_t now_ms = get_monotonic_time_ms();  // Need helper function
    int64_t burst_timeout = ik_arrow_burst_get_timeout_ms(repl->arrow_detector, now_ms);
    if (burst_timeout >= 0) {
        if (min_timeout < 0 || burst_timeout < min_timeout) {
            min_timeout = burst_timeout;
        }
    }

    return min_timeout;
}
```

Add helper for monotonic time:
```c
// In src/repl_event_handlers.c or a utility header
static int64_t get_monotonic_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}
```

### 3. Intercept Arrow Events

Modify `handle_terminal_input()` or `ik_repl_process_action()`:

```c
// Before normal action processing, intercept arrow up/down
if (action->type == IK_INPUT_ARROW_UP || action->type == IK_INPUT_ARROW_DOWN) {
    int64_t now_ms = get_monotonic_time_ms();
    ik_arrow_burst_result_t result = ik_arrow_burst_process(
        repl->arrow_detector, action->type, now_ms);

    switch (result) {
        case IK_ARROW_BURST_RESULT_SCROLL_UP:
            return ik_repl_handle_scroll_up_action(repl);
        case IK_ARROW_BURST_RESULT_SCROLL_DOWN:
            return ik_repl_handle_scroll_down_action(repl);
        case IK_ARROW_BURST_RESULT_CURSOR_UP:
            return ik_input_buffer_cursor_up(repl->input_buffer);
        case IK_ARROW_BURST_RESULT_CURSOR_DOWN:
            return ik_input_buffer_cursor_down(repl->input_buffer);
        case IK_ARROW_BURST_RESULT_NONE:
            // Still buffering, don't process yet
            return OK(NULL);
    }
}
```

### 4. Handle Burst Timeout in Event Loop

In the main event loop (after select returns due to timeout):
```c
// Check if arrow burst timeout expired
int64_t now_ms = get_monotonic_time_ms();
ik_arrow_burst_result_t burst_result = ik_arrow_burst_check_timeout(
    repl->arrow_detector, now_ms);

if (burst_result != IK_ARROW_BURST_RESULT_NONE) {
    switch (burst_result) {
        case IK_ARROW_BURST_RESULT_CURSOR_UP:
            ik_input_buffer_cursor_up(repl->input_buffer);
            ik_repl_render_frame(repl);
            break;
        case IK_ARROW_BURST_RESULT_CURSOR_DOWN:
            ik_input_buffer_cursor_down(repl->input_buffer);
            ik_repl_render_frame(repl);
            break;
        // SCROLL cases won't happen on timeout (only on rapid events)
        default:
            break;
    }
}
```

### 5. Add Ctrl+P/N for History

In `src/input.h`, add:
```c
IK_INPUT_CTRL_P,      // Ctrl+P (history previous)
IK_INPUT_CTRL_N,      // Ctrl+N (history next)
```

In `src/input.c`, add parsing:
```c
if (byte == 0x10) {  // Ctrl+P
    action_out->type = IK_INPUT_CTRL_P;
    return;
}
if (byte == 0x0E) {  // Ctrl+N
    action_out->type = IK_INPUT_CTRL_N;
    return;
}
```

In `src/repl_actions.c`, add cases:
```c
case IK_INPUT_CTRL_P:
    return ik_repl_handle_history_prev_action(repl);
case IK_INPUT_CTRL_N:
    return ik_repl_handle_history_next_action(repl);
```

Create new handlers in `src/repl_actions_history.c`:
- `ik_repl_handle_history_prev_action()` - extract history navigation from arrow_up
- `ik_repl_handle_history_next_action()` - extract history navigation from arrow_down

### 6. Modify Existing Arrow Handlers

The existing `ik_repl_handle_arrow_up_action()` and `ik_repl_handle_arrow_down_action()` should be simplified:
- Remove history navigation logic (moved to Ctrl+P/N)
- Arrow up/down now ONLY handle cursor movement and completion navigation

## TDD Cycle

### Red

Create/modify tests in `tests/unit/repl/`:

```c
// Test 1: Rapid arrows scroll viewport
START_TEST(test_rapid_arrows_scroll_viewport)
{
    // Setup REPL with scrollback content
    // Inject two rapid arrow events (fake timestamps via test helper)
    // Verify viewport_offset changed (scrolled)
    // Verify input buffer cursor unchanged
}
END_TEST

// Test 2: Single arrow moves cursor
START_TEST(test_single_arrow_moves_cursor)
{
    // Setup REPL with multi-line input
    // Inject single arrow event
    // Trigger timeout check
    // Verify cursor position changed
    // Verify viewport_offset unchanged
}
END_TEST

// Test 3: Ctrl+P navigates history
START_TEST(test_ctrl_p_history_prev)
{
    // Setup REPL with history entries
    // Process Ctrl+P action
    // Verify input buffer contains previous history entry
}
END_TEST

// Test 4: Ctrl+N navigates history
START_TEST(test_ctrl_n_history_next)
{
    // Setup REPL browsing history
    // Process Ctrl+N action
    // Verify input buffer contains next history entry
}
END_TEST

// Test 5: Ctrl+P/N parsing
START_TEST(test_ctrl_p_parsing)
{
    ik_input_parser_t *parser = ik_input_parser_create(ctx);
    ik_input_action_t action;

    ik_input_parse_byte(parser, 0x10, &action);  // Ctrl+P
    ck_assert_int_eq(action.type, IK_INPUT_CTRL_P);
}
END_TEST

START_TEST(test_ctrl_n_parsing)
{
    ik_input_parser_t *parser = ik_input_parser_create(ctx);
    ik_input_action_t action;

    ik_input_parse_byte(parser, 0x0E, &action);  // Ctrl+N
    ck_assert_int_eq(action.type, IK_INPUT_CTRL_N);
}
END_TEST

// Test 6: Timeout calculation includes burst timeout
START_TEST(test_timeout_includes_burst)
{
    // Setup REPL
    // Process arrow event (starts buffering)
    // Call calculate_select_timeout_ms
    // Verify timeout is <= 15ms (burst threshold)
}
END_TEST
```

**Testing challenge:** The integration tests need to inject fake timestamps. Options:
1. Create test helper that sets time on detector directly
2. Use a time provider interface (mockable)
3. Test at action level, mock the detector

Recommended: Create test helpers that directly manipulate detector state and verify outcomes.

Run `make check` - expect new tests to FAIL.

### Green

1. Add `arrow_detector` to `ik_repl_ctx_t` and initialize in REPL init
2. Add `get_monotonic_time_ms()` helper (or use wrapper pattern)
3. Modify `calculate_select_timeout_ms()` to include burst timeout
4. Intercept arrow events in action processing
5. Handle burst timeout in event loop
6. Add Ctrl+P/N parsing to input.c
7. Add Ctrl+P/N action handlers
8. Extract history logic from arrow handlers into Ctrl+P/N handlers
9. Simplify arrow up/down handlers (cursor only, no history)

Run `make check` - all tests should pass.

### Verify

1. Run `make check` - all tests pass
2. Run `make lint` - complexity checks pass
3. Run `make coverage` - maintain 100% coverage
4. Manual test:
   - Run `bin/ikigai`
   - Submit messages to create scrollback and history
   - Type multi-line input (use Ctrl+J for newlines)
   - **Mouse wheel up** → scrollback scrolls up
   - **Mouse wheel down** → scrollback scrolls down
   - **Arrow up** → cursor moves up in multi-line input
   - **Arrow down** → cursor moves down in multi-line input
   - **Ctrl+P** → shows previous history entry
   - **Ctrl+N** → shows next history entry
   - **Right-click** → terminal context menu appears
   - **Left-drag** → text selection works

## Post-conditions

- `make check` passes
- Arrow burst detector integrated into REPL event loop
- Mouse wheel (rapid arrows) scrolls viewport
- Keyboard arrows move cursor in input buffer
- Ctrl+P/N navigate history
- Terminal right-click and selection still work
- 100% test coverage maintained
- Working tree is clean (all changes committed)

## Notes

The key insight is that we're NOT changing terminal modes. The terminal still converts mouse wheel to arrows. We're detecting the difference based on timing patterns.

The 15ms threshold may need tuning based on real-world testing. It's defined as a constant for easy adjustment.

For testing, prefer testing the state transitions and outcomes rather than actual timing. Use the arrow_burst detector's injectable timestamp API.
