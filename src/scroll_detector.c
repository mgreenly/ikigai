// Scroll detector module - Distinguish mouse scroll from keyboard arrows
#include "scroll_detector.h"

#include <assert.h>
#include <talloc.h>

#include "panic.h"


#include "poison.h"
// Create detector (talloc-based)
ik_scroll_detector_t *ik_scroll_detector_create(void *parent)
{
    assert(parent != NULL);  // LCOV_EXCL_BR_LINE

    ik_scroll_detector_t *det = talloc_zero(parent, ik_scroll_detector_t);
    if (det == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    det->state = IK_SCROLL_STATE_IDLE;
    det->pending_dir = IK_INPUT_ARROW_UP;  // Arbitrary initial value
    det->timer_start_ms = 0;
    det->burst_threshold_ms = IK_SCROLL_BURST_THRESHOLD_MS;

    return det;
}

// Process an arrow event with explicit timestamp
ik_scroll_result_t ik_scroll_detector_process_arrow(
    ik_scroll_detector_t *det,
    ik_input_action_type_t arrow_type,
    int64_t timestamp_ms
    )
{
    assert(det != NULL);  // LCOV_EXCL_BR_LINE
    assert(arrow_type == IK_INPUT_ARROW_UP || arrow_type == IK_INPUT_ARROW_DOWN);  // LCOV_EXCL_BR_LINE

    int64_t elapsed = timestamp_ms - det->timer_start_ms;

    switch (det->state) {  // LCOV_EXCL_BR_LINE - All states covered
        case IK_SCROLL_STATE_IDLE:
            // First arrow - start timer, transition to WAITING
            det->state = IK_SCROLL_STATE_WAITING;
            det->pending_dir = arrow_type;
            det->timer_start_ms = timestamp_ms;
            return IK_SCROLL_RESULT_NONE;

        case IK_SCROLL_STATE_WAITING:
            if (elapsed <= det->burst_threshold_ms) {
                // Second arrow within timer - it's a mouse wheel burst!
                ik_scroll_result_t result = (det->pending_dir == IK_INPUT_ARROW_UP)
                    ? IK_SCROLL_RESULT_SCROLL_UP
                    : IK_SCROLL_RESULT_SCROLL_DOWN;

                det->state = IK_SCROLL_STATE_ABSORBING;
                det->timer_start_ms = timestamp_ms;
                return result;
            }
            // Timer expired while waiting - treat pending as keyboard arrow
            {
                // LCOV_EXCL_BR_START - Defensive: both UP and DOWN tested in other branches
                ik_scroll_result_t result = (det->pending_dir == IK_INPUT_ARROW_UP)
                    ? IK_SCROLL_RESULT_ARROW_UP
                    : IK_SCROLL_RESULT_ARROW_DOWN;
                // LCOV_EXCL_BR_STOP

                det->pending_dir = arrow_type;
                det->timer_start_ms = timestamp_ms;
                return result;
            }

        case IK_SCROLL_STATE_ABSORBING:
            if (elapsed <= det->burst_threshold_ms) {
                // Additional arrow within timer - absorb it
                det->timer_start_ms = timestamp_ms;
                return IK_SCROLL_RESULT_ABSORBED;
            }
            // Timer expired while absorbing - new burst starting
            det->state = IK_SCROLL_STATE_WAITING;
            det->pending_dir = arrow_type;
            det->timer_start_ms = timestamp_ms;
            return IK_SCROLL_RESULT_NONE;
    }

    return IK_SCROLL_RESULT_NONE;  // LCOV_EXCL_LINE
}

// Check if timeout expired and flush pending event
ik_scroll_result_t ik_scroll_detector_check_timeout(
    ik_scroll_detector_t *det,
    int64_t timestamp_ms
    )
{
    assert(det != NULL);  // LCOV_EXCL_BR_LINE

    if (det->state == IK_SCROLL_STATE_IDLE) {
        return IK_SCROLL_RESULT_NONE;
    }

    int64_t elapsed = timestamp_ms - det->timer_start_ms;

    if (elapsed <= det->burst_threshold_ms) {
        return IK_SCROLL_RESULT_NONE;
    }

    // Timer expired
    if (det->state == IK_SCROLL_STATE_WAITING) {
        // Only got one arrow - it's a keyboard arrow
        // LCOV_EXCL_BR_START - Defensive: both UP and DOWN tested in other branches
        ik_scroll_result_t result = (det->pending_dir == IK_INPUT_ARROW_UP)
            ? IK_SCROLL_RESULT_ARROW_UP
            : IK_SCROLL_RESULT_ARROW_DOWN;
        // LCOV_EXCL_BR_STOP

        det->state = IK_SCROLL_STATE_IDLE;
        return result;
    }

    // ABSORBING state - timer expired, go back to IDLE
    det->state = IK_SCROLL_STATE_IDLE;
    return IK_SCROLL_RESULT_NONE;
}

// Get timeout for select()
int64_t ik_scroll_detector_get_timeout_ms(
    ik_scroll_detector_t *det,
    int64_t timestamp_ms
    )
{
    assert(det != NULL);  // LCOV_EXCL_BR_LINE

    if (det->state == IK_SCROLL_STATE_IDLE) {
        return -1;  // No timeout needed
    }

    int64_t elapsed = timestamp_ms - det->timer_start_ms;
    int64_t remaining = det->burst_threshold_ms - elapsed;

    if (remaining < 0) {
        return 0;  // Already expired
    }

    return remaining;
}

// Flush pending event immediately
ik_scroll_result_t ik_scroll_detector_flush(ik_scroll_detector_t *det)
{
    assert(det != NULL);  // LCOV_EXCL_BR_LINE

    if (det->state == IK_SCROLL_STATE_WAITING) {
        // Flush the pending arrow
        // LCOV_EXCL_BR_START - Defensive: both UP and DOWN tested in other branches
        ik_scroll_result_t result = (det->pending_dir == IK_INPUT_ARROW_UP)
            ? IK_SCROLL_RESULT_ARROW_UP
            : IK_SCROLL_RESULT_ARROW_DOWN;
        // LCOV_EXCL_BR_STOP

        det->state = IK_SCROLL_STATE_IDLE;
        return result;
    }

    // IDLE or ABSORBING - nothing to flush (ABSORBING has already emitted WHEEL)
    det->state = IK_SCROLL_STATE_IDLE;
    return IK_SCROLL_RESULT_NONE;
}

