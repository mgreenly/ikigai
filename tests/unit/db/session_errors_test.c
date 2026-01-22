#include "../../test_constants.h"
// Error path tests for db/session.c using mocks
#include "../../../src/db/session.h"
#include "../../../src/db/connection.h"
#include "../../../src/error.h"
#include "../../../src/wrapper.h"
#include <check.h>
#include <talloc.h>
#include <libpq-fe.h>
#include <string.h>

// Mock connection for error tests (no real DB needed)
static ik_db_ctx_t *create_mock_db_ctx(TALLOC_CTX *ctx)
{
    ik_db_ctx_t *db = talloc_zero(ctx, ik_db_ctx_t);
    // conn will be NULL but not used in error paths we're testing
    db->conn = (PGconn *)0xDEADBEEF; // Non-NULL placeholder
    return db;
}

// Mock results - using pointer constants like mark_errors_test.c
static PGresult *mock_failed_result = (PGresult *)1;

// Override pq_exec_ to return failure
PGresult *pq_exec_(PGconn *conn, const char *command)
{
    (void)conn;
    (void)command;
    return mock_failed_result;
}

// Override pq_exec_params_ to return failure
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
    return mock_failed_result;
}

// Override PQresultStatus to return error status
ExecStatusType PQresultStatus(const PGresult *res)
{
    if (res == mock_failed_result) {
        return PGRES_FATAL_ERROR;
    }
    return PGRES_TUPLES_OK;
}

// Override PQerrorMessage to return test error
char *PQerrorMessage(const PGconn *conn)
{
    (void)conn;
    static char error_msg[] = "Mock database error";
    return error_msg;
}

// Override PQclear to do nothing (our mock result is static)
void PQclear(PGresult *res)
{
    (void)res;
}

// Test: ik_db_session_create handles query failure
START_TEST(test_db_session_create_query_failure) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_db_ctx_t *db = create_mock_db_ctx(ctx);

    // Mock is already set to return error result
    int64_t session_id = 0;
    res_t res = ik_db_session_create(db, &session_id);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    ck_assert(strstr(res.err->msg, "Failed to create session") != NULL);

    talloc_free(ctx);
}
END_TEST
// Test: ik_db_session_get_active handles query failure
START_TEST(test_db_session_get_active_query_failure) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_db_ctx_t *db = create_mock_db_ctx(ctx);

    // Mock is already set to return error result
    int64_t session_id = 0;
    res_t res = ik_db_session_get_active(db, &session_id);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    ck_assert(strstr(res.err->msg, "Failed to get active session") != NULL);

    talloc_free(ctx);
}

END_TEST

// Suite configuration
static Suite *db_session_errors_suite(void)
{
    Suite *s = suite_create("db_session_errors");

    TCase *tc_errors = tcase_create("Errors");
    tcase_set_timeout(tc_errors, IK_TEST_TIMEOUT);
    tcase_add_test(tc_errors, test_db_session_create_query_failure);
    tcase_add_test(tc_errors, test_db_session_get_active_query_failure);

    suite_add_tcase(s, tc_errors);
    return s;
}

int main(void)
{
    Suite *s = db_session_errors_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/db/session_errors_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
