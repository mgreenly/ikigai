#include "tests/helpers/test_utils_helper.h"

#include "apps/ikigai/db/migration.h"
#include "shared/panic.h"

#include <libpq-fe.h>
#include <talloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

// ========== Database Test Utilities ==========

// Get PostgreSQL host from environment or default to localhost
static const char *get_pg_host(void)
{
    const char *host = getenv("PGHOST");
    return host ? host : "localhost";
}

// Build admin database URL
// Using __thread to ensure each test file running in parallel has its own buffer
static char *get_admin_db_url(void)
{
    static __thread char buf[256];
    snprintf(buf, sizeof(buf), "postgresql://ikigai:ikigai@%s/postgres", get_pg_host());
    return buf;
}

// Build test database URL prefix
static void get_test_db_url(char *buf, size_t bufsize, const char *db_name)
{
    snprintf(buf, bufsize, "postgresql://ikigai:ikigai@%s/%s", get_pg_host(), db_name);
}

void ik_test_db_conn_str(char *buf, size_t bufsize, const char *db_name)
{
    get_test_db_url(buf, bufsize, db_name);
}

/**
 * Talloc destructor for raw database context (no auto-migrations)
 */
static int raw_db_ctx_destructor(ik_db_ctx_t *ctx)
{
    if (ctx != NULL && ctx->conn != NULL) {
        PQfinish(ctx->conn);
        ctx->conn = NULL;
    }
    return 0;
}

