/* bg_reader_test.c — event loop integration for background processes */
#include <check.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <talloc.h>
#include <time.h>
#include <unistd.h>

#include "apps/ikigai/bg_line_index.h"
#include "apps/ikigai/bg_process.h"
#include "apps/ikigai/bg_reader.h"
#include "shared/error.h"

/* ================================================================
 * Mock state
 * ================================================================ */

typedef struct { pid_t pid; int sig; } kill_call_t;
static kill_call_t g_kills[16];
static int         g_kill_count      = 0;
static pid_t       g_waitpid_return  = 0;
static int         g_waitpid_status  = 0;

static void reset_mocks(void)
{
    g_kill_count     = 0;
    g_waitpid_return = 0;
    g_waitpid_status = 0;
    memset(g_kills, 0, sizeof(g_kills));
}

/* ================================================================
 * Weak symbol overrides
 * ================================================================ */

ssize_t posix_read_(int fd, void *buf, size_t count);
ssize_t posix_write_(int fd, const void *buf, size_t count);
int     posix_close_(int fd);
int     kill_(pid_t pid, int sig);
pid_t   waitpid_(pid_t pid, int *status, int options);

ssize_t posix_read_(int fd, void *buf, size_t count)
{
    return read(fd, buf, count);
}

ssize_t posix_write_(int fd, const void *buf, size_t count)
{
    return write(fd, buf, count);
}

int posix_close_(int fd)
{
    return close(fd);
}

int kill_(pid_t pid, int sig)
{
    if (g_kill_count < 16) {
        g_kills[g_kill_count].pid = pid;
        g_kills[g_kill_count].sig = sig;
        g_kill_count++;
    }
    return 0;
}

pid_t waitpid_(pid_t pid, int *status, int options)
{
    (void)pid;
    (void)options;
    if (status) *status = g_waitpid_status;
    return g_waitpid_return;
}

/* ================================================================
 * Helpers
 * ================================================================ */

static int make_tmpfile(void)
{
    char path[] = "/tmp/bg_reader_test_XXXXXX";
    int  fd     = mkstemp(path);
    if (fd < 0) ck_abort_msg("mkstemp failed");
    unlink(path);
    return fd;
}

/* Create a pipe with O_NONBLOCK on the read end. */
static void make_pipe_nb(int pipefd[2])
{
    if (pipe(pipefd) < 0) ck_abort_msg("pipe failed");
    int flags = fcntl(pipefd[0], F_GETFL);
    fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK);
}

/* Add a minimal RUNNING bg_process_t directly to the manager. */
static bg_process_t *make_running_proc(bg_manager_t *mgr,
                                       int master_fd,
                                       int pidfd,
                                       int output_fd)
{
    bg_process_t *proc = talloc_zero(mgr, bg_process_t);
    proc->id         = ++mgr->next_id;
    proc->pid        = 9999;
    proc->master_fd  = master_fd;
    proc->pidfd      = pidfd;
    proc->output_fd  = output_fd;
    proc->status     = BG_STATUS_RUNNING;
    proc->ttl_seconds = -1;
    proc->stdin_open = true;
    proc->line_index = bg_line_index_create(proc);
    clock_gettime(CLOCK_MONOTONIC, &proc->started_at);
    mgr->processes[mgr->count++] = proc;
    return proc;
}

/* ================================================================
 * bg_reader_collect_fds tests
 * ================================================================ */

START_TEST(test_collect_fds_empty_manager)
{
    TALLOC_CTX   *ctx = talloc_new(NULL);
    bg_manager_t *mgr = bg_manager_create(ctx);
    fd_set rfds;
    FD_ZERO(&rfds);
    int max = bg_reader_collect_fds(mgr, &rfds);
    ck_assert_int_eq(max, -1);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_collect_fds_running_proc)
{
    reset_mocks();
    TALLOC_CTX   *ctx = talloc_new(NULL);
    bg_manager_t *mgr = bg_manager_create(ctx);
    make_running_proc(mgr, 5, 6, -1);

    fd_set rfds;
    FD_ZERO(&rfds);
    int max = bg_reader_collect_fds(mgr, &rfds);

    ck_assert_int_eq(max, 6);
    ck_assert(FD_ISSET(5, &rfds));
    ck_assert(FD_ISSET(6, &rfds));
    talloc_free(ctx);
}
END_TEST

