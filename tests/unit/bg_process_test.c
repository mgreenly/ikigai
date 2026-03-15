/* bg_process_test.c — state machine, lifecycle, concurrency limit */
#include <check.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <talloc.h>
#include <termios.h>
#include <unistd.h>

#include "apps/ikigai/bg_process.h"
#include "shared/error.h"

/* Mock state */
static pid_t g_forkpty_return  = 1234;
static int   g_forkpty_master  = 50;
static int   g_pidfd_return    = 51;
static int   g_kill_last_sig   = -1;
static pid_t g_kill_last_pid   = 0;
static int   g_close_call_count = 0;

static void reset_mocks(void)
{
    g_forkpty_return   = 1234;
    g_forkpty_master   = 50;
    g_pidfd_return     = 51;
    g_kill_last_sig    = -1;
    g_kill_last_pid    = 0;
    g_close_call_count = 0;
}

/* Weak symbol overrides */
pid_t forkpty_(int *amaster, char *name,
               struct termios *termp, struct winsize *winp);
int   pidfd_open_(pid_t pid, unsigned int flags);
int   kill_(pid_t pid, int sig);
int   setpgid_(pid_t pid, pid_t pgid);
int   prctl_(int option, unsigned long arg2, unsigned long arg3,
             unsigned long arg4, unsigned long arg5);
int   posix_fcntl_(int fd, int cmd, int arg);
int   posix_close_(int fd);
int   posix_mkdir_(const char *pathname, mode_t mode);

pid_t forkpty_(int *amaster, char *name,
               struct termios *termp, struct winsize *winp)
{
    (void)name; (void)termp; (void)winp;
    if (amaster != NULL) *amaster = g_forkpty_master;
    return g_forkpty_return;
}

int pidfd_open_(pid_t pid, unsigned int flags)
{
    (void)pid; (void)flags;
    return g_pidfd_return;
}

int kill_(pid_t pid, int sig)
{
    g_kill_last_pid = pid;
    g_kill_last_sig = sig;
    return 0;
}

int setpgid_(pid_t pid, pid_t pgid)      { (void)pid; (void)pgid; return 0; }

int prctl_(int option, unsigned long a2, unsigned long a3,
           unsigned long a4, unsigned long a5)
{
    (void)option; (void)a2; (void)a3; (void)a4; (void)a5;
    return 0;
}

int posix_fcntl_(int fd, int cmd, int arg) { (void)fd; (void)cmd; (void)arg; return 0; }

int posix_close_(int fd)
{
    (void)fd;
    g_close_call_count++;
    return 0;
}

int posix_mkdir_(const char *pathname, mode_t mode)
{
    (void)mode;
    int r = mkdir(pathname, 0755);
    if (r < 0 && errno == EEXIST) return 0;
    return r;
}

/* Temp dir helpers */
static char g_tmpdir[256];

static void setup_tmpdir(void)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "/tmp/bg_process_test_XXXXXX"); // NOLINT
    if (mkdtemp(buf) == NULL) ck_abort_msg("mkdtemp failed");
    snprintf(g_tmpdir, sizeof(g_tmpdir), "%s", buf); // NOLINT
}

static void teardown_tmpdir(void)
{
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", g_tmpdir); // NOLINT
    system(cmd); /* NOLINT — test cleanup */
}

static res_t start_one(bg_manager_t *mgr, bg_process_t **out)
{
    return bg_process_start(mgr, NULL, g_tmpdir,
                            "echo hello", "test-label",
                            "agent-uuid-1", 60, out);
}

/* Manager lifecycle */
START_TEST(test_manager_create_destroy)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    bg_manager_t *mgr = bg_manager_create(ctx);
    ck_assert_ptr_nonnull(mgr);
    ck_assert_int_eq(mgr->count, 0);
    ck_assert_int_gt(mgr->capacity, 0);
    ck_assert_ptr_nonnull(mgr->processes);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_manager_active_count_empty)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    bg_manager_t *mgr = bg_manager_create(ctx);
    ck_assert_int_eq(bg_manager_active_count(mgr), 0);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_manager_destroy_closes_fds)
{
    reset_mocks(); setup_tmpdir();
    TALLOC_CTX *ctx = talloc_new(NULL);
    bg_manager_t *mgr = bg_manager_create(ctx);
    bg_process_t *proc = NULL;
    start_one(mgr, &proc);
    ck_assert_int_ge(proc->master_fd, 0);
    ck_assert_int_ge(proc->pidfd, 0);
    int before = g_close_call_count;
    talloc_free(ctx);
    ck_assert_int_gt(g_close_call_count, before);
    teardown_tmpdir();
}
END_TEST

