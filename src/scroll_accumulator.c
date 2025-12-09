// Scroll accumulator module - Distinguish mouse scroll from keyboard arrows
#include "scroll_accumulator.h"

#include <assert.h>
#include <talloc.h>

#include "panic.h"
#include "wrapper.h"

// Helper to get minimum of two values
static inline int64_t min_i64(int64_t a, int64_t b)
{
    return (a < b) ? a : b;
}

// Create accumulator (talloc-based)
ik_scroll_accumulator_t *ik_scroll_accumulator_create(void *parent)
{
    assert(parent != NULL);  // LCOV_EXCL_BR_LINE

    ik_scroll_accumulator_t *acc = talloc_zero_(parent, sizeof(ik_scroll_accumulator_t));
    if (acc == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    acc->previous_time_ms = 0;
    acc->accumulator = IK_SCROLL_ACCUMULATOR_MAX;

    return acc;
}

// Process an arrow event with explicit timestamp
ik_scroll_result_t ik_scroll_accumulator_process_arrow(
    ik_scroll_accumulator_t *acc,
    ik_input_action_type_t arrow_type,
    int64_t timestamp_ms
)
{
    assert(acc != NULL);  // LCOV_EXCL_BR_LINE
    assert(arrow_type == IK_INPUT_ARROW_UP || arrow_type == IK_INPUT_ARROW_DOWN);  // LCOV_EXCL_BR_LINE

    // Calculate elapsed time since last event
    int64_t elapsed = timestamp_ms - acc->previous_time_ms;
    acc->previous_time_ms = timestamp_ms;

    // Check if this is a slow arrow (keyboard)
    if (elapsed > IK_SCROLL_KEYBOARD_THRESHOLD_MS) {
        // Emit cursor movement immediately
        return (arrow_type == IK_INPUT_ARROW_UP)
            ? IK_SCROLL_RESULT_ARROW_UP
            : IK_SCROLL_RESULT_ARROW_DOWN;
    }

    // Fast arrow - drain accumulator
    acc->accumulator -= IK_SCROLL_ACCUMULATOR_DRAIN;

    // Check if accumulator depleted
    if (acc->accumulator < 1) {
        // Reset accumulator
        acc->accumulator = IK_SCROLL_ACCUMULATOR_MAX;

        // Emit scroll
        return (arrow_type == IK_INPUT_ARROW_UP)
            ? IK_SCROLL_RESULT_SCROLL_UP
            : IK_SCROLL_RESULT_SCROLL_DOWN;
    }

    // Still accumulating, swallow this event
    return IK_SCROLL_RESULT_NONE;
}

// Process a non-arrow event (refills accumulator)
void ik_scroll_accumulator_process_other(
    ik_scroll_accumulator_t *acc,
    int64_t timestamp_ms
)
{
    assert(acc != NULL);  // LCOV_EXCL_BR_LINE

    // Calculate elapsed time since last event
    int64_t elapsed = timestamp_ms - acc->previous_time_ms;
    acc->previous_time_ms = timestamp_ms;

    // Refill accumulator (cap at max)
    acc->accumulator = min_i64(IK_SCROLL_ACCUMULATOR_MAX, acc->accumulator + elapsed);
}

// Reset to initial state
void ik_scroll_accumulator_reset(ik_scroll_accumulator_t *acc)
{
    assert(acc != NULL);  // LCOV_EXCL_BR_LINE

    acc->previous_time_ms = 0;
    acc->accumulator = IK_SCROLL_ACCUMULATOR_MAX;
}
