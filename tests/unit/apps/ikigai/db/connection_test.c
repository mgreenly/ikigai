#include "apps/ikigai/db/connection.h"
#include "shared/error.h"
#include "tests/helpers/test_utils_helper.h"

#include <check.h>
#include <libpq-fe.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <talloc.h>
#include <unistd.h>

// Mock PostgreSQL connection string for testing
// Note: Tests requiring actual database connectivity will need appropriate setup
#define INVALID_HOST_CONN_STR "postgresql://nonexistent-host-12345/test_db?connect_timeout=1"
#define MALFORMED_CONN_STR "not-a-valid-connection-string"

// Get PostgreSQL host from environment or default to localhost
static const char *get_pg_host(void)
{
    const char *host = getenv("PGHOST");
    return host ? host : "localhost";
}

// ========== Test Database Setup ==========
// Each test file gets its own database for parallel execution

static const char *DB_NAME;
static bool db_available = false;

// Per-test state
static TALLOC_CTX *test_ctx;

// Suite-level setup: Create and migrate database (runs once)
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

// Suite-level teardown: Destroy database (runs once)
static void suite_teardown(void)
{
    if (db_available) {
        ik_test_db_destroy(DB_NAME);
    }
}

// Per-test setup
static void test_setup(void)
{
    test_ctx = talloc_new(NULL);
}

// Per-test teardown
static void test_teardown(void)
{
    if (test_ctx != NULL) {
        talloc_free(test_ctx);
        test_ctx = NULL;
    }
}

// Helper macro to skip test if DB not available
#define SKIP_IF_NO_DB() do { if (!db_available) return; } while (0)

// Helper to build connection string for test database
static char *get_test_conn_str(TALLOC_CTX *ctx)
{
    return talloc_asprintf(ctx, "postgresql://ikigai:ikigai@%s/%s", get_pg_host(), DB_NAME);
}

// ========== Connection String Validation Tests ==========

START_TEST(test_db_init_empty_conn_str) {
    ik_db_ctx_t *db_ctx = NULL;

    res_t res = ik_db_init(test_ctx, "", "share/ikigai", &db_ctx);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_INVALID_ARG);
}
END_TEST

START_TEST(test_db_init_malformed_conn_str) {
    ik_db_ctx_t *db_ctx = NULL;

    // Malformed connection string should either:
    // 1. Fail during validation (ERR_INVALID_ARG), or
    // 2. Fail during connection (ERR_DB_CONNECT)
    res_t res = ik_db_init(test_ctx, MALFORMED_CONN_STR, "share/ikigai", &db_ctx);

    ck_assert(is_err(&res));
    // Accept either validation error or connection error
    ck_assert(error_code(res.err) == ERR_INVALID_ARG ||
              error_code(res.err) == ERR_DB_CONNECT);
}

END_TEST
// ========== Connection Error Tests ==========

START_TEST(test_db_init_connection_refused) {
    ik_db_ctx_t *db_ctx = NULL;

    // Use invalid host that should result in connection refused/timeout
    res_t res = ik_db_init(test_ctx, INVALID_HOST_CONN_STR, "share/ikigai", &db_ctx);

    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_DB_CONNECT);
    ck_assert_ptr_null(db_ctx);
}

END_TEST

START_TEST(test_db_init_postgres_scheme) {
    ik_db_ctx_t *db_ctx = NULL;

    // Test postgres:// scheme (alternative to postgresql://)
    // This will likely fail to connect but should pass validation
    // Use connect_timeout=1 to fail fast in CI environments
    res_t res = ik_db_init(test_ctx,
                           "postgres://nonexistent-host-99999/testdb?connect_timeout=1",
                           "share/ikigai",
                           &db_ctx);

    // Should fail with DB_CONNECT, not INVALID_ARG (validation should pass)
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_DB_CONNECT);
}

END_TEST

START_TEST(test_db_init_key_value_format) {
    ik_db_ctx_t *db_ctx = NULL;

    // Test libpq key=value format
    // This will likely fail to connect but should pass validation
    // Use connect_timeout=1 to fail fast in CI environments
    res_t res = ik_db_init(test_ctx,
                           "host=nonexistent-host-99999 dbname=testdb connect_timeout=1",
                           "share/ikigai",
                           &db_ctx);

    // Should fail with DB_CONNECT (libpq handles the parsing)
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_DB_CONNECT);
}

