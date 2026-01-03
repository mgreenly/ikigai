// tests/unit/db/pg_result_test.c - PGresult wrapper tests

#include "../../../src/db/pg_result.h"

#include "../../test_utils.h"

#include <check.h>
#include <libpq-fe.h>
#include <talloc.h>

static TALLOC_CTX *test_ctx = NULL;
static ik_db_ctx_t *db = NULL;
static const char *DB_NAME = NULL;

static void suite_setup(void)
{
    DB_NAME = ik_test_db_name(NULL, __FILE__);
    ik_test_db_create(DB_NAME);
    ik_test_db_migrate(NULL, DB_NAME);
}

static void suite_teardown(void)
{
    ik_test_db_destroy(DB_NAME);
}

static void test_setup(void)
{
    test_ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(test_ctx);
    ik_test_db_connect(test_ctx, DB_NAME, &db);
    ik_test_db_begin(db);
}

static void test_teardown(void)
{
    ik_test_db_rollback(db);
    talloc_free(test_ctx);
}

START_TEST(test_wrap_pg_result_basic) {
    // Execute a simple query
    PGresult *pg_res = PQexec(db->conn, "SELECT 1 AS value");
    ck_assert_ptr_nonnull(pg_res);
    ck_assert_int_eq(PQresultStatus(pg_res), PGRES_TUPLES_OK);

    // Wrap the result
    ik_pg_result_wrapper_t *wrapper = ik_db_wrap_pg_result(test_ctx, pg_res);
    ck_assert_ptr_nonnull(wrapper);
    ck_assert_ptr_eq(wrapper->pg_result, pg_res);

    // Access data through wrapper
    ck_assert_int_eq(PQntuples(wrapper->pg_result), 1);
    ck_assert_str_eq(PQgetvalue(wrapper->pg_result, 0, 0), "1");

    // talloc_free should trigger destructor and call PQclear
    // No explicit PQclear needed - destructor handles it
}
END_TEST

START_TEST(test_wrap_pg_result_null_handling) {
    // Wrapping NULL should work (defensive programming)
    ik_pg_result_wrapper_t *wrapper = ik_db_wrap_pg_result(test_ctx, NULL);
    ck_assert_ptr_nonnull(wrapper);
    ck_assert_ptr_null(wrapper->pg_result);

    // talloc_free should handle NULL pg_result gracefully
}

END_TEST

START_TEST(test_wrap_pg_result_auto_cleanup) {
    TALLOC_CTX *tmp_ctx = talloc_new(test_ctx);
    ck_assert_ptr_nonnull(tmp_ctx);

    // Create and wrap a result in tmp_ctx
    PGresult *pg_res = PQexec(db->conn, "SELECT 2 AS value");
    ck_assert_ptr_nonnull(pg_res);

    ik_pg_result_wrapper_t *wrapper = ik_db_wrap_pg_result(tmp_ctx, pg_res);
    ck_assert_ptr_nonnull(wrapper);

    // Free the temporary context - destructor should cleanup PGresult
    talloc_free(tmp_ctx);

    // If destructor worked correctly, sanitizer won't report leaks
    // (This is verified by make check-sanitize, not assertions)
}

END_TEST

static Suite *pg_result_suite(void)
{
    Suite *s = suite_create("db/pg_result");

    TCase *tc_basic = tcase_create("basic");
    tcase_set_timeout(tc_basic, 30);
    tcase_add_checked_fixture(tc_basic, test_setup, test_teardown);
    tcase_add_test(tc_basic, test_wrap_pg_result_basic);
    tcase_add_test(tc_basic, test_wrap_pg_result_null_handling);
    tcase_add_test(tc_basic, test_wrap_pg_result_auto_cleanup);
    suite_add_tcase(s, tc_basic);

    return s;
}

int main(void)
{
    suite_setup();

    Suite *s = pg_result_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    suite_teardown();

    return (number_failed == 0) ? 0 : 1;
}
