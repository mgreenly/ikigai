// Error path tests for db/agent.c using mocks
#include "../../../src/db/agent.h"
#include "../../../src/db/agent_zero.h"
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
static PGresult *mock_success_result = (PGresult *)2;
static PGresult *mock_parse_fail_result = (PGresult *)3;

// Mock control flags
static bool mock_query_fail = false;
static bool mock_parse_fail = false;
static int mock_parse_fail_column = -1; // Column to fail parsing on (-1 = all, >=0 = specific column)
static int mock_getvalue_call_count = 0; // Track calls to PQgetvalue_

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
    if (mock_parse_fail) {
        return mock_parse_fail_result;
    }
    return mock_success_result;
}

// Override PQresultStatus
ExecStatusType PQresultStatus(const PGresult *res)
{
    if (res == mock_failed_result) {
        return PGRES_FATAL_ERROR;
    }
    if (res == mock_parse_fail_result) {
        return PGRES_TUPLES_OK; // Query succeeds but data is invalid
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

// Override PQntuples - return 1 for parse failure tests
int PQntuples(const PGresult *res)
{
    if (res == mock_parse_fail_result) {
        return 1; // Has data but it's invalid
    }
    return 0;
}

// Override PQgetvalue_ - return invalid data for parse failure tests
char *PQgetvalue_(const PGresult *res, int row_number, int column_number)
{
    (void)res;
    (void)row_number;

    mock_getvalue_call_count++;

    // If we're testing parse failures on a specific column
    if (mock_parse_fail && mock_parse_fail_column >= 0) {
        if (column_number == mock_parse_fail_column) {
            static char invalid[] = "not_a_number";
            return invalid;
        }
    }
    // If we're testing parse failures on all columns
    else if (mock_parse_fail) {
        static char invalid[] = "not_a_number";
        return invalid;
    }

    static char valid[] = "0";
    return valid;
}

// Override PQgetisnull
int PQgetisnull(const PGresult *res, int row_number, int column_number)
{
    (void)res;
    (void)row_number;
    (void)column_number;
    return 0;
}

// Test: ik_db_agent_mark_dead handles query failure (line 92)
START_TEST(test_agent_mark_dead_query_failure) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_db_ctx_t *db = create_mock_db_ctx(ctx);

    mock_query_fail = true;
    mock_parse_fail = false;

    res_t res = ik_db_agent_mark_dead(db, "test-uuid");

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    ck_assert(strstr(res.err->msg, "Failed to mark agent as dead") != NULL);

    talloc_free(ctx);
}
END_TEST
// Test: ik_db_agent_get handles query failure (line 129)
START_TEST(test_agent_get_query_failure)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_db_ctx_t *db = create_mock_db_ctx(ctx);

    mock_query_fail = true;
    mock_parse_fail = false;

    ik_db_agent_row_t *row = NULL;
    res_t res = ik_db_agent_get(db, ctx, "test-uuid", &row);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    ck_assert(strstr(res.err->msg, "Failed to get agent") != NULL);

    talloc_free(ctx);
}

END_TEST
// Test: ik_db_agent_get handles created_at parse failure (line 171)
START_TEST(test_agent_get_created_at_parse_failure)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_db_ctx_t *db = create_mock_db_ctx(ctx);

    mock_query_fail = false;
    mock_parse_fail = true;
    mock_parse_fail_column = 5; // created_at is column 5
    mock_getvalue_call_count = 0;

    ik_db_agent_row_t *row = NULL;
    res_t res = ik_db_agent_get(db, ctx, "test-uuid", &row);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_PARSE);
    ck_assert(strstr(res.err->msg, "Failed to parse created_at") != NULL);

    talloc_free(ctx);
}

END_TEST
// Test: ik_db_agent_get handles ended_at parse failure (line 177)
START_TEST(test_agent_get_ended_at_parse_failure)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_db_ctx_t *db = create_mock_db_ctx(ctx);

    mock_query_fail = false;
    mock_parse_fail = true;
    mock_parse_fail_column = 6; // ended_at is column 6
    mock_getvalue_call_count = 0;

    ik_db_agent_row_t *row = NULL;
    res_t res = ik_db_agent_get(db, ctx, "test-uuid", &row);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_PARSE);
    ck_assert(strstr(res.err->msg, "Failed to parse ended_at") != NULL);

    talloc_free(ctx);
}

