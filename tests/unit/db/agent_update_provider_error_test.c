#include "../../test_constants.h"
// Error path test for ik_db_agent_update_provider (line 408)
#include "../../../src/db/agent.h"
#include "../../../src/db/connection.h"
#include "../../../src/error.h"
#include "../../../src/wrapper.h"

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

// Mock control flag
static bool mock_query_fail = false;

// Override pq_exec_params_ to control failures
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

    if (mock_query_fail) {
        return mock_failed_result;
    }
    return (PGresult *)2; // success
}

// Override PQresultStatus
ExecStatusType PQresultStatus(const PGresult *res)
{
    if (res == mock_failed_result) {
        return PGRES_FATAL_ERROR;
    }
    return PGRES_COMMAND_OK;
}

// Override PQerrorMessage
char *PQerrorMessage(const PGconn *conn)
{
    (void)conn;
    static char error_msg[] = "Mock database error";
    return error_msg;
}

// Override PQclear
void PQclear(PGresult *res)
{
    (void)res;
}

// Test: ik_db_agent_update_provider handles query failure (line 408)
START_TEST(test_agent_update_provider_query_failure) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_db_ctx_t *db = create_mock_db_ctx(ctx);

    mock_query_fail = true;

    res_t res = ik_db_agent_update_provider(db, "test-uuid", "provider", "model", "low");

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    ck_assert(strstr(res.err->msg, "Failed to update agent provider") != NULL);

    talloc_free(ctx);
}
END_TEST

// Setup function to reset mock state before each test
static void setup(void)
{
    mock_query_fail = false;
}

// Suite configuration
static Suite *db_agent_update_provider_errors_suite(void)
{
    Suite *s = suite_create("db_agent_update_provider_errors");

    TCase *tc_errors = tcase_create("Errors");
    tcase_set_timeout(tc_errors, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_errors, setup, NULL);
    tcase_add_test(tc_errors, test_agent_update_provider_query_failure);

    suite_add_tcase(s, tc_errors);
    return s;
}

int main(void)
{
    Suite *s = db_agent_update_provider_errors_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/db/agent_update_provider_error_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
