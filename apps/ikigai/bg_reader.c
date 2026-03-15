/**
 * @file bg_reader.c
 * @brief Event loop integration for background processes
 */

#include "apps/ikigai/bg_reader.h"

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <time.h>

#include "apps/ikigai/bg_process_io.h"
#include "shared/error.h"
#include "shared/wrapper.h"

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

res_t bg_reader_handle_ready(bg_manager_t *mgr, fd_set *read_fds)
{
    assert(mgr != NULL);      /* LCOV_EXCL_BR_LINE */
    assert(read_fds != NULL); /* LCOV_EXCL_BR_LINE */

    for (int i = 0; i < mgr->count; i++) {
        bg_process_t *proc = mgr->processes[i];
        if (proc->status != BG_STATUS_RUNNING) continue;

        bool process_exited = false;

        /* pidfd readable: process has exited — collect status */
        if (proc->pidfd >= 0 && FD_ISSET(proc->pidfd, read_fds)) {
            int wstatus = 0;
            pid_t ret = waitpid_(proc->pid, &wstatus, WNOHANG);
            if (ret > 0) {
                clock_gettime(CLOCK_MONOTONIC, &proc->exited_at);
                if (WIFEXITED(wstatus)) {
                    proc->exit_code = WEXITSTATUS(wstatus);
                    res_t r = bg_process_apply_transition(proc, BG_STATUS_EXITED);
                    if (is_err(&r)) talloc_free(r.err);
                } else if (WIFSIGNALED(wstatus)) {
                    proc->exit_signal = WTERMSIG(wstatus);
                    res_t r = bg_process_apply_transition(proc, BG_STATUS_KILLED);
                    if (is_err(&r)) talloc_free(r.err);
                }
                process_exited = true;
            }
        }

        /* Drain master_fd: either select said it's readable, or we just
         * detected an exit and want to capture any trailing output. */
        bool master_ready = proc->master_fd >= 0 &&
                            FD_ISSET(proc->master_fd, read_fds);
        if (master_ready || process_exited) {
            drain_master_fd(proc);
        }
    }

    return OK(NULL);
}

/* ================================================================
 * bg_reader_check_ttls
 * ================================================================ */

res_t bg_reader_check_ttls(bg_manager_t *mgr)
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
                if (is_err(&r)) talloc_free(r.err);
            }
        }
    }

    return OK(NULL);
}
