/**
 * @file commands_bg_test.c
 * @brief Unit tests for /ps, /pinspect, /pkill, /pwrite, /pclose commands
 */

#include <check.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <talloc.h>
#include <time.h>

#include "apps/ikigai/agent.h"
#include "apps/ikigai/bg_line_index.h"
#include "apps/ikigai/bg_process.h"
#include "apps/ikigai/commands.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/scrollback.h"
#include "shared/error.h"
#include "tests/helpers/test_utils_helper.h"

/* Weak symbol mocks */
int kill_(pid_t p, int s);
int posix_close_(int fd);
ssize_t posix_write_(int fd, const void *buf, size_t n);

int kill_(pid_t p, int s)               { (void)p;(void)s; return 0; }
int posix_close_(int fd)                { (void)fd; return 0; }

static int g_write_count = 0;
ssize_t posix_write_(int fd, const void *b, size_t n) { (void)fd;(void)b; g_write_count++; return (ssize_t)n; }

/* Test fixtures */
static TALLOC_CTX *g_ctx;
static ik_agent_ctx_t *g_agent;
static ik_repl_ctx_t *g_repl;

static void setup(void)  { ik_test_set_log_dir(__FILE__); }

static void test_setup(void)
{
    g_ctx = talloc_new(NULL);
    res_t r = ik_test_create_agent(g_ctx, &g_agent);
    ck_assert(is_ok(&r));
    g_agent->bg_manager = bg_manager_create(g_agent);
    g_repl = talloc_zero(g_ctx, ik_repl_ctx_t);
    g_repl->current = g_agent;
    g_repl->shared  = g_agent->shared;
    g_write_count = 0;
}

static void test_teardown(void) { talloc_free(g_ctx); }

static const char *get_line(size_t idx)
{
    const char *t = NULL; size_t l = 0;
    res_t r = ik_scrollback_get_line_text(g_agent->scrollback, idx, &t, &l);
    if (is_err(&r)) { talloc_free(r.err); return NULL; }
    return t;
}

static bool sb_has(const char *needle)
{
    size_t n = ik_scrollback_get_line_count(g_agent->scrollback);
    for (size_t i = 0; i < n; i++) { const char *l = get_line(i); if (l && strstr(l, needle)) return true; }
    return false;
}

static bg_process_t *add_proc(int32_t id, bg_status_t st, const char *cmd, int32_t ttl)
{
    bg_process_t *p = talloc_zero(g_agent->bg_manager, bg_process_t);
    p->id=id; p->pid=(pid_t)(1000+id);
    p->command=talloc_strdup(p,cmd); p->label=talloc_strdup(p,"lbl");
    p->master_fd=-1; p->pidfd=-1; p->output_fd=-1;
    p->status=st; p->stdin_open=(st==BG_STATUS_RUNNING||st==BG_STATUS_STARTING);
    p->ttl_seconds=ttl; p->exit_code=0;
    p->total_bytes=(int64_t)(id*1024);
    clock_gettime(CLOCK_MONOTONIC,&p->started_at);
    p->line_index=bg_line_index_create(p);
    bg_manager_t *mgr = g_agent->bg_manager;
    if (mgr->count>=mgr->capacity) {
        int nc=mgr->capacity*2;
        mgr->processes=talloc_realloc(mgr,mgr->processes,bg_process_t*,(unsigned)nc);
        mgr->capacity=nc;
    }
    mgr->processes[mgr->count++]=p;
    return p;
}

static res_t run(const char *cmd)
{
    ik_scrollback_clear(g_agent->scrollback);
    return ik_cmd_dispatch(g_ctx, g_repl, cmd);
}

/* ================================================================ /ps ================================================================ */

START_TEST(test_ps_no_processes) {
    /* NULL manager */
    g_agent->bg_manager = NULL;
    res_t r = run("/ps"); ck_assert(is_ok(&r));
    ck_assert(sb_has("No background processes."));
    /* Empty manager */
    g_agent->bg_manager = bg_manager_create(g_agent);
    r = run("/ps"); ck_assert(is_ok(&r));
    ck_assert(sb_has("No background processes."));
}
END_TEST

