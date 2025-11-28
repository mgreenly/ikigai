# Database

## Description
PostgreSQL database architecture, connection management, migrations, and testing patterns.

## Core Architecture

**PostgreSQL via libpq** - All database access uses libpq directly (no ORM).

**Memory Management** - Database contexts integrate with talloc:
- `ik_db_ctx_t` allocated as child of caller's context
- Destructor calls `PQfinish()` automatically
- `ik_pg_result_wrapper_t` wraps PGresult for automatic cleanup
- Destructor calls `PQclear()` automatically
- Single `talloc_free(parent)` cleans up everything

## Key Types

```c
typedef struct {
    PGconn *conn;  // PostgreSQL connection handle
} ik_db_ctx_t;

typedef struct {
    PGresult *pg_result;  // PostgreSQL result handle
} ik_pg_result_wrapper_t;
```

## Connection API

```c
// Standard init (runs migrations from ./migrations/)
res_t ik_db_init(TALLOC_CTX *mem_ctx, const char *conn_str, ik_db_ctx_t **out_ctx);

// Custom migrations directory (for testing)
res_t ik_db_init_with_migrations(TALLOC_CTX *mem_ctx, const char *conn_str,
                                  const char *migrations_dir, ik_db_ctx_t **out_ctx);
```

Connection string format: `postgresql://user:pass@host:port/dbname`

## PGresult Memory Management

**CRITICAL:** NEVER call `PQclear()` manually. Always wrap with `ik_db_wrap_pg_result()`.

```c
// Wrap PGresult - destructor automatically calls PQclear() when context freed
ik_pg_result_wrapper_t *ik_db_wrap_pg_result(TALLOC_CTX *ctx, PGresult *pg_res);
```

**Usage pattern:**
```c
// Wrap immediately after PQexec/PQexecParams
ik_pg_result_wrapper_t *wrapper = ik_db_wrap_pg_result(tmp_ctx, PQexec(conn, query));
PGresult *res = wrapper->pg_result;

// Use normally - no manual cleanup needed
if (PQresultStatus(res) != PGRES_TUPLES_OK) {
    talloc_free(tmp_ctx);  // Destructor calls PQclear() automatically
    return ERR(...);
}
```

This integrates PGresult (malloc-based) with talloc's hierarchical memory model.

## Migration System

- Files in `migrations/` directory
- Naming: `NNN-description.sql` (e.g., `001-initial-schema.sql`)
- Tracks version in `schema_metadata` table
- Idempotent - safe to run multiple times
- Each file must use `BEGIN`/`COMMIT` for atomicity

```c
res_t ik_db_migrate(ik_db_ctx_t *db_ctx, const char *migrations_dir);
```

## Testing Patterns

### Per-File Database Isolation (Recommended)
Each test file gets its own database for parallel execution.

**Rolemodel:** `tests/unit/db/session_test.c`

```c
// Suite-level: Create/migrate DB once
static void suite_setup(void) {
    DB_NAME = ik_test_db_name(NULL, __FILE__);
    ik_test_db_create(DB_NAME);
    ik_test_db_migrate(NULL, DB_NAME);
}
static void suite_teardown(void) {
    ik_test_db_destroy(DB_NAME);
}

// Per-test: Transaction isolation
static void test_setup(void) {
    ik_test_db_connect(test_ctx, DB_NAME, &db);
    ik_test_db_begin(db);
}
static void test_teardown(void) {
    ik_test_db_rollback(db);
}
```

**Key benefits:**
- Parallel execution across test files (separate DBs)
- Fast isolation within a file (transaction rollback)
- Idempotent - works regardless of previous state

### Migration Tests (No Auto-Migration)
```c
ik_test_db_create(DB_NAME);  // Create empty DB, no migrate
ik_test_db_connect(ctx, DB_NAME, &db);
ik_db_migrate(db, "test_migrations/");  // Test custom migrations
```

### Mocking for Error Paths
Use wrapper functions for unit tests:
- `pq_exec_()` instead of `PQexec()`
- `pq_exec_params_()` instead of `PQexecParams()`

## Test Database Configuration

**Fixed configuration** - hardcoded in `tests/test_utils.c`:
- **User**: `ikigai`
- **Password**: `ikigai`
- **Host**: `localhost`
- **Admin DB**: `postgres` (for CREATE/DROP DATABASE operations)
- **Test DBs**: Per-file databases named `ikigai_test_<basename>`

**Connection strings:**
```c
ADMIN_DB_URL = "postgresql://ikigai:ikigai@localhost/postgres"
TEST_DB_URL_PREFIX = "postgresql://ikigai:ikigai@localhost/"
```

**Database setup requirements:**
1. PostgreSQL server running on localhost
2. User `ikigai` with password `ikigai` created
3. User must have CREATEDB privilege (for creating test databases)
4. No need to pre-create test databases (created/destroyed per test file)

## Environment Variables

| Variable | Purpose |
|----------|---------|
| `DATABASE_URL` | Production connection string |
| `SKIP_LIVE_DB_TESTS` | Set to `1` to skip DB tests |

## Key Files

| File | Purpose |
|------|---------|
| `src/db/connection.h` | Connection API |
| `src/db/connection.c` | Connection implementation |
| `src/db/migration.h` | Migration API |
| `src/db/migration.c` | Migration implementation |
| `src/db/pg_result.h` | PGresult wrapper API |
| `src/db/pg_result.c` | PGresult wrapper implementation |
| `src/db/session.h` | Session CRUD operations |
| `src/db/message.h` | Message CRUD operations |
| `src/db/replay.h` | Conversation replay |
| `migrations/` | SQL migration files |
| `tests/test_utils.h` | Test utilities API (includes DB utils) |
| `tests/test_utils.c` | Test utilities implementation |
| `tests/unit/db/session_test.c` | Rolemodel for DB test pattern |
| `tests/unit/db/pg_result_test.c` | PGresult wrapper tests |

## Related Skills

- `mocking` - MOCKABLE wrapper patterns for DB functions
- `testability` - Refactoring for better DB testing
