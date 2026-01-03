#include "../../../src/db/migration.h"
#include "../../../src/db/pg_result.h"
#include "../../../src/error.h"
#include "../../test_utils.h"

#include <check.h>
#include <libpq-fe.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <talloc.h>
#include <unistd.h>

static const char *DB_NAME = NULL;

// Forward declaration for destructor (same as in connection.c)
static int ik_db_ctx_destructor(ik_db_ctx_t *ctx)
{
    if (ctx->conn != NULL) {
        PQfinish(ctx->conn);
        ctx->conn = NULL;
    }
    return 0;
}

// Helper to create a db context WITHOUT running migrations
static ik_db_ctx_t *create_db_ctx_no_migrate(TALLOC_CTX *ctx, const char *conn_str)
{
    PGconn *conn = PQconnectdb(conn_str);
    if (PQstatus(conn) != CONNECTION_OK) {
        PQfinish(conn);
        return NULL;
    }

    ik_db_ctx_t *db_ctx = talloc(ctx, ik_db_ctx_t);
    if (!db_ctx) {
        PQfinish(conn);
        return NULL;
    }

    db_ctx->conn = conn;
    talloc_set_destructor(db_ctx, ik_db_ctx_destructor);

    return db_ctx;
}

// Helper to create temporary test directory with migrations
static char *create_test_migration_dir(TALLOC_CTX *ctx)
{
    char *template = talloc_strdup(ctx, "/tmp/ik_migration_test_XXXXXX");
    char *dir = mkdtemp(template);
    if (!dir) {
        return NULL;
    }
    return talloc_strdup(ctx, template);
}

// Helper to create a test migration file
static int create_migration_file(const char *dir, const char *filename, const char *content)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", dir, filename);
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "%s", content);
    fclose(f);
    return 0;
}

// Helper to cleanup test directory
static void cleanup_test_dir(const char *dir)
{
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", dir);
    int result = system(cmd);
    (void)result; // Suppress unused variable warning
}

// Test: Invalid migration filenames are skipped
START_TEST(test_migration_invalid_filenames_skipped) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    char conn_str[256];
    ik_test_db_conn_str(conn_str, sizeof(conn_str), DB_NAME);

    ik_db_ctx_t *db_ctx = create_db_ctx_no_migrate(ctx, conn_str);
    ck_assert_ptr_nonnull(db_ctx);

    ik_db_wrap_pg_result(ctx, PQexec(db_ctx->conn, "DROP TABLE IF EXISTS schema_metadata"));

    char *test_dir = create_test_migration_dir(ctx);

    // Valid migration
    create_migration_file(test_dir, "0001-init.sql",
                          "BEGIN;\n"
                          "CREATE TABLE schema_metadata (schema_version INTEGER);\n"
                          "INSERT INTO schema_metadata VALUES (1);\n"
                          "COMMIT;\n");

    // Invalid filenames that should be skipped (tests parse_migration_number)
    create_migration_file(test_dir, "README.md", "Not a migration");
    create_migration_file(test_dir, "migration.sql", "Missing number");
    create_migration_file(test_dir, "001.sql", "Missing dash");
    create_migration_file(test_dir, "01-short.sql", "Too few digits");
    create_migration_file(test_dir, "short.sql", "No number at all");
    create_migration_file(test_dir, "12345-toolong.sql", "Too many digits");

    res_t res = ik_db_migrate(db_ctx, test_dir);
    ck_assert(is_ok(&res));

    // Only migration 0001 should have run
    ik_pg_result_wrapper_t *result_wrapper = ik_db_wrap_pg_result(ctx, PQexec(db_ctx->conn,
                                                                              "SELECT schema_version FROM schema_metadata"));
    PGresult *result = result_wrapper->pg_result;
    ck_assert_int_eq(atoi(PQgetvalue(result, 0, 0)), 1);

    cleanup_test_dir(test_dir);
    talloc_free(ctx);
}

END_TEST
// Test: Non-existent migrations directory
START_TEST(test_migration_nonexistent_directory) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    char conn_str[256];
    ik_test_db_conn_str(conn_str, sizeof(conn_str), DB_NAME);

    ik_db_ctx_t *db_ctx = create_db_ctx_no_migrate(ctx, conn_str);
    ck_assert_ptr_nonnull(db_ctx);

    res_t res = ik_db_migrate(db_ctx, "/nonexistent/directory/path");
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);

    talloc_free(ctx);
}

END_TEST
// Test: Legacy 3-digit migration format
START_TEST(test_migration_legacy_three_digit_format) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    char conn_str[256];
    ik_test_db_conn_str(conn_str, sizeof(conn_str), DB_NAME);

    ik_db_ctx_t *db_ctx = create_db_ctx_no_migrate(ctx, conn_str);
    ck_assert_ptr_nonnull(db_ctx);

    ik_db_wrap_pg_result(ctx, PQexec(db_ctx->conn, "DROP TABLE IF EXISTS schema_metadata"));

    char *test_dir = create_test_migration_dir(ctx);

    // Use 3-digit format (legacy)
    create_migration_file(test_dir, "001-init.sql",
                          "BEGIN;\n"
                          "CREATE TABLE schema_metadata (schema_version INTEGER);\n"
                          "INSERT INTO schema_metadata VALUES (1);\n"
                          "COMMIT;\n");

    res_t res = ik_db_migrate(db_ctx, test_dir);
    ck_assert(is_ok(&res));

    ik_pg_result_wrapper_t *result_wrapper = ik_db_wrap_pg_result(ctx, PQexec(db_ctx->conn,
                                                                              "SELECT schema_version FROM schema_metadata"));
    PGresult *result = result_wrapper->pg_result;
    ck_assert_int_eq(atoi(PQgetvalue(result, 0, 0)), 1);

    cleanup_test_dir(test_dir);
    talloc_free(ctx);
}

