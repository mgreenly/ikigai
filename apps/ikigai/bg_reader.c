/**
 * @file bg_reader.c
 * @brief Event loop integration for background processes
 */

#include "apps/ikigai/bg_reader.h"

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <talloc.h>
#include <time.h>

#include "apps/ikigai/bg_line_index.h"
#include "apps/ikigai/bg_process_io.h"
#include "apps/ikigai/db/mail.h"
#include "apps/ikigai/mail/msg.h"
#include "shared/error.h"
#include "shared/wrapper.h"
#include "vendor/yyjson/yyjson.h"

#include "shared/poison.h"

/* ================================================================
 * Constants
 * ================================================================ */

#define READ_BUF_SIZE            4096
#define SIGKILL_ESCALATION_SECS  5

/* ================================================================
 * Internal helpers
 * ================================================================ */

/*
 * Send a process exit mail message to the owning agent.
 *
 * Builds a JSON body with process metadata and the last 20 lines of output,
 * then inserts it into the mail table. If the tail read fails (e.g., no
 * output file), an empty string is used. No-op when db is NULL.
 */
static void send_exit_message(ik_db_ctx_t *db, int64_t session_id,
                              bg_process_t *proc)
{
    if (db == NULL) return;

    void *tmp = talloc_new(NULL);

    double age_s = (double)(proc->exited_at.tv_sec  - proc->started_at.tv_sec) +
                   (double)(proc->exited_at.tv_nsec - proc->started_at.tv_nsec) / 1e9;
    if (age_s < 0.0) age_s = 0.0;

    const char *status_str;
    switch (proc->status) {
        case BG_STATUS_EXITED:    status_str = "exited";    break;
        case BG_STATUS_KILLED:    status_str = "killed";    break;
        case BG_STATUS_TIMED_OUT: status_str = "timed_out"; break;
        default:
            talloc_free(tmp);
            return;
    }

    /* 20-line tail of process output */
    uint8_t *tail_buf = NULL;
    size_t   tail_len = 0;
    res_t r = bg_process_read_output(proc, tmp, BG_READ_TAIL, 20, 0, 0,
                                     &tail_buf, &tail_len);
    if (is_err(&r)) {
        talloc_free(r.err);
        tail_buf = NULL;
        tail_len = 0;
    }

    char from_buf[32];
    snprintf(from_buf, sizeof(from_buf), "process:%" PRId32, proc->id);

    yyjson_mut_doc *doc  = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);

    yyjson_mut_obj_add_str(doc, root, "from",        from_buf);
    yyjson_mut_obj_add_str(doc, root, "type",        "process_exit");
    yyjson_mut_obj_add_str(doc, root, "label",       proc->label ? proc->label : "");
    yyjson_mut_obj_add_str(doc, root, "status",      status_str);
    yyjson_mut_obj_add_int(doc, root, "exit_code",   (int64_t)proc->exit_code);
    yyjson_mut_obj_add_real(doc, root, "age_seconds", age_s);
    yyjson_mut_obj_add_int(doc, root, "total_lines",
                           bg_line_index_count(proc->line_index));

    const char *tail_str = (tail_buf != NULL && tail_len > 0)
                           ? (const char *)tail_buf : "";
    yyjson_mut_obj_add_str(doc, root, "tail", tail_str);

    char *body = yyjson_mut_write(doc, 0, NULL);
    yyjson_mut_doc_free(doc);

    if (body != NULL) {
        ik_mail_msg_t *msg = ik_mail_msg_create(tmp, from_buf,
                                                proc->agent_uuid, body);
        if (msg != NULL) {
            res_t ins = ik_db_mail_insert(db, session_id, msg);
            if (is_err(&ins)) talloc_free(ins.err);
        }
        free(body);
    }

    talloc_free(tmp);
}

/*
 * Handle a readable pidfd: collect exit status, apply state transition.
 *
 * Sets *process_terminal to true on a successful EXITED or KILLED transition.
 * Returns true if waitpid indicated the process has gone (ret > 0), regardless
 * of whether the transition succeeded.
 */
static bool handle_pidfd_exit(bg_process_t *proc, bool *process_terminal)
{
    int   wstatus = 0;
    pid_t ret     = waitpid_(proc->pid, &wstatus, WNOHANG);
    if (ret <= 0) return false;

    clock_gettime(CLOCK_MONOTONIC, &proc->exited_at);

    bg_status_t new_status;
    if (WIFEXITED(wstatus)) {
        proc->exit_code   = WEXITSTATUS(wstatus);
        new_status        = BG_STATUS_EXITED;
    } else if (WIFSIGNALED(wstatus)) {
        proc->exit_signal = WTERMSIG(wstatus);
        new_status        = BG_STATUS_KILLED;
    } else {
        return true; /* waitpid returned but status unclear — treat as exited */
    }

    res_t r = bg_process_apply_transition(proc, new_status);
    if (is_err(&r)) {
        talloc_free(r.err);
    } else {
        *process_terminal = true;
    }
    return true;
}

/*
 * Drain all available output from proc->master_fd to disk.
 *
 * Reads in a loop until EAGAIN/EWOULDBLOCK (no data), EOF (n==0),
 * or EIO (slave side closed). On EOF or EIO the master_fd is closed.
 * Append errors are best-effort: the error object is freed and reading
 * continues.
 */
