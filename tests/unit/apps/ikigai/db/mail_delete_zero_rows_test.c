#include "tests/test_constants.h"
// Test for db/mail.c delete zero rows case
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

// Mock results
static PGresult *mock_delete_zero_rows = (PGresult *)3;

// Override pq_exec_params_ to return success result
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
    return mock_delete_zero_rows;
}

// Override PQresultStatus to return success
ExecStatusType PQresultStatus(const PGresult *res)
{
    (void)res;
    return PGRES_COMMAND_OK;
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

// Override PQcmdTuples to return zero
char *PQcmdTuples(PGresult *res)
{
    (void)res;
    static char zero[] = "0";
    return zero;
}

// Test: ik_db_mail_delete handles zero rows affected
START_TEST(test_db_mail_delete_zero_rows_affected) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_db_ctx_t *db = create_mock_db_ctx(ctx);

    res_t res = ik_db_mail_delete(db, 1, "agent-2");

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    ck_assert(strstr(res.err->msg, "Mail not found or not yours") != NULL);

    talloc_free(ctx);
}
END_TEST

// Suite configuration
static Suite *db_mail_delete_zero_rows_suite(void)
{
    Suite *s = suite_create("db_mail_delete_zero_rows");

    TCase *tc = tcase_create("ZeroRows");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_test(tc, test_db_mail_delete_zero_rows_affected);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    Suite *s = db_mail_delete_zero_rows_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/db/mail_delete_zero_rows_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
