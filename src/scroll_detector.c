// Scroll detector module - Distinguish mouse scroll from keyboard arrows
#include "scroll_detector.h"

#include <assert.h>
#include <talloc.h>

#include "logger.h"
#include "panic.h"

// Create detector (talloc-based)
ik_scroll_detector_t *ik_scroll_detector_create(void *parent)
{
    assert(parent != NULL);  // LCOV_EXCL_BR_LINE

    ik_scroll_detector_t *det = talloc_zero(parent, ik_scroll_detector_t);
    if (det == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    det->pending = false;
    det->pending_dir = IK_INPUT_ARROW_UP;  // Arbitrary initial value
    det->pending_time_ms = 0;
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

    // Debug: log every arrow arrival
    {
        yyjson_mut_doc *doc = ik_log_create();
        yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);
        yyjson_mut_obj_add_str(doc, root, "event", "arrow_arrival");
        yyjson_mut_obj_add_str(doc, root, "dir", arrow_type == IK_INPUT_ARROW_UP ? "UP" : "DOWN");
        yyjson_mut_obj_add_bool(doc, root, "pending", det->pending);
        yyjson_mut_obj_add_int(doc, root, "t_ms", timestamp_ms);
        if (det->pending) {
            yyjson_mut_obj_add_int(doc, root, "elapsed_ms", timestamp_ms - det->pending_time_ms);
        }
        ik_log_debug_json(doc);
    }

    if (!det->pending) {
        // First arrow after idle - buffer it
        det->pending = true;
        det->pending_dir = arrow_type;
        det->pending_time_ms = timestamp_ms;
        return IK_SCROLL_RESULT_NONE;
    }

    // There's a pending arrow - check elapsed time
    int64_t elapsed = timestamp_ms - det->pending_time_ms;

    if (elapsed <= det->burst_threshold_ms) {
        // Rapid follow-up = mouse wheel burst
        ik_scroll_result_t scroll_result = (det->pending_dir == IK_INPUT_ARROW_UP)
            ? IK_SCROLL_RESULT_SCROLL_UP
            : IK_SCROLL_RESULT_SCROLL_DOWN;

        // Buffer current event as new pending
        det->pending_dir = arrow_type;
        det->pending_time_ms = timestamp_ms;

        {
            yyjson_mut_doc *doc = ik_log_create();
            yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);
            yyjson_mut_obj_add_str(doc, root, "event", "scroll_detect");
            yyjson_mut_obj_add_str(doc, root, "type", "MOUSE_WHEEL");
            yyjson_mut_obj_add_str(doc, root, "dir", scroll_result == IK_SCROLL_RESULT_SCROLL_UP ? "UP" : "DOWN");
            yyjson_mut_obj_add_int(doc, root, "t_ms", timestamp_ms);
            yyjson_mut_obj_add_int(doc, root, "elapsed_ms", elapsed);
            ik_log_debug_json(doc);
        }
        return scroll_result;
    }

    // Slow follow-up = keyboard
    ik_scroll_result_t arrow_result = (det->pending_dir == IK_INPUT_ARROW_UP)
        ? IK_SCROLL_RESULT_ARROW_UP
        : IK_SCROLL_RESULT_ARROW_DOWN;

    // Buffer current event as new pending
    det->pending_dir = arrow_type;
    det->pending_time_ms = timestamp_ms;

    {
        yyjson_mut_doc *doc = ik_log_create();
        yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);
        yyjson_mut_obj_add_str(doc, root, "event", "scroll_detect");
        yyjson_mut_obj_add_str(doc, root, "type", "ARROW");
        yyjson_mut_obj_add_str(doc, root, "dir", arrow_result == IK_SCROLL_RESULT_ARROW_UP ? "UP" : "DOWN");
        yyjson_mut_obj_add_int(doc, root, "t_ms", timestamp_ms);
        yyjson_mut_obj_add_int(doc, root, "elapsed_ms", elapsed);
        yyjson_mut_obj_add_str(doc, root, "reason", "slow_followup");
        ik_log_debug_json(doc);
    }
    return arrow_result;
}

// Check if timeout expired and flush pending event
ik_scroll_result_t ik_scroll_detector_check_timeout(
    ik_scroll_detector_t *det,
    int64_t timestamp_ms
)
{
    assert(det != NULL);  // LCOV_EXCL_BR_LINE

    if (!det->pending) {
        return IK_SCROLL_RESULT_NONE;
    }

    int64_t elapsed = timestamp_ms - det->pending_time_ms;
    if (elapsed > det->burst_threshold_ms) {
        // Timeout expired - flush as arrow
        ik_scroll_result_t result = (det->pending_dir == IK_INPUT_ARROW_UP)
            ? IK_SCROLL_RESULT_ARROW_UP
            : IK_SCROLL_RESULT_ARROW_DOWN;

        det->pending = false;
        {
            yyjson_mut_doc *doc = ik_log_create();
            yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);
            yyjson_mut_obj_add_str(doc, root, "event", "scroll_detect");
            yyjson_mut_obj_add_str(doc, root, "type", "ARROW");
            yyjson_mut_obj_add_str(doc, root, "dir", result == IK_SCROLL_RESULT_ARROW_UP ? "UP" : "DOWN");
            yyjson_mut_obj_add_int(doc, root, "t_ms", timestamp_ms);
            yyjson_mut_obj_add_int(doc, root, "elapsed_ms", elapsed);
            yyjson_mut_obj_add_str(doc, root, "reason", "timeout");
            ik_log_debug_json(doc);
        }
        return result;
    }

    return IK_SCROLL_RESULT_NONE;
}

// Get timeout for select()
int64_t ik_scroll_detector_get_timeout_ms(
    ik_scroll_detector_t *det,
    int64_t timestamp_ms
)
{
    assert(det != NULL);  // LCOV_EXCL_BR_LINE

    if (!det->pending) {
        return -1;  // No pending event
    }

    int64_t elapsed = timestamp_ms - det->pending_time_ms;
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

    if (!det->pending) {
        return IK_SCROLL_RESULT_NONE;
    }

    ik_scroll_result_t result = (det->pending_dir == IK_INPUT_ARROW_UP)
        ? IK_SCROLL_RESULT_ARROW_UP
        : IK_SCROLL_RESULT_ARROW_DOWN;

    det->pending = false;
    return result;
}

// Reset to initial state
void ik_scroll_detector_reset(ik_scroll_detector_t *det)
{
    assert(det != NULL);  // LCOV_EXCL_BR_LINE

    det->pending = false;
    det->pending_time_ms = 0;
}
