# Database

## Description
PostgreSQL-backed event stream architecture with session management, message persistence, and conversation replay using talloc-based memory management.

## Schema

### schema_metadata
Tracks applied migrations for incremental schema updates.

| Column         | Type    | Constraints    | Description                   |
|----------------|---------|----------------|-------------------------------|
| schema_version | INTEGER | PRIMARY KEY    | Current schema version number |

### sessions
Groups messages by conversation session with persistent state across app launches.

| Column     | Type        | Constraints    | Description                              |
|------------|-------------|----------------|------------------------------------------|
| id         | BIGSERIAL   | PRIMARY KEY    | Session identifier                       |
| started_at | TIMESTAMPTZ | NOT NULL, DEFAULT NOW() | Session start timestamp       |
| ended_at   | TIMESTAMPTZ | NULL           | Session end timestamp (NULL = active)    |
| title      | TEXT        | NULL           | Optional user-defined session title      |

**Indexes:**
- `idx_sessions_started`: `started_at DESC` for recent session lookup

### messages
Event stream storage for conversation timeline with support for replay and rollback.

| Column     | Type        | Constraints                          | Description                        |
|------------|-------------|--------------------------------------|------------------------------------|
| id         | BIGSERIAL   | PRIMARY KEY                          | Message identifier                 |
| session_id | BIGINT      | NOT NULL, FK sessions(id) ON DELETE CASCADE | Parent session           |
| kind       | TEXT        | NOT NULL                             | Event type discriminator           |
| content    | TEXT        | NULL                                 | Message text (NULL for clear)      |
| data       | JSONB       | NULL                                 | Event metadata (LLM params, etc.)  |
| created_at | TIMESTAMPTZ | NOT NULL, DEFAULT NOW()              | Event timestamp                    |

**Event Kinds:**
- `clear`: Context reset (session start or /clear command)
- `system`: System prompt message
- `user`: User input message
- `assistant`: LLM response message
- `tool_call`: Tool invocation request
- `tool_result`: Tool execution result
- `mark`: Checkpoint created by /mark command
- `rewind`: Rollback operation created by /rewind command

**Indexes:**
- `idx_messages_session`: `(session_id, created_at)` for chronological event stream processing
- `idx_messages_search`: GIN index on `to_tsvector('english', content)` for full-text search

## Key Types

### ik_db_ctx_t
Database context managing PostgreSQL connection lifecycle.
```c
typedef struct {
    PGconn *conn;  // PostgreSQL connection handle
} ik_db_ctx_t;
```
- Memory: Allocated as child of caller's talloc context
- Destructor: Automatically calls `PQfinish()` on conn
- Cleanup: Single `talloc_free()` on parent releases everything

### ik_message_t
Message structure representing a single event from database.
```c
typedef struct {
    int64_t id;        // Message ID from database
    char *kind;        // Event kind (clear, system, user, assistant, mark, rewind)
    char *content;     // Message content
    char *data_json;   // JSONB data as string
} ik_message_t;
```

### ik_replay_mark_t
Checkpoint information for conversation rollback.
```c
typedef struct {
    int64_t message_id;  // ID of the mark event
    char *label;         // User label or NULL
    size_t context_idx;  // Position in context array when mark was created
} ik_replay_mark_t;
```

### ik_replay_mark_stack_t
Dynamic array of checkpoint marks.
```c
typedef struct {
    ik_replay_mark_t *marks;  // Dynamic array of marks
    size_t count;             // Number of marks
    size_t capacity;          // Allocated capacity
} ik_replay_mark_stack_t;
```

### ik_replay_context_t
Dynamic array of messages representing conversation state with mark stack for rollback.
```c
typedef struct {
    ik_message_t **messages;          // Dynamic array of message pointers
    size_t count;                     // Number of messages in context
    size_t capacity;                  // Allocated capacity
    ik_replay_mark_stack_t mark_stack; // Stack of checkpoint marks
} ik_replay_context_t;
```
- Initial capacity: 16 messages
- Growth: Geometric (capacity *= 2)
- Memory: All structures allocated under parent talloc context

### ik_pg_result_wrapper_t
Wrapper for PGresult with automatic cleanup via talloc destructor.
```c
typedef struct {
    PGresult *pg_result;
} ik_pg_result_wrapper_t;
```
- Destructor automatically calls `PQclear()` when talloc context is freed

