#include "../../../src/db/migration.h"
#include "../../../src/db/pg_result.h"
#include "../../../src/error.h"
#include "../../../src/wrapper.h"
#include "../../test_utils_helper.h"

#include <check.h>
#include <dirent.h>
#include <errno.h>
#include <libpq-fe.h>
#include <stdio.h>
#include <string.h>
#include <talloc.h>

static const char *DB_NAME = NULL;

static int ik_db_ctx_destructor(ik_db_ctx_t *ctx)
{
    if (ctx->conn != NULL) {
        PQfinish(ctx->conn);
        ctx->conn = NULL;
    }
    return 0;
}

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

// Mock opendir that always fails
DIR *opendir_(const char *name)
{
    (void)name;
    errno = ENOENT;
    return NULL;
}

START_TEST(test_migration_opendir_failure) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    char conn_str[256];
    ik_test_db_conn_str(conn_str, sizeof(conn_str), DB_NAME);

    ik_db_ctx_t *db_ctx = create_db_ctx_no_migrate(ctx, conn_str);
    ck_assert_ptr_nonnull(db_ctx);

    // Should fail because opendir returns NULL
    res_t res = ik_db_migrate(db_ctx, "/nonexistent/directory");
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);

    talloc_free(ctx);
}
END_TEST

static void suite_setup(void)
{
    DB_NAME = ik_test_db_name(NULL, __FILE__);
    ik_test_db_create(DB_NAME);
}

static void suite_teardown(void)
{
    ik_test_db_destroy(DB_NAME);
}

static void migration_test_setup(void)
{
    TALLOC_CTX *tmp_ctx = talloc_new(NULL);
    ik_db_ctx_t *db = NULL;

    res_t res = ik_test_db_connect(tmp_ctx, DB_NAME, &db);
    if (is_ok(&res)) {
        ik_db_wrap_pg_result(tmp_ctx, PQexec(db->conn,
                                             "DROP TABLE IF EXISTS schema_metadata CASCADE"));
    }

    talloc_free(tmp_ctx);
}

static Suite *migration_suite(void)
{
    Suite *s = suite_create("db_migration_opendir");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_core, migration_test_setup, NULL);
    tcase_add_test(tc_core, test_migration_opendir_failure);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    suite_setup();

    int number_failed;
    Suite *s = migration_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/db/zzz_migration_opendir_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    suite_teardown();

    return (number_failed == 0) ? 0 : 1;
}
