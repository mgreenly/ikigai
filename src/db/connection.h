#ifndef IK_DB_CONNECTION_H
#define IK_DB_CONNECTION_H

#include "../error.h"
#include <libpq-fe.h>
#include <talloc.h>

/**
 * Database context - manages PostgreSQL connection
 *
 * Lifecycle:
 * - Created by ik_db_init() as child of caller's talloc context
 * - Destructor automatically calls PQfinish() on conn
 * - Free with talloc_free() on parent context
 */
typedef struct {
    PGconn *conn;  // PostgreSQL connection handle
} ik_db_ctx_t;

/**
 * Initialize database connection
 *
 * Creates a new database context and establishes connection to PostgreSQL
 * using the provided connection string. Runs migrations from data directory.
 *
 * Connection string format:
 *   postgresql://[user[:password]@][host][:port][/dbname]
 *
 * Examples:
 *   postgresql://localhost
 *   postgresql://localhost/mydb
 *   postgresql://user:pass@localhost:5432/mydb
 *
 * Memory management:
 * - db_ctx allocated as child of ctx
 * - Talloc destructor registered to call PQfinish()
 * - Single talloc_free(ctx) cleans up everything
 *
 * @param ctx Talloc context for allocations (must not be NULL)
 * @param conn_str PostgreSQL connection string (must not be NULL or empty)
 * @param data_dir Data directory path from paths module (must not be NULL)
 * @param out_ctx Output parameter for database context (must not be NULL)
 * @return OK with db_ctx on success, ERR on failure
 *
 * Error codes:
 * - ERR_INVALID_ARG: NULL or invalid parameters
 * - ERR_DB_CONNECT: Connection failed (network, auth, etc.)
 * - ERR_DB_MIGRATE: Migration failed
 */
res_t ik_db_init(TALLOC_CTX *ctx, const char *conn_str, const char *data_dir, ik_db_ctx_t **out_ctx);

/**
 * Initialize database connection with custom migrations directory
 *
 * Like ik_db_init(), but allows specifying a custom migrations directory.
 * Useful for testing migration failure scenarios.
 *
 * @param ctx Talloc context for allocations (must not be NULL)
 * @param conn_str PostgreSQL connection string (must not be NULL or empty)
 * @param migrations_dir Path to migrations directory (must not be NULL)
 * @param out_ctx Output parameter for database context (must not be NULL)
 * @return OK with db_ctx on success, ERR on failure
 *
 * Error codes:
 * - ERR_INVALID_ARG: NULL or invalid parameters
 * - ERR_DB_CONNECT: Connection failed (network, auth, etc.)
 * - ERR_DB_MIGRATE: Migration failed
 * - ERR_IO: Cannot read migrations directory
 */
res_t ik_db_init_with_migrations(TALLOC_CTX *ctx,
                                 const char *conn_str,
                                 const char *migrations_dir,
                                 ik_db_ctx_t **out_ctx);

/**
 * Begin transaction
 *
 * Executes "BEGIN" to start a new transaction.
 *
 * @param db_ctx Database context (must not be NULL)
 * @return OK on success, ERR on failure
 */
res_t ik_db_begin(ik_db_ctx_t *db_ctx);

/**
 * Commit transaction
 *
 * Executes "COMMIT" to commit the current transaction.
 *
 * @param db_ctx Database context (must not be NULL)
 * @return OK on success, ERR on failure
 */
res_t ik_db_commit(ik_db_ctx_t *db_ctx);

/**
 * Rollback transaction
 *
 * Executes "ROLLBACK" to abort the current transaction.
 *
 * @param db_ctx Database context (must not be NULL)
 * @return OK on success, ERR on failure
 */
res_t ik_db_rollback(ik_db_ctx_t *db_ctx);

#endif // IK_DB_CONNECTION_H
