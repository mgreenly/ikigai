#include "test_utils.h"
#include "../src/agent.h"
#include "../src/db/migration.h"
#include "../src/panic.h"
#include "../src/vendor/yyjson/yyjson.h"
#include <check.h>
#include <libpq-fe.h>
#include <talloc.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// ========== Allocator Wrapper Overrides ==========
// Strong symbols that override the weak symbols in src/wrapper.c

// Thread-local mocking controls - tests can set these to inject failures
// Using __thread to ensure each test file running in parallel has its own state
__thread int ik_test_talloc_realloc_fail_on_call = -1;  // -1 = don't fail, >= 0 = fail on this call
__thread int ik_test_talloc_realloc_call_count = 0;

void *talloc_zero_(TALLOC_CTX *ctx, size_t size)
{
    return talloc_zero_size(ctx, size);
}

char *talloc_strdup_(TALLOC_CTX *ctx, const char *str)
{
    return talloc_strdup(ctx, str);
}

void *talloc_array_(TALLOC_CTX *ctx, size_t el_size, size_t count)
{
    return talloc_zero_size(ctx, el_size * count);
}

void *talloc_realloc_(TALLOC_CTX *ctx, void *ptr, size_t size)
{
    int current_call = ik_test_talloc_realloc_call_count++;
    if (ik_test_talloc_realloc_fail_on_call >= 0 && current_call == ik_test_talloc_realloc_fail_on_call) {
        return NULL;  // Simulate OOM
    }
    return talloc_realloc_size(ctx, ptr, size);
}

char *talloc_asprintf_(TALLOC_CTX *ctx, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char *result = talloc_vasprintf(ctx, fmt, ap);
    va_end(ap);
    return result;
}

// ========== Test Config Helper ==========

ik_cfg_t *ik_test_create_config(TALLOC_CTX *ctx)
{
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    if (cfg == NULL) return NULL;

    // Set minimal required fields for testing
    cfg->openai_api_key = talloc_strdup(cfg, "test-api-key");
    cfg->openai_model = talloc_strdup(cfg, "gpt-4-turbo");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 4096;
    cfg->openai_system_message = NULL;
    cfg->listen_address = talloc_strdup(cfg, "127.0.0.1");
    cfg->listen_port = 8080;
    cfg->history_size = 10000;  // Default history size

    return cfg;
}

// ========== File I/O Helpers ==========

char *load_file_to_string(TALLOC_CTX *ctx, const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }

    // Get file size
    struct stat st;
    if (fstat(fileno(f), &st) != 0) {
        fclose(f);
        return NULL;
    }

    size_t size = (size_t)st.st_size;

    // Allocate buffer (talloc panics on OOM)
    char *buffer = talloc_zero_size(ctx, size + 1);

    // Read file
    size_t bytes_read = fread(buffer, 1, size, f);
    fclose(f);

    if (bytes_read != size) {
        talloc_free(buffer);
        return NULL;
    }

    buffer[size] = '\0';
    return buffer;
}

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

