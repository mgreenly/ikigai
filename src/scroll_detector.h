#ifndef IK_SCROLL_DETECTOR_H
#define IK_SCROLL_DETECTOR_H

#include <inttypes.h>
#include <stdbool.h>

#include "input.h"

// State machine for scroll detection
typedef enum {
    IK_SCROLL_STATE_IDLE,      // No pending event
    IK_SCROLL_STATE_WAITING,   // First arrow received, waiting to classify
    IK_SCROLL_STATE_ABSORBING, // Burst detected, absorbing remaining arrows
} ik_scroll_state_t;

// Scroll detector for mouse wheel detection using state machine
typedef struct {
    ik_scroll_state_t state;           // Current state
    ik_input_action_type_t pending_dir; // ARROW_UP or ARROW_DOWN (valid in WAITING/ABSORBING)
    int64_t timer_start_ms;            // When timer started

    // Constants
    int64_t burst_threshold_ms;        // Timer duration (20ms)
} ik_scroll_detector_t;

// Result of processing an arrow event
typedef enum {
    IK_SCROLL_RESULT_NONE,        // Event buffered, waiting for more
    IK_SCROLL_RESULT_ABSORBED,    // Arrow absorbed (part of burst, already emitted wheel)
    IK_SCROLL_RESULT_SCROLL_UP,   // Emit scroll up (mouse wheel detected)
    IK_SCROLL_RESULT_SCROLL_DOWN, // Emit scroll down (mouse wheel detected)
    IK_SCROLL_RESULT_ARROW_UP,    // Emit arrow up (keyboard detected)
    IK_SCROLL_RESULT_ARROW_DOWN,  // Emit arrow down (keyboard detected)
} ik_scroll_result_t;

#define IK_SCROLL_BURST_THRESHOLD_MS 5

// Create detector (talloc-based)
ik_scroll_detector_t *ik_scroll_detector_create(void *parent);

// Process an arrow event
// May return NONE (buffered), SCROLL_*, or ARROW_*
ik_scroll_result_t ik_scroll_detector_process_arrow(ik_scroll_detector_t *det, ik_input_action_type_t arrow_type,  // ARROW_UP or ARROW_DOWN
                                                    int64_t timestamp_ms);

// Check if timeout expired and flush pending event
// Called from event loop when select() times out
// Returns ARROW_* if pending event flushed, NONE otherwise
ik_scroll_result_t ik_scroll_detector_check_timeout(ik_scroll_detector_t *det, int64_t timestamp_ms);

// Get timeout for select() (returns -1 if no pending, else ms until flush)
int64_t ik_scroll_detector_get_timeout_ms(ik_scroll_detector_t *det, int64_t timestamp_ms);

// Flush pending event immediately (for non-arrow input)
// Returns ARROW_* if pending event flushed, NONE otherwise
ik_scroll_result_t ik_scroll_detector_flush(ik_scroll_detector_t *det);

// Reset to initial state
void ik_scroll_detector_reset(ik_scroll_detector_t *det);

#endif // IK_SCROLL_DETECTOR_H
