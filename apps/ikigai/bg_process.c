/**
 * @file bg_process.c
 * @brief Background process manager — lifecycle, state machine, concurrency limit
 */

#include "apps/ikigai/bg_process.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <talloc.h>
#include <time.h>
#include <unistd.h>

#include "apps/ikigai/db/pg_result.h"
#include "apps/ikigai/tmp_ctx.h"
#include "apps/ikigai/wrapper_postgres.h"
#include "shared/error.h"
#include "shared/panic.h"
#include "shared/wrapper.h"

#include "shared/poison.h"

/* ================================================================
 * Constants
 * ================================================================ */

#define BG_MANAGER_INITIAL_CAPACITY 16
#define BG_PATH_MAX                 512

/* ================================================================
 * State machine
 * ================================================================ */

res_t bg_process_apply_transition(bg_process_t *proc, bg_status_t new_status)
{
    assert(proc != NULL); // LCOV_EXCL_BR_LINE

    bg_status_t cur = proc->status;
    bool valid = false;

    switch (new_status) {
        case BG_STATUS_RUNNING:
            valid = (cur == BG_STATUS_STARTING);
            break;
        case BG_STATUS_FAILED:
            valid = (cur == BG_STATUS_STARTING);
            break;
        case BG_STATUS_EXITED:
            valid = (cur == BG_STATUS_RUNNING);
            break;
        case BG_STATUS_KILLED:
            valid = (cur == BG_STATUS_RUNNING);
            break;
        case BG_STATUS_TIMED_OUT:
            valid = (cur == BG_STATUS_RUNNING);
            break;
        case BG_STATUS_STARTING:
            valid = false;
            break;
        default: // LCOV_EXCL_LINE
            valid = false; // LCOV_EXCL_LINE
            break; // LCOV_EXCL_LINE
    }

    if (!valid) {
        return ERR(proc, INVALID_ARG,
                   "invalid state transition: %d -> %d", (int)cur, (int)new_status);
    }

    proc->status = new_status;
    return OK(NULL);
}

/* ================================================================
 * Process destructor — closes open fds
 * ================================================================ */

static int bg_process_destructor(bg_process_t *proc)
{
    if (proc->master_fd >= 0) {
        posix_close_(proc->master_fd);
        proc->master_fd = -1;
    }
    if (proc->pidfd >= 0) {
        posix_close_(proc->pidfd);
        proc->pidfd = -1;
    }
    if (proc->output_fd >= 0) {
        posix_close_(proc->output_fd);
        proc->output_fd = -1;
    }
    return 0;
}

/* ================================================================
 * Manager
 * ================================================================ */

