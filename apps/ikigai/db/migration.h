#ifndef IK_DB_MIGRATION_H
#define IK_DB_MIGRATION_H

#include "shared/error.h"
#include "apps/ikigai/db/connection.h"

/**
 * Apply all pending database migrations from specified directory
 *
 * Detects and applies pending migrations from the migrations directory.
 * Migrations are applied in numerical order based on filename prefix (001-, 002-, etc.).
 * The system tracks applied migrations via the schema_metadata table.
 *
 * Algorithm:
 * 1. Query current schema version from schema_metadata table
 *    - If table doesn't exist: current_version = 0 (fresh database)
 * 2. Scan migrations directory for .sql files
 * 3. Parse migration numbers from filenames (001-name.sql → 1)
 * 4. Sort migrations by number
 * 5. Filter to pending migrations (number > current_version)
 * 6. For each pending migration in order:
 *    a. Read SQL file contents
 *    b. Execute SQL via PQexec() within transaction
 *    c. Check for errors: PQresultStatus()
 *    d. On error: rollback and return descriptive error
 *    e. On success: continue to next migration
 * 7. Return OK() if all migrations applied successfully
 *
 * Migration file naming convention:
 *   NNN-description.sql (e.g., 001-initial-schema.sql, 002-add-indexes.sql)
 *
 * Each migration file must:
 * - Be valid SQL
 * - Update schema_metadata version on success
 * - Use transactions (BEGIN/COMMIT) for atomicity
 *
 * Idempotency:
 * - Safe to run multiple times
 * - Only applies migrations with number > current version
 * - Already-applied migrations are skipped
 *
 * Error handling:
 * - Invalid SQL: transaction rolled back, descriptive error returned
 * - Missing directory: error returned
 * - File read errors: error returned
 * - Malformed filenames: skipped or error (implementation defined)
 *
 * @param db_ctx Database context (must not be NULL)
 * @param migrations_dir Path to migrations directory (must not be NULL)
 * @return OK on success, ERR on failure
 *
 * Error codes:
 * - ERR_INVALID_ARG: NULL parameters
 * - ERR_IO: Cannot read migrations directory or files
 * - ERR_DB_MIGRATE: Migration SQL execution failed
 */
res_t ik_db_migrate(ik_db_ctx_t *db_ctx, const char *migrations_dir);

#endif // IK_DB_MIGRATION_H