START_TEST(test_ps_mixed_states) {
    add_proc(1, BG_STATUS_RUNNING,   "make -j8",  600);
    add_proc(2, BG_STATUS_EXITED,    "echo test",  60);
    add_proc(3, BG_STATUS_KILLED,    "sleep 100",  -1);
    res_t r = run("/ps"); ck_assert(is_ok(&r));
    /* Header columns */
    ck_assert(sb_has("ID")); ck_assert(sb_has("STATUS")); ck_assert(sb_has("COMMAND"));
    /* Commands appear */
    ck_assert(sb_has("make -j8")); ck_assert(sb_has("echo test")); ck_assert(sb_has("sleep 100"));
    /* Status values */
    ck_assert(sb_has("running")); ck_assert(sb_has("exited(0)")); ck_assert(sb_has("killed"));
    /* TTL: killed → em-dash, running with ttl>0 → time, running with ttl=-1 → forever */
    ck_assert(sb_has("\xe2\x80\x94"));   /* em-dash for terminal state */
}
END_TEST

START_TEST(test_ps_forever_ttl) {
    add_proc(1, BG_STATUS_RUNNING, "server", -1);
    res_t r = run("/ps"); ck_assert(is_ok(&r));
    ck_assert(sb_has("forever"));
}
END_TEST

/* ================================================================ /pinspect ================================================================ */

START_TEST(test_pinspect_bad_args) {
    /* No args */
    res_t r = run("/pinspect"); ck_assert(is_ok(&r)); ck_assert(sb_has("Usage:")); ck_assert(sb_has("/pinspect"));
    /* Non-numeric id */
    r = run("/pinspect abc"); ck_assert(is_ok(&r)); ck_assert(sb_has("Usage:"));
    /* Not found */
    r = run("/pinspect 99"); ck_assert(is_ok(&r)); ck_assert(sb_has("not found"));
    /* Unknown option */
    add_proc(1, BG_STATUS_RUNNING, "cmd", 60);
    r = run("/pinspect 1 --bogus"); ck_assert(is_ok(&r)); ck_assert(sb_has("Unknown option"));
    /* Bad --tail */
    r = run("/pinspect 1 --tail=0"); ck_assert(is_ok(&r)); ck_assert(sb_has("Invalid --tail"));
    /* Bad --lines */
    r = run("/pinspect 1 --lines=50-10"); ck_assert(is_ok(&r)); ck_assert(sb_has("Invalid --lines"));
}
END_TEST

START_TEST(test_pinspect_modes) {
    add_proc(2, BG_STATUS_EXITED,    "cargo test", 60);
    add_proc(3, BG_STATUS_RUNNING,   "watcher",    -1);
    add_proc(4, BG_STATUS_TIMED_OUT, "slow",       10);

    /* Default tail → header shows status */
    res_t r = run("/pinspect 2"); ck_assert(is_ok(&r)); ck_assert(sb_has("Process 2:")); ck_assert(sb_has("exited(0)"));
    /* --tail=100 */
    r = run("/pinspect 3 --tail=100"); ck_assert(is_ok(&r)); ck_assert(sb_has("Process 3:")); ck_assert(sb_has("running"));
    /* --lines=1-10 */
    r = run("/pinspect 2 --lines=1-10"); ck_assert(is_ok(&r)); ck_assert(sb_has("Process 2:"));
    /* --since-last */
    r = run("/pinspect 3 --since-last"); ck_assert(is_ok(&r)); ck_assert(sb_has("Process 3:"));
    /* --full */
    r = run("/pinspect 4 --full"); ck_assert(is_ok(&r)); ck_assert(sb_has("Process 4:")); ck_assert(sb_has("timed_out"));
}
END_TEST

/* ================================================================ /pkill ================================================================ */

START_TEST(test_pkill_args) {
    /* No args */
    res_t r = run("/pkill"); ck_assert(is_ok(&r)); ck_assert(sb_has("Usage:")); ck_assert(sb_has("/pkill"));
    /* Zero id → invalid */
    r = run("/pkill 0"); ck_assert(is_ok(&r)); ck_assert(sb_has("Usage:"));
    /* Not found */
    r = run("/pkill 5"); ck_assert(is_ok(&r)); ck_assert(sb_has("not found"));
    /* Wrong state (exited) */
    add_proc(1, BG_STATUS_EXITED, "done", 60);
    r = run("/pkill 1"); ck_assert(is_ok(&r)); ck_assert(sb_has("Cannot kill"));
    /* Success on running */
    add_proc(2, BG_STATUS_RUNNING, "server", 300);
    r = run("/pkill 2"); ck_assert(is_ok(&r)); ck_assert(sb_has("killed"));
}
END_TEST

