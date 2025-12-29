// Error path tests for db/connection.c transaction functions using mocks
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

// Mock results - using pointer constants
static PGresult *mock_failed_result = (PGresult *)1;

// Override PQexec to return failure
PGresult *PQexec(PGconn *conn, const char *command)
{
    (void)conn;
    (void)command;
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

// Test: ik_db_begin handles query failure
START_TEST(test_db_begin_query_failure) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_db_ctx_t *db = create_mock_db_ctx(ctx);

    // Mock is already set to return error result
    res_t res = ik_db_begin(db);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    ck_assert(strstr(res.err->msg, "BEGIN failed") != NULL);

    talloc_free(ctx);
}
END_TEST
// Test: ik_db_commit handles query failure
START_TEST(test_db_commit_query_failure)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_db_ctx_t *db = create_mock_db_ctx(ctx);

    // Mock is already set to return error result
    res_t res = ik_db_commit(db);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    ck_assert(strstr(res.err->msg, "COMMIT failed") != NULL);

    talloc_free(ctx);
}

END_TEST
// Test: ik_db_rollback handles query failure
START_TEST(test_db_rollback_query_failure)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_db_ctx_t *db = create_mock_db_ctx(ctx);

    // Mock is already set to return error result
    res_t res = ik_db_rollback(db);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    ck_assert(strstr(res.err->msg, "ROLLBACK failed") != NULL);

    talloc_free(ctx);
}

END_TEST

// Suite configuration
static Suite *db_connection_errors_suite(void)
{
    Suite *s = suite_create("db_connection_errors");

    TCase *tc_errors = tcase_create("TransactionErrors");
    tcase_set_timeout(tc_errors, 30);
    tcase_add_test(tc_errors, test_db_begin_query_failure);
    tcase_add_test(tc_errors, test_db_commit_query_failure);
    tcase_add_test(tc_errors, test_db_rollback_query_failure);

    suite_add_tcase(s, tc_errors);
    return s;
}

int main(void)
{
    Suite *s = db_connection_errors_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
