#ifndef IK_ARROW_BURST_H
#define IK_ARROW_BURST_H

#include <inttypes.h>

#include "input.h"

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

#endif // IK_ARROW_BURST_H
