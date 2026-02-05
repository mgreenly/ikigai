#pragma once

#include "shared/error.h"
#include <stdbool.h>
#include <stddef.h>

// Forward declaration
typedef struct ik_logger ik_logger_t;

/**
 * History management module
 *
 * Provides command history functionality with configurable size limit.
 * Supports navigation through history (up/down) and preserves pending input.
 */

/**
 * History context structure
 *
 * Maintains an array of command strings with:
 * - Automatic oldest entry removal when at capacity
 * - Navigation state tracking (current position)
 * - Pending input preservation during browsing
 */
typedef struct {
    char **entries;       // Array of command strings (talloc'd)
    size_t count;         // Current number of entries
    size_t capacity;      // Maximum entries (from config)
    size_t index;         // Current browsing position (count = not browsing)
    char *pending;        // User's pending input before browsing started
} ik_history_t;

/**
 * Create history context
 *
 * @param ctx       Talloc parent context
 * @param capacity  Maximum number of entries to store (must be > 0)
 * @return          Pointer to allocated history context (never NULL - PANICs on OOM)
 *
 * Assertions:
 * - ctx must not be NULL
 * - capacity must be > 0
 */
ik_history_t *ik_history_create(void *ctx, size_t capacity);

/**
 * Add entry to history
 *
 * Appends entry to end of history. If at capacity, removes oldest entry first.
 * Empty strings are not added to history.
 *
 * @param hist   History context
 * @param entry  Command string to add (will be copied)
 * @return       OK(NULL) on success
 *
 * Assertions:
 * - hist must not be NULL
 * - entry must not be NULL
 */
res_t ik_history_add(ik_history_t *hist, const char *entry);

/**
 * Start browsing history
 *
 * Saves pending input and moves to last entry in history.
 * If already browsing, updates pending input and moves to last entry.
 *
 * @param hist           History context
 * @param pending_input  User's current input to preserve
 * @return               OK(NULL) on success
 *
 * Assertions:
 * - hist must not be NULL
 * - pending_input must not be NULL
 */
res_t ik_history_start_browsing(ik_history_t *hist, const char *pending_input);

/**
 * Navigate to previous entry
 *
 * Moves backward in history. Returns NULL if already at beginning.
 *
 * @param hist  History context
 * @return      Entry string or NULL if at beginning
 *
 * Assertions:
 * - hist must not be NULL
 */
const char *ik_history_prev(ik_history_t *hist);

/**
 * Navigate to next entry
 *
 * Moves forward in history. Returns pending input when moving past last entry.
 * Returns NULL if not browsing or already at pending input position.
 *
 * @param hist  History context
 * @return      Entry string, pending input, or NULL
 *
 * Assertions:
 * - hist must not be NULL
 */
const char *ik_history_next(ik_history_t *hist);

/**
 * Stop browsing and return to pending input
 *
 * Resets index to non-browsing state and frees pending input.
 *
 * @param hist  History context
 *
 * Assertions:
 * - hist must not be NULL
 */
void ik_history_stop_browsing(ik_history_t *hist);

/**
 * Get current entry during browsing
 *
 * Returns entry at current position, or pending input if not browsing.
 * Returns NULL if history is empty and not browsing.
 *
 * @param hist  History context
 * @return      Current entry or pending input or NULL
 *
 * Assertions:
 * - hist must not be NULL
 */
const char *ik_history_get_current(const ik_history_t *hist);

/**
 * Check if currently browsing history
 *
 * @param hist  History context
 * @return      true if browsing, false otherwise
 *
 * Assertions:
 * - hist must not be NULL
 */
bool ik_history_is_browsing(const ik_history_t *hist);
