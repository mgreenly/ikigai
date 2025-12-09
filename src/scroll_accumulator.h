#ifndef IK_SCROLL_ACCUMULATOR_H
#define IK_SCROLL_ACCUMULATOR_H

#include <inttypes.h>

#include "input.h"

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

#endif // IK_SCROLL_ACCUMULATOR_H
