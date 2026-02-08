/**
 * @file notify_errors_test.c
 * @brief Error path tests for db/notify.c using mocks
 *
 * Tests error handling branches:
 * - ik_db_listen when PQresultStatus_ returns error
 * - ik_db_unlisten when PQresultStatus_ returns error
 * - ik_db_notify when PQresultStatus_ returns error
 * - ik_db_consume_notifications when PQconsumeInput_ fails
 */

#include "tests/test_constants.h"
#include "apps/ikigai/db/notify.h"
#include "apps/ikigai/db/connection.h"
#include "shared/error.h"
#include "apps/ikigai/wrapper_postgres.h"
#include <check.h>
#include <talloc.h>
#include <libpq-fe.h>
#include <string.h>

// Mock connection for error tests (no real DB needed)
static ik_db_ctx_t *create_mock_db_ctx(TALLOC_CTX *ctx)
{
    ik_db_ctx_t *mock_db = talloc_zero(ctx, ik_db_ctx_t);
    mock_db->conn = (PGconn *)(uintptr_t)0xDEADBEEF; // Non-NULL placeholder
    return mock_db;
}

// Mock result pointer
static PGresult *mock_failed_result = (PGresult *)1;

// Control which mock behavior to use
static bool mock_consume_input_fail = false;

// Override pq_exec_ to return a mock result
PGresult *pq_exec_(PGconn *conn, const char *command)
{
    (void)conn;
    (void)command;
    return mock_failed_result;
}

// Override PQresultStatus_ to return error
ExecStatusType PQresultStatus_(const PGresult *res)
{
    (void)res;
    return PGRES_FATAL_ERROR;
}

// Override PQresultErrorMessage to return test error
char *PQresultErrorMessage(const PGresult *res)
{
    (void)res;
    static char error_msg[] = "Mock error";
    return error_msg;
}

// Override PQerrorMessage to return test error
char *PQerrorMessage(const PGconn *conn)
{
    (void)conn;
    static char error_msg[] = "Mock connection error";
    return error_msg;
}

// Override PQclear to do nothing (our mock result is static)
void PQclear(PGresult *res)
{
    (void)res;
}

// Override PQconsumeInput_ to simulate failure
int PQconsumeInput_(PGconn *conn)
{
    (void)conn;
    if (mock_consume_input_fail) {
        return 0; // failure
    }
    return 1; // success
}

// Override PQnotifies_ - no notifications
PGnotify *PQnotifies_(PGconn *conn)
{
    (void)conn;
    return NULL;
}

// Dummy callback for consume_notifications
static void dummy_callback(void *ctx, const char *channel, const char *payload)
{
    (void)ctx;
    (void)channel;
    (void)payload;
}

// ========== Tests ==========

// Test: ik_db_listen error path
START_TEST(test_listen_error) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_db_ctx_t *mock_db = create_mock_db_ctx(ctx);

    res_t res = ik_db_listen(mock_db, "test_channel");
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_DB_CONNECT);

    talloc_free(ctx);
}
END_TEST

// Test: ik_db_unlisten error path
START_TEST(test_unlisten_error) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_db_ctx_t *mock_db = create_mock_db_ctx(ctx);

    res_t res = ik_db_unlisten(mock_db, "test_channel");
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_DB_CONNECT);

    talloc_free(ctx);
}
END_TEST

// Test: ik_db_notify error path
START_TEST(test_notify_error) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_db_ctx_t *mock_db = create_mock_db_ctx(ctx);

    res_t res = ik_db_notify(mock_db, "test_channel", "test_payload");
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_DB_CONNECT);

    talloc_free(ctx);
}
END_TEST

// Test: ik_db_consume_notifications when PQconsumeInput fails
START_TEST(test_consume_input_failure) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_db_ctx_t *mock_db = create_mock_db_ctx(ctx);

    mock_consume_input_fail = true;
    res_t res = ik_db_consume_notifications(mock_db, dummy_callback, NULL);
    mock_consume_input_fail = false;

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_DB_CONNECT);

    talloc_free(ctx);
}
END_TEST

// ========== Suite Configuration ==========

static Suite *notify_errors_suite(void)
{
    Suite *s = suite_create("DB Notify Errors");

    TCase *tc = tcase_create("Errors");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_test(tc, test_listen_error);
    tcase_add_test(tc, test_unlisten_error);
    tcase_add_test(tc, test_notify_error);
    tcase_add_test(tc, test_consume_input_failure);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    Suite *s = notify_errors_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/db/notify_errors_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