START_TEST(test_collect_fds_skips_non_running)
{
    reset_mocks();
    TALLOC_CTX   *ctx = talloc_new(NULL);
    bg_manager_t *mgr = bg_manager_create(ctx);
    bg_process_t *proc = make_running_proc(mgr, 5, 6, -1);
    proc->status = BG_STATUS_EXITED;

    fd_set rfds;
    FD_ZERO(&rfds);
    int max = bg_reader_collect_fds(mgr, &rfds);

    ck_assert_int_eq(max, -1);
    ck_assert(!FD_ISSET(5, &rfds));
    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * bg_reader_calculate_timeout tests
 * ================================================================ */

START_TEST(test_timeout_no_running)
{
    TALLOC_CTX   *ctx = talloc_new(NULL);
    bg_manager_t *mgr = bg_manager_create(ctx);
    ck_assert_int_eq(bg_reader_calculate_timeout(mgr), -1L);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_timeout_with_running)
{
    reset_mocks();
    TALLOC_CTX   *ctx = talloc_new(NULL);
    bg_manager_t *mgr = bg_manager_create(ctx);
    make_running_proc(mgr, 5, 6, -1);
    ck_assert_int_eq(bg_reader_calculate_timeout(mgr), 1000L);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_timeout_only_exited)
{
    reset_mocks();
    TALLOC_CTX   *ctx = talloc_new(NULL);
    bg_manager_t *mgr = bg_manager_create(ctx);
    bg_process_t *proc = make_running_proc(mgr, 5, 6, -1);
    proc->status = BG_STATUS_EXITED;
    ck_assert_int_eq(bg_reader_calculate_timeout(mgr), -1L);
    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * bg_reader_handle_ready tests
 * ================================================================ */

START_TEST(test_handle_ready_drains_output_to_disk)
{
    reset_mocks();
    int pipefd[2];
    make_pipe_nb(pipefd);
    int output_fd = make_tmpfile();

    TALLOC_CTX   *ctx = talloc_new(NULL);
    bg_manager_t *mgr = bg_manager_create(ctx);
    bg_process_t *proc = make_running_proc(mgr, pipefd[0], -1, output_fd);

    /* Write data and close write end so drain reads to EOF */
    const char *data = "hello\nworld\n";
    write(pipefd[1], data, strlen(data));
    close(pipefd[1]);

    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(pipefd[0], &rfds);

    res_t r = bg_reader_handle_ready(mgr, &rfds, NULL, 0);

    ck_assert(is_ok(&r));
    ck_assert_int_eq(proc->total_bytes, (int64_t)strlen(data));
    ck_assert_int_eq(bg_line_index_count(proc->line_index), 2);
    ck_assert_int_eq(proc->master_fd, -1); /* closed on EOF */

    talloc_free(ctx);
    close(output_fd);
}
END_TEST

START_TEST(test_handle_ready_pidfd_transitions_exited)
{
    reset_mocks();
    int pidfd_pipe[2];
    make_pipe_nb(pidfd_pipe);

    /* Make pidfd readable (simulates process exit) */
    write(pidfd_pipe[1], "x", 1);
    g_waitpid_return = 9999;
    g_waitpid_status = 0; /* WIFEXITED=true, exit code 0 */

    TALLOC_CTX   *ctx = talloc_new(NULL);
    bg_manager_t *mgr = bg_manager_create(ctx);
    bg_process_t *proc = make_running_proc(mgr, -1, pidfd_pipe[0], -1);

    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(pidfd_pipe[0], &rfds);

    res_t r = bg_reader_handle_ready(mgr, &rfds, NULL, 0);

    ck_assert(is_ok(&r));
    ck_assert_int_eq(proc->status, BG_STATUS_EXITED);
    ck_assert_int_eq(proc->exit_code, 0);

    talloc_free(ctx);
    close(pidfd_pipe[0]);
    close(pidfd_pipe[1]);
}
END_TEST

START_TEST(test_handle_ready_pidfd_transitions_killed)
{
    reset_mocks();
    int pidfd_pipe[2];
    make_pipe_nb(pidfd_pipe);

    write(pidfd_pipe[1], "x", 1);
    g_waitpid_return = 9999;
    g_waitpid_status = SIGKILL; /* WIFSIGNALED=true */

    TALLOC_CTX   *ctx = talloc_new(NULL);
    bg_manager_t *mgr = bg_manager_create(ctx);
    bg_process_t *proc = make_running_proc(mgr, -1, pidfd_pipe[0], -1);

    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(pidfd_pipe[0], &rfds);

    res_t r = bg_reader_handle_ready(mgr, &rfds, NULL, 0);

    ck_assert(is_ok(&r));
    ck_assert_int_eq(proc->status, BG_STATUS_KILLED);
    ck_assert_int_eq(proc->exit_signal, SIGKILL);

    talloc_free(ctx);
    close(pidfd_pipe[0]);
    close(pidfd_pipe[1]);
}
END_TEST

START_TEST(test_handle_ready_output_drained_after_exit)
{
    reset_mocks();
    int pipefd[2];
    make_pipe_nb(pipefd);
    int pidfd_pipe[2];
    make_pipe_nb(pidfd_pipe);
    int output_fd = make_tmpfile();

    /* Write trailing output and close write end */
    const char *data = "trailing\n";
    write(pipefd[1], data, strlen(data));
    close(pipefd[1]);

    /* Make pidfd readable */
    write(pidfd_pipe[1], "x", 1);
    g_waitpid_return = 9999;
    g_waitpid_status = 0; /* normal exit */

    TALLOC_CTX   *ctx = talloc_new(NULL);
    bg_manager_t *mgr = bg_manager_create(ctx);
    bg_process_t *proc = make_running_proc(mgr, pipefd[0], pidfd_pipe[0], output_fd);

    /* Only pidfd in fd_set — master_fd NOT set */
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(pidfd_pipe[0], &rfds);

    res_t r = bg_reader_handle_ready(mgr, &rfds, NULL, 0);

    ck_assert(is_ok(&r));
    ck_assert_int_eq(proc->status, BG_STATUS_EXITED);
    /* Trailing output drained even though master_fd was not in fd_set */
    ck_assert_int_eq(proc->total_bytes, (int64_t)strlen(data));

    talloc_free(ctx);
    close(pidfd_pipe[0]);
    close(pidfd_pipe[1]);
    close(output_fd);
}
END_TEST

/* ================================================================
 * bg_reader_check_ttls tests
 * ================================================================ */

START_TEST(test_check_ttls_not_expired)
{
    reset_mocks();
    TALLOC_CTX   *ctx = talloc_new(NULL);
    bg_manager_t *mgr = bg_manager_create(ctx);
    bg_process_t *proc = make_running_proc(mgr, -1, -1, -1);
    proc->ttl_seconds = 60; /* just started, far from expired */

    res_t r = bg_reader_check_ttls(mgr, NULL, 0);

    ck_assert(is_ok(&r));
    ck_assert_int_eq(g_kill_count, 0);
    ck_assert(!proc->sigterm_pending);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_check_ttls_expired_sends_sigterm)
{
    reset_mocks();
    TALLOC_CTX   *ctx = talloc_new(NULL);
    bg_manager_t *mgr = bg_manager_create(ctx);
    bg_process_t *proc = make_running_proc(mgr, -1, -1, -1);
    proc->ttl_seconds = 1;
    /* Simulate started 10 seconds ago */
    proc->started_at.tv_sec -= 10;

    res_t r = bg_reader_check_ttls(mgr, NULL, 0);

    ck_assert(is_ok(&r));
    ck_assert(proc->sigterm_pending);
    bool found_sigterm = false;
    for (int i = 0; i < g_kill_count; i++) {
        if (g_kills[i].sig == SIGTERM) {
            found_sigterm = true;
            break;
        }
    }
    ck_assert(found_sigterm);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_check_ttls_no_ttl_never_expires)
{
    reset_mocks();
    TALLOC_CTX   *ctx = talloc_new(NULL);
    bg_manager_t *mgr = bg_manager_create(ctx);
    bg_process_t *proc = make_running_proc(mgr, -1, -1, -1);
    proc->ttl_seconds = -1;
    /* Running for a long time — still no expiry */
    proc->started_at.tv_sec -= 3600;

    res_t r = bg_reader_check_ttls(mgr, NULL, 0);

    ck_assert(is_ok(&r));
    ck_assert_int_eq(g_kill_count, 0);
    ck_assert(!proc->sigterm_pending);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_check_ttls_sigkill_escalation)
{
    reset_mocks();
    TALLOC_CTX   *ctx = talloc_new(NULL);
    bg_manager_t *mgr = bg_manager_create(ctx);
    bg_process_t *proc = make_running_proc(mgr, -1, -1, -1);
    proc->ttl_seconds = 1;
    proc->started_at.tv_sec -= 20;

    /* SIGTERM already sent 6 seconds ago */
    proc->sigterm_pending = true;
    clock_gettime(CLOCK_MONOTONIC, &proc->sigterm_sent_at);
    proc->sigterm_sent_at.tv_sec -= 6;

    res_t r = bg_reader_check_ttls(mgr, NULL, 0);

    ck_assert(is_ok(&r));
    ck_assert_int_eq(proc->status, BG_STATUS_TIMED_OUT);
    bool found_sigkill = false;
    for (int i = 0; i < g_kill_count; i++) {
        if (g_kills[i].sig == SIGKILL) {
            found_sigkill = true;
            break;
        }
    }
    ck_assert(found_sigkill);
    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * Suite
 * ================================================================ */

static Suite *bg_reader_suite(void)
{
    Suite *s = suite_create("bg_reader");

    TCase *tc_collect = tcase_create("CollectFds");
    tcase_add_test(tc_collect, test_collect_fds_empty_manager);
    tcase_add_test(tc_collect, test_collect_fds_running_proc);
    tcase_add_test(tc_collect, test_collect_fds_skips_non_running);
    suite_add_tcase(s, tc_collect);

    TCase *tc_timeout = tcase_create("CalculateTimeout");
    tcase_add_test(tc_timeout, test_timeout_no_running);
    tcase_add_test(tc_timeout, test_timeout_with_running);
    tcase_add_test(tc_timeout, test_timeout_only_exited);
    suite_add_tcase(s, tc_timeout);

    TCase *tc_ready = tcase_create("HandleReady");
    tcase_add_test(tc_ready, test_handle_ready_drains_output_to_disk);
    tcase_add_test(tc_ready, test_handle_ready_pidfd_transitions_exited);
    tcase_add_test(tc_ready, test_handle_ready_pidfd_transitions_killed);
    tcase_add_test(tc_ready, test_handle_ready_output_drained_after_exit);
    suite_add_tcase(s, tc_ready);

    TCase *tc_ttls = tcase_create("CheckTtls");
    tcase_add_test(tc_ttls, test_check_ttls_not_expired);
    tcase_add_test(tc_ttls, test_check_ttls_expired_sends_sigterm);
    tcase_add_test(tc_ttls, test_check_ttls_no_ttl_never_expires);
    tcase_add_test(tc_ttls, test_check_ttls_sigkill_escalation);
    suite_add_tcase(s, tc_ttls);

    return s;
}

int32_t main(void)
{
    Suite   *s  = bg_reader_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/bg_reader_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