static void drain_master_fd(bg_process_t *proc)
{
    if (proc->master_fd < 0) return;

    uint8_t buf[READ_BUF_SIZE];

    for (;;) {
        ssize_t n = posix_read_(proc->master_fd, buf, sizeof(buf));

        if (n > 0) {
            res_t r = bg_process_append_output(proc, buf, (size_t)n);
            if (is_err(&r)) talloc_free(r.err);
            continue;
        }

        if (n == 0) {
            /* EOF: slave side of PTY closed */
            posix_close_(proc->master_fd);
            proc->master_fd = -1;
            break;
        }

        /* n < 0: check errno */
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            break; /* no more data right now */
        }
        if (errno == EIO) {
            /* PTY slave has gone away (process exited) */
            posix_close_(proc->master_fd);
            proc->master_fd = -1;
        }
        break;
    }
}

/* ================================================================
 * bg_reader_collect_fds
 * ================================================================ */

int bg_reader_collect_fds(const bg_manager_t *mgr, fd_set *read_fds)
{
    assert(mgr != NULL);      /* LCOV_EXCL_BR_LINE */
    assert(read_fds != NULL); /* LCOV_EXCL_BR_LINE */

    int max_fd = -1;

    for (int i = 0; i < mgr->count; i++) {
        const bg_process_t *proc = mgr->processes[i];
        if (proc->status != BG_STATUS_RUNNING) continue;

        if (proc->master_fd >= 0) {
            FD_SET(proc->master_fd, read_fds);
            if (proc->master_fd > max_fd) max_fd = proc->master_fd;
        }
        if (proc->pidfd >= 0) {
            FD_SET(proc->pidfd, read_fds);
            if (proc->pidfd > max_fd) max_fd = proc->pidfd;
        }
    }

    return max_fd;
}

/* ================================================================
 * bg_reader_calculate_timeout
 * ================================================================ */

long bg_reader_calculate_timeout(const bg_manager_t *mgr)
{
    assert(mgr != NULL); /* LCOV_EXCL_BR_LINE */

    for (int i = 0; i < mgr->count; i++) {
        if (mgr->processes[i]->status == BG_STATUS_RUNNING) {
            return 1000L;
        }
    }

    return -1L;
}

/* ================================================================
 * bg_reader_handle_ready
 * ================================================================ */

res_t bg_reader_handle_ready(bg_manager_t *mgr, fd_set *read_fds,
                             ik_db_ctx_t *db, int64_t session_id)
{
    assert(mgr != NULL);      /* LCOV_EXCL_BR_LINE */
    assert(read_fds != NULL); /* LCOV_EXCL_BR_LINE */

    for (int i = 0; i < mgr->count; i++) {
        bg_process_t *proc = mgr->processes[i];
        if (proc->status != BG_STATUS_RUNNING) continue;

        bool process_exited   = false;
        bool process_terminal = false;

        /* pidfd readable: process has exited — collect status */
        if (proc->pidfd >= 0 && FD_ISSET(proc->pidfd, read_fds)) {
            process_exited = handle_pidfd_exit(proc, &process_terminal);
        }

        /* Drain master_fd: either select said it's readable, or we just
         * detected an exit and want to capture any trailing output. */
        bool master_ready = proc->master_fd >= 0 &&
                            FD_ISSET(proc->master_fd, read_fds);
        if (master_ready || process_exited) {
            drain_master_fd(proc);
        }

        /* Send exit mail after trailing output is drained */
        if (process_terminal) {
            send_exit_message(db, session_id, proc);
        }
    }

    return OK(NULL);
}

/* ================================================================
 * bg_reader_check_ttls
 * ================================================================ */

res_t bg_reader_check_ttls(bg_manager_t *mgr, ik_db_ctx_t *db,
                           int64_t session_id)
{
    assert(mgr != NULL); /* LCOV_EXCL_BR_LINE */

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    for (int i = 0; i < mgr->count; i++) {
        bg_process_t *proc = mgr->processes[i];
        if (proc->status != BG_STATUS_RUNNING) continue;
        if (proc->ttl_seconds < 0) continue; /* -1 = no TTL */

        double elapsed_s = (double)(now.tv_sec  - proc->started_at.tv_sec) +
                           (double)(now.tv_nsec - proc->started_at.tv_nsec) / 1e9;

        if (!proc->sigterm_pending) {
            if (elapsed_s >= (double)proc->ttl_seconds) {
                /* TTL expired: signal EOF then SIGTERM */
                char eof_char = '\x04';
                posix_write_(proc->master_fd, &eof_char, 1);
                kill_(-(proc->pid), SIGTERM);
                proc->sigterm_pending  = true;
                proc->sigterm_sent_at  = now;
            }
        } else {
            /* SIGTERM already sent; escalate to SIGKILL after 5 seconds */
            double sigterm_age_s =
                (double)(now.tv_sec  - proc->sigterm_sent_at.tv_sec) +
                (double)(now.tv_nsec - proc->sigterm_sent_at.tv_nsec) / 1e9;

            if (sigterm_age_s >= (double)SIGKILL_ESCALATION_SECS) {
                kill_(-(proc->pid), SIGKILL);
                drain_master_fd(proc);
                clock_gettime(CLOCK_MONOTONIC, &proc->exited_at);
                res_t r = bg_process_apply_transition(proc, BG_STATUS_TIMED_OUT);
                if (is_err(&r)) {
                    talloc_free(r.err);
                } else {
                    send_exit_message(db, session_id, proc);
                }
            }
        }
    }

    return OK(NULL);
}
