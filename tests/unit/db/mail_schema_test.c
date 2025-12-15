/**
 * @file mail_schema_test.c
 * @brief Tests for mail table schema (migration 004)
 *
 * Verifies that the mail table migration creates the correct schema.
 * Uses a simple non-transactional approach to avoid rollback complications.
 */

#include "../../../src/db/connection.h"
#include "../../../src/error.h"
#include "../../test_utils.h"

#include <check.h>
#include <libpq-fe.h>
#include <string.h>
#include <talloc.h>
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

// Test that mail table exists
START_TEST(test_mail_table_exists)
{
    if (!db_available) return;

    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_db_ctx_t *db = NULL;
    ik_test_db_connect(ctx, DB_NAME, &db);

    PGresult *res = exec_query(db->conn,
        "SELECT EXISTS ("
        "  SELECT FROM information_schema.tables "
        "  WHERE table_schema = 'public' AND table_name = 'mail'"
        ")");

    ck_assert(PQresultStatus(res) == PGRES_TUPLES_OK);
    ck_assert_str_eq(PQgetvalue(res, 0, 0), "t");
    PQclear(res);
    talloc_free(ctx);
}
END_TEST

// Test that all required columns exist with correct types
START_TEST(test_mail_columns_exist)
{
    if (!db_available) return;

    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_db_ctx_t *db = NULL;
    ik_test_db_connect(ctx, DB_NAME, &db);

    // Check columns with their expected types
    struct {
        const char *column;
        const char *type;
    } columns[] = {
        {"id", "bigint"},
        {"session_id", "bigint"},
        {"from_uuid", "text"},
        {"to_uuid", "text"},
        {"body", "text"},
        {"timestamp", "bigint"},
        {"read", "integer"}
    };

    for (size_t i = 0; i < sizeof(columns) / sizeof(columns[0]); i++) {
        char query[512];
        snprintf(query, sizeof(query),
            "SELECT data_type FROM information_schema.columns "
            "WHERE table_name = 'mail' AND column_name = '%s'",
            columns[i].column);

        PGresult *res = exec_query(db->conn, query);
        ck_assert_msg(PQresultStatus(res) == PGRES_TUPLES_OK,
            "Query failed for column %s", columns[i].column);
        ck_assert_msg(PQntuples(res) == 1,
            "Column %s does not exist", columns[i].column);
        ck_assert_msg(strcmp(PQgetvalue(res, 0, 0), columns[i].type) == 0,
            "Column %s has wrong type: expected %s, got %s",
            columns[i].column, columns[i].type, PQgetvalue(res, 0, 0));
        PQclear(res);
    }

    talloc_free(ctx);
}
END_TEST

// Test foreign key to sessions table
START_TEST(test_mail_foreign_key_sessions)
{
    if (!db_available) return;

    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_db_ctx_t *db = NULL;
    ik_test_db_connect(ctx, DB_NAME, &db);

    PGresult *res = exec_query(db->conn,
        "SELECT tc.constraint_name, ccu.table_name AS foreign_table "
        "FROM information_schema.table_constraints tc "
        "JOIN information_schema.constraint_column_usage ccu "
        "  ON tc.constraint_name = ccu.constraint_name "
        "WHERE tc.table_name = 'mail' "
        "  AND tc.constraint_type = 'FOREIGN KEY' "
        "  AND ccu.table_name = 'sessions'");

    ck_assert(PQresultStatus(res) == PGRES_TUPLES_OK);
    ck_assert_msg(PQntuples(res) >= 1,
        "Foreign key to sessions table does not exist");
    PQclear(res);
    talloc_free(ctx);
}
END_TEST

// Test that idx_mail_recipient index exists
START_TEST(test_mail_recipient_index_exists)
{
    if (!db_available) return;

    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_db_ctx_t *db = NULL;
    ik_test_db_connect(ctx, DB_NAME, &db);

    PGresult *res = exec_query(db->conn,
        "SELECT indexname FROM pg_indexes "
        "WHERE tablename = 'mail' AND indexname = 'idx_mail_recipient'");

    ck_assert(PQresultStatus(res) == PGRES_TUPLES_OK);
    ck_assert_msg(PQntuples(res) == 1,
        "Index idx_mail_recipient does not exist");
    PQclear(res);
    talloc_free(ctx);
}
END_TEST

// Test migration is idempotent (running migrations again should not fail)
START_TEST(test_mail_migration_idempotent)
{
    if (!db_available) return;

    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_db_ctx_t *db = NULL;
    ik_test_db_connect(ctx, DB_NAME, &db);

    // Running migrations again should succeed (idempotent)
    res_t res = ik_test_db_migrate(ctx, DB_NAME);
    ck_assert_msg(is_ok(&res), "Second migration run should succeed");

    // Verify table still exists after re-migration
    PGresult *pgres = exec_query(db->conn,
        "SELECT EXISTS ("
        "  SELECT FROM information_schema.tables "
        "  WHERE table_schema = 'public' AND table_name = 'mail'"
        ")");

    ck_assert(PQresultStatus(pgres) == PGRES_TUPLES_OK);
    ck_assert_str_eq(PQgetvalue(pgres, 0, 0), "t");
    PQclear(pgres);
    talloc_free(ctx);
}
END_TEST

static Suite *mail_schema_suite(void)
{
    Suite *s = suite_create("Mail Schema");

    TCase *tc = tcase_create("Schema");
    tcase_add_test(tc, test_mail_table_exists);
    tcase_add_test(tc, test_mail_columns_exist);
    tcase_add_test(tc, test_mail_foreign_key_sessions);
    tcase_add_test(tc, test_mail_recipient_index_exists);
    tcase_add_test(tc, test_mail_migration_idempotent);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    suite_setup();

    Suite *s = mail_schema_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    suite_teardown();

    return (number_failed == 0) ? 0 : 1;
}
