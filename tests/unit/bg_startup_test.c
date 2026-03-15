/* bg_startup_test.c — startup recovery scan for orphaned background processes */
#include <check.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <talloc.h>

#include "apps/ikigai/bg_startup.h"
#include "apps/ikigai/db/connection.h"
#include "apps/ikigai/wrapper_postgres.h"
#include "shared/error.h"

/* ================================================================
 * Mock state
 * ================================================================ */

/* Sentinel PGresult pointers — never dereferenced by real libpq code */
static PGresult *const SELECT_RESULT = (PGresult *)0x100;
static PGresult *const UPDATE_RESULT = (PGresult *)0x200;

/* SELECT mock rows */
typedef struct { char id_str[32]; char pid_str[32]; } mock_row_t;
static mock_row_t g_mock_rows[8];
static int        g_mock_nrows = 0;

/* Kill tracking */
typedef struct { pid_t pid; int sig; } kill_call_t;
static kill_call_t g_kills[16];
static int         g_kill_count = 0;
static pid_t       g_alive_pid  = -1; /* pid that kill(pid,0) returns 0 for */

/* UPDATE capture */
static int32_t g_killed_id = -1;
static int32_t g_exited_id = -1;

static void reset_mocks(void)
{
    memset(g_mock_rows, 0, sizeof(g_mock_rows));
    g_mock_nrows = 0;
    memset(g_kills, 0, sizeof(g_kills));
    g_kill_count = 0;
    g_alive_pid  = -1;
    g_killed_id  = -1;
    g_exited_id  = -1;
}

/* ================================================================
 * Weak symbol overrides
 * ================================================================ */

PGresult *pq_exec_(PGconn *conn, const char *command);
PGresult *pq_exec_params_(PGconn *conn,
                          const char *command,
                          int nParams,
                          const Oid *paramTypes,
                          const char *const *paramValues,
                          const int *paramLengths,
                          const int *paramFormats,
                          int resultFormat);
ExecStatusType PQresultStatus_(const PGresult *res);
int            PQntuples_(const PGresult *res);
char          *PQgetvalue_(const PGresult *res, int row_number, int column_number);
int            kill_(pid_t pid, int sig);

/* SELECT: return our sentinel */
PGresult *pq_exec_(PGconn *conn, const char *command)
{
    (void)conn;
    (void)command;
    return SELECT_RESULT;
}

/* UPDATE: capture id and which kind of update it was */
PGresult *pq_exec_params_(PGconn *conn,
                          const char *command,
                          int nParams,
                          const Oid *paramTypes,
                          const char *const *paramValues,
                          const int *paramLengths,
                          const int *paramFormats,
                          int resultFormat)
{
    (void)conn;
    (void)nParams;
    (void)paramTypes;
    (void)paramLengths;
    (void)paramFormats;
    (void)resultFormat;

    if (paramValues != NULL && paramValues[0] != NULL) {
        int32_t id = (int32_t)atoi(paramValues[0]);
        if (strstr(command, "'killed'") != NULL) {
            g_killed_id = id;
        } else if (strstr(command, "'exited'") != NULL) {
            g_exited_id = id;
        }
    }
    return UPDATE_RESULT;
}

ExecStatusType PQresultStatus_(const PGresult *res)
{
    if (res == SELECT_RESULT) return PGRES_TUPLES_OK;
    if (res == UPDATE_RESULT) return PGRES_COMMAND_OK;
    return PGRES_FATAL_ERROR;
}

int PQntuples_(const PGresult *res)
{
    (void)res;
    return g_mock_nrows;
}

char *PQgetvalue_(const PGresult *res, int row_number, int column_number)
{
    static char empty[] = "";
    (void)res;
    if (row_number < 0 || row_number >= g_mock_nrows) return empty;
    if (column_number == 0) return g_mock_rows[row_number].id_str;
    if (column_number == 1) return g_mock_rows[row_number].pid_str;
    return empty;
}

int kill_(pid_t pid, int sig)
{
    if (sig == 0) {
        /* Liveness probe */
        return (pid == g_alive_pid) ? 0 : -1;
    }
    if (g_kill_count < 16) {
        g_kills[g_kill_count].pid = pid;
        g_kills[g_kill_count].sig = sig;
        g_kill_count++;
    }
    return 0;
}

/* PQclear — no-op to prevent crash on sentinel pointers */
void PQclear(PGresult *res) { (void)res; }

/* PQerrorMessage — static string */
char *PQerrorMessage(const PGconn *conn)
{
    (void)conn;
    static char msg[] = "mock error";
    return msg;
}

/* ================================================================
 * Tests
 * ================================================================ */