## Connection API

### ik_db_init
Initialize database connection with default migrations directory.
```c
res_t ik_db_init(TALLOC_CTX *mem_ctx, const char *conn_str, ik_db_ctx_t **out_ctx);
```
- **Parameters:**
  - `mem_ctx`: Talloc context for allocations (must not be NULL)
  - `conn_str`: PostgreSQL connection string (e.g., `postgresql://user:pass@host:port/dbname`)
  - `out_ctx`: Output parameter for database context (must not be NULL)
- **Returns:** `OK` with db_ctx on success, `ERR` on failure
- **Error Codes:** `ERR_INVALID_ARG`, `ERR_DB_CONNECT`, `ERR_DB_MIGRATE`
- **Behavior:** Establishes connection and runs migrations from `./share/ikigai/migrations/`

### ik_db_init_with_migrations
Initialize database connection with custom migrations directory.
```c
res_t ik_db_init_with_migrations(TALLOC_CTX *mem_ctx, const char *conn_str,
                                  const char *migrations_dir, ik_db_ctx_t **out_ctx);
```
- **Parameters:**
  - `mem_ctx`: Talloc context for allocations (must not be NULL)
  - `conn_str`: PostgreSQL connection string (must not be NULL or empty)
  - `migrations_dir`: Path to migrations directory (must not be NULL)
  - `out_ctx`: Output parameter for database context (must not be NULL)
- **Returns:** `OK` with db_ctx on success, `ERR` on failure
- **Error Codes:** `ERR_INVALID_ARG`, `ERR_DB_CONNECT`, `ERR_DB_MIGRATE`, `ERR_IO`

## Session API

### ik_db_session_create
Create a new session with started_at=NOW() and ended_at=NULL.
```c
res_t ik_db_session_create(ik_db_ctx_t *db_ctx, int64_t *session_id_out);
```
- **Parameters:**
  - `db_ctx`: Database context (must not be NULL)
  - `session_id_out`: Output parameter for new session ID (must not be NULL)
- **Returns:** `OK` with session_id on success, `ERR` on failure

### ik_db_session_get_active
Get the most recent active session (ended_at IS NULL).
```c
res_t ik_db_session_get_active(ik_db_ctx_t *db_ctx, int64_t *session_id_out);
```
- **Parameters:**
  - `db_ctx`: Database context (must not be NULL)
  - `session_id_out`: Output parameter for session ID (must not be NULL)
- **Returns:** `OK` with session_id (0 if none found), `ERR` on database failure
- **Behavior:** Returns 0 (not an error) if no active session exists

### ik_db_session_end
End a session by setting ended_at=NOW().
```c
res_t ik_db_session_end(ik_db_ctx_t *db_ctx, int64_t session_id);
```
- **Parameters:**
  - `db_ctx`: Database context (must not be NULL)
  - `session_id`: Session ID to end (must be > 0)
- **Returns:** `OK` on success, `ERR` on failure
- **Behavior:** Session will no longer be returned by `get_active`

## Message API

### ik_db_message_insert
Insert a message event into the database.
```c
res_t ik_db_message_insert(ik_db_ctx_t *db, int64_t session_id, const char *kind,
                            const char *content, const char *data_json);
```
- **Parameters:**
  - `db`: Database connection context (must not be NULL)
  - `session_id`: Session ID (must be positive, references sessions.id)
  - `kind`: Event kind string (must be valid kind: clear, system, user, assistant, tool_call, tool_result, mark, rewind)
  - `content`: Message content (may be NULL for clear events, empty string allowed)
  - `data_json`: JSONB data as JSON string (may be NULL)
- **Returns:** `OK` on success, `ERR` on failure (invalid params or database error)

### ik_db_message_is_valid_kind
Validate that a kind string is one of the allowed event kinds.
```c
bool ik_db_message_is_valid_kind(const char *kind);
```
- **Parameters:**
  - `kind`: The kind string to validate (may be NULL)
- **Returns:** true if kind is valid, false otherwise
- **Valid Kinds:** clear, system, user, assistant, tool_call, tool_result, mark, rewind

