#include "connection.h"
#include "migration.h"
#include "../error.h"
#include "../panic.h"
#include <libpq-fe.h>
#include <string.h>
#include <talloc.h>

/**
 * Talloc destructor for database context
 *
 * Automatically called when db_ctx is freed via talloc.
 * Ensures PQfinish() is called to properly close connection.
 *
 * @param ctx Database context being destroyed
 * @return 0 (talloc destructor return value)
 */
static int ik_db_ctx_destructor(ik_db_ctx_t *ctx)
{
    assert(ctx != NULL); // LCOV_EXCL_BR_LINE

    if (ctx->conn != NULL) {  // LCOV_EXCL_BR_LINE
        PQfinish(ctx->conn);
        ctx->conn = NULL;
    }

    return 0;
}

/**
 * Validate connection string format
 *
 * Performs basic validation of PostgreSQL connection string.
 * Accepts postgresql:// or postgres:// prefixes.
 *
 * @param conn_str Connection string to validate
 * @return true if format is valid, false otherwise
 */
static bool validate_conn_str(const char *conn_str)
{
    assert(conn_str != NULL); // LCOV_EXCL_BR_LINE

    // Check for empty string
    if (strlen(conn_str) == 0) {
        return false;
    }

    // Accept both postgresql:// and postgres:// schemes
    if (strncmp(conn_str, "postgresql://", 13) == 0) {
        return true;
    }
    if (strncmp(conn_str, "postgres://", 11) == 0) {
        return true;
    }

    // Also accept libpq key=value format (doesn't start with postgresql://)
    // libpq will validate the actual format
    // For now, if it doesn't start with postgresql://, we'll let libpq handle it
    // This allows for formats like: "host=localhost dbname=mydb"
    return true;
}

res_t ik_db_init(TALLOC_CTX *ctx, const char *conn_str, ik_db_ctx_t **out_ctx)
{
    return ik_db_init_with_migrations(ctx, conn_str, "migrations", out_ctx);
}

res_t ik_db_init_with_migrations(TALLOC_CTX *ctx, const char *conn_str, const char *migrations_dir, ik_db_ctx_t **out_ctx)
{
    // Validate input parameters
    assert(ctx != NULL);            // LCOV_EXCL_BR_LINE
    assert(conn_str != NULL);       // LCOV_EXCL_BR_LINE
    assert(migrations_dir != NULL); // LCOV_EXCL_BR_LINE
    assert(out_ctx != NULL);        // LCOV_EXCL_BR_LINE

    // Validate connection string format
    if (!validate_conn_str(conn_str)) {
        return ERR(ctx, INVALID_ARG, "Invalid connection string format");
    }

    // Allocate database context on caller's talloc context
    ik_db_ctx_t *db_ctx = talloc_zero(ctx, ik_db_ctx_t);
    if (db_ctx == NULL) {                 // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");           // LCOV_EXCL_LINE
    }

    // Register destructor to clean up connection
    talloc_set_destructor(db_ctx, ik_db_ctx_destructor);

    // Attempt to connect to database
    db_ctx->conn = PQconnectdb(conn_str);
    if (db_ctx->conn == NULL) {           // LCOV_EXCL_BR_LINE
        // PQconnectdb returned NULL - out of memory in libpq
        talloc_free(db_ctx);              // LCOV_EXCL_LINE
        PANIC("Out of memory");           // LCOV_EXCL_LINE
    }

    // Check connection status
    if (PQstatus(db_ctx->conn) != CONNECTION_OK) {  // LCOV_EXCL_BR_LINE
        // Connection failed - get error message from libpq
        const char *pq_err = PQerrorMessage(db_ctx->conn);

        // Create error with libpq's message
        res_t result = ERR(ctx, DB_CONNECT, "Database connection failed: %s", pq_err);

        // Clean up failed connection
        talloc_free(db_ctx);

        return result;
    }

    // Success - database connected
    // Now run pending migrations from specified migrations directory
    res_t migrate_result = ik_db_migrate(db_ctx, migrations_dir);
    if (is_err(&migrate_result)) {
        // Migration failed - reparent error to ctx before cleanup
        talloc_steal(ctx, migrate_result.err);
        talloc_free(db_ctx);
        return migrate_result;
    }

    // All good - return database context
    *out_ctx = db_ctx;
    return OK(db_ctx);
}

res_t ik_db_begin(ik_db_ctx_t *db_ctx)
{
    assert(db_ctx != NULL);  // LCOV_EXCL_BR_LINE
    assert(db_ctx->conn != NULL);  // LCOV_EXCL_BR_LINE

    PGresult *res = PQexec(db_ctx->conn, "BEGIN");
    if (res == NULL) {  // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");  // LCOV_EXCL_LINE
    }

    ExecStatusType status = PQresultStatus(res);
    if (status != PGRES_COMMAND_OK) {
        const char *err_msg = PQerrorMessage(db_ctx->conn);
        res_t result = ERR(db_ctx, IO, "BEGIN failed: %s", err_msg);
        PQclear(res);
        return result;
    }

    PQclear(res);
    return OK(NULL);
}

res_t ik_db_commit(ik_db_ctx_t *db_ctx)
{
    assert(db_ctx != NULL);  // LCOV_EXCL_BR_LINE
    assert(db_ctx->conn != NULL);  // LCOV_EXCL_BR_LINE

    PGresult *res = PQexec(db_ctx->conn, "COMMIT");
    if (res == NULL) {  // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");  // LCOV_EXCL_LINE
    }

    ExecStatusType status = PQresultStatus(res);
    if (status != PGRES_COMMAND_OK) {
        const char *err_msg = PQerrorMessage(db_ctx->conn);
        res_t result = ERR(db_ctx, IO, "COMMIT failed: %s", err_msg);
        PQclear(res);
        return result;
    }

    PQclear(res);
    return OK(NULL);
}

res_t ik_db_rollback(ik_db_ctx_t *db_ctx)
{
    assert(db_ctx != NULL);  // LCOV_EXCL_BR_LINE
    assert(db_ctx->conn != NULL);  // LCOV_EXCL_BR_LINE

    PGresult *res = PQexec(db_ctx->conn, "ROLLBACK");
    if (res == NULL) {  // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");  // LCOV_EXCL_LINE
    }

    ExecStatusType status = PQresultStatus(res);
    if (status != PGRES_COMMAND_OK) {
        const char *err_msg = PQerrorMessage(db_ctx->conn);
        res_t result = ERR(db_ctx, IO, "ROLLBACK failed: %s", err_msg);
        PQclear(res);
        return result;
    }

    PQclear(res);
    return OK(NULL);
}
