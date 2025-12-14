#include "migration.h"
#include "../error.h"
#include "../file_utils.h"
#include "../panic.h"
#include "../wrapper.h"
#include "pg_result.h"
#include <ctype.h>
#include <dirent.h>
#include <limits.h>
#include <libpq-fe.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/**
 * Migration file entry
 */
typedef struct {
    int number;         // Migration number parsed from filename
    char *path;         // Full path to migration file
} migration_entry_t;

/**
 * Get current schema version from database
 *
 * @param conn PostgreSQL connection
 * @return Current schema version (0 if table doesn't exist)
 */
static int get_current_version(PGconn *conn)
{
    assert(conn != NULL); // LCOV_EXCL_BR_LINE

    TALLOC_CTX *tmp_ctx = talloc_new(NULL);
    if (tmp_ctx == NULL) {  // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");  // LCOV_EXCL_LINE
    }

    ik_pg_result_wrapper_t *res_wrapper = ik_db_wrap_pg_result(tmp_ctx, PQexec(conn, "SELECT schema_version FROM schema_metadata LIMIT 1"));
    PGresult *res = res_wrapper->pg_result;

    // If query fails, table probably doesn't exist - return 0 for fresh database
    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        talloc_free(tmp_ctx);
        return 0;
    }

    int version = atoi(PQgetvalue(res, 0, 0));
    talloc_free(tmp_ctx);
    return version;
}

/**
 * Read entire file contents into talloc-allocated string
 *
 * @param ctx Talloc context for allocation
 * @param path Path to file
 * @param content_out Output for file contents (must not be NULL)
 * @return OK on success, ERR on failure
 */
static res_t read_file_contents(TALLOC_CTX *ctx, const char *path, char **content_out)
{
    assert(ctx != NULL);          // LCOV_EXCL_BR_LINE
    assert(path != NULL);         // LCOV_EXCL_BR_LINE
    assert(content_out != NULL);  // LCOV_EXCL_BR_LINE

    // Use shared file utility
    res_t result = ik_file_read_all(ctx, path, content_out, NULL);

    // Customize error messages to mention "migration file" for better context
    if (result.is_err) {
        const char *generic_msg = result.err->msg;
        if (strstr(generic_msg, "Failed to open")) {
            return ERR(ctx, IO, "Cannot open migration file: %s", path);
        } else if (strstr(generic_msg, "Failed to seek")) {
            return ERR(ctx, IO, "Cannot seek migration file: %s", path);
        } else if (strstr(generic_msg, "Failed to get size")) {
            return ERR(ctx, IO, "Cannot get migration file size: %s", path);
        } else if (strstr(generic_msg, "File too large")) {
            return ERR(ctx, IO, "Migration file too large: %s", path);
        } else {
            // Failed to read or any future error type
            return ERR(ctx, IO, "Failed to read migration file: %s", path);
        }
    }

    return result;
}

/**
 * Parse migration number from filename
 *
 * Expected format: NNNN-description.sql (e.g., 0001-initial-schema.sql, 1234-add-feature.sql)
 * Also accepts legacy 3-digit format: NNN-description.sql for backwards compatibility
 *
 * @param filename Filename to parse
 * @return Migration number, or -1 if invalid format
 */
static int parse_migration_number(const char *filename)
{
    assert(filename != NULL); // LCOV_EXCL_BR_LINE

    size_t len = strlen(filename);

    // Check minimum length (NNN-*.sql = 9 chars minimum)
    if (len < 9) {
        return -1;
    }

    // Check .sql extension
    const char *ext = strrchr(filename, '.');
    if (ext == NULL || strcmp(ext, ".sql") != 0) {
        return -1;
    }

    // Count leading digits (support 3 or 4 digits)
    int digit_count = 0;
    for (int i = 0; i < 4 && isdigit((unsigned char)filename[i]); i++) {
        digit_count++;
    }

    // Must have 3 or 4 digits
    if (digit_count != 3 && digit_count != 4) {
        return -1;
    }

    // Check dash after digits
    if (filename[digit_count] != '-') {
        return -1;
    }

    // Convert to number
    char num_str[5] = {0};
    strncpy(num_str, filename, (size_t)digit_count);
    return atoi(num_str);
}

/**
 * Comparison function for qsort - sort migrations by number
 */
static int compare_migrations(const void *a, const void *b)
{
    const migration_entry_t *ma = a;
    const migration_entry_t *mb = b;
    return ma->number - mb->number;
}

/**
 * Scan migrations directory and build sorted list of migrations
 *
 * @param ctx Talloc context for allocations
 * @param migrations_dir Path to migrations directory
 * @param entries_out Output array of migration entries (must not be NULL)
 * @param count_out Output count of entries (must not be NULL)
 * @return OK on success, ERR on failure
 */