END_TEST
// Test: ik_db_agent_list_running handles query failure (line 211)
START_TEST(test_agent_list_running_query_failure)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_db_ctx_t *db = create_mock_db_ctx(ctx);

    mock_query_fail = true;
    mock_parse_fail = false;

    ik_db_agent_row_t **rows = NULL;
    size_t count = 0;
    res_t res = ik_db_agent_list_running(db, ctx, &rows, &count);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    ck_assert(strstr(res.err->msg, "Failed to list running agents") != NULL);

    talloc_free(ctx);
}

END_TEST
// Test: ik_db_agent_list_running handles created_at parse failure (line 261)
START_TEST(test_agent_list_running_created_at_parse_failure)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_db_ctx_t *db = create_mock_db_ctx(ctx);

    mock_query_fail = false;
    mock_parse_fail = true;
    mock_parse_fail_column = 5; // created_at is column 5
    mock_getvalue_call_count = 0;

    ik_db_agent_row_t **rows = NULL;
    size_t count = 0;
    res_t res = ik_db_agent_list_running(db, ctx, &rows, &count);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_PARSE);
    ck_assert(strstr(res.err->msg, "Failed to parse created_at") != NULL);

    talloc_free(ctx);
}

END_TEST
// Test: ik_db_agent_list_running handles ended_at parse failure (line 267)
START_TEST(test_agent_list_running_ended_at_parse_failure)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_db_ctx_t *db = create_mock_db_ctx(ctx);

    mock_query_fail = false;
    mock_parse_fail = true;
    mock_parse_fail_column = 6; // ended_at is column 6
    mock_getvalue_call_count = 0;

    ik_db_agent_row_t **rows = NULL;
    size_t count = 0;
    res_t res = ik_db_agent_list_running(db, ctx, &rows, &count);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_PARSE);
    ck_assert(strstr(res.err->msg, "Failed to parse ended_at") != NULL);

    talloc_free(ctx);
}

END_TEST
// Test: ik_db_agent_get_children handles query failure (line 309)
START_TEST(test_agent_get_children_query_failure)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_db_ctx_t *db = create_mock_db_ctx(ctx);

    mock_query_fail = true;
    mock_parse_fail = false;

    ik_db_agent_row_t **rows = NULL;
    size_t count = 0;
    res_t res = ik_db_agent_get_children(db, ctx, "parent-uuid", &rows, &count);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    ck_assert(strstr(res.err->msg, "Failed to get children") != NULL);

    talloc_free(ctx);
}

END_TEST
// Test: ik_db_agent_get_children handles created_at parse failure (line 359)
START_TEST(test_agent_get_children_created_at_parse_failure)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_db_ctx_t *db = create_mock_db_ctx(ctx);

    mock_query_fail = false;
    mock_parse_fail = true;
    mock_parse_fail_column = 5; // created_at is column 5
    mock_getvalue_call_count = 0;

    ik_db_agent_row_t **rows = NULL;
    size_t count = 0;
    res_t res = ik_db_agent_get_children(db, ctx, "parent-uuid", &rows, &count);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_PARSE);
    ck_assert(strstr(res.err->msg, "Failed to parse created_at") != NULL);

    talloc_free(ctx);
}

END_TEST
// Test: ik_db_agent_get_children handles ended_at parse failure (line 365)
START_TEST(test_agent_get_children_ended_at_parse_failure)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_db_ctx_t *db = create_mock_db_ctx(ctx);

    mock_query_fail = false;
    mock_parse_fail = true;
    mock_parse_fail_column = 6; // ended_at is column 6
    mock_getvalue_call_count = 0;

    ik_db_agent_row_t **rows = NULL;
    size_t count = 0;
    res_t res = ik_db_agent_get_children(db, ctx, "parent-uuid", &rows, &count);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_PARSE);
    ck_assert(strstr(res.err->msg, "Failed to parse ended_at") != NULL);

    talloc_free(ctx);
}

END_TEST
// Test: ik_db_agent_get_parent handles query failure (line 407)
START_TEST(test_agent_get_parent_query_failure)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_db_ctx_t *db = create_mock_db_ctx(ctx);

    mock_query_fail = true;
    mock_parse_fail = false;

    ik_db_agent_row_t *row = NULL;
    res_t res = ik_db_agent_get_parent(db, ctx, "child-uuid", &row);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    ck_assert(strstr(res.err->msg, "Failed to get parent") != NULL);

    talloc_free(ctx);
}

END_TEST
// Test: ik_db_agent_get_parent handles created_at parse failure (line 451)
START_TEST(test_agent_get_parent_created_at_parse_failure)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_db_ctx_t *db = create_mock_db_ctx(ctx);

    mock_query_fail = false;
    mock_parse_fail = true;
    mock_parse_fail_column = 5; // created_at is column 5
    mock_getvalue_call_count = 0;

    ik_db_agent_row_t *row = NULL;
    res_t res = ik_db_agent_get_parent(db, ctx, "child-uuid", &row);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_PARSE);
    ck_assert(strstr(res.err->msg, "Failed to parse created_at") != NULL);

    talloc_free(ctx);
}