/* ================================================================ /pwrite ================================================================ */

START_TEST(test_pwrite_args) {
    /* No args */
    res_t r = run("/pwrite"); ck_assert(is_ok(&r)); ck_assert(sb_has("Usage:")); ck_assert(sb_has("/pwrite"));
    /* Id only, no text */
    add_proc(1, BG_STATUS_RUNNING, "repl", 60);
    r = run("/pwrite 1"); ck_assert(is_ok(&r)); ck_assert(sb_has("Usage:"));
    /* Not found */
    r = run("/pwrite 99 hello"); ck_assert(is_ok(&r)); ck_assert(sb_has("not found"));
    /* Success (mocked write) */
    g_write_count = 0;
    r = run("/pwrite 1 import sys"); ck_assert(is_ok(&r)); ck_assert(sb_has("Written to process 1")); ck_assert_int_gt(g_write_count, 0);
    /* --raw: only 1 write call for 1-char text */
    g_write_count = 0;
    r = run("/pwrite 1 --raw x"); ck_assert(is_ok(&r)); ck_assert_int_eq(g_write_count, 1);
    /* --eof: stdin closed after write */
    bg_process_t *p = g_agent->bg_manager->processes[0]; /* proc id=1 */
    p->stdin_open = true; /* re-open since --raw doesn't close */
    r = run("/pwrite 1 --eof done"); ck_assert(is_ok(&r)); ck_assert(!p->stdin_open); ck_assert(sb_has("stdin closed"));
}
END_TEST

/* ================================================================ /pclose ================================================================ */

START_TEST(test_pclose_args) {
    /* No args */
    res_t r = run("/pclose"); ck_assert(is_ok(&r)); ck_assert(sb_has("Usage:")); ck_assert(sb_has("/pclose"));
    /* Not found */
    r = run("/pclose 42"); ck_assert(is_ok(&r)); ck_assert(sb_has("not found"));
    /* Success */
    add_proc(1, BG_STATUS_RUNNING, "prog", 60);
    r = run("/pclose 1"); ck_assert(is_ok(&r));
    ck_assert(!g_agent->bg_manager->processes[0]->stdin_open);
    ck_assert(sb_has("EOF"));
    /* Idempotent: already closed */
    r = run("/pclose 1"); ck_assert(is_ok(&r));
}
END_TEST

/* ================================================================ Suite ================================================================ */

static Suite *commands_bg_suite(void)
{
    Suite *s = suite_create("commands_bg");

    TCase *tc_ps = tcase_create("ps");
    tcase_add_unchecked_fixture(tc_ps, setup, NULL);
    tcase_add_checked_fixture(tc_ps, test_setup, test_teardown);
    tcase_add_test(tc_ps, test_ps_no_processes);
    tcase_add_test(tc_ps, test_ps_mixed_states);
    tcase_add_test(tc_ps, test_ps_forever_ttl);
    suite_add_tcase(s, tc_ps);

    TCase *tc_pi = tcase_create("pinspect");
    tcase_add_unchecked_fixture(tc_pi, setup, NULL);
    tcase_add_checked_fixture(tc_pi, test_setup, test_teardown);
    tcase_add_test(tc_pi, test_pinspect_bad_args);
    tcase_add_test(tc_pi, test_pinspect_modes);
    suite_add_tcase(s, tc_pi);

    TCase *tc_pk = tcase_create("pkill");
    tcase_add_unchecked_fixture(tc_pk, setup, NULL);
    tcase_add_checked_fixture(tc_pk, test_setup, test_teardown);
    tcase_add_test(tc_pk, test_pkill_args);
    suite_add_tcase(s, tc_pk);

    TCase *tc_pw = tcase_create("pwrite");
    tcase_add_unchecked_fixture(tc_pw, setup, NULL);
    tcase_add_checked_fixture(tc_pw, test_setup, test_teardown);
    tcase_add_test(tc_pw, test_pwrite_args);
    suite_add_tcase(s, tc_pw);

    TCase *tc_pc = tcase_create("pclose");
    tcase_add_unchecked_fixture(tc_pc, setup, NULL);
    tcase_add_checked_fixture(tc_pc, test_setup, test_teardown);
    tcase_add_test(tc_pc, test_pclose_args);
    suite_add_tcase(s, tc_pc);

    return s;
}

int32_t main(void)
{
    Suite *s = commands_bg_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/commands_bg_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    int nf = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