START_TEST(test_null_db_is_noop)
{
    reset_mocks();
    bg_startup_recover(NULL); /* must not crash or do anything */
    ck_assert_int_eq(g_kill_count, 0);
    ck_assert_int_eq(g_killed_id, -1);
    ck_assert_int_eq(g_exited_id, -1);
}
END_TEST

START_TEST(test_zero_rows_no_action)
{
    reset_mocks();
    g_mock_nrows = 0;

    TALLOC_CTX     *ctx = talloc_new(NULL);
    ik_db_ctx_t    *db  = talloc_zero(ctx, ik_db_ctx_t);
    db->conn = NULL;

    bg_startup_recover(db);

    ck_assert_int_eq(g_kill_count, 0);
    ck_assert_int_eq(g_killed_id, -1);
    ck_assert_int_eq(g_exited_id, -1);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_dead_process_marks_exited)
{
    reset_mocks();
    g_mock_nrows = 1;
    snprintf(g_mock_rows[0].id_str,  sizeof(g_mock_rows[0].id_str),  "42");   /* NOLINT */
    snprintf(g_mock_rows[0].pid_str, sizeof(g_mock_rows[0].pid_str), "9999"); /* NOLINT */
    g_alive_pid  = -1; /* pid 9999 is dead */

    TALLOC_CTX     *ctx = talloc_new(NULL);
    ik_db_ctx_t    *db  = talloc_zero(ctx, ik_db_ctx_t);

    bg_startup_recover(db);

    /* No SIGKILL should have been sent */
    ck_assert_int_eq(g_kill_count, 0);
    /* Process was dead — marked exited */
    ck_assert_int_eq(g_exited_id, 42);
    ck_assert_int_eq(g_killed_id, -1);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_alive_process_kills_and_marks_killed)
{
    reset_mocks();
    g_mock_nrows = 1;
    snprintf(g_mock_rows[0].id_str,  sizeof(g_mock_rows[0].id_str),  "7");    /* NOLINT */
    snprintf(g_mock_rows[0].pid_str, sizeof(g_mock_rows[0].pid_str), "1234"); /* NOLINT */
    g_alive_pid  = 1234; /* pid 1234 is alive */

    TALLOC_CTX     *ctx = talloc_new(NULL);
    ik_db_ctx_t    *db  = talloc_zero(ctx, ik_db_ctx_t);

    bg_startup_recover(db);

    /* SIGKILL to the process group (-1234) */
    ck_assert_int_eq(g_kill_count, 1);
    ck_assert_int_eq(g_kills[0].pid, -1234);
    ck_assert_int_eq(g_kills[0].sig, SIGKILL);
    /* Process was alive — marked killed */
    ck_assert_int_eq(g_killed_id, 7);
    ck_assert_int_eq(g_exited_id, -1);
    talloc_free(ctx);
}
END_TEST

START_TEST(test_mixed_rows_each_handled_correctly)
{
    reset_mocks();
    g_mock_nrows = 2;
    snprintf(g_mock_rows[0].id_str,  sizeof(g_mock_rows[0].id_str),  "10");   /* NOLINT */
    snprintf(g_mock_rows[0].pid_str, sizeof(g_mock_rows[0].pid_str), "5555"); /* NOLINT */
    snprintf(g_mock_rows[1].id_str,  sizeof(g_mock_rows[1].id_str),  "20");   /* NOLINT */
    snprintf(g_mock_rows[1].pid_str, sizeof(g_mock_rows[1].pid_str), "6666"); /* NOLINT */
    g_alive_pid = 5555;

    TALLOC_CTX     *ctx = talloc_new(NULL);
    ik_db_ctx_t    *db  = talloc_zero(ctx, ik_db_ctx_t);

    bg_startup_recover(db);

    /* Alive row → SIGKILL + marked killed */
    ck_assert_int_eq(g_kill_count, 1);
    ck_assert_int_eq(g_kills[0].pid, -5555);
    ck_assert_int_eq(g_kills[0].sig, SIGKILL);
    ck_assert_int_eq(g_killed_id, 10);
    /* Dead row → marked exited */
    ck_assert_int_eq(g_exited_id, 20);
    talloc_free(ctx);
}
END_TEST

/* ================================================================
 * Suite
 * ================================================================ */

static Suite *bg_startup_suite(void)
{
    Suite *s = suite_create("bg_startup");

    TCase *tc = tcase_create("Recover");
    tcase_add_test(tc, test_null_db_is_noop);
    tcase_add_test(tc, test_zero_rows_no_action);
    tcase_add_test(tc, test_dead_process_marks_exited);
    tcase_add_test(tc, test_alive_process_kills_and_marks_killed);
    tcase_add_test(tc, test_mixed_rows_each_handled_correctly);
    suite_add_tcase(s, tc);

    return s;
}

int32_t main(void)
{
    Suite   *s  = bg_startup_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/bg_startup_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
