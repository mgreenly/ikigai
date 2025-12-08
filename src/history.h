#pragma once

#include "error.h"
#include <stdbool.h>
#include <stddef.h>

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

/**
 * Ensure history directory exists, creating it if necessary
 *
 * Checks if $PWD/.ikigai/ directory exists and creates it with mode 0755 if missing.
 * This function is idempotent - safe to call multiple times.
 * Uses access() to check existence and mkdir() to create.
 *
 * @param ctx   Talloc context for error allocation
 * @return      OK(NULL) on success, ERR with IO error if creation fails
 *
 * Error cases:
 * - Permission denied when creating directory
 * - Disk full when creating directory
 * - Path exists as non-directory
 *
 * Assertions:
 * - ctx must not be NULL
 */
res_t ik_history_ensure_directory(TALLOC_CTX *ctx);

/**
 * Load history from .ikigai/history file
 *
 * Reads JSONL file from $PWD/.ikigai/history and populates history structure.
 * Creates empty file if it doesn't exist. Skips malformed lines with warning.
 * If file has more entries than capacity, keeps only the most recent ones.
 *
 * @param ctx   Talloc context for allocations
 * @param hist  History context to populate
 * @return      OK(NULL) on success, ERR with IO or PARSE error on failure
 *
 * Error cases:
 * - Directory creation fails
 * - File read fails (permissions, etc.)
 * - File contains only malformed entries (all skipped)
 *
 * Assertions:
 * - ctx must not be NULL
 * - hist must not be NULL
 */
res_t ik_history_load(TALLOC_CTX *ctx, ik_history_t *hist);

/**
 * Save entire history to .ikigai/history file
 *
 * Writes all entries to JSONL file using atomic write pattern (temp file + rename).
 * Each entry formatted as: {"cmd": "...", "ts": "ISO-8601-timestamp"}\n
 * Multi-line commands preserved with JSON \n escaping.
 *
 * @param hist  History context to save
 * @return      OK(NULL) on success, ERR with IO error on failure
 *
 * Error cases:
 * - Directory creation fails
 * - Temp file creation fails
 * - Write fails (disk full, permissions)
 * - Rename fails
 *
 * Assertions:
 * - hist must not be NULL
 */
res_t ik_history_save(const ik_history_t *hist);

/**
 * Append single entry to .ikigai/history file
 *
 * Opens file in append mode and writes one JSONL line.
 * Creates file if it doesn't exist.
 * Format: {"cmd": "...", "ts": "ISO-8601-timestamp"}\n
 *
 * @param entry  Command string to append
 * @return       OK(NULL) on success, ERR with IO error on failure
 *
 * Error cases:
 * - Directory creation fails
 * - File open fails (permissions, disk full)
 * - Write fails
 *
 * Assertions:
 * - entry must not be NULL
 */
res_t ik_history_append_entry(const char *entry);
