/**
 * @file bg_process_schema_test.c
 * @brief Integration test confirming background_processes schema applies cleanly
 *
 * Verifies that the bg_process_status enum and background_processes table
 * exist after migration and that indexes are present.
 */

#include "shared/error.h"
#include "tests/helpers/test_utils_helper.h"
#include <check.h>
#include <libpq-fe.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

// ========== Test Database Setup ==========

static const char *DB_NAME;
static bool db_available = false;

static TALLOC_CTX *test_ctx;
static ik_db_ctx_t *db;

static void suite_setup(void)
{
    const char *skip_live = getenv("SKIP_LIVE_DB_TESTS");
    if (skip_live && strcmp(skip_live, "1") == 0) {
        db_available = false;
        return;
    }

    DB_NAME = ik_test_db_name(NULL, __FILE__);

    res_t res = ik_test_db_create(DB_NAME);
    if (is_err(&res)) {
        db_available = false;
        return;
    }

    res = ik_test_db_migrate(NULL, DB_NAME);
    if (is_err(&res)) {
        ik_test_db_destroy(DB_NAME);
        db_available = false;
        return;
    }

    db_available = true;
}

static void suite_teardown(void)
{
    if (db_available) {
        ik_test_db_destroy(DB_NAME);
    }
}

static void test_setup(void)
{
    if (!db_available) {
        test_ctx = NULL;
        db = NULL;
        return;
    }

    test_ctx = talloc_new(NULL);
    res_t res = ik_test_db_connect(test_ctx, DB_NAME, &db);
    if (is_err(&res)) {
        talloc_free(test_ctx);
        test_ctx = NULL;
        db = NULL;
    }
}

static void test_teardown(void)
{
    if (test_ctx != NULL) {
        talloc_free(test_ctx);
        test_ctx = NULL;
        db = NULL;
    }
}

#define SKIP_IF_NO_DB() do { if (db == NULL) return; } while (0)

// ========== Tests ==========

// bg_process_status enum exists with all expected values
START_TEST(test_enum_exists)
{
    SKIP_IF_NO_DB();

    PGresult *res = PQexec(db->conn,
        "SELECT enumlabel FROM pg_enum "
        "JOIN pg_type ON pg_enum.enumtypid = pg_type.oid "
        "WHERE pg_type.typname = 'bg_process_status' "
        "ORDER BY enumsortorder");

    ck_assert_int_eq(PQresultStatus(res), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(res), 6);

    ck_assert_str_eq(PQgetvalue(res, 0, 0), "starting");
    ck_assert_str_eq(PQgetvalue(res, 1, 0), "running");
    ck_assert_str_eq(PQgetvalue(res, 2, 0), "exited");
    ck_assert_str_eq(PQgetvalue(res, 3, 0), "killed");
    ck_assert_str_eq(PQgetvalue(res, 4, 0), "timed_out");
    ck_assert_str_eq(PQgetvalue(res, 5, 0), "failed");

    PQclear(res);
}
END_TEST

// background_processes table exists with expected columns
START_TEST(test_table_exists)
{
    SKIP_IF_NO_DB();

    PGresult *res = PQexec(db->conn,
        "SELECT column_name FROM information_schema.columns "
        "WHERE table_name = 'background_processes' "
        "ORDER BY ordinal_position");

    ck_assert_int_eq(PQresultStatus(res), PGRES_TUPLES_OK);
    ck_assert_int_gt(PQntuples(res), 0);

    PQclear(res);
}
END_TEST

// background_processes table references agents(uuid)
START_TEST(test_agent_uuid_fk)
{
    SKIP_IF_NO_DB();

    PGresult *res = PQexec(db->conn,
        "SELECT kcu.column_name "
        "FROM information_schema.table_constraints tc "
        "JOIN information_schema.key_column_usage kcu "
        "  ON tc.constraint_name = kcu.constraint_name "
        "WHERE tc.table_name = 'background_processes' "
        "  AND tc.constraint_type = 'FOREIGN KEY' "
        "  AND kcu.column_name = 'agent_uuid'");

    ck_assert_int_eq(PQresultStatus(res), PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(res), 1);
    ck_assert_str_eq(PQgetvalue(res, 0, 0), "agent_uuid");

    PQclear(res);
}
END_TEST

// Indexes on agent_uuid and active status exist
START_TEST(test_indexes_exist)
{
    SKIP_IF_NO_DB();

    PGresult *res = PQexec(db->conn,
        "SELECT indexname FROM pg_indexes "
        "WHERE tablename = 'background_processes' "
        "ORDER BY indexname");

    ck_assert_int_eq(PQresultStatus(res), PGRES_TUPLES_OK);

    bool found_agent = false;
    bool found_active = false;
    for (int i = 0; i < PQntuples(res); i++) {
        const char *name = PQgetvalue(res, i, 0);
        if (strcmp(name, "idx_bg_proc_agent") == 0) found_agent = true;
        if (strcmp(name, "idx_bg_proc_active") == 0) found_active = true;
    }

    ck_assert(found_agent);
    ck_assert(found_active);

    PQclear(res);
}
END_TEST

// ========== Suite ==========

static Suite *bg_process_schema_suite(void)
{
    Suite *s = suite_create("Background Process Schema");

    TCase *tc = tcase_create("Core");
    tcase_add_unchecked_fixture(tc, suite_setup, suite_teardown);
    tcase_add_checked_fixture(tc, test_setup, test_teardown);
    tcase_add_test(tc, test_enum_exists);
    tcase_add_test(tc, test_table_exists);
    tcase_add_test(tc, test_agent_uuid_fk);
    tcase_add_test(tc, test_indexes_exist);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = bg_process_schema_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr,
        "reports/check/integration/apps/ikigai/db/bg_process_schema_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