/* Successful start */
START_TEST(test_start_success_running)
{
    reset_mocks(); setup_tmpdir();
    TALLOC_CTX *ctx = talloc_new(NULL);
    bg_manager_t *mgr = bg_manager_create(ctx);
    bg_process_t *proc = NULL;
    res_t r = start_one(mgr, &proc);
    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(proc);
    ck_assert_int_eq(proc->status, BG_STATUS_RUNNING);
    ck_assert_int_eq(proc->pid, 1234);
    ck_assert_int_eq(proc->master_fd, g_forkpty_master);
    ck_assert_int_eq(proc->pidfd, g_pidfd_return);
    ck_assert(proc->stdin_open);
    ck_assert_str_eq(proc->command, "echo hello");
    ck_assert_str_eq(proc->label, "test-label");
    ck_assert_str_eq(proc->agent_uuid, "agent-uuid-1");
    ck_assert_int_eq(proc->ttl_seconds, 60);
    ck_assert_int_eq(mgr->count, 1);
    ck_assert_int_eq(bg_manager_active_count(mgr), 1);
    talloc_free(ctx);
    teardown_tmpdir();
}
END_TEST

START_TEST(test_start_assigns_sequential_ids)
{
    reset_mocks(); setup_tmpdir();
    TALLOC_CTX *ctx = talloc_new(NULL);
    bg_manager_t *mgr = bg_manager_create(ctx);
    bg_process_t *p1 = NULL, *p2 = NULL;
    start_one(mgr, &p1);
    start_one(mgr, &p2);
    ck_assert_int_ne(p1->id, p2->id);
    ck_assert_int_eq(mgr->count, 2);
    talloc_free(ctx);
    teardown_tmpdir();
}
END_TEST

/* Output directory creation */
START_TEST(test_start_creates_output_directory)
{
    reset_mocks(); setup_tmpdir();
    TALLOC_CTX *ctx = talloc_new(NULL);
    bg_manager_t *mgr = bg_manager_create(ctx);
    bg_process_t *proc = NULL;
    res_t r = start_one(mgr, &proc);
    ck_assert(is_ok(&r));
    char dir_path[512];
    snprintf(dir_path, sizeof(dir_path), "%s/%" PRId32, g_tmpdir, proc->id); // NOLINT
    struct stat st;
    ck_assert_int_eq(stat(dir_path, &st), 0);
    ck_assert(S_ISDIR(st.st_mode));
    char out_path[520];
    snprintf(out_path, sizeof(out_path), "%s/output", dir_path); // NOLINT
    ck_assert_int_eq(stat(out_path, &st), 0);
    ck_assert(S_ISREG(st.st_mode));
    talloc_free(ctx);
    teardown_tmpdir();
}
END_TEST

/* Fork failure: STARTING→FAILED */
START_TEST(test_start_fork_failure_status_failed)
{
    reset_mocks(); setup_tmpdir();
    g_forkpty_return = -1;
    TALLOC_CTX *ctx = talloc_new(NULL);
    bg_manager_t *mgr = bg_manager_create(ctx);
    bg_process_t *proc = NULL;
    res_t r = start_one(mgr, &proc);
    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(proc);
    ck_assert_int_eq(proc->status, BG_STATUS_FAILED);
    ck_assert_int_eq(bg_manager_active_count(mgr), 0);
    ck_assert_int_eq(mgr->count, 1);
    talloc_free(ctx);
    teardown_tmpdir();
}
END_TEST

START_TEST(test_start_null_args_rejected)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    bg_manager_t *mgr = bg_manager_create(ctx);
    bg_process_t *proc = NULL;
    res_t r = bg_process_start(mgr, NULL, NULL, "echo", "lbl", "uuid", 10, &proc);
    ck_assert(is_err(&r));
    ck_assert_int_eq(r.err->code, ERR_INVALID_ARG);
    talloc_free(ctx);
}
END_TEST

