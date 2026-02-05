#include "apps/ikigai/db/connection.h"
#include "shared/error.h"
#include "tests/helpers/test_utils_helper.h"

#include <check.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <talloc.h>
#include <unistd.h>

static TALLOC_CTX *test_ctx;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    if (test_ctx != NULL) {
        talloc_free(test_ctx);
        test_ctx = NULL;
    }
}

// Test: db_init constructs correct migrations path from data_dir
START_TEST(test_db_init_uses_data_dir) {
    // Skip if no PostgreSQL available
    if (getenv("SKIP_LIVE_DB_TESTS")) {
        ck_assert(1);
        return;
    }

    // Create temp directory for fake migrations
    char temp_dir[256];
    snprintf(temp_dir, sizeof(temp_dir), "/tmp/ikigai_test_%d", getpid());
    mkdir(temp_dir, 0755);

    char migrations_dir[512];
    snprintf(migrations_dir, sizeof(migrations_dir), "%s/migrations", temp_dir);
    mkdir(migrations_dir, 0755);

    // Create a minimal migration file
    char migration_file[768];
    snprintf(migration_file, sizeof(migration_file), "%s/001-test.sql", migrations_dir);
    FILE *f = fopen(migration_file, "w");
    fprintf(f, "CREATE TABLE test_table (id INTEGER);\n");
    fclose(f);

    // Get PostgreSQL host from environment or default to localhost
    const char *pg_host = getenv("PGHOST");
    if (pg_host == NULL) pg_host = "localhost";

    // Build connection string
    char conn_str[256];
    snprintf(conn_str, sizeof(conn_str), "postgresql://ikigai:ikigai@%s/postgres", pg_host);

    // Call ik_db_init with custom data_dir
    ik_db_ctx_t *db = NULL;
    res_t result = ik_db_init(test_ctx, conn_str, temp_dir, &db);

    // Should succeed if migrations were found at data_dir/migrations
    // (or fail for other reasons, but not "Cannot open migrations directory")
    if (is_err(&result)) {
        const char *msg = error_message(result.err);
        ck_assert_msg(strstr(msg, "Cannot open migrations directory") == NULL,
                      "ik_db_init should use data_dir/migrations, not hardcoded path");
    }

    // Cleanup
    if (db != NULL) {
        talloc_free(db);
    }
    unlink(migration_file);
    rmdir(migrations_dir);
    rmdir(temp_dir);
}
END_TEST

// Test: db_init fails gracefully when migrations dir missing
START_TEST(test_db_init_missing_migrations) {
    if (getenv("SKIP_LIVE_DB_TESTS")) {
        ck_assert(1);
        return;
    }

    // Get PostgreSQL host from environment or default to localhost
    const char *pg_host = getenv("PGHOST");
    if (pg_host == NULL) pg_host = "localhost";

    // Build connection string
    char conn_str[256];
    snprintf(conn_str, sizeof(conn_str), "postgresql://ikigai:ikigai@%s/postgres", pg_host);

    ik_db_ctx_t *db = NULL;
    res_t result = ik_db_init(test_ctx, conn_str, "/nonexistent", &db);

    ck_assert(is_err(&result));
    ck_assert_int_eq(error_code(result.err), ERR_IO);
    ck_assert(strstr(error_message(result.err), "Cannot open migrations directory") != NULL);
}
END_TEST

static Suite *connection_paths_suite(void)
{
    Suite *s = suite_create("db_connection_paths");

    TCase *tc_paths = tcase_create("data_dir_usage");
    tcase_add_checked_fixture(tc_paths, setup, teardown);
    tcase_add_test(tc_paths, test_db_init_uses_data_dir);
    tcase_add_test(tc_paths, test_db_init_missing_migrations);
    suite_add_tcase(s, tc_paths);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = connection_paths_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/db/connection_paths_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
