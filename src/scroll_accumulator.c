// Scroll accumulator module - Distinguish mouse scroll from keyboard arrows
#include "scroll_accumulator.h"

#include <assert.h>
#include <talloc.h>

#include "panic.h"

// Create accumulator (talloc-based)
ik_scroll_accumulator_t *ik_scroll_accumulator_create(void *parent)
{
    assert(parent != NULL);  // LCOV_EXCL_BR_LINE

    ik_scroll_accumulator_t *acc = talloc_zero(parent, ik_scroll_accumulator_t);
    if (acc == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    acc->pending = false;
    acc->pending_dir = IK_INPUT_ARROW_UP;  // Arbitrary initial value
    acc->pending_time_ms = 0;
    acc->burst_threshold_ms = IK_SCROLL_BURST_THRESHOLD_MS;

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

    if (!acc->pending) {
        // First arrow after idle - buffer it
        acc->pending = true;
        acc->pending_dir = arrow_type;
        acc->pending_time_ms = timestamp_ms;
        return IK_SCROLL_RESULT_NONE;
    }

    // There's a pending arrow - check elapsed time
    int64_t elapsed = timestamp_ms - acc->pending_time_ms;

    if (elapsed <= acc->burst_threshold_ms) {
        // Rapid follow-up = mouse wheel burst
        ik_scroll_result_t scroll_result = (acc->pending_dir == IK_INPUT_ARROW_UP)
            ? IK_SCROLL_RESULT_SCROLL_UP
            : IK_SCROLL_RESULT_SCROLL_DOWN;

        // Buffer current event as new pending
        acc->pending_dir = arrow_type;
        acc->pending_time_ms = timestamp_ms;

        return scroll_result;
    }

    // Slow follow-up = keyboard
    ik_scroll_result_t arrow_result = (acc->pending_dir == IK_INPUT_ARROW_UP)
        ? IK_SCROLL_RESULT_ARROW_UP
        : IK_SCROLL_RESULT_ARROW_DOWN;

    // Buffer current event as new pending
    acc->pending_dir = arrow_type;
    acc->pending_time_ms = timestamp_ms;

    return arrow_result;
}

// Check if timeout expired and flush pending event
ik_scroll_result_t ik_scroll_accumulator_check_timeout(
    ik_scroll_accumulator_t *acc,
    int64_t timestamp_ms
)
{
    assert(acc != NULL);  // LCOV_EXCL_BR_LINE

    if (!acc->pending) {
        return IK_SCROLL_RESULT_NONE;
    }

    int64_t elapsed = timestamp_ms - acc->pending_time_ms;
    if (elapsed > acc->burst_threshold_ms) {
        // Timeout expired - flush as arrow
        ik_scroll_result_t result = (acc->pending_dir == IK_INPUT_ARROW_UP)
            ? IK_SCROLL_RESULT_ARROW_UP
            : IK_SCROLL_RESULT_ARROW_DOWN;

        acc->pending = false;
        return result;
    }

    return IK_SCROLL_RESULT_NONE;
}

// Get timeout for select()
int64_t ik_scroll_accumulator_get_timeout_ms(
    ik_scroll_accumulator_t *acc,
    int64_t timestamp_ms
)
{
    assert(acc != NULL);  // LCOV_EXCL_BR_LINE

    if (!acc->pending) {
        return -1;  // No pending event
    }

    int64_t elapsed = timestamp_ms - acc->pending_time_ms;
    int64_t remaining = acc->burst_threshold_ms - elapsed;

    if (remaining < 0) {
        return 0;  // Already expired
    }

    return remaining;
}

// Flush pending event immediately
ik_scroll_result_t ik_scroll_accumulator_flush(ik_scroll_accumulator_t *acc)
{
    assert(acc != NULL);  // LCOV_EXCL_BR_LINE

    if (!acc->pending) {
        return IK_SCROLL_RESULT_NONE;
    }

    ik_scroll_result_t result = (acc->pending_dir == IK_INPUT_ARROW_UP)
        ? IK_SCROLL_RESULT_ARROW_UP
        : IK_SCROLL_RESULT_ARROW_DOWN;

    acc->pending = false;
    return result;
}

// Reset to initial state
void ik_scroll_accumulator_reset(ik_scroll_accumulator_t *acc)
{
    assert(acc != NULL);  // LCOV_EXCL_BR_LINE

    acc->pending = false;
    acc->pending_time_ms = 0;
}