END_TEST
// Test: ik_db_agent_get_parent handles ended_at parse failure (line 457)
START_TEST(test_agent_get_parent_ended_at_parse_failure)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_db_ctx_t *db = create_mock_db_ctx(ctx);

    mock_query_fail = false;
    mock_parse_fail = true;
    mock_parse_fail_column = 6; // ended_at is column 6
    mock_getvalue_call_count = 0;

    ik_db_agent_row_t *row = NULL;
    res_t res = ik_db_agent_get_parent(db, ctx, "child-uuid", &row);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_PARSE);
    ck_assert(strstr(res.err->msg, "Failed to parse ended_at") != NULL);

    talloc_free(ctx);
}

END_TEST
// Test: ik_db_ensure_agent_zero handles root query failure (line 483)
START_TEST(test_ensure_agent_zero_root_query_failure)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_db_ctx_t *db = create_mock_db_ctx(ctx);

    mock_query_fail = true;
    mock_parse_fail = false;

    char *uuid = NULL;
    res_t res = ik_db_ensure_agent_zero(db, &uuid);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    ck_assert(strstr(res.err->msg, "Failed to query for root agent") != NULL);

    talloc_free(ctx);
}

END_TEST
// Test: ik_db_agent_get_last_message_id handles query failure (line 605)
START_TEST(test_agent_get_last_message_id_query_failure)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_db_ctx_t *db = create_mock_db_ctx(ctx);

    mock_query_fail = true;
    mock_parse_fail = false;

    int64_t message_id = 0;
    res_t res = ik_db_agent_get_last_message_id(db, "test-uuid", &message_id);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);
    ck_assert(strstr(res.err->msg, "Failed to get last message ID") != NULL);

    talloc_free(ctx);
}

END_TEST
// Test: ik_db_agent_get_last_message_id handles parse failure (line 613)
START_TEST(test_agent_get_last_message_id_parse_failure)
{
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_db_ctx_t *db = create_mock_db_ctx(ctx);

    mock_query_fail = false;
    mock_parse_fail = true;

    int64_t message_id = 0;
    res_t res = ik_db_agent_get_last_message_id(db, "test-uuid", &message_id);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_PARSE);
    ck_assert(strstr(res.err->msg, "Failed to parse message ID") != NULL);

    talloc_free(ctx);
}

END_TEST

// Setup function to reset mock state before each test
static void setup(void)
{
    mock_query_fail = false;
    mock_parse_fail = false;
    mock_parse_fail_column = -1;
    mock_getvalue_call_count = 0;
}

// Suite configuration
static Suite *db_agent_errors_suite(void)
{
    Suite *s = suite_create("db_agent_errors");

    TCase *tc_errors = tcase_create("Errors");
    tcase_add_checked_fixture(tc_errors, setup, NULL);
    tcase_add_test(tc_errors, test_agent_mark_dead_query_failure);
    tcase_add_test(tc_errors, test_agent_get_query_failure);
    tcase_add_test(tc_errors, test_agent_get_created_at_parse_failure);
    tcase_add_test(tc_errors, test_agent_get_ended_at_parse_failure);
    tcase_add_test(tc_errors, test_agent_list_running_query_failure);
    tcase_add_test(tc_errors, test_agent_list_running_created_at_parse_failure);
    tcase_add_test(tc_errors, test_agent_list_running_ended_at_parse_failure);
    tcase_add_test(tc_errors, test_agent_get_children_query_failure);
    tcase_add_test(tc_errors, test_agent_get_children_created_at_parse_failure);
    tcase_add_test(tc_errors, test_agent_get_children_ended_at_parse_failure);
    tcase_add_test(tc_errors, test_agent_get_parent_query_failure);
    tcase_add_test(tc_errors, test_agent_get_parent_created_at_parse_failure);
    tcase_add_test(tc_errors, test_agent_get_parent_ended_at_parse_failure);
    tcase_add_test(tc_errors, test_ensure_agent_zero_root_query_failure);
    tcase_add_test(tc_errors, test_agent_get_last_message_id_query_failure);
    tcase_add_test(tc_errors, test_agent_get_last_message_id_parse_failure);

    suite_add_tcase(s, tc_errors);
    return s;
}

int main(void)
{
    Suite *s = db_agent_errors_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