END_TEST
// Test: Migration files sorted by number, not alphabetically
START_TEST(test_migration_sorting_by_number) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    char conn_str[256];
    ik_test_db_conn_str(conn_str, sizeof(conn_str), DB_NAME);

    ik_db_ctx_t *db_ctx = create_db_ctx_no_migrate(ctx, conn_str);
    ck_assert_ptr_nonnull(db_ctx);

    ik_db_wrap_pg_result(ctx, PQexec(db_ctx->conn, "DROP TABLE IF EXISTS schema_metadata"));

    char *test_dir = create_test_migration_dir(ctx);

    // Create files in non-sequential order to test sorting
    create_migration_file(test_dir, "0002-second.sql",
                          "BEGIN;\n"
                          "UPDATE schema_metadata SET schema_version = 2;\n"
                          "COMMIT;\n");

    create_migration_file(test_dir, "0001-first.sql",
                          "BEGIN;\n"
                          "CREATE TABLE schema_metadata (schema_version INTEGER);\n"
                          "INSERT INTO schema_metadata VALUES (1);\n"
                          "COMMIT;\n");

    create_migration_file(test_dir, "0003-third.sql",
                          "BEGIN;\n"
                          "UPDATE schema_metadata SET schema_version = 3;\n"
                          "COMMIT;\n");

    res_t res = ik_db_migrate(db_ctx, test_dir);
    ck_assert(is_ok(&res));

    // Should end up at version 3 if sorted correctly
    ik_pg_result_wrapper_t *result_wrapper = ik_db_wrap_pg_result(ctx, PQexec(db_ctx->conn,
                                                                              "SELECT schema_version FROM schema_metadata"));
    PGresult *result = result_wrapper->pg_result;
    ck_assert_int_eq(atoi(PQgetvalue(result, 0, 0)), 3);

    cleanup_test_dir(test_dir);
    talloc_free(ctx);
}

END_TEST
// Test: Array growth when many migration files (>10)
START_TEST(test_migration_array_growth) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    char conn_str[256];
    ik_test_db_conn_str(conn_str, sizeof(conn_str), DB_NAME);

    ik_db_ctx_t *db_ctx = create_db_ctx_no_migrate(ctx, conn_str);
    ck_assert_ptr_nonnull(db_ctx);

    ik_db_wrap_pg_result(ctx, PQexec(db_ctx->conn, "DROP TABLE IF EXISTS schema_metadata"));

    char *test_dir = create_test_migration_dir(ctx);

    // Create first migration to set up schema_metadata
    create_migration_file(test_dir, "0001-init.sql",
                          "BEGIN;\n"
                          "CREATE TABLE schema_metadata (schema_version INTEGER);\n"
                          "INSERT INTO schema_metadata VALUES (1);\n"
                          "COMMIT;\n");

    // Create 14 more migration files (total 15 > initial capacity of 10)
    for (int i = 2; i <= 15; i++) {
        char filename[64];
        char content[256];
        snprintf(filename, sizeof(filename), "%04d-migration.sql", i);
        snprintf(content, sizeof(content),
                 "BEGIN;\n"
                 "UPDATE schema_metadata SET schema_version = %d;\n"
                 "COMMIT;\n", i);
        create_migration_file(test_dir, filename, content);
    }

    res_t res = ik_db_migrate(db_ctx, test_dir);
    ck_assert(is_ok(&res));

    // Verify all 15 migrations ran
    ik_pg_result_wrapper_t *result_wrapper = ik_db_wrap_pg_result(ctx, PQexec(db_ctx->conn,
                                                                              "SELECT schema_version FROM schema_metadata"));
    PGresult *result = result_wrapper->pg_result;
    ck_assert_int_eq(atoi(PQgetvalue(result, 0, 0)), 15);

    cleanup_test_dir(test_dir);
    talloc_free(ctx);
}

END_TEST

// Suite-level setup: Create unique database for this test file
static void suite_setup(void)
{
    DB_NAME = ik_test_db_name(NULL, __FILE__);
    ik_test_db_create(DB_NAME);
    // DO NOT migrate - these tests need to test migration from scratch
}

// Suite-level teardown: Destroy the unique database
static void suite_teardown(void)
{
    ik_test_db_destroy(DB_NAME);
}

// Per-test setup: Drop all tables to ensure clean slate for each test
static void migration_test_setup(void)
{
    TALLOC_CTX *tmp_ctx = talloc_new(NULL);
    ik_db_ctx_t *db = NULL;

    // Connect without running migrations
    res_t res = ik_test_db_connect(tmp_ctx, DB_NAME, &db);
    if (is_ok(&res)) {
        // Drop all tables to ensure fresh state for each test
        ik_db_wrap_pg_result(tmp_ctx, PQexec(db->conn,
                                             "DROP TABLE IF EXISTS schema_metadata, sessions, messages, auto_test, test_table CASCADE"));
    }

    talloc_free(tmp_ctx);
}

static Suite *migration_suite(void)
{
    Suite *s = suite_create("db_migration_parsing");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);
    tcase_add_checked_fixture(tc_core, migration_test_setup, NULL);
    tcase_add_test(tc_core, test_migration_invalid_filenames_skipped);
    tcase_add_test(tc_core, test_migration_nonexistent_directory);
    tcase_add_test(tc_core, test_migration_legacy_three_digit_format);
    tcase_add_test(tc_core, test_migration_sorting_by_number);
    tcase_add_test(tc_core, test_migration_array_growth);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    suite_setup();

    int number_failed;
    Suite *s = migration_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    suite_teardown();

    return (number_failed == 0) ? 0 : 1;
}
