/**
 * @file debug_pipe.h
 * @brief Debug output pipe system for capturing library output
 *
 * Provides a pipe-based mechanism to capture debug output from libraries
 * (like curl, libulfius) that write to FILE* handles and route it to
 * ikigai's scrollback buffer. Output can be toggled on/off at runtime
 * via the /debug command without blocking writers.
 *
 * Design principles:
 * - Pipe-based: Each debug source gets its own pipe (write end = FILE*)
 * - Event loop integrated: Read ends monitored by select() in REPL
 * - Always drain: Pipes read even when debug disabled to prevent blocking
 * - Line buffered: Partial lines accumulated until newline arrives
 * - Prefixed output: Each pipe can have optional prefix (e.g., "[curl]")
 *
 * Usage:
 * ```c
 * // Create debug pipe manager in REPL
 * repl->debug_mgr = ik_debug_manager_create(repl);
 * repl->debug_enabled = false;
 *
 * // Create pipe for curl output
 * ik_debug_pipe_t *curl_pipe = ik_debug_manager_add_pipe(repl->debug_mgr, "[curl]");
 * curl_easy_setopt(handle, CURLOPT_VERBOSE, 1L);
 * curl_easy_setopt(handle, CURLOPT_STDERR, curl_pipe->write_end);
 *
 * // Event loop handles the rest automatically
 * ```
 */

#ifndef IKIGAI_DEBUG_PIPE_H
#define IKIGAI_DEBUG_PIPE_H

#include "error.h"
#include "scrollback.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/select.h>

/**
 * @brief Single debug pipe for capturing output from a subsystem
 *
 * Represents a pipe where one end (write_end) is given to a library/subsystem
 * for writing debug output, and the other end (read_fd) is monitored by the
 * event loop. Partial lines are buffered until complete lines arrive.
 */
typedef struct {
    FILE *write_end;           /**< FILE* given to subsystem for writing */
    int read_fd;               /**< File descriptor monitored by select() */
    char *prefix;              /**< Optional prefix prepended to lines (e.g., "[curl]") */
    char *line_buffer;         /**< Buffer for accumulating partial lines */
    size_t buffer_pos;         /**< Current position in line_buffer */
    size_t buffer_capacity;    /**< Allocated size of line_buffer */
} ik_debug_pipe_t;

/**
 * @brief Manager for collection of debug pipes
 *
 * Manages multiple debug pipes and provides unified operations for
 * adding pipes to fd_set and handling ready descriptors.
 */
typedef struct {
    ik_debug_pipe_t **pipes;   /**< Array of debug pipe pointers */
    size_t count;              /**< Number of pipes currently managed */
    size_t capacity;           /**< Allocated capacity of pipes array */
} ik_debug_pipe_manager_t;

/**
 * @brief Create a new debug pipe
 *
 * Creates a pipe, sets read end to non-blocking, opens write end as FILE*,
 * and allocates line buffer for partial line accumulation.
 *
 * @param parent Talloc parent context (typically the manager or repl)
 * @param prefix Optional prefix for output lines (NULL for no prefix)
 * @return Result containing new debug pipe, or error on failure
 */
res_t ik_debug_pipe_create(void *parent, const char *prefix);

/**
 * @brief Read and parse lines from debug pipe
 *
 * Reads available data from pipe (non-blocking), accumulates partial lines,
 * and returns complete lines when newlines are encountered. Handles arbitrary
 * line lengths by growing buffer as needed.
 *
 * @param pipe Debug pipe to read from
 * @param lines_out Output array of complete lines (NULL-terminated)
 * @param count_out Number of lines in output array
 * @return Result with OK(NULL) on success, ERR() on failure
 */
res_t ik_debug_pipe_read(ik_debug_pipe_t *pipe, char ***lines_out, size_t *count_out);

/**
 * @brief Create a new debug pipe manager
 *
 * @param parent Talloc parent context (typically the repl)
 * @return Result containing new manager, or error on failure
 */
res_t ik_debug_manager_create(void *parent);

/**
 * @brief Add a new debug pipe to the manager
 *
 * Creates a new debug pipe and adds it to the manager's collection.
 * Automatically grows the internal array if needed.
 *
 * @param mgr Debug pipe manager
 * @param prefix Optional prefix for the pipe's output (NULL for no prefix)
 * @return Result containing new debug pipe, or error on failure
 */
res_t ik_debug_manager_add_pipe(ik_debug_pipe_manager_t *mgr, const char *prefix);

/**
 * @brief Add all managed pipe read descriptors to fd_set
 *
 * Populates fd_set with all managed pipe read descriptors for use with select().
 * Updates max_fd if any pipe descriptor exceeds current value.
 *
 * @param mgr Debug pipe manager
 * @param read_fds File descriptor set to populate
 * @param max_fd Pointer to max fd value (updated if necessary)
 */
void ik_debug_manager_add_to_fdset(ik_debug_pipe_manager_t *mgr, fd_set *read_fds, int *max_fd);

/**
 * @brief Handle ready pipe descriptors after select()
 *
 * Checks which pipes are ready, reads from them, and either appends output
 * to scrollback (if debug_enabled) or discards it (to prevent blocking writers).
 * Always drains ready pipes regardless of debug_enabled state.
 *
 * @param mgr Debug pipe manager
 * @param read_fds File descriptor set from select()
 * @param scrollback Scrollback buffer for output (can be NULL if debug_enabled is false)
 * @param debug_enabled Whether to append output to scrollback
 * @return Result with OK(NULL) on success, ERR() on failure
 */
res_t ik_debug_manager_handle_ready(ik_debug_pipe_manager_t *mgr,
                                    fd_set *read_fds,
                                    ik_scrollback_t *scrollback,
                                    bool debug_enabled);

#endif // IKIGAI_DEBUG_PIPE_H