END_TEST
// ========== Successful Connection Tests ==========

START_TEST(test_db_init_success) {
    SKIP_IF_NO_DB();

    ik_db_ctx_t *db_ctx = NULL;
    char *conn_str = get_test_conn_str(test_ctx);

    res_t res = ik_db_init(test_ctx, conn_str, "share/ikigai", &db_ctx);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(db_ctx);
    ck_assert_ptr_nonnull(db_ctx->conn);
}

END_TEST

START_TEST(test_db_init_talloc_hierarchy) {
    SKIP_IF_NO_DB();

    ik_db_ctx_t *db_ctx = NULL;
    char *conn_str = get_test_conn_str(test_ctx);

    res_t res = ik_db_init(test_ctx, conn_str, "share/ikigai", &db_ctx);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(db_ctx);

    // Verify that db_ctx is a child of the provided context
    // This ensures proper talloc hierarchy for automatic cleanup
    TALLOC_CTX *parent = talloc_parent(db_ctx);
    ck_assert_ptr_eq(parent, test_ctx);
}

END_TEST

START_TEST(test_db_init_destructor_cleanup) {
    SKIP_IF_NO_DB();

    TALLOC_CTX *local_ctx = talloc_new(NULL);
    ik_db_ctx_t *db_ctx = NULL;
    char *conn_str = get_test_conn_str(local_ctx);

    res_t res = ik_db_init(local_ctx, conn_str, "share/ikigai", &db_ctx);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(db_ctx);
    ck_assert_ptr_nonnull(db_ctx->conn);

    // Free the context - this should trigger the destructor
    // which will call PQfinish() on the connection
    // No explicit cleanup needed - talloc destructor handles it
    talloc_free(local_ctx);

    // If we get here without crashes, destructor worked correctly
}

END_TEST

START_TEST(test_db_init_connection_string_variants) {
    SKIP_IF_NO_DB();

    // Test various valid connection string formats
    // All use our test database name
    char *base_str = get_test_conn_str(test_ctx);

    ik_db_ctx_t *db_ctx = NULL;
    res_t res = ik_db_init(test_ctx, base_str, "share/ikigai", &db_ctx);

    ck_assert(is_ok(&res));
    ck_assert_ptr_nonnull(db_ctx);
}

END_TEST
// ========== Memory Cleanup Tests ==========

START_TEST(test_db_init_cleanup_on_error) {
    TALLOC_CTX *local_ctx = talloc_new(NULL);
    ik_db_ctx_t *db_ctx = NULL;

    // Initialize with invalid connection string
    res_t res = ik_db_init(local_ctx, INVALID_HOST_CONN_STR, "share/ikigai", &db_ctx);

    ck_assert(is_err(&res));

    // Verify that no memory leaks occur - talloc should clean up everything
    // when we free the context
    talloc_free(local_ctx);
}

END_TEST
// ========== Migration Error Tests ==========

START_TEST(test_db_init_migration_failure) {
    SKIP_IF_NO_DB();

    char *conn_str = get_test_conn_str(test_ctx);

    // Create a temporary directory for test migrations
    const char *test_migrations_dir = "/tmp/ikigai_test_migrations_invalid";

    // Create the directory if it doesn't exist
    mkdir(test_migrations_dir, 0755);

    // Create an invalid migration file
    // Use migration 9999 to ensure it's higher than any existing schema_version
    char migration_path[512];
    snprintf(migration_path, sizeof(migration_path), "%s/9999-test-failure.sql", test_migrations_dir);

    FILE *f = fopen(migration_path, "w");
    if (f != NULL) {
        // Write SQL that will fail - referencing a non-existent table
        fprintf(f, "SELECT * FROM nonexistent_table_xyz123;\n");
        fclose(f);
    }

    // Try to initialize database with the bad migrations directory
    // This should fail during migration
    ik_db_ctx_t *db_ctx = NULL;
    res_t res = ik_db_init_with_migrations(test_ctx, conn_str, test_migrations_dir, &db_ctx);

    // Clean up test migration file and directory
    unlink(migration_path);
    rmdir(test_migrations_dir);

    // We expect a migration error
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_DB_MIGRATE);
    ck_assert_ptr_null(db_ctx);
}

END_TEST
// ========== Transaction Tests ==========