### ik_msg_create_tool_result
Create a canonical tool result message with kind="tool_result".
```c
ik_message_t *ik_msg_create_tool_result(void *parent, const char *tool_call_id,
                                         const char *name, const char *output,
                                         bool success, const char *content);
```
- **Parameters:**
  - `parent`: Talloc context for allocation (can be NULL for root)
  - `tool_call_id`: Unique tool call ID (e.g., "call_abc123")
  - `name`: Tool name (e.g., "glob", "file_read")
  - `output`: Tool output string (can be empty string)
  - `success`: Whether the tool executed successfully (true/false)
  - `content`: Human-readable summary for the message (e.g., "3 files found")
- **Returns:** Allocated `ik_message_t` struct (owned by parent), or NULL on OOM
- **Memory:** All fields are children of the message; single `talloc_free()` releases all

## Replay API

### ik_db_messages_load
Load messages from database and replay to build conversation context.
```c
res_t ik_db_messages_load(TALLOC_CTX *ctx, ik_db_ctx_t *db_ctx, int64_t session_id);
```
- **Parameters:**
  - `ctx`: Talloc context for allocations (must not be NULL)
  - `db_ctx`: Database connection context (must not be NULL)
  - `session_id`: Session ID to load messages for (must be positive)
- **Returns:** `OK` with `ik_replay_context_t*` on success, `ERR` on failure
- **Behavior:**
  - Queries messages table ordered by `created_at`
  - Processes events to build conversation context:
    - `clear`: Empty context (set count = 0)
    - `system`/`user`/`assistant`/`tool_call`/`tool_result`: Append to context array
    - `mark`: Append to context and push mark onto mark stack
    - `rewind`: Truncate context to target mark's position, append rewind event
  - Uses geometric growth (capacity *= 2) for dynamic arrays
  - Initial capacity: 16 messages
  - Memory: Single `talloc_free(ctx)` releases everything

## Migration System

### ik_db_migrate
Apply all pending database migrations from specified directory.
```c
res_t ik_db_migrate(ik_db_ctx_t *db_ctx, const char *migrations_dir);
```
- **Parameters:**
  - `db_ctx`: Database context (must not be NULL)
  - `migrations_dir`: Path to migrations directory (must not be NULL)
- **Returns:** `OK` on success, `ERR` on failure
- **Error Codes:** `ERR_INVALID_ARG`, `ERR_IO`, `ERR_DB_MIGRATE`

