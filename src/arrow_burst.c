// Arrow burst detector module - Distinguish mouse scroll from keyboard arrows
#include "arrow_burst.h"

#include <assert.h>
#include <talloc.h>

#include "panic.h"
#include "wrapper.h"

// Create detector (talloc-based)
ik_arrow_burst_detector_t *ik_arrow_burst_create(void *parent)
{
    assert(parent != NULL);  // LCOV_EXCL_BR_LINE

    ik_arrow_burst_detector_t *det = talloc_zero_(parent, sizeof(ik_arrow_burst_detector_t));
    if (det == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    det->state = IK_ARROW_BURST_IDLE;
    det->pending_direction = IK_INPUT_UNKNOWN;
    det->first_event_time_ms = 0;
    det->burst_count = 0;

    return det;
}

// Process an arrow event with explicit timestamp
ik_arrow_burst_result_t ik_arrow_burst_process(
    ik_arrow_burst_detector_t *det,
    ik_input_action_type_t arrow_type,
    int64_t timestamp_ms
)
{
    assert(det != NULL);  // LCOV_EXCL_BR_LINE
    assert(arrow_type == IK_INPUT_ARROW_UP || arrow_type == IK_INPUT_ARROW_DOWN);  // LCOV_EXCL_BR_LINE

    if (det->state == IK_ARROW_BURST_IDLE) {
        // Start new buffering
        det->state = IK_ARROW_BURST_BUFFERING;
        det->pending_direction = arrow_type;
        det->first_event_time_ms = timestamp_ms;
        det->burst_count = 1;
        return IK_ARROW_BURST_RESULT_NONE;
    }

    // State is BUFFERING
    int64_t time_since_first = timestamp_ms - det->first_event_time_ms;

    // Direction change?
    if (arrow_type != det->pending_direction) {
        // Emit cursor for previous direction
        ik_arrow_burst_result_t result = (det->pending_direction == IK_INPUT_ARROW_UP)
            ? IK_ARROW_BURST_RESULT_CURSOR_UP
            : IK_ARROW_BURST_RESULT_CURSOR_DOWN;

        // Start new buffering for new direction
        det->pending_direction = arrow_type;
        det->first_event_time_ms = timestamp_ms;
        det->burst_count = 1;

        return result;
    }

    // Same direction - check if within threshold
    if (time_since_first > IK_ARROW_BURST_THRESHOLD_MS) {
        // Outside threshold - emit cursor for previous, start new buffer
        ik_arrow_burst_result_t result = (det->pending_direction == IK_INPUT_ARROW_UP)
            ? IK_ARROW_BURST_RESULT_CURSOR_UP
            : IK_ARROW_BURST_RESULT_CURSOR_DOWN;

        // Start new buffering
        det->pending_direction = arrow_type;
        det->first_event_time_ms = timestamp_ms;
        det->burst_count = 1;

        return result;
    }

    // Within threshold, same direction
    det->burst_count++;

    // Burst detected - emit scroll (burst_count is now >= 2 since we start at 1)
    return (arrow_type == IK_INPUT_ARROW_UP)
        ? IK_ARROW_BURST_RESULT_SCROLL_UP
        : IK_ARROW_BURST_RESULT_SCROLL_DOWN;
}

// Check if timeout has expired
ik_arrow_burst_result_t ik_arrow_burst_check_timeout(
    ik_arrow_burst_detector_t *det,
    int64_t current_time_ms
)
{
    assert(det != NULL);  // LCOV_EXCL_BR_LINE

    if (det->state != IK_ARROW_BURST_BUFFERING) {
        return IK_ARROW_BURST_RESULT_NONE;
    }

    int64_t elapsed = current_time_ms - det->first_event_time_ms;
    if (elapsed > IK_ARROW_BURST_THRESHOLD_MS) {
        // Timeout expired - emit cursor
        ik_arrow_burst_result_t result = (det->pending_direction == IK_INPUT_ARROW_UP)
            ? IK_ARROW_BURST_RESULT_CURSOR_UP
            : IK_ARROW_BURST_RESULT_CURSOR_DOWN;

        // Reset to idle
        det->state = IK_ARROW_BURST_IDLE;
        det->pending_direction = IK_INPUT_UNKNOWN;
        det->first_event_time_ms = 0;
        det->burst_count = 0;

        return result;
    }

    return IK_ARROW_BURST_RESULT_NONE;
}

// Get remaining timeout in ms
int64_t ik_arrow_burst_get_timeout_ms(
    ik_arrow_burst_detector_t *det,
    int64_t current_time_ms
)
{
    assert(det != NULL);  // LCOV_EXCL_BR_LINE

    if (det->state != IK_ARROW_BURST_BUFFERING) {
        return -1;
    }

    int64_t elapsed = current_time_ms - det->first_event_time_ms;
    int64_t remaining = IK_ARROW_BURST_THRESHOLD_MS - elapsed;

    if (remaining < 0) {
        return 0;
    }

    return remaining;
}

// Reset detector to idle state
void ik_arrow_burst_reset(ik_arrow_burst_detector_t *det)
{
    assert(det != NULL);  // LCOV_EXCL_BR_LINE

    det->state = IK_ARROW_BURST_IDLE;
    det->pending_direction = IK_INPUT_UNKNOWN;
    det->first_event_time_ms = 0;
    det->burst_count = 0;
}