static res_t scan_migrations(TALLOC_CTX *ctx, const char *migrations_dir,
                              migration_entry_t **entries_out, size_t *count_out)
{
    assert(ctx != NULL);               // LCOV_EXCL_BR_LINE
    assert(migrations_dir != NULL);    // LCOV_EXCL_BR_LINE
    assert(entries_out != NULL);       // LCOV_EXCL_BR_LINE
    assert(count_out != NULL);         // LCOV_EXCL_BR_LINE

    DIR *dir = opendir_(migrations_dir);
    if (dir == NULL) {
        return ERR(ctx, IO, "Cannot open migrations directory: %s", migrations_dir);
    }

    // Allocate array for migration entries (initial size 10, grow as needed)
    size_t capacity = 10;
    size_t count = 0;
    // talloc_array expects unsigned int, cast is safe for small values
    migration_entry_t *entries = talloc_array(ctx, migration_entry_t, (unsigned)capacity);
    if (entries == NULL) {         // LCOV_EXCL_BR_LINE
        closedir(dir);             // LCOV_EXCL_LINE
        PANIC("Out of memory");     // LCOV_EXCL_LINE
    }

    // Scan directory
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Parse migration number
        int number = parse_migration_number(entry->d_name);
        if (number < 0) {
            // Skip files with invalid format
            continue;
        }

        // Grow array if needed
        if (count >= capacity) {
            capacity *= 2;
            // talloc_realloc expects unsigned int, cast is safe for reasonable migration counts
            migration_entry_t *new_entries = talloc_realloc(ctx, entries, migration_entry_t, (unsigned)capacity);
            if (new_entries == NULL) {   // LCOV_EXCL_BR_LINE
                closedir(dir);           // LCOV_EXCL_LINE
                PANIC("Out of memory");   // LCOV_EXCL_LINE
            }
            entries = new_entries;
        }

        // Build full path
        char *path = talloc_asprintf(ctx, "%s/%s", migrations_dir, entry->d_name);
        if (path == NULL) {            // LCOV_EXCL_BR_LINE
            closedir(dir);             // LCOV_EXCL_LINE
            PANIC("Out of memory");     // LCOV_EXCL_LINE
        }

        // Add entry
        entries[count].number = number;
        entries[count].path = path;
        count++;
    }

    closedir(dir);

    // Sort by migration number
    if (count > 0) {
        qsort(entries, count, sizeof(migration_entry_t), compare_migrations);
    }

    *entries_out = entries;
    *count_out = count;
    return OK(entries);
}

/**
 * Apply a single migration file
 *
 * @param ctx Talloc context for error allocations
 * @param conn PostgreSQL connection
 * @param migration Migration entry to apply
 * @return OK on success, ERR on failure
 */
static res_t apply_migration(TALLOC_CTX *ctx, PGconn *conn, const migration_entry_t *migration)
{
    assert(ctx != NULL);          // LCOV_EXCL_BR_LINE
    assert(conn != NULL);         // LCOV_EXCL_BR_LINE
    assert(migration != NULL);    // LCOV_EXCL_BR_LINE

    // Read migration file
    char *sql;
    res_t res = read_file_contents(ctx, migration->path, &sql);
    if (is_err(&res)) {
        return res;
    }

    // Execute SQL (migrations should include their own BEGIN/COMMIT)
    ik_pg_result_wrapper_t *result_wrapper = ik_db_wrap_pg_result(ctx, PQexec(conn, sql));
    PGresult *result = result_wrapper->pg_result;
    ExecStatusType status = PQresultStatus(result);

    // Check for errors
    if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK) {
        const char *pg_err = PQerrorMessage(conn);
        return ERR(ctx, DB_MIGRATE, "Migration %d failed: %s", migration->number, pg_err);
    }

    return OK(NULL);
}

res_t ik_db_migrate(ik_db_ctx_t *db_ctx, const char *migrations_dir)
{
    // Validate parameters
    assert(db_ctx != NULL);         // LCOV_EXCL_BR_LINE
    assert(migrations_dir != NULL); // LCOV_EXCL_BR_LINE

    // Create temporary context for migration work
    TALLOC_CTX *tmp = talloc_new(db_ctx);
    if (tmp == NULL) {               // LCOV_EXCL_BR_LINE
        PANIC("Out of memory");       // LCOV_EXCL_LINE
    }

    // Get current schema version
    int current_version = get_current_version(db_ctx->conn);

    // Scan migrations directory
    migration_entry_t *migrations;
    size_t migration_count;
    res_t res = scan_migrations(tmp, migrations_dir, &migrations, &migration_count);
    if (is_err(&res)) {
        // Reparent error to db_ctx before freeing tmp
        talloc_steal(db_ctx, res.err);
        talloc_free(tmp);
        return res;
    }

    // Apply pending migrations (number > current_version)
    for (size_t i = 0; i < migration_count; i++) {
        if (migrations[i].number > current_version) {
            res = apply_migration(tmp, db_ctx->conn, &migrations[i]);
            if (is_err(&res)) {
                // Reparent error to db_ctx before freeing tmp
                talloc_steal(db_ctx, res.err);
                talloc_free(tmp);
                return res;
            }
        }
    }

    talloc_free(tmp);
    return OK(NULL);
}
