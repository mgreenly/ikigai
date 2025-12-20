#pragma once

#include "error.h"
#include "history.h"

// Forward declaration
typedef struct ik_logger ik_logger_t;

/**
 * History I/O module - File persistence for command history
 *
 * Provides file system operations for loading and saving history:
 * - JSONL format with {"cmd": "...", "ts": "..."} entries
 * - Atomic writes using temp file + rename pattern
 * - Directory creation and management
 */

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
 * @param ctx     Talloc context for allocations
 * @param hist    History context to populate
 * @param logger  Logger for warnings about malformed lines
 * @return        OK(NULL) on success, ERR with IO or PARSE error on failure
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
res_t ik_history_load(TALLOC_CTX *ctx, ik_history_t *hist, ik_logger_t *logger);

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
