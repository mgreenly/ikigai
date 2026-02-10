#include "tests/test_constants.h"
// Error path tests for db/mail.c using mocks
#include "apps/ikigai/db/mail.h"
#include "apps/ikigai/db/connection.h"
#include "shared/error.h"
#include "apps/ikigai/mail/msg.h"
#include "shared/wrapper.h"
#include "apps/ikigai/wrapper_postgres.h"

#include <check.h>
#include <libpq-fe.h>
#include <string.h>
#include <talloc.h>

// Mock connection for error tests (no real DB needed)
static ik_db_ctx_t *create_mock_db_ctx(TALLOC_CTX *ctx)
{
    ik_db_ctx_t *db = talloc_zero(ctx, ik_db_ctx_t);
    db->conn = (PGconn *)0xDEADBEEF; // Non-NULL placeholder
    return db;
}

// Mock results - using pointer constants
static PGresult *mock_failed_result = (PGresult *)1;
static PGresult *mock_delete_zero_rows = (PGresult *)3;

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
    if (res == mock_delete_zero_rows) {
        return PGRES_COMMAND_OK;
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

// Override PQgetvalue for mock results
char *PQgetvalue(const PGresult *res, int row_number, int column_number)
{
    (void)res;
    (void)row_number;
    (void)column_number;
    static char id_value[] = "42";
    return id_value;
}

// Override PQntuples for mock results
int PQntuples(const PGresult *res)
{
    (void)res;
    return 0;
}

// Override PQcmdTuples for delete zero rows case
char *PQcmdTuples(PGresult *res)
{
    if (res == mock_delete_zero_rows) {
        static char zero[] = "0";
        return zero;
    }
    static char one[] = "1";
    return one;
}

// Test: ik_db_mail_insert handles query failure
START_TEST(test_db_mail_insert_query_failure) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_db_ctx_t *db = create_mock_db_ctx(ctx);

    ik_mail_msg_t *msg = ik_mail_msg_create(ctx, "agent-1", "agent-2", "Test");

    res_t res = ik_db_mail_insert(db, 1, msg);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    ck_assert(strstr(res.err->msg, "Mail insert failed") != NULL);

    talloc_free(ctx);
}
END_TEST
// Test: ik_db_mail_inbox handles query failure
START_TEST(test_db_mail_inbox_query_failure) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_db_ctx_t *db = create_mock_db_ctx(ctx);

    ik_mail_msg_t **out = NULL;
    size_t count = 0;

    res_t res = ik_db_mail_inbox(db, ctx, 1, "agent-2", &out, &count);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    ck_assert(strstr(res.err->msg, "Mail inbox query failed") != NULL);

    talloc_free(ctx);
}

END_TEST

// Test: ik_db_mail_delete handles query failure
START_TEST(test_db_mail_delete_query_failure) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_db_ctx_t *db = create_mock_db_ctx(ctx);

    res_t res = ik_db_mail_delete(db, 1, "agent-2");

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    ck_assert(strstr(res.err->msg, "Mail delete failed") != NULL);

    talloc_free(ctx);
}

END_TEST
// Test: ik_db_mail_inbox_filtered handles query failure
START_TEST(test_db_mail_inbox_filtered_query_failure) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_db_ctx_t *db = create_mock_db_ctx(ctx);

    ik_mail_msg_t **out = NULL;
    size_t count = 0;

    res_t res = ik_db_mail_inbox_filtered(db, ctx, 1, "agent-2", "agent-1", &out, &count);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    ck_assert(strstr(res.err->msg, "Mail filtered inbox query failed") != NULL);

    talloc_free(ctx);
}

END_TEST

// Suite configuration
static Suite *db_mail_errors_suite(void)
{
    Suite *s = suite_create("db_mail_errors");

    TCase *tc_errors = tcase_create("Errors");
    tcase_set_timeout(tc_errors, IK_TEST_TIMEOUT);
    tcase_add_test(tc_errors, test_db_mail_insert_query_failure);
    tcase_add_test(tc_errors, test_db_mail_inbox_query_failure);
    tcase_add_test(tc_errors, test_db_mail_delete_query_failure);
    tcase_add_test(tc_errors, test_db_mail_inbox_filtered_query_failure);

    suite_add_tcase(s, tc_errors);
    return s;
}

int main(void)
{
    Suite *s = db_mail_errors_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/db/mail_errors_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