const char *ik_test_db_name(TALLOC_CTX *ctx, const char *file_path)
{
    if (file_path == NULL) {
        return NULL;
    }

    // Find the last '/' to get basename
    const char *basename = strrchr(file_path, '/');
    if (basename != NULL) {
        basename++;  // Skip the '/'
    } else {
        basename = file_path;
    }

    // Find the last '.' to remove extension
    const char *dot = strrchr(basename, '.');
    size_t name_len;
    if (dot != NULL && dot > basename) {
        name_len = (size_t)(dot - basename);
    } else {
        name_len = strlen(basename);
    }

    // Build result: "ikigai_test_" + basename (without extension)
    if (ctx != NULL) {
        return talloc_asprintf(ctx, "ikigai_test_%.*s", (int)name_len, basename);
    } else {
        // Use thread-local buffer for NULL ctx (for suite-level setup before talloc)
        // Using __thread to ensure each test file running in parallel has its own buffer
        static __thread char static_buf[256];
        snprintf(static_buf, sizeof(static_buf), "ikigai_test_%.*s", (int)name_len, basename);
        return static_buf;
    }
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
    if (db_name == NULL) {
        return ERR(NULL, INVALID_ARG, "db_name cannot be NULL");
    }

    // Connect to admin database
    PGconn *conn = PQconnectdb(get_admin_db_url());
    if (conn == NULL) {
        return ERR(NULL, DB_CONNECT, "Failed to allocate connection");
    }

    if (PQstatus(conn) != CONNECTION_OK) {
        const char *pq_err = PQerrorMessage(conn);
        PQfinish(conn);
        return ERR(NULL, DB_CONNECT, "Failed to connect to admin database: %s", pq_err);
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
        const char *pq_err = PQerrorMessage(conn);
        PQclear(result);
        PQfinish(conn);
        return ERR(NULL, DB_CONNECT, "Failed to drop database: %s", pq_err);
    }
    PQclear(result);

    // Create fresh database
    char create_cmd[256];
    snprintf(create_cmd, sizeof(create_cmd), "CREATE DATABASE %s", db_name);
    result = PQexec(conn, create_cmd);
    if (PQresultStatus(result) != PGRES_COMMAND_OK) {
        const char *pq_err = PQerrorMessage(conn);
        PQclear(result);
        PQfinish(conn);
        return ERR(NULL, DB_CONNECT, "Failed to create database: %s", pq_err);
    }
    PQclear(result);

    PQfinish(conn);
    return OK(NULL);
}

res_t ik_test_db_migrate(TALLOC_CTX *ctx, const char *db_name)
{
    if (db_name == NULL) {
        return ERR(ctx, INVALID_ARG, "db_name cannot be NULL");
    }

    TALLOC_CTX *tmp_ctx = ctx ? ctx : talloc_new(NULL);

    // Connect to the test database
    ik_db_ctx_t *db = NULL;
    res_t res = ik_test_db_connect(tmp_ctx, db_name, &db);
    if (is_err(&res)) {
        if (ctx == NULL) talloc_free(tmp_ctx);
        return res;
    }

    // Run migrations
    res = ik_db_migrate(db, "migrations");
    if (is_err(&res)) {
        if (ctx == NULL) talloc_free(tmp_ctx);
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
        return ERR(NULL, INVALID_ARG, "ctx cannot be NULL");
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
        const char *pq_err = PQerrorMessage(db_ctx->conn);
        res_t res = ERR(ctx, DB_CONNECT, "Failed to connect to database: %s", pq_err);
        talloc_free(db_ctx);
        return res;
    }

    *out = db_ctx;
    return OK(db_ctx);
}

res_t ik_test_db_begin(ik_db_ctx_t *db)
{
    if (db == NULL || db->conn == NULL) {
        return ERR(NULL, INVALID_ARG, "db cannot be NULL");
    }

    PGresult *result = PQexec(db->conn, "BEGIN");
    if (PQresultStatus(result) != PGRES_COMMAND_OK) {
        const char *pq_err = PQerrorMessage(db->conn);
        PQclear(result);
        return ERR(NULL, DB_CONNECT, "BEGIN failed: %s", pq_err);
    }
    PQclear(result);

    return OK(NULL);
}

res_t ik_test_db_rollback(ik_db_ctx_t *db)
{
    if (db == NULL || db->conn == NULL) {
        return ERR(NULL, INVALID_ARG, "db cannot be NULL");
    }

    PGresult *result = PQexec(db->conn, "ROLLBACK");
    if (PQresultStatus(result) != PGRES_COMMAND_OK) {
        const char *pq_err = PQerrorMessage(db->conn);
        PQclear(result);
        return ERR(NULL, DB_CONNECT, "ROLLBACK failed: %s", pq_err);
    }
    PQclear(result);

    return OK(NULL);
}

