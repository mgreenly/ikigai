/**
 * @file agents_schema_test.c
 * @brief Tests for agents table schema (migration 002)
 *
 * Verifies that the agents table migration creates the correct schema.
 * Uses a simple non-transactional approach to avoid rollback complications.
 */

#include "../../../src/db/connection.h"
#include "../../../src/error.h"
#include "../../test_utils.h"
#include <check.h>
#include <libpq-fe.h>
#include <talloc.h>
#include <string.h>
#include <unistd.h>

static const char *DB_NAME;
static bool db_available = false;

// Suite-level setup: Create and migrate database
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

// Suite-level teardown: Destroy database
static void suite_teardown(void)
{
    if (db_available) {
        ik_test_db_destroy(DB_NAME);
    }
}

// Helper to execute a query (caller must PQclear result)
static PGresult *exec_query(PGconn *conn, const char *query)
{
    return PQexec(conn, query);
}

// Test that agents table exists
START_TEST(test_agents_table_exists)
{
    if (!db_available) return;

    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_db_ctx_t *db = NULL;
    res_t conn_res = ik_test_db_connect(ctx, DB_NAME, &db);
    if (is_err(&conn_res)) {
        talloc_free(ctx);
        ck_abort_msg("Failed to connect to database");
    }

    PGresult *res = exec_query(db->conn,
        "SELECT EXISTS ("
        "  SELECT FROM information_schema.tables "
        "  WHERE table_schema = 'public' AND table_name = 'agents'"
        ")");

    ck_assert(PQresultStatus(res) == PGRES_TUPLES_OK);
    ck_assert_str_eq(PQgetvalue(res, 0, 0), "t");
    PQclear(res);
    talloc_free(ctx);
}
END_TEST

// Test that agent_status enum exists with correct values
START_TEST(test_agent_status_enum)
{
    if (!db_available) return;

    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_db_ctx_t *db = NULL;
    res_t conn_res = ik_test_db_connect(ctx, DB_NAME, &db);
    if (is_err(&conn_res)) {
        talloc_free(ctx);
        ck_abort_msg("Failed to connect to database");
    }

    PGresult *res = exec_query(db->conn,
        "SELECT enumlabel FROM pg_enum "
        "WHERE enumtypid = 'agent_status'::regtype "
        "ORDER BY enumsortorder");

    ck_assert(PQresultStatus(res) == PGRES_TUPLES_OK);
    ck_assert_int_eq(PQntuples(res), 2);
    ck_assert_str_eq(PQgetvalue(res, 0, 0), "running");
    ck_assert_str_eq(PQgetvalue(res, 1, 0), "dead");
    PQclear(res);
    talloc_free(ctx);
}
END_TEST

// Test that all required columns exist
START_TEST(test_required_columns_exist)
{
    if (!db_available) return;

    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_db_ctx_t *db = NULL;
    res_t conn_res = ik_test_db_connect(ctx, DB_NAME, &db);
    if (is_err(&conn_res)) {
        talloc_free(ctx);
        ck_abort_msg("Failed to connect to database");
    }

    const char *columns[] = {
        "uuid", "name", "parent_uuid", "fork_message_id",
        "status", "created_at", "ended_at"
    };

    for (size_t i = 0; i < sizeof(columns) / sizeof(columns[0]); i++) {
        char query[256];
        snprintf(query, sizeof(query),
            "SELECT column_name FROM information_schema.columns "
            "WHERE table_name = 'agents' AND column_name = '%s'",
            columns[i]);

        PGresult *res = exec_query(db->conn, query);
        ck_assert_msg(PQresultStatus(res) == PGRES_TUPLES_OK,
            "Query failed for column %s", columns[i]);
        ck_assert_msg(PQntuples(res) == 1,
            "Column %s does not exist", columns[i]);
        PQclear(res);
    }

    talloc_free(ctx);
}
END_TEST

// Test that required indexes exist
START_TEST(test_required_indexes_exist)
{
    if (!db_available) return;

    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_db_ctx_t *db = NULL;
    res_t conn_res = ik_test_db_connect(ctx, DB_NAME, &db);
    if (is_err(&conn_res)) {
        talloc_free(ctx);
        ck_abort_msg("Failed to connect to database");
    }

    const char *indexes[] = {
        "idx_agents_parent",
        "idx_agents_status"
    };

    for (size_t i = 0; i < sizeof(indexes) / sizeof(indexes[0]); i++) {
        char query[256];
        snprintf(query, sizeof(query),
            "SELECT indexname FROM pg_indexes "
            "WHERE tablename = 'agents' AND indexname = '%s'",
            indexes[i]);

        PGresult *res = exec_query(db->conn, query);
        ck_assert_msg(PQresultStatus(res) == PGRES_TUPLES_OK,
            "Query failed for index %s", indexes[i]);
        ck_assert_msg(PQntuples(res) == 1,
            "Index %s does not exist", indexes[i]);
        PQclear(res);
    }

    talloc_free(ctx);
}
END_TEST

static Suite *agents_schema_suite(void)
{
    Suite *s = suite_create("Agents Schema");

    TCase *tc = tcase_create("Schema");
    tcase_set_timeout(tc, 30);
    tcase_add_test(tc, test_agents_table_exists);
    tcase_add_test(tc, test_agent_status_enum);
    tcase_add_test(tc, test_required_columns_exist);
    tcase_add_test(tc, test_required_indexes_exist);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    suite_setup();

    Suite *s = agents_schema_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    suite_teardown();

    return (number_failed == 0) ? 0 : 1;
}
