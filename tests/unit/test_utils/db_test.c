/**
 * @file db_test.c
 * @brief Tests for database test utilities
 */

#include "../../test_utils.h"
#include "../../../src/db/connection.h"
#include "../../../src/error.h"
#include <check.h>
#include <libpq-fe.h>
#include <string.h>
#include <talloc.h>

// Test: ik_test_db_name derives correct name from file path
START_TEST(test_db_name_from_file_path) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    const char *name = ik_test_db_name(ctx, "tests/unit/db/session_test.c");
    ck_assert_str_eq(name, "ikigai_test_session_test");

    talloc_free(ctx);
}
END_TEST
// Test: ik_test_db_name handles nested paths
START_TEST(test_db_name_from_nested_path) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    const char *name = ik_test_db_name(ctx, "tests/unit/commands/mark_db_test.c");
    ck_assert_str_eq(name, "ikigai_test_mark_db_test");

    talloc_free(ctx);
}

END_TEST
// Test: ik_test_db_name handles simple filename
START_TEST(test_db_name_simple_file) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    const char *name = ik_test_db_name(ctx, "foo_test.c");
    ck_assert_str_eq(name, "ikigai_test_foo_test");

    talloc_free(ctx);
}

END_TEST
// Test: Full lifecycle - create, migrate, connect, begin, rollback, destroy
START_TEST(test_db_full_lifecycle) {
    const char *skip_live = getenv("SKIP_LIVE_DB_TESTS");
    if (skip_live && strcmp(skip_live, "1") == 0) {
        return;  // Skip if DB not available
    }

    // Get db_name with NULL ctx so it uses static buffer (survives talloc_free)
    const char *db_name = ik_test_db_name(NULL, __FILE__);
    TALLOC_CTX *ctx = talloc_new(NULL);

    // Create database
    res_t res = ik_test_db_create(db_name);
    ck_assert_msg(is_ok(&res), "Failed to create database");

    // Run migrations
    res = ik_test_db_migrate(ctx, db_name);
    ck_assert_msg(is_ok(&res), "Failed to migrate database");

    // Connect to database
    ik_db_ctx_t *db = NULL;
    res = ik_test_db_connect(ctx, db_name, &db);
    ck_assert_msg(is_ok(&res), "Failed to connect to database");
    ck_assert_ptr_nonnull(db);

    // Begin transaction
    res = ik_test_db_begin(db);
    ck_assert_msg(is_ok(&res), "Failed to begin transaction");

    // Insert something
    PGresult *result = PQexec(db->conn, "INSERT INTO sessions DEFAULT VALUES RETURNING id");
    ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
    PQclear(result);

    // Rollback transaction
    res = ik_test_db_rollback(db);
    ck_assert_msg(is_ok(&res), "Failed to rollback transaction");

    // Verify rollback worked - sessions table should be empty
    result = PQexec(db->conn, "SELECT COUNT(*) FROM sessions");
    ck_assert_int_eq(PQresultStatus(result), PGRES_TUPLES_OK);
    ck_assert_str_eq(PQgetvalue(result, 0, 0), "0");
    PQclear(result);

    talloc_free(ctx);

    // Destroy database
    res = ik_test_db_destroy(db_name);
    ck_assert_msg(is_ok(&res), "Failed to destroy database");
}

END_TEST
// Test: Truncate all tables
START_TEST(test_db_truncate_all) {
    const char *skip_live = getenv("SKIP_LIVE_DB_TESTS");
    if (skip_live && strcmp(skip_live, "1") == 0) {
        return;  // Skip if DB not available
    }

    const char *db_name = ik_test_db_name(NULL, __FILE__);
    TALLOC_CTX *ctx = talloc_new(NULL);

    // Setup
    res_t res = ik_test_db_create(db_name);
    ck_assert(is_ok(&res));
    res = ik_test_db_migrate(ctx, db_name);
    ck_assert(is_ok(&res));

    ik_db_ctx_t *db = NULL;
    res = ik_test_db_connect(ctx, db_name, &db);
    ck_assert(is_ok(&res));

    // Insert data (not in transaction)
    PGresult *result = PQexec(db->conn, "INSERT INTO sessions DEFAULT VALUES");
    ck_assert_int_eq(PQresultStatus(result), PGRES_COMMAND_OK);
    PQclear(result);

    // Verify data exists
    result = PQexec(db->conn, "SELECT COUNT(*) FROM sessions");
    ck_assert_str_eq(PQgetvalue(result, 0, 0), "1");
    PQclear(result);

    // Truncate all
    res = ik_test_db_truncate_all(db);
    ck_assert_msg(is_ok(&res), "Failed to truncate tables");

    // Verify data gone
    result = PQexec(db->conn, "SELECT COUNT(*) FROM sessions");
    ck_assert_str_eq(PQgetvalue(result, 0, 0), "0");
    PQclear(result);

    talloc_free(ctx);
    ik_test_db_destroy(db_name);
}

END_TEST
// Test: Create without migrate (for migration tests)
START_TEST(test_db_create_without_migrate) {
    const char *skip_live = getenv("SKIP_LIVE_DB_TESTS");
    if (skip_live && strcmp(skip_live, "1") == 0) {
        return;  // Skip if DB not available
    }

    const char *db_name = ik_test_db_name(NULL, __FILE__);
    TALLOC_CTX *ctx = talloc_new(NULL);

    // Create database without running migrations
    res_t res = ik_test_db_create(db_name);
    ck_assert(is_ok(&res));

    // Connect directly (without using ik_db_init which runs migrations)
    ik_db_ctx_t *db = NULL;
    res = ik_test_db_connect(ctx, db_name, &db);
    ck_assert(is_ok(&res));

    // Verify sessions table doesn't exist (no migrations ran)
    PGresult *result = PQexec(db->conn, "SELECT COUNT(*) FROM sessions");
    ck_assert_int_eq(PQresultStatus(result), PGRES_FATAL_ERROR);
    PQclear(result);

    talloc_free(ctx);
    ik_test_db_destroy(db_name);
}

END_TEST

// Suite configuration
static Suite *test_utils_db_suite(void)
{
    Suite *s = suite_create("test_utils_db");

    TCase *tc_name = tcase_create("DB Name Derivation");
    tcase_set_timeout(tc_name, 30);
    tcase_set_timeout(tc_name, 30);
    tcase_set_timeout(tc_name, 30);
    tcase_set_timeout(tc_name, 30);
    tcase_add_test(tc_name, test_db_name_from_file_path);
    tcase_add_test(tc_name, test_db_name_from_nested_path);
    tcase_add_test(tc_name, test_db_name_simple_file);
    suite_add_tcase(s, tc_name);

    TCase *tc_lifecycle = tcase_create("DB Lifecycle");
    tcase_set_timeout(tc_lifecycle, 30);
    tcase_set_timeout(tc_lifecycle, 30);
    tcase_set_timeout(tc_lifecycle, 30);
    tcase_set_timeout(tc_lifecycle, 30);
    tcase_set_timeout(tc_lifecycle, 30);
    tcase_add_test(tc_lifecycle, test_db_full_lifecycle);
    tcase_add_test(tc_lifecycle, test_db_truncate_all);
    tcase_add_test(tc_lifecycle, test_db_create_without_migrate);
    suite_add_tcase(s, tc_lifecycle);

    return s;
}

int main(void)
{
    Suite *s = test_utils_db_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