res_t ik_test_db_create(const char *db_name)
{
    // Create temporary context for error allocations
    TALLOC_CTX *tmp_ctx = talloc_new(NULL);
    if (tmp_ctx == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    if (db_name == NULL) {
        res_t res = ERR(tmp_ctx, INVALID_ARG, "db_name cannot be NULL");
        // Don't free tmp_ctx - error is parented to it and caller will free
        return res;
    }

    // Connect to admin database
    PGconn *conn = PQconnectdb(get_admin_db_url());
    if (conn == NULL) {
        res_t res = ERR(tmp_ctx, DB_CONNECT, "Failed to allocate connection");
        // Don't free tmp_ctx - error is parented to it and caller will free
        return res;
    }

    if (PQstatus(conn) != CONNECTION_OK) {
        // Copy error message before closing connection (PQerrorMessage points to internal buffer)
        char err_copy[256];
        snprintf(err_copy, sizeof(err_copy), "%s", PQerrorMessage(conn));
        PQfinish(conn);
        res_t res = ERR(tmp_ctx, DB_CONNECT, "Failed to connect to admin database: %s", err_copy);
        // Don't free tmp_ctx - error is parented to it and caller will free
        return res;
    }

    // Suppress NOTICE messages (e.g., "database does not exist, skipping")
    PGresult *notice_result = PQexec(conn, "SET client_min_messages = WARNING");
    PQclear(notice_result);

    // Drop database if exists (terminate connections first)
    char drop_conns[512];
    snprintf(drop_conns, sizeof(drop_conns),
             "SELECT pg_terminate_backend(pid) FROM pg_stat_activity "
             "WHERE datname = '%s' AND pid <> pg_backend_pid()",
             db_name);
    PGresult *result = PQexec(conn, drop_conns);
    PQclear(result);

    char drop_cmd[256];
    snprintf(drop_cmd, sizeof(drop_cmd), "DROP DATABASE IF EXISTS %s", db_name);
    result = PQexec(conn, drop_cmd);
    if (PQresultStatus(result) != PGRES_COMMAND_OK) {
        // Copy error message before closing connection (PQerrorMessage points to internal buffer)
        char err_copy[256];
        snprintf(err_copy, sizeof(err_copy), "%s", PQerrorMessage(conn));
        PQclear(result);
        PQfinish(conn);
        res_t res = ERR(tmp_ctx, DB_CONNECT, "Failed to drop database: %s", err_copy);
        // Don't free tmp_ctx - error is parented to it and caller will free
        return res;
    }
    PQclear(result);

    // Under ThreadSanitizer, give terminated connections time to fully close
    // to avoid race conditions where another test's suite_teardown drops this database
    usleep(200000);  // 200ms - negligible overhead, prevents TSAN timing races

    // Create fresh database
    char create_cmd[256];
    snprintf(create_cmd, sizeof(create_cmd), "CREATE DATABASE %s", db_name);
    result = PQexec(conn, create_cmd);
    if (PQresultStatus(result) != PGRES_COMMAND_OK) {
        // Copy error message before closing connection (PQerrorMessage points to internal buffer)
        char err_copy[256];
        snprintf(err_copy, sizeof(err_copy), "%s", PQerrorMessage(conn));
        PQclear(result);
        PQfinish(conn);
        res_t res = ERR(tmp_ctx, DB_CONNECT, "Failed to create database: %s", err_copy);
        // Don't free tmp_ctx - error is parented to it and caller will free
        return res;
    }
    PQclear(result);

    PQfinish(conn);

    // Give database creation time to fully complete before attempting to connect
    // Prevents "database does not exist" errors on fast test suites
    usleep(50000);  // 50ms - minimal overhead, ensures database is ready

    // Success - clean up temp context since no error was created
    talloc_free(tmp_ctx);
    return OK(NULL);
}

res_t ik_test_db_migrate(TALLOC_CTX *ctx, const char *db_name)
{
    TALLOC_CTX *tmp_ctx = ctx ? ctx : talloc_new(NULL);

    if (db_name == NULL) {
        res_t res = ERR(tmp_ctx, INVALID_ARG, "db_name cannot be NULL");
        if (ctx == NULL) talloc_free(tmp_ctx);
        return res;
    }

    // Connect to the test database
    ik_db_ctx_t *db = NULL;
    res_t res = ik_test_db_connect(tmp_ctx, db_name, &db);
    if (is_err(&res)) {
        // Reparent error to NULL before freeing tmp_ctx
        if (ctx == NULL) {
            talloc_steal(NULL, res.err);
            talloc_free(tmp_ctx);
        }
        return res;
    }

    // Run migrations
    res = ik_db_migrate(db, "share/ikigai/migrations");
    if (is_err(&res)) {
        // Reparent error to NULL before freeing tmp_ctx (error is attached to db, which is a child of tmp_ctx)
        if (ctx == NULL) {
            talloc_steal(NULL, res.err);
            talloc_free(tmp_ctx);
        }
        return res;
    }

    if (ctx == NULL) {
        talloc_free(tmp_ctx);
    }

    return OK(NULL);
}

res_t ik_test_db_connect(TALLOC_CTX *ctx, const char *db_name, ik_db_ctx_t **out)
{
    if (ctx == NULL) {
        // Create temporary context for this error only
        TALLOC_CTX *tmp_ctx = talloc_new(NULL);
        if (tmp_ctx == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        return ERR(tmp_ctx, INVALID_ARG, "ctx cannot be NULL");
    }
    if (db_name == NULL) {
        return ERR(ctx, INVALID_ARG, "db_name cannot be NULL");
    }
    if (out == NULL) {
        return ERR(ctx, INVALID_ARG, "out cannot be NULL");
    }

    // Build connection string
    char conn_str[256];
    get_test_db_url(conn_str, sizeof(conn_str), db_name);

    // Allocate database context
    ik_db_ctx_t *db_ctx = talloc_zero(ctx, ik_db_ctx_t);
    if (db_ctx == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    talloc_set_destructor(db_ctx, raw_db_ctx_destructor);

    // Connect to database
    db_ctx->conn = PQconnectdb(conn_str);
    if (db_ctx->conn == NULL) {
        talloc_free(db_ctx);
        return ERR(ctx, DB_CONNECT, "Failed to allocate connection");
    }

    if (PQstatus(db_ctx->conn) != CONNECTION_OK) {
        // Copy error message before freeing db_ctx (PQerrorMessage points to internal buffer)
        char err_copy[256];
        snprintf(err_copy, sizeof(err_copy), "%s", PQerrorMessage(db_ctx->conn));
        talloc_free(db_ctx);
        return ERR(ctx, DB_CONNECT, "Failed to connect to database: %s", err_copy);
    }

    *out = db_ctx;
    return OK(db_ctx);
}

res_t ik_test_db_begin(ik_db_ctx_t *db)
{
    if (db == NULL || db->conn == NULL) {
        TALLOC_CTX *tmp_ctx = talloc_new(NULL);
        if (tmp_ctx == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        return ERR(tmp_ctx, INVALID_ARG, "db cannot be NULL");
    }

    PGresult *result = PQexec(db->conn, "BEGIN");
    if (PQresultStatus(result) != PGRES_COMMAND_OK) {
        const char *pq_err = PQerrorMessage(db->conn);
        PQclear(result);
        TALLOC_CTX *tmp_ctx = talloc_new(NULL);
        if (tmp_ctx == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        return ERR(tmp_ctx, DB_CONNECT, "BEGIN failed: %s", pq_err);
    }
    PQclear(result);

    return OK(NULL);
}

res_t ik_test_db_rollback(ik_db_ctx_t *db)
{
    if (db == NULL || db->conn == NULL) {
        TALLOC_CTX *tmp_ctx = talloc_new(NULL);
        if (tmp_ctx == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        return ERR(tmp_ctx, INVALID_ARG, "db cannot be NULL");
    }

    PGresult *result = PQexec(db->conn, "ROLLBACK");
    if (PQresultStatus(result) != PGRES_COMMAND_OK) {
        const char *pq_err = PQerrorMessage(db->conn);
        PQclear(result);
        TALLOC_CTX *tmp_ctx = talloc_new(NULL);
        if (tmp_ctx == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        return ERR(tmp_ctx, DB_CONNECT, "ROLLBACK failed: %s", pq_err);
    }
    PQclear(result);

    return OK(NULL);
}

res_t ik_test_db_truncate_all(ik_db_ctx_t *db)
{
    if (db == NULL || db->conn == NULL) {
        TALLOC_CTX *tmp_ctx = talloc_new(NULL);
        if (tmp_ctx == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        return ERR(tmp_ctx, INVALID_ARG, "db cannot be NULL");
    }

    // Truncate all application tables (order matters due to FK constraints)
    const char *truncate_sql =
        "TRUNCATE TABLE agents, messages, sessions RESTART IDENTITY CASCADE";

    PGresult *result = PQexec(db->conn, truncate_sql);
    if (PQresultStatus(result) != PGRES_COMMAND_OK) {
        const char *pq_err = PQerrorMessage(db->conn);
        PQclear(result);
        TALLOC_CTX *tmp_ctx = talloc_new(NULL);
        if (tmp_ctx == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        return ERR(tmp_ctx, DB_CONNECT, "TRUNCATE failed: %s", pq_err);
    }
    PQclear(result);

    return OK(NULL);
}

res_t ik_test_db_destroy(const char *db_name)
{
    // Create temporary context for error allocations
    TALLOC_CTX *tmp_ctx = talloc_new(NULL);
    if (tmp_ctx == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    if (db_name == NULL) {
        return ERR(tmp_ctx, INVALID_ARG, "db_name cannot be NULL");
    }

    // Connect to admin database
    PGconn *conn = PQconnectdb(get_admin_db_url());
    if (conn == NULL) {
        return ERR(tmp_ctx, DB_CONNECT, "Failed to allocate connection");
    }

    if (PQstatus(conn) != CONNECTION_OK) {
        // Copy error message before closing connection (PQerrorMessage points to internal buffer)
        char err_copy[256];
        snprintf(err_copy, sizeof(err_copy), "%s", PQerrorMessage(conn));
        PQfinish(conn);
        return ERR(tmp_ctx, DB_CONNECT, "Failed to connect to admin database: %s", err_copy);
    }

    // Suppress NOTICE messages (e.g., "database does not exist, skipping")
    PGresult *notice_result = PQexec(conn, "SET client_min_messages = WARNING");
    PQclear(notice_result);

    // Terminate any remaining connections
    char drop_conns[512];
    snprintf(drop_conns, sizeof(drop_conns),
             "SELECT pg_terminate_backend(pid) FROM pg_stat_activity "
             "WHERE datname = '%s' AND pid <> pg_backend_pid()",
             db_name);
    PGresult *result = PQexec(conn, drop_conns);
    PQclear(result);

    // Under ThreadSanitizer, give terminated connections time to fully close
    // to avoid "database does not exist" race conditions during sequential test runs
    usleep(200000);  // 200ms - negligible overhead, prevents TSAN timing races

    // Drop database
    char drop_cmd[256];
    snprintf(drop_cmd, sizeof(drop_cmd), "DROP DATABASE IF EXISTS %s", db_name);
    result = PQexec(conn, drop_cmd);
    if (PQresultStatus(result) != PGRES_COMMAND_OK) {
        // Copy error message before closing connection (PQerrorMessage points to internal buffer)
        char err_copy[256];
        snprintf(err_copy, sizeof(err_copy), "%s", PQerrorMessage(conn));
        PQclear(result);
        PQfinish(conn);
        return ERR(tmp_ctx, DB_CONNECT, "Failed to drop database: %s", err_copy);
    }
    PQclear(result);

    PQfinish(conn);

    // Success - clean up temp context
    talloc_free(tmp_ctx);
    return OK(NULL);
}