/* Concurrency limit */
START_TEST(test_concurrency_limit_enforced)
{
    reset_mocks(); setup_tmpdir();
    TALLOC_CTX *ctx = talloc_new(NULL);
    bg_manager_t *mgr = bg_manager_create(ctx);
    for (int i = 0; i < BG_PROCESS_MAX_CONCURRENT; i++) {
        bg_process_t *proc = NULL;
        res_t r = start_one(mgr, &proc);
        ck_assert_msg(is_ok(&r), "Expected OK for process %d", i);
        ck_assert_int_eq(proc->status, BG_STATUS_RUNNING);
    }
    ck_assert_int_eq(bg_manager_active_count(mgr), BG_PROCESS_MAX_CONCURRENT);
    bg_process_t *over = NULL;
    res_t r = start_one(mgr, &over);
    ck_assert(is_err(&r));
    ck_assert_int_eq(r.err->code, ERR_IO);
    ck_assert_ptr_null(over);
    talloc_free(ctx);
    teardown_tmpdir();
}
END_TEST

START_TEST(test_terminal_processes_not_counted)
{
    reset_mocks(); setup_tmpdir();
    TALLOC_CTX *ctx = talloc_new(NULL);
    bg_manager_t *mgr = bg_manager_create(ctx);
    bg_process_t *proc = NULL;
    start_one(mgr, &proc);
    ck_assert_int_eq(bg_manager_active_count(mgr), 1);
    bg_process_kill(proc);
    ck_assert_int_eq(proc->status, BG_STATUS_KILLED);
    ck_assert_int_eq(bg_manager_active_count(mgr), 0);
    bg_process_t *proc2 = NULL;
    res_t r = start_one(mgr, &proc2);
    ck_assert(is_ok(&r));
    ck_assert_int_eq(proc2->status, BG_STATUS_RUNNING);
    ck_assert_int_eq(bg_manager_active_count(mgr), 1);
    talloc_free(ctx);
    teardown_tmpdir();
}
END_TEST

/* Kill transitions */
START_TEST(test_kill_running_succeeds)
{
    reset_mocks(); setup_tmpdir();
    TALLOC_CTX *ctx = talloc_new(NULL);
    bg_manager_t *mgr = bg_manager_create(ctx);
    bg_process_t *proc = NULL;
    start_one(mgr, &proc);
    res_t r = bg_process_kill(proc);
    ck_assert(is_ok(&r));
    ck_assert_int_eq(proc->status, BG_STATUS_KILLED);
    ck_assert_int_eq(g_kill_last_sig, SIGTERM);
    ck_assert_int_eq(g_kill_last_pid, -(proc->pid));
    talloc_free(ctx);
    teardown_tmpdir();
}
END_TEST

START_TEST(test_kill_failed_process_rejected)
{
    reset_mocks(); setup_tmpdir();
    g_forkpty_return = -1;
    TALLOC_CTX *ctx = talloc_new(NULL);
    bg_manager_t *mgr = bg_manager_create(ctx);
    bg_process_t *proc = NULL;
    start_one(mgr, &proc);
    ck_assert_int_eq(proc->status, BG_STATUS_FAILED);
    res_t r = bg_process_kill(proc);
    ck_assert(is_err(&r));
    ck_assert_int_eq(r.err->code, ERR_INVALID_ARG);
    ck_assert_int_eq(proc->status, BG_STATUS_FAILED);
    talloc_free(ctx);
    teardown_tmpdir();
}
END_TEST

START_TEST(test_kill_already_killed_rejected)
{
    reset_mocks(); setup_tmpdir();
    TALLOC_CTX *ctx = talloc_new(NULL);
    bg_manager_t *mgr = bg_manager_create(ctx);
    bg_process_t *proc = NULL;
    start_one(mgr, &proc);
    bg_process_kill(proc);
    res_t r = bg_process_kill(proc);
    ck_assert(is_err(&r));
    ck_assert_int_eq(r.err->code, ERR_INVALID_ARG);
    talloc_free(ctx);
    teardown_tmpdir();
}
END_TEST