bg_manager_t *bg_manager_create(TALLOC_CTX *ctx)
{
    assert(ctx != NULL); // LCOV_EXCL_BR_LINE

    bg_manager_t *mgr = talloc_zero(ctx, bg_manager_t);
    if (mgr == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    mgr->processes = talloc_array(mgr, bg_process_t*, BG_MANAGER_INITIAL_CAPACITY);
    if (mgr->processes == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    mgr->capacity = BG_MANAGER_INITIAL_CAPACITY;
    mgr->count    = 0;
    mgr->next_id  = 1;

    return mgr;
}

int bg_manager_active_count(const bg_manager_t *mgr)
{
    assert(mgr != NULL); // LCOV_EXCL_BR_LINE

    int active = 0;
    for (int i = 0; i < mgr->count; i++) {
        bg_status_t s = mgr->processes[i]->status;
        if (s == BG_STATUS_STARTING || s == BG_STATUS_RUNNING) {
            active++;
        }
    }
    return active;
}

/* ================================================================
 * Internal helpers
 * ================================================================ */

/* Grow the process array by 2x and add proc. */
static res_t manager_add(bg_manager_t *mgr, bg_process_t *proc)
{
    if (mgr->count >= mgr->capacity) {
        int new_cap = mgr->capacity * 2;
        bg_process_t **new_arr = talloc_realloc(mgr, mgr->processes,
                                                bg_process_t*, (unsigned)new_cap);
        if (new_arr == NULL) {
            return ERR(mgr, OUT_OF_MEMORY, "out of memory growing process array");
        }
        mgr->processes = new_arr;
        mgr->capacity  = new_cap;
    }
    mgr->processes[mgr->count++] = proc;
    return OK(NULL);
}

/* Ensure a directory exists (create if absent; EEXIST is not an error). */
static res_t ensure_dir(TALLOC_CTX *ctx, const char *path)
{
    int r = posix_mkdir_(path, 0755);
    if (r < 0 && errno != EEXIST) {
        return ERR(ctx, IO, "cannot create directory %s: %s", path, strerror(errno));
    }
    return OK(NULL);
}

/* ================================================================
 * DB helpers (no-ops when db is NULL)
 * ================================================================ */

/*
 * Insert a new background_processes row with status='starting'.
 * Returns the assigned id via out_id.
 */
static res_t db_insert_starting(ik_db_ctx_t *db, const char *agent_uuid,
                                 const char *command, const char *label,
                                 int32_t ttl_seconds, int32_t *out_id)
{
    assert(db != NULL);       // LCOV_EXCL_BR_LINE
    assert(out_id != NULL);   // LCOV_EXCL_BR_LINE

    TALLOC_CTX *tmp = tmp_ctx_create();

    const char *query =
        "INSERT INTO background_processes "
        "  (agent_uuid, command, label, ttl_seconds) "
        "VALUES ($1, $2, $3, $4) "
        "RETURNING id";

    char ttl_str[32];
    int written = snprintf(ttl_str, sizeof(ttl_str), "%" PRId32, ttl_seconds);
    if (written < 0 || (size_t)written >= sizeof(ttl_str)) { // LCOV_EXCL_BR_LINE
        talloc_free(tmp);                                     // LCOV_EXCL_LINE
        return ERR(db, IO, "ttl_seconds formatting failed"); // LCOV_EXCL_LINE
    }

    const char *params[4] = { agent_uuid, command, label, ttl_str };

    ik_pg_result_wrapper_t *wr =
        ik_db_wrap_pg_result(tmp, pq_exec_params_(db->conn, query, 4,
                                                   NULL, params, NULL, NULL, 0));
    PGresult *res = wr->pg_result;

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        const char *pq_err = PQerrorMessage(db->conn);
        res_t r = ERR(db, IO, "bg_process insert failed: %s", pq_err);
        talloc_free(tmp);
        return r;
    }

    const char *id_str = PQgetvalue(res, 0, 0);
    *out_id = (int32_t)strtol(id_str, NULL, 10);

    talloc_free(tmp);
    return OK(NULL);
}

/* Update status to 'running' and set pid. */
static void db_update_running(ik_db_ctx_t *db, int32_t id, pid_t pid)
{
    if (db == NULL) return;

    TALLOC_CTX *tmp = tmp_ctx_create();

    const char *query =
        "UPDATE background_processes "
        "SET pid = $1, status = 'running', started_at = NOW() "
        "WHERE id = $2";

    char pid_str[32];
    char id_str[32];
    snprintf(pid_str, sizeof(pid_str), "%d", (int)pid);  // NOLINT
    snprintf(id_str,  sizeof(id_str),  "%" PRId32, id);  // NOLINT

    const char *params[2] = { pid_str, id_str };
    ik_pg_result_wrapper_t *wr =
        ik_db_wrap_pg_result(tmp, pq_exec_params_(db->conn, query, 2,
                                                   NULL, params, NULL, NULL, 0));
    (void)wr; /* errors are best-effort here */
    talloc_free(tmp);
}

/* Update status to 'failed'. */
static void db_update_failed(ik_db_ctx_t *db, int32_t id)
{
    if (db == NULL) return;

    TALLOC_CTX *tmp = tmp_ctx_create();

    const char *query =
        "UPDATE background_processes SET status = 'failed' WHERE id = $1";

    char id_str[32];
    snprintf(id_str, sizeof(id_str), "%" PRId32, id); // NOLINT

    const char *params[1] = { id_str };
    ik_pg_result_wrapper_t *wr =
        ik_db_wrap_pg_result(tmp, pq_exec_params_(db->conn, query, 1,
                                                   NULL, params, NULL, NULL, 0));
    (void)wr;
    talloc_free(tmp);
}

/* ================================================================
 * bg_process_start
 * ================================================================ */

res_t bg_process_start(bg_manager_t *mgr,
                       ik_db_ctx_t *db,
                       const char *output_base_dir,
                       const char *command,
                       const char *label,
                       const char *agent_uuid,
                       int32_t ttl_seconds,
                       bg_process_t **out_proc)
{
    if (mgr == NULL || output_base_dir == NULL || command == NULL ||
        label == NULL || agent_uuid == NULL || out_proc == NULL) {
        return ERR(mgr != NULL ? (TALLOC_CTX *)mgr : talloc_new(NULL),
                   INVALID_ARG, "NULL argument to bg_process_start");
    }

    /* Check concurrency limit */
    if (bg_manager_active_count(mgr) >= BG_PROCESS_MAX_CONCURRENT) {
        return ERR(mgr, IO,
                   "concurrency limit: %d/%d slots in use; kill a process first",
                   BG_PROCESS_MAX_CONCURRENT, BG_PROCESS_MAX_CONCURRENT);
    }

    /* Allocate process struct as talloc child of manager */
    bg_process_t *proc = talloc_zero(mgr, bg_process_t);
    if (proc == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    proc->master_fd   = -1;
    proc->pidfd       = -1;
    proc->output_fd   = -1;
    proc->status      = BG_STATUS_STARTING;
    proc->ttl_seconds = ttl_seconds;
    proc->stdin_open  = true;

    proc->command    = talloc_strdup(proc, command);
    proc->label      = talloc_strdup(proc, label);
    proc->agent_uuid = talloc_strdup(proc, agent_uuid);
    if (!proc->command || !proc->label || !proc->agent_uuid) {
        PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    }

    proc->line_index = bg_line_index_create(proc);
    talloc_set_destructor(proc, bg_process_destructor);

    /* Assign ID */
    if (db != NULL) {
        res_t r = db_insert_starting(db, agent_uuid, command, label,
                                     ttl_seconds, &proc->id);
        if (is_err(&r)) {
            talloc_free(proc);
            return r;
        }
    } else {
        proc->id = mgr->next_id++;
    }

    /* Ensure output directories exist */
    CHECK(ensure_dir(proc, output_base_dir));

    char dir_path[BG_PATH_MAX];
    int n = snprintf(dir_path, sizeof(dir_path), "%s/%" PRId32,
                     output_base_dir, proc->id);
    if (n < 0 || (size_t)n >= sizeof(dir_path)) { // LCOV_EXCL_BR_LINE
        talloc_free(proc);                         // LCOV_EXCL_LINE
        return ERR(mgr, IO, "output path too long"); // LCOV_EXCL_LINE
    }

    CHECK(ensure_dir(proc, dir_path));

    char out_path[BG_PATH_MAX];
    n = snprintf(out_path, sizeof(out_path), "%s/output", dir_path);
    if (n < 0 || (size_t)n >= sizeof(out_path)) { // LCOV_EXCL_BR_LINE
        talloc_free(proc);                         // LCOV_EXCL_LINE
        return ERR(mgr, IO, "output path too long"); // LCOV_EXCL_LINE
    }

    /* Fork via PTY */
    struct winsize ws = { .ws_row = 50, .ws_col = 200 };
    pid_t ppid = getpid();
    pid_t pid  = forkpty_(&proc->master_fd, NULL, NULL, &ws);

    if (pid == 0) {
        /* ---- child ---- */
        setpgid_(0, 0);
        prctl_(PR_SET_PDEATHSIG, SIGTERM, 0, 0, 0);

        /* Race guard: if parent already died, exit immediately */
        if (getppid() != ppid) _exit(1); // LCOV_EXCL_LINE

        /* Set terminal type */
        setenv("TERM", "xterm-256color", 1);

        execl("/bin/sh", "sh", "-c", command, NULL);
        _exit(127); // LCOV_EXCL_LINE
    }

    /* ---- parent ---- */

    if (pid < 0) {
        /* Fork failed — mark FAILED, add to manager, and return */
        proc->master_fd = -1; /* forkpty may have corrupted it */
        db_update_failed(db, proc->id);
        bg_process_apply_transition(proc, BG_STATUS_FAILED);
        manager_add(mgr, proc);
        *out_proc = proc;
        return OK(NULL);
    }

    proc->pid = pid;

    /* Get a pollable file descriptor for process exit */
    proc->pidfd = pidfd_open_(pid, 0);
    /* pidfd failure is non-fatal: set -1, caller detects via waitpid fallback */

    /* Make PTY master non-blocking for the event loop */
    posix_fcntl_(proc->master_fd, F_SETFL, O_NONBLOCK);

    /* Open output file (append, create) */
    proc->output_fd = open(out_path, O_WRONLY | O_CREAT | O_APPEND, 0600);
    /* output_fd failure is non-fatal; output just won't be persisted */

    /* Record start time */
    clock_gettime(CLOCK_MONOTONIC, &proc->started_at);

    /* Update DB */
    db_update_running(db, proc->id, pid);

    /* Transition to RUNNING */
    bg_process_apply_transition(proc, BG_STATUS_RUNNING);

    /* Register with manager */
    res_t r = manager_add(mgr, proc);
    if (is_err(&r)) {
        talloc_free(proc);
        return r;
    }

    *out_proc = proc;
    return OK(NULL);
}

/* ================================================================
 * bg_process_kill
 * ================================================================ */

res_t bg_process_kill(bg_process_t *proc)
{
    assert(proc != NULL); // LCOV_EXCL_BR_LINE

    res_t r = bg_process_apply_transition(proc, BG_STATUS_KILLED);
    if (is_err(&r)) return r;

    /* Send SIGTERM to the entire process group */
    kill_(-(proc->pid), SIGTERM);

    return OK(NULL);
}
