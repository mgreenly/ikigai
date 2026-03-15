/**
 * @file bg_process.h
 * @brief Background process manager — lifecycle, state machine, concurrency limit
 *
 * Manages background processes started by agents. Each process runs in a PTY
 * allocated via forkpty(). This module handles process creation, termination,
 * and state transitions. I/O and event loop integration live in bg_reader.c.
 *
 * All operations are single-threaded (main thread only).
 */

#ifndef IK_BG_PROCESS_H
#define IK_BG_PROCESS_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <sys/types.h>
#include <talloc.h>

#include "shared/error.h"
#include "apps/ikigai/bg_line_index.h"
#include "apps/ikigai/db/connection.h"

/** Maximum concurrent RUNNING+STARTING processes per manager. */
#define BG_PROCESS_MAX_CONCURRENT 20

/**
 * Process status — mirrors the bg_process_status DB enum.
 * Terminal states (EXITED, KILLED, TIMED_OUT, FAILED) are permanent.
 */
typedef enum {
    BG_STATUS_STARTING,   /* initial — between allocation and fork success */
    BG_STATUS_RUNNING,    /* fork succeeded, process alive                 */
    BG_STATUS_EXITED,     /* process exited normally                       */
    BG_STATUS_KILLED,     /* terminated by user or agent                   */
    BG_STATUS_TIMED_OUT,  /* TTL expired                                   */
    BG_STATUS_FAILED,     /* forkpty failed                                */
} bg_status_t;

/**
 * Per-process state. Talloc-owned by bg_manager_t.
 * A talloc destructor closes open file descriptors on free.
 */
typedef struct {
    int32_t          id;           /* monotonic, never reused (DB SERIAL or local counter) */
    pid_t            pid;          /* OS pid; 0 until fork succeeds                        */
    int              master_fd;    /* PTY master (read+write); -1 until fork               */
    int              pidfd;        /* pollable exit fd (pidfd_open); -1 on failure         */
    int              output_fd;    /* disk output file fd (append); -1 until fork          */

    bg_status_t      status;
    int              exit_code;    /* valid when EXITED                */
    int              exit_signal;  /* valid when KILLED by signal      */

    char            *command;      /* shell command string             */
    char            *label;        /* human-readable description       */
    char            *agent_uuid;   /* owning agent UUID                */

    int32_t          ttl_seconds;  /* -1 = forever                     */
    struct timespec  started_at;   /* CLOCK_MONOTONIC at fork          */
    struct timespec  exited_at;    /* CLOCK_MONOTONIC at exit          */

    bg_line_index_t *line_index;   /* in-memory newline offsets        */
    int64_t          total_bytes;  /* total output bytes written       */
    int64_t          cursor;       /* last byte offset read by agent   */

    bool             stdin_open;   /* false after close-stdin          */
} bg_process_t;

/**
 * Process manager: owns all bg_process_t instances for one agent.
 * Talloc destructor cleans up all owned processes on free.
 */
typedef struct {
    bg_process_t   **processes;   /* array of talloc-owned process pointers */
    int              count;       /* total processes in array                */
    int              capacity;    /* allocated slots in processes            */
    int32_t          next_id;     /* local ID counter (used when db is NULL) */
} bg_manager_t;

/**
 * Create a background process manager.
 *
 * Allocates the manager as a talloc child of ctx. All processes it starts
 * are talloc children of the manager, so freeing the manager cleans up
 * all owned processes and closes their file descriptors.
 *
 * @param ctx  Talloc parent. Must not be NULL.
 * @return     Allocated manager. Never NULL; panics on OOM.
 */
bg_manager_t *bg_manager_create(TALLOC_CTX *ctx);

/**
 * Count active (RUNNING + STARTING) processes in the manager.
 *
 * @param mgr  Manager instance. Must not be NULL.
 * @return     Number of active processes.
 */
int bg_manager_active_count(const bg_manager_t *mgr);

/**
 * Start a background process.
 *
 * Checks the concurrency limit, inserts a DB row (if db != NULL), forks via
 * forkpty, sets up child and parent sides, and creates the output directory
 * and file. On fork failure the process is added to the manager with status
 * FAILED and *out_proc is set.
 *
 * State transitions:
 *   STARTING → RUNNING  on successful fork
 *   STARTING → FAILED   on fork failure
 *
 * @param mgr              Manager instance. Must not be NULL.
 * @param db               DB context. May be NULL (skips DB operations).
 * @param output_base_dir  Base dir for per-process output subdirs. Must not
 *                         be NULL. Created if absent.
 * @param command          Shell command. Must not be NULL.
 * @param label            Human-readable label. Must not be NULL.
 * @param agent_uuid       Owning agent UUID. Must not be NULL.
 * @param ttl_seconds      Time-to-live in seconds. -1 = forever.
 * @param out_proc         Output: set to the new bg_process_t on success.
 *                         Set even when status=FAILED. Must not be NULL.
 * @return OK on success (status=RUNNING or FAILED).
 *         ERR_IO if the concurrency limit is reached (no process created).
 *         ERR_INVALID_ARG if required pointer arguments are NULL.
 */
res_t bg_process_start(bg_manager_t *mgr,
                       ik_db_ctx_t *db,
                       const char *output_base_dir,
                       const char *command,
                       const char *label,
                       const char *agent_uuid,
                       int32_t ttl_seconds,
                       bg_process_t **out_proc);

/**
 * Kill a background process.
 *
 * Sends SIGTERM to the process group. Valid only in RUNNING state.
 * Transitions status to KILLED.
 *
 * @param proc  Process to kill. Must not be NULL.
 * @return OK on success.
 *         ERR_INVALID_ARG if proc is not in RUNNING state.
 */
res_t bg_process_kill(bg_process_t *proc);

/**
 * Apply a state transition.
 *
 * Validates that new_status is a legal successor of the current status
 * and updates proc->status. Exposed for use by bg_reader and tests.
 *
 * Valid transitions:
 *   STARTING → RUNNING | FAILED
 *   RUNNING  → EXITED | KILLED | TIMED_OUT
 *
 * @param proc        Process. Must not be NULL.
 * @param new_status  Target status.
 * @return OK on success.
 *         ERR_INVALID_ARG if the transition is not permitted.
 */
res_t bg_process_apply_transition(bg_process_t *proc, bg_status_t new_status);

#endif /* IK_BG_PROCESS_H */
