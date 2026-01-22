#include "../../src/paths.h"
#include "../../src/db/connection.h"
#include "../../src/error.h"
#include "../test_utils_helper.h"

#include <check.h>
#include <stdlib.h>
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
    test_paths_cleanup_env();
}

START_TEST(test_paths_to_db_init_integration) {
    if (getenv("SKIP_LIVE_DB_TESTS")) {
        ck_assert(1);
        return;
    }

    // Setup paths environment with migrations directory
    const char *test_prefix = test_paths_setup_env();
    ck_assert_ptr_nonnull(test_prefix);

    // Create paths instance
    ik_paths_t *paths = NULL;
    res_t result = ik_paths_init(test_ctx, &paths);
    ck_assert(is_ok(&result));

    // Get data_dir from paths
    const char *data_dir = ik_paths_get_data_dir(paths);
    ck_assert_ptr_nonnull(data_dir);

    // Create migrations directory (test_paths_setup_env should have created it)
    char migrations_path[512];
    snprintf(migrations_path, sizeof(migrations_path), "%s/migrations", data_dir);

    struct stat st;
    ck_assert_int_eq(stat(migrations_path, &st), 0);
    ck_assert(S_ISDIR(st.st_mode));

    // Create a minimal migration file for testing
    char migration_file[768];
    snprintf(migration_file, sizeof(migration_file), "%s/001-test.sql", migrations_path);
    FILE *f = fopen(migration_file, "w");
    ck_assert_ptr_nonnull(f);
    fprintf(f, "CREATE TABLE IF NOT EXISTS test_integration (id INTEGER);\n");
    fclose(f);

    // Get PostgreSQL host from environment or default to localhost
    const char *pg_host = getenv("PGHOST");
    if (pg_host == NULL) pg_host = "localhost";

    // Build connection string
    char conn_str[256];
    snprintf(conn_str, sizeof(conn_str), "postgresql://ikigai:ikigai@%s/postgres", pg_host);

    // Now db_init should work
    ik_db_ctx_t *db = NULL;
    result = ik_db_init(test_ctx, conn_str, data_dir, &db);

    // Should not fail with "Cannot open migrations directory"
    if (is_err(&result)) {
        const char *msg = error_message(result.err);
        ck_assert_msg(strstr(msg, "Cannot open migrations directory") == NULL,
                      "Integration: paths -> db_init should find migrations at %s", migrations_path);
    }

    // Cleanup
    if (db != NULL) {
        talloc_free(db);
    }
}
END_TEST

static Suite *paths_db_integration_suite(void)
{
    Suite *s = suite_create("paths_db_integration");

    TCase *tc_integration = tcase_create("end_to_end");
    tcase_add_checked_fixture(tc_integration, setup, teardown);
    tcase_add_test(tc_integration, test_paths_to_db_init_integration);
    suite_add_tcase(s, tc_integration);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = paths_db_integration_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/integration/paths_db_integration_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
