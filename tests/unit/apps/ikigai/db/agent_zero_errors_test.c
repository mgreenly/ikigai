/**
 * @file agent_zero_errors_test.c
 * @brief Error path tests for db/agent_zero.c using mocks
 */

#include "apps/ikigai/db/agent_zero.h"
#include "apps/ikigai/db/connection.h"
#include "shared/error.h"
#include "apps/ikigai/paths.h"
#include "shared/wrapper.h"
#include "apps/ikigai/wrapper_postgres.h"
#include "tests/helpers/test_utils_helper.h"

#include <check.h>
#include <libpq-fe.h>
#include <string.h>
#include <talloc.h>

static TALLOC_CTX *test_ctx;
static ik_db_ctx_t *mock_db;
static ik_paths_t *paths;

// Mock result for failed query
static PGresult *mock_failed_result = (PGresult *)1;

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

static void setup(void)
{
    test_ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(test_ctx);

    test_paths_setup_env();
    res_t paths_res = ik_paths_init(test_ctx, &paths);
    ck_assert(is_ok(&paths_res));
    ck_assert_ptr_nonnull(paths);

    mock_db = talloc_zero(test_ctx, ik_db_ctx_t);
    ck_assert_ptr_nonnull(mock_db);
    mock_db->conn = (PGconn *)0xDEADBEEF;
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

START_TEST(test_ensure_agent_zero_query_error) {
    char *uuid = NULL;
    res_t res = ik_db_ensure_agent_zero(mock_db, paths, &uuid);

    ck_assert(is_err(&res));
    ck_assert_ptr_nonnull(res.err);
    ck_assert(strstr(res.err->msg, "Failed to query for root agent") != NULL);
    ck_assert(strstr(res.err->msg, "Mock database error") != NULL);

    talloc_free(res.err);
}

END_TEST

static Suite *agent_zero_errors_suite(void)
{
    Suite *s = suite_create("Agent 0 Errors");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_core, setup, teardown);

    tcase_add_test(tc_core, test_ensure_agent_zero_query_error);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    Suite *s = agent_zero_errors_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/db/agent_zero_errors_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