### Migration Algorithm
1. Query current schema version from `schema_metadata` table (0 if table doesn't exist)
2. Scan migrations directory for `.sql` files
3. Parse migration numbers from filenames (e.g., `001-initial-schema.sql` → 1)
4. Sort migrations by number
5. Filter to pending migrations (number > current_version)
6. For each pending migration:
   - Read SQL file contents
   - Execute SQL via `PQexec()` within transaction
   - Check for errors (rollback on failure)
   - Continue to next migration on success

### Migration File Format
- **Naming:** `NNN-description.sql` or `NNNN-description.sql` (e.g., `001-initial-schema.sql`)
- **Content:** Valid SQL with `BEGIN`/`COMMIT` for atomicity
- **Version Update:** Each migration must update `schema_metadata.schema_version`
- **Idempotency:** Safe to run multiple times; already-applied migrations are skipped

### Current Migrations
- **001-initial-schema.sql**: Creates `schema_metadata`, `sessions`, and `messages` tables with indexes
- **999-auto-test.sql**: Test migration (creates `auto_test` table, updates version to 999)

## PGresult Memory Management

**CRITICAL:** NEVER call `PQclear()` manually. Always wrap with `ik_db_wrap_pg_result()`.

### ik_db_wrap_pg_result
Wrap PGresult with automatic cleanup via talloc destructor.
```c
ik_pg_result_wrapper_t *ik_db_wrap_pg_result(TALLOC_CTX *mem_ctx, PGresult *pg_res);
```
- **Usage:** Wrap all `PQexec()` and `PQexecParams()` results to prevent leaks
- **Cleanup:** Destructor automatically calls `PQclear()` when parent talloc context is freed

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

## Testing Patterns

### Test Isolation Pattern A (Transaction-based)
Most tests use transaction isolation for speed:
```c
static const char *DB_NAME;
static ik_db_ctx_t *db;
static TALLOC_CTX *test_ctx;

// Suite setup (once per file)
DB_NAME = ik_test_db_name(NULL, __FILE__);
ik_test_db_create(DB_NAME);
ik_test_db_migrate(NULL, DB_NAME);

// Per-test setup
test_ctx = talloc_new(NULL);
ik_test_db_connect(test_ctx, DB_NAME, &db);
ik_test_db_begin(db);

// Per-test teardown
ik_test_db_rollback(db);
talloc_free(test_ctx);

// Suite teardown (once per file)
ik_test_db_destroy(DB_NAME);
```

**Rolemodel:** `tests/unit/db/session_test.c`

**Key benefits:**
- Parallel execution across test files (separate DBs)
- Fast isolation within a file (transaction rollback)
- Idempotent - works regardless of previous state

### Test Isolation Pattern B (Empty database)
Migration tests use empty database without migrations:
```c
DB_NAME = ik_test_db_name(NULL, __FILE__);
ik_test_db_create(DB_NAME);  // No migrate call
// ... test migration logic ...
ik_test_db_destroy(DB_NAME);
```

### Key Test Functions

#### ik_test_db_name
Derive database name from source file path.
```c
const char *ik_test_db_name(TALLOC_CTX *ctx, const char *file_path);
```
- Example: `tests/unit/db/session_test.c` → `ikigai_test_session_test`

#### ik_test_db_create
Create test database (drops if exists).
```c
res_t ik_test_db_create(const char *db_name);
```
- Idempotent: Safe to call regardless of previous state
- Uses admin database connection to drop/create

#### ik_test_db_migrate
Run migrations on test database from `./share/ikigai/migrations/` directory.
```c
res_t ik_test_db_migrate(TALLOC_CTX *ctx, const char *db_name);
```

#### ik_test_db_connect
Open connection to test database (no migrations).
```c
res_t ik_test_db_connect(TALLOC_CTX *ctx, const char *db_name, ik_db_ctx_t **out);
```

#### ik_test_db_begin
Begin transaction for test isolation.
```c
res_t ik_test_db_begin(ik_db_ctx_t *db);
```

#### ik_test_db_rollback
Rollback transaction to discard test changes.
```c
res_t ik_test_db_rollback(ik_db_ctx_t *db);
```

#### ik_test_db_truncate_all
Truncate all application tables (for non-transactional tests).
```c
res_t ik_test_db_truncate_all(ik_db_ctx_t *db);
```
- Executes: `TRUNCATE TABLE messages, sessions RESTART IDENTITY CASCADE`

#### ik_test_db_destroy
Drop test database completely.
```c
res_t ik_test_db_destroy(const char *db_name);
```

## Test Database Configuration

**Fixed configuration** - hardcoded in `tests/test_utils.c`:
- **User**: `ikigai`
- **Password**: `ikigai`
- **Host**: `localhost` (override with `PGHOST` environment variable)
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
| `PGHOST` | Override PostgreSQL host (default: localhost) |
| `SKIP_LIVE_DB_TESTS` | Set to `1` to skip DB tests |

## Key Files

| File | Purpose |
|------|---------|
| `share/ikigai/migrations/001-initial-schema.sql` | Initial database schema with sessions and messages tables |
| `share/ikigai/migrations/999-auto-test.sql` | Test migration for migration system validation |
| `src/db/connection.h` | Database connection API (init, destructor) |
| `src/db/connection.c` | Connection implementation with validation and migration runner |
| `src/db/session.h` | Session CRUD API (create, get_active, end) |
| `src/db/session.c` | Session implementation with parameterized queries |
| `src/db/message.h` | Message insertion API and tool result helpers |
| `src/db/message.c` | Message implementation with kind validation and yyjson |
| `src/db/replay.h` | Replay context structures and message loading API |
| `src/db/replay.c` | Event stream replay algorithm with mark/rewind support |
| `src/db/migration.h` | Migration system API |
| `src/db/migration.c` | Migration scanner, parser, and executor |
| `src/db/pg_result.h` | PGresult wrapper API |
| `src/db/pg_result.c` | PGresult wrapper implementation with talloc destructor |
| `tests/test_utils.h` | Database test utilities header |
| `tests/test_utils.c` | Database test utilities implementation |
| `tests/unit/db/session_test.c` | Rolemodel for DB test pattern |
| `tests/unit/db/pg_result_test.c` | PGresult wrapper tests |

## Related Skills

- `mocking` - MOCKABLE wrapper patterns for DB functions
- `testability` - Refactoring for better DB testing
- `errors` - Error handling with res_t pattern