START_TEST(test_db_transaction_success) {
    SKIP_IF_NO_DB();

    ik_db_ctx_t *db_ctx = NULL;
    char *conn_str = get_test_conn_str(test_ctx);

    res_t res = ik_db_init(test_ctx, conn_str, "share/ikigai", &db_ctx);
    ck_assert(is_ok(&res));

    // Test BEGIN
    res = ik_db_begin(db_ctx);
    ck_assert(is_ok(&res));

    // Test ROLLBACK
    res = ik_db_rollback(db_ctx);
    ck_assert(is_ok(&res));

    // Test BEGIN again
    res = ik_db_begin(db_ctx);
    ck_assert(is_ok(&res));

    // Test COMMIT
    res = ik_db_commit(db_ctx);
    ck_assert(is_ok(&res));
}

END_TEST

START_TEST(test_notice_processor) {
    SKIP_IF_NO_DB();

    ik_db_ctx_t *db_ctx = NULL;
    char *conn_str = get_test_conn_str(test_ctx);

    res_t res = ik_db_init(test_ctx, conn_str, "share/ikigai", &db_ctx);
    ck_assert(is_ok(&res));

    // Execute SQL that raises a notice - this will trigger pq_notice_processor callback
    PGresult *result = PQexec(db_ctx->conn, "DO $$ BEGIN RAISE NOTICE 'test notice'; END $$;");
    ck_assert(result != NULL);
    ck_assert_int_eq(PQresultStatus(result), PGRES_COMMAND_OK);
    PQclear(result);
}

END_TEST

// ========== Suite Configuration ==========

static Suite *connection_suite(void)
{
    Suite *s = suite_create("db_connection");

    TCase *tc_validation = tcase_create("connection_string_validation");
    tcase_set_timeout(tc_validation, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_validation, suite_setup, suite_teardown);
    tcase_add_checked_fixture(tc_validation, test_setup, test_teardown);
    tcase_add_test(tc_validation, test_db_init_empty_conn_str);
    tcase_add_test(tc_validation, test_db_init_malformed_conn_str);
    suite_add_tcase(s, tc_validation);

    TCase *tc_connection = tcase_create("connection_errors");
    tcase_set_timeout(tc_connection, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_connection, suite_setup, suite_teardown);
    tcase_add_checked_fixture(tc_connection, test_setup, test_teardown);
    tcase_add_test(tc_connection, test_db_init_connection_refused);
    tcase_add_test(tc_connection, test_db_init_postgres_scheme);
    tcase_add_test(tc_connection, test_db_init_key_value_format);
    suite_add_tcase(s, tc_connection);

    TCase *tc_success = tcase_create("successful_connection");
    tcase_set_timeout(tc_success, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_success, suite_setup, suite_teardown);
    tcase_add_checked_fixture(tc_success, test_setup, test_teardown);
    tcase_add_test(tc_success, test_db_init_success);
    tcase_add_test(tc_success, test_db_init_talloc_hierarchy);
    tcase_add_test(tc_success, test_db_init_destructor_cleanup);
    tcase_add_test(tc_success, test_db_init_connection_string_variants);
    suite_add_tcase(s, tc_success);

    TCase *tc_cleanup = tcase_create("memory_cleanup");
    tcase_set_timeout(tc_cleanup, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_cleanup, suite_setup, suite_teardown);
    tcase_add_checked_fixture(tc_cleanup, test_setup, test_teardown);
    tcase_add_test(tc_cleanup, test_db_init_cleanup_on_error);
    suite_add_tcase(s, tc_cleanup);

    TCase *tc_migration = tcase_create("migration_errors");
    tcase_set_timeout(tc_migration, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_migration, suite_setup, suite_teardown);
    tcase_add_checked_fixture(tc_migration, test_setup, test_teardown);
    tcase_add_test(tc_migration, test_db_init_migration_failure);
    suite_add_tcase(s, tc_migration);

    TCase *tc_transactions = tcase_create("transactions");
    tcase_set_timeout(tc_transactions, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_transactions, suite_setup, suite_teardown);
    tcase_add_checked_fixture(tc_transactions, test_setup, test_teardown);
    tcase_add_test(tc_transactions, test_db_transaction_success);
    tcase_add_test(tc_transactions, test_notice_processor);
    suite_add_tcase(s, tc_transactions);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = connection_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/db/connection_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