res_t ik_test_db_truncate_all(ik_db_ctx_t *db)
{
    if (db == NULL || db->conn == NULL) {
        return ERR(NULL, INVALID_ARG, "db cannot be NULL");
    }

    // Truncate all application tables (order matters due to FK constraints)
    const char *truncate_sql =
        "TRUNCATE TABLE messages, sessions RESTART IDENTITY CASCADE";

    PGresult *result = PQexec(db->conn, truncate_sql);
    if (PQresultStatus(result) != PGRES_COMMAND_OK) {
        const char *pq_err = PQerrorMessage(db->conn);
        PQclear(result);
        return ERR(NULL, DB_CONNECT, "TRUNCATE failed: %s", pq_err);
    }
    PQclear(result);

    return OK(NULL);
}

res_t ik_test_db_destroy(const char *db_name)
{
    if (db_name == NULL) {
        return ERR(NULL, INVALID_ARG, "db_name cannot be NULL");
    }

    // Connect to admin database
    PGconn *conn = PQconnectdb(get_admin_db_url());
    if (conn == NULL) {
        return ERR(NULL, DB_CONNECT, "Failed to allocate connection");
    }

    if (PQstatus(conn) != CONNECTION_OK) {
        const char *pq_err = PQerrorMessage(conn);
        PQfinish(conn);
        return ERR(NULL, DB_CONNECT, "Failed to connect to admin database: %s", pq_err);
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

    // Drop database
    char drop_cmd[256];
    snprintf(drop_cmd, sizeof(drop_cmd), "DROP DATABASE IF EXISTS %s", db_name);
    result = PQexec(conn, drop_cmd);
    if (PQresultStatus(result) != PGRES_COMMAND_OK) {
        const char *pq_err = PQerrorMessage(conn);
        PQclear(result);
        PQfinish(conn);
        return ERR(NULL, DB_CONNECT, "Failed to drop database: %s", pq_err);
    }
    PQclear(result);

    PQfinish(conn);
    return OK(NULL);
}

// ========== Terminal Reset Utilities ==========

void ik_test_reset_terminal(void)
{
    // Reset sequence:
    // - \x1b[?25h  Show cursor (may be hidden)
    // - \x1b[0m    Reset text attributes (future-proof)
    //
    // Do NOT exit alternate screen - tests don't enter it.
    // Write to stdout which is where test output goes.
    const char reset_seq[] = "\x1b[?25h\x1b[0m";
    (void)write(STDOUT_FILENO, reset_seq, sizeof(reset_seq) - 1);
}

// ========== Agent Test Utilities ==========

res_t ik_test_create_agent(TALLOC_CTX *ctx, ik_agent_ctx_t **out)
{
    if (ctx == NULL || out == NULL) {
        return ERR(NULL, INVALID_ARG, "NULL argument to ik_test_create_agent");
    }

    // Create minimal shared context
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    if (shared == NULL) {
        return ERR(ctx, OUT_OF_MEMORY, "Failed to allocate shared context");
    }

    // Create agent (ik_agent_create will initialize display state)
    return ik_agent_create(ctx, shared, NULL, out);
}

// ========== Tool JSON Test Helpers ==========

yyjson_val *ik_test_tool_parse_success(const char *json, yyjson_doc **out_doc)
{
    (void)json;
    (void)out_doc;
    // Stub: return NULL to fail assertions
    return NULL;
}

const char *ik_test_tool_parse_error(const char *json, yyjson_doc **out_doc)
{
    (void)json;
    (void)out_doc;
    // Stub: return NULL to fail assertions
    return NULL;
}

const char *ik_test_tool_get_output(yyjson_val *data)
{
    (void)data;
    // Stub: return NULL to fail assertions
    return NULL;
}

int64_t ik_test_tool_get_exit_code(yyjson_val *data)
{
    (void)data;
    // Stub: return -1 to fail assertions
    return -1;
}
