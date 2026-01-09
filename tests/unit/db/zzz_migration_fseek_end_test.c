#include "../../../src/db/migration.h"
#include "../../../src/db/pg_result.h"
#include "../../../src/error.h"
#include "../../../src/wrapper.h"
#include "../../test_utils.h"

#include <check.h>
#include <libpq-fe.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <talloc.h>
#include <unistd.h>

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

static char *create_test_migration_dir(TALLOC_CTX *ctx)
{
    char *template = talloc_strdup(ctx, "/tmp/ik_migration_test_XXXXXX");
    char *dir = mkdtemp(template);
    if (!dir) {
        return NULL;
    }
    return talloc_strdup(ctx, template);
}

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

static void cleanup_test_dir(const char *dir)
{
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", dir);
    int result = system(cmd);
    (void)result;
}

// Mock fseek that fails on SEEK_END
int fseek_(FILE *stream, long offset, int whence)
{
    if (whence == SEEK_END) {
        return -1;  // Simulate failure
    }
    return fseek(stream, offset, whence);
}

START_TEST(test_migration_fseek_seek_end_failure) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    char conn_str[256];
    ik_test_db_conn_str(conn_str, sizeof(conn_str), DB_NAME);

    ik_db_ctx_t *db_ctx = create_db_ctx_no_migrate(ctx, conn_str);
    ck_assert_ptr_nonnull(db_ctx);

    ik_db_wrap_pg_result(ctx, PQexec(db_ctx->conn, "DROP TABLE IF EXISTS schema_metadata"));

    char *test_dir = create_test_migration_dir(ctx);
    ck_assert_ptr_nonnull(test_dir);

    create_migration_file(test_dir, "0001-init.sql", "SELECT 1;");

    res_t res = ik_db_migrate(db_ctx, test_dir);
    ck_assert(is_err(&res));
    ck_assert_int_eq(error_code(res.err), ERR_IO);

    cleanup_test_dir(test_dir);
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
    Suite *s = suite_create("db_migration_fseek_end");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);
    tcase_add_checked_fixture(tc_core, migration_test_setup, NULL);
    tcase_add_test(tc_core, test_migration_fseek_seek_end_failure);
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
