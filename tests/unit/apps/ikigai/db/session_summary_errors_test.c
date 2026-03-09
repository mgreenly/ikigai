#include "tests/test_constants.h"
// Error path tests for db/session_summary.c using mocks
#include "apps/ikigai/db/session_summary.h"
#include "apps/ikigai/db/connection.h"
#include "shared/error.h"
#include "shared/wrapper.h"
#include "apps/ikigai/wrapper_postgres.h"
#include <check.h>
#include <talloc.h>
#include <libpq-fe.h>
#include <string.h>

// Mock result sentinels
static PGresult *mock_ok_result   = (PGresult *)2;
static PGresult *mock_fail_result = (PGresult *)1;

// Controls: which call number should fail (1-based; 0 = all succeed)
static int mock_call_count        = 0;
static int mock_fail_on_call      = 0;

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
    (void)command;
    (void)nParams;
    (void)paramTypes;
    (void)paramValues;
    (void)paramLengths;
    (void)paramFormats;
    (void)resultFormat;

    mock_call_count++;
    if (mock_fail_on_call != 0 && mock_call_count == mock_fail_on_call) {
        return mock_fail_result;
    }
    return mock_ok_result;
}

ExecStatusType PQresultStatus(const PGresult *res)
{
    if (res == mock_fail_result) {
        return PGRES_FATAL_ERROR;
    }
    // For INSERT/DELETE: PGRES_COMMAND_OK; for SELECT: PGRES_TUPLES_OK
    // The session_summary functions check PGRES_COMMAND_OK for insert/delete
    // and PGRES_TUPLES_OK for select.  Since both == ok_result, the caller
    // decides per function which status means success.  We return COMMAND_OK
    // as the default; the load test will need TUPLES_OK — handled by using
    // a separate sentinel.
    return PGRES_COMMAND_OK;
}

char *PQerrorMessage(const PGconn *conn)
{
    (void)conn;
    static char msg[] = "Mock DB error";
    return msg;
}

void PQclear(PGresult *res)
{
    (void)res;
}

int PQntuples(const PGresult *res)
{
    (void)res;
    return 0;
}

// ----------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------

static ik_db_ctx_t *make_db(TALLOC_CTX *ctx)
{
    ik_db_ctx_t *db = talloc_zero(ctx, ik_db_ctx_t);
    db->conn = (PGconn *)0xDEADBEEF;
    return db;
}

static void reset_mocks(void)
{
    mock_call_count   = 0;
    mock_fail_on_call = 0;
}

// ----------------------------------------------------------------
// Fixture
// ----------------------------------------------------------------

static TALLOC_CTX *ctx;

static void setup(void)
{
    ctx = talloc_new(NULL);
    reset_mocks();
}

static void teardown(void)
{
    talloc_free(ctx);
}

// ----------------------------------------------------------------
// ik_db_session_summary_insert — INSERT failure (first DB call)
// ----------------------------------------------------------------
START_TEST(test_insert_db_failure)
{
    ik_db_ctx_t *db = make_db(ctx);
    mock_fail_on_call = 1; // First call (INSERT) fails

    res_t r = ik_db_session_summary_insert(db, "uuid-1", "summary text",
                                           1, 5, 100);
    ck_assert(!is_ok(&r));
}
END_TEST

// ----------------------------------------------------------------
// ik_db_session_summary_insert — cap DELETE failure (second DB call)
// ----------------------------------------------------------------
START_TEST(test_cap_enforcement_failure)
{
    ik_db_ctx_t *db = make_db(ctx);
    mock_fail_on_call = 2; // Second call (DELETE cap) fails

    res_t r = ik_db_session_summary_insert(db, "uuid-1", "summary text",
                                           1, 5, 100);
    ck_assert(!is_ok(&r));
}
END_TEST

// ----------------------------------------------------------------
// ik_db_session_summary_load — SELECT failure
// Uses a separate mock override via a compile-time trick: we swap
// PQresultStatus to return PGRES_FATAL_ERROR for the load test
// by failing the only call made in load (call 1).
// ----------------------------------------------------------------
START_TEST(test_load_db_failure)
{
    ik_db_ctx_t *db = make_db(ctx);
    mock_fail_on_call = 1; // Only call (SELECT) fails

    ik_session_summary_t **out = NULL;
    size_t count = 0;
    res_t r = ik_db_session_summary_load(db, ctx, "uuid-1", &out, &count);
    ck_assert(!is_ok(&r));
}
END_TEST

// ----------------------------------------------------------------
// Suite
// ----------------------------------------------------------------

static Suite *session_summary_errors_suite(void)
{
    Suite *s = suite_create("db_session_summary_errors");

    TCase *tc = tcase_create("error_paths");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_insert_db_failure);
    tcase_add_test(tc, test_cap_enforcement_failure);
    tcase_add_test(tc, test_load_db_failure);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    Suite   *s  = session_summary_errors_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr,
        "reports/check/unit/apps/ikigai/db/session_summary_errors_test.xml");
    srunner_run_all(sr, CK_NORMAL);
    int32_t nfail = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (nfail == 0) ? 0 : 1;
}
