/**
 * @file bg_reader.h
 * @brief Event loop integration for background processes
 *
 * Integrates the background process manager with ikigai's select()-based
 * event loop. Drains PTY output, detects process exit via pidfd, and
 * enforces TTL expiry with SIGTERM/SIGKILL escalation.
 *
 * All operations are single-threaded (main thread only).
 */

#ifndef IK_BG_READER_H
#define IK_BG_READER_H

#include <sys/select.h>

#include <stdint.h>

#include "shared/error.h"
#include "apps/ikigai/bg_process.h"
#include "apps/ikigai/db/connection.h"

/**
 * Add RUNNING process fds to a select() read fd_set.
 *
 * For each RUNNING process: adds master_fd (if >= 0) and pidfd (if >= 0)
 * to read_fds. The pidfd fires when the process exits; master_fd fires
 * when output is available.
 *
 * @param mgr       Manager. Must not be NULL.
 * @param read_fds  fd_set to populate. Must not be NULL.
 * @return          Highest fd number added, or -1 if no fds were added.
 */
int bg_reader_collect_fds(const bg_manager_t *mgr, fd_set *read_fds);

/**
 * Calculate the select() timeout contribution from background processes.
 *
 * Returns 1000 (ms) when any RUNNING process exists, to ensure TTL checks
 * run at least every second. Returns -1 if no RUNNING processes exist
 * (no constraint imposed on the timeout).
 *
 * @param mgr  Manager. Must not be NULL.
 * @return     Timeout in milliseconds, or -1 for no constraint.
 */
long bg_reader_calculate_timeout(const bg_manager_t *mgr);

/**
 * Handle readable fds after select() returns.
 *
 * For each RUNNING process:
 *   - If master_fd is in read_fds: drain output via bg_process_append_output().
 *   - If pidfd is in read_fds: call waitpid(WNOHANG), transition to EXITED
 *     or KILLED, then drain master_fd until EOF to capture trailing output.
 *   - On terminal state transition, sends an exit mail to the owning agent
 *     (if db is non-NULL).
 *
 * @param mgr        Manager. Must not be NULL.
 * @param read_fds   fd_set returned by select(). Must not be NULL.
 * @param db         DB context. May be NULL (skips mail delivery).
 * @param session_id Current session ID (used for mail insert).
 * @return           OK always (individual I/O errors are best-effort).
 */
res_t bg_reader_handle_ready(bg_manager_t *mgr, fd_set *read_fds,
                             ik_db_ctx_t *db, int64_t session_id);

/**
 * Enforce TTL expiry for all RUNNING processes.
 *
 * For each RUNNING process with ttl_seconds >= 0:
 *   - At expiry: write '\x04' (EOF) to master_fd, send SIGTERM to the
 *     process group, record sigterm_sent_at.
 *   - 5 seconds after SIGTERM: send SIGKILL to the process group,
 *     drain remaining output, transition to TIMED_OUT.
 *   - On TIMED_OUT transition, sends an exit mail to the owning agent
 *     (if db is non-NULL).
 *
 * Processes with ttl_seconds == -1 are never expired.
 *
 * @param mgr        Manager. Must not be NULL.
 * @param db         DB context. May be NULL (skips mail delivery).
 * @param session_id Current session ID (used for mail insert).
 * @return           OK always (signal errors are best-effort).
 */
res_t bg_reader_check_ttls(bg_manager_t *mgr, ik_db_ctx_t *db,
                           int64_t session_id);

#endif /* IK_BG_READER_H */