/* State transitions */
START_TEST(test_transition_running_to_running_rejected)
{
    reset_mocks(); setup_tmpdir();
    TALLOC_CTX *ctx = talloc_new(NULL);
    bg_manager_t *mgr = bg_manager_create(ctx);
    bg_process_t *proc = NULL;
    start_one(mgr, &proc);
    res_t r = bg_process_apply_transition(proc, BG_STATUS_RUNNING);
    ck_assert(is_err(&r));
    ck_assert_int_eq(r.err->code, ERR_INVALID_ARG);
    talloc_free(ctx);
    teardown_tmpdir();
}
END_TEST

START_TEST(test_transition_exited_from_running)
{
    reset_mocks(); setup_tmpdir();
    TALLOC_CTX *ctx = talloc_new(NULL);
    bg_manager_t *mgr = bg_manager_create(ctx);
    bg_process_t *proc = NULL;
    start_one(mgr, &proc);
    res_t r = bg_process_apply_transition(proc, BG_STATUS_EXITED);
    ck_assert(is_ok(&r));
    ck_assert_int_eq(proc->status, BG_STATUS_EXITED);
    talloc_free(ctx);
    teardown_tmpdir();
}
END_TEST

START_TEST(test_transition_timed_out_from_running)
{
    reset_mocks(); setup_tmpdir();
    TALLOC_CTX *ctx = talloc_new(NULL);
    bg_manager_t *mgr = bg_manager_create(ctx);
    bg_process_t *proc = NULL;
    start_one(mgr, &proc);
    res_t r = bg_process_apply_transition(proc, BG_STATUS_TIMED_OUT);
    ck_assert(is_ok(&r));
    ck_assert_int_eq(proc->status, BG_STATUS_TIMED_OUT);
    talloc_free(ctx);
    teardown_tmpdir();
}
END_TEST

START_TEST(test_transition_to_starting_rejected)
{
    reset_mocks(); setup_tmpdir();
    TALLOC_CTX *ctx = talloc_new(NULL);
    bg_manager_t *mgr = bg_manager_create(ctx);
    bg_process_t *proc = NULL;
    start_one(mgr, &proc);
    res_t r = bg_process_apply_transition(proc, BG_STATUS_STARTING);
    ck_assert(is_err(&r));
    ck_assert_int_eq(r.err->code, ERR_INVALID_ARG);
    talloc_free(ctx);
    teardown_tmpdir();
}
END_TEST

static Suite *bg_process_suite(void)
{
    Suite *s = suite_create("bg_process");

    TCase *tc_manager = tcase_create("Manager");
    tcase_add_test(tc_manager, test_manager_create_destroy);
    tcase_add_test(tc_manager, test_manager_active_count_empty);
    tcase_add_test(tc_manager, test_manager_destroy_closes_fds);
    suite_add_tcase(s, tc_manager);

    TCase *tc_start = tcase_create("Start");
    tcase_add_test(tc_start, test_start_success_running);
    tcase_add_test(tc_start, test_start_assigns_sequential_ids);
    tcase_add_test(tc_start, test_start_creates_output_directory);
    tcase_add_test(tc_start, test_start_fork_failure_status_failed);
    tcase_add_test(tc_start, test_start_null_args_rejected);
    suite_add_tcase(s, tc_start);

    TCase *tc_limit = tcase_create("ConcurrencyLimit");
    tcase_add_test(tc_limit, test_concurrency_limit_enforced);
    tcase_add_test(tc_limit, test_terminal_processes_not_counted);
    suite_add_tcase(s, tc_limit);

    TCase *tc_kill = tcase_create("Kill");
    tcase_add_test(tc_kill, test_kill_running_succeeds);
    tcase_add_test(tc_kill, test_kill_failed_process_rejected);
    tcase_add_test(tc_kill, test_kill_already_killed_rejected);
    suite_add_tcase(s, tc_kill);

    TCase *tc_state = tcase_create("StateTransitions");
    tcase_add_test(tc_state, test_transition_running_to_running_rejected);
    tcase_add_test(tc_state, test_transition_exited_from_running);
    tcase_add_test(tc_state, test_transition_timed_out_from_running);
    tcase_add_test(tc_state, test_transition_to_starting_rejected);
    suite_add_tcase(s, tc_state);

    return s;
}

int32_t main(void)
{
    Suite   *s  = bg_process_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/bg_process_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
