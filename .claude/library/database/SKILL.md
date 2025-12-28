# Database

PostgreSQL event stream architecture for persistent conversation storage with agent hierarchy and inter-agent messaging.

## Description

The database layer provides persistent storage for conversation events using an event stream model, agent registry with parent-child hierarchy, and Erlang-style inter-agent messaging, all managed through libpq with talloc-integrated memory management.

## Schema

Current version: 5

### schema_metadata

Tracks applied migrations.

| Column | Type | Constraints |
|--------|------|-------------|
| schema_version | INTEGER | PRIMARY KEY |

### sessions

Groups messages by launch session; persists across app launches until explicitly ended.

| Column | Type | Constraints |
|--------|------|-------------|
| id | BIGSERIAL | PRIMARY KEY |
| started_at | TIMESTAMPTZ | NOT NULL DEFAULT NOW() |
| ended_at | TIMESTAMPTZ | NULL (active if NULL) |
| title | TEXT | NULL |

Index: `idx_sessions_started` on (started_at DESC)

### messages

Event stream storage; each row is a single event in the conversation timeline.

| Column | Type | Constraints |
|--------|------|-------------|
| id | BIGSERIAL | PRIMARY KEY |
| session_id | BIGINT | NOT NULL, FK sessions(id) ON DELETE CASCADE |
| agent_uuid | TEXT | FK agents(uuid), nullable for backward compat |
| kind | TEXT | NOT NULL |
| content | TEXT | NULL |
| data | JSONB | NULL (event-specific metadata) |
| created_at | TIMESTAMPTZ | NOT NULL DEFAULT NOW() |

Valid kinds: `clear`, `system`, `user`, `assistant`, `tool_call`, `tool_result`, `mark`, `rewind`, `agent_killed`, `command`, `fork`, `usage`

Indexes:
- `idx_messages_session` on (session_id, created_at)
- `idx_messages_search` GIN index on to_tsvector('english', content)
- `idx_messages_agent` on (agent_uuid, id)

### agents

Agent registry with parent-child hierarchy and lifecycle tracking.

| Column | Type | Constraints |
|--------|------|-------------|
| uuid | TEXT | PRIMARY KEY (base64url, 22 chars) |
| name | TEXT | NULL |
| parent_uuid | TEXT | FK agents(uuid) ON DELETE RESTRICT |
| fork_message_id | BIGINT | NULL (tracks fork point in parent history) |
| status | agent_status | NOT NULL DEFAULT 'running' (ENUM: running, dead) |
| created_at | BIGINT | NOT NULL (Unix epoch) |
| ended_at | BIGINT | NULL |
| provider | TEXT | NULL (LLM provider: anthropic, openai, google) |
| model | TEXT | NULL (model identifier) |
| thinking_level | TEXT | NULL (thinking budget/level) |

Indexes: `idx_agents_parent` on (parent_uuid), `idx_agents_status` on (status)

### mail

Inter-agent messaging (Erlang-style message passing).

| Column | Type | Constraints |
|--------|------|-------------|
| id | BIGSERIAL | PRIMARY KEY |
| session_id | BIGINT | NOT NULL, FK sessions(id) ON DELETE CASCADE |
| from_uuid | TEXT | NOT NULL |
| to_uuid | TEXT | NOT NULL |
| body | TEXT | NOT NULL |
| timestamp | BIGINT | NOT NULL |
| read | INTEGER | NOT NULL DEFAULT 0 |

Index: `idx_mail_recipient` on (session_id, to_uuid, read)

## Key Types

```c
// Database context - manages PostgreSQL connection
typedef struct {
    PGconn *conn;
} ik_db_ctx_t;

// Unified message structure
typedef struct {
    int64_t id;       // DB row ID (0 if not from DB)
    char *kind;       // Message kind discriminator
    char *content;    // Message text or human-readable summary
    char *data_json;  // Structured data for tool messages
} ik_msg_t;

// Agent row from database queries
typedef struct {
    char *uuid;
    char *name;
    char *parent_uuid;
    char *fork_message_id;
    char *status;
    int64_t created_at;
    int64_t ended_at;  // 0 if still running
    char *provider;
    char *model;
    char *thinking_level;
} ik_db_agent_row_t;

// Inter-agent mail message
typedef struct ik_mail_msg {
    int64_t id;
    char *from_uuid;
    char *to_uuid;
    char *body;
    int64_t timestamp;
    bool read;
} ik_mail_msg_t;

// Replay range for agent history reconstruction
typedef struct {
    char *agent_uuid;   // Which agent's messages to query
    int64_t start_id;   // Start AFTER this message ID (0 = from beginning)
    int64_t end_id;     // End AT this message ID (0 = no limit)
} ik_replay_range_t;

// Replay context - reconstructed conversation state
typedef struct {
    ik_msg_t **messages;
    size_t count;
    size_t capacity;
    ik_replay_mark_stack_t mark_stack;
} ik_replay_context_t;

// Mark entry for conversation rollback
typedef struct {
    int64_t message_id;
    char *label;
    size_t context_idx;
} ik_replay_mark_t;
```

## Connection API

```c
// Initialize database connection and run migrations
res_t ik_db_init(TALLOC_CTX *ctx, const char *conn_str, ik_db_ctx_t **out_ctx);

// Initialize with custom migrations directory (for testing)
res_t ik_db_init_with_migrations(TALLOC_CTX *ctx, const char *conn_str,
                                  const char *migrations_dir, ik_db_ctx_t **out_ctx);

// Transaction control
res_t ik_db_begin(ik_db_ctx_t *db_ctx);
res_t ik_db_commit(ik_db_ctx_t *db_ctx);
res_t ik_db_rollback(ik_db_ctx_t *db_ctx);
```

Connection string format: `postgresql://[user[:password]@][host][:port][/dbname]`

## Session API

```c
// Create new session (started_at = NOW(), ended_at = NULL)
res_t ik_db_session_create(ik_db_ctx_t *db_ctx, int64_t *session_id_out);

// Get most recent active session (returns 0 if none found)
res_t ik_db_session_get_active(ik_db_ctx_t *db_ctx, int64_t *session_id_out);

// End session (set ended_at = NOW())
res_t ik_db_session_end(ik_db_ctx_t *db_ctx, int64_t session_id);
```

## Message API

```c
// Insert message event into database
res_t ik_db_message_insert(ik_db_ctx_t *db, int64_t session_id,
                            const char *agent_uuid, const char *kind,
                            const char *content, const char *data_json);

// Validate event kind
bool ik_db_message_is_valid_kind(const char *kind);

// Check if kind should be included in LLM conversation context
bool ik_msg_is_conversation_kind(const char *kind);

// Create canonical tool result message
ik_msg_t *ik_msg_create_tool_result(void *parent, const char *tool_call_id,
                                     const char *name, const char *output,
                                     bool success, const char *content);
```

## Replay API

Agent-based replay uses "walk backwards, play forwards" algorithm:
1. Start at leaf agent, end_id=0
2. Find most recent clear (within range) for each agent
3. If clear found: add range starting after clear, terminate walk
4. If no clear: add range from beginning, continue to parent
5. Reverse array for chronological order (root first)

```c
// Find most recent clear event for an agent
res_t ik_agent_find_clear(ik_db_ctx_t *db_ctx, TALLOC_CTX *ctx,
                           const char *agent_uuid, int64_t max_id,
                           int64_t *clear_id_out);

// Build replay ranges by walking ancestor chain
res_t ik_agent_build_replay_ranges(ik_db_ctx_t *db_ctx, TALLOC_CTX *ctx,
                                    const char *agent_uuid,
                                    ik_replay_range_t **ranges_out,
                                    size_t *count_out);

// Query messages for a single replay range
res_t ik_agent_query_range(ik_db_ctx_t *db_ctx, TALLOC_CTX *ctx,
                            const ik_replay_range_t *range,
                            ik_msg_t ***messages_out, size_t *count_out);

// High-level: replay history for an agent (production use)
res_t ik_agent_replay_history(ik_db_ctx_t *db_ctx, TALLOC_CTX *ctx,
                               const char *agent_uuid,
                               ik_replay_context_t **ctx_out);

// TEST-ONLY: Load messages by session_id (not agent-aware)
res_t ik_db_messages_load(TALLOC_CTX *ctx, ik_db_ctx_t *db_ctx,
                           int64_t session_id, ik_logger_t *logger);
```

## Agent API

```c
// Insert agent into registry (status='running')
res_t ik_db_agent_insert(ik_db_ctx_t *db_ctx, const ik_agent_ctx_t *agent);

// Mark agent as dead (idempotent)
res_t ik_db_agent_mark_dead(ik_db_ctx_t *db_ctx, const char *uuid);

// Lookup agent by UUID
res_t ik_db_agent_get(ik_db_ctx_t *db_ctx, TALLOC_CTX *ctx,
                       const char *uuid, ik_db_agent_row_t **out);

// List all running agents
res_t ik_db_agent_list_running(ik_db_ctx_t *db_ctx, TALLOC_CTX *ctx,
                                ik_db_agent_row_t ***out, size_t *count);

// Get children of an agent
res_t ik_db_agent_get_children(ik_db_ctx_t *db_ctx, TALLOC_CTX *ctx,
                                const char *parent_uuid,
                                ik_db_agent_row_t ***out, size_t *count);

// Get parent agent (NULL for root)
res_t ik_db_agent_get_parent(ik_db_ctx_t *db_ctx, TALLOC_CTX *ctx,
                              const char *uuid, ik_db_agent_row_t **out);

// Get last message ID for an agent (for fork point)
res_t ik_db_agent_get_last_message_id(ik_db_ctx_t *db_ctx,
                                       const char *agent_uuid,
                                       int64_t *out_message_id);

// Update provider configuration
res_t ik_db_agent_update_provider(ik_db_ctx_t *db_ctx, const char *uuid,
                                   const char *provider, const char *model,
                                   const char *thinking_level);

// Ensure Agent 0 exists (creates if missing, adopts orphan messages)
res_t ik_db_ensure_agent_zero(ik_db_ctx_t *db, char **out_uuid);
```

## Mail API

```c
// Insert mail message (sets msg->id on success)
res_t ik_db_mail_insert(ik_db_ctx_t *db, int64_t session_id,
                         ik_mail_msg_t *msg);

// Get inbox for agent (unread first, then by timestamp desc)
res_t ik_db_mail_inbox(ik_db_ctx_t *db, TALLOC_CTX *ctx,
                        int64_t session_id, const char *to_uuid,
                        ik_mail_msg_t ***out, size_t *count);

// Get filtered inbox by sender
res_t ik_db_mail_inbox_filtered(ik_db_ctx_t *db, TALLOC_CTX *ctx,
                                 int64_t session_id, const char *to_uuid,
                                 const char *from_uuid,
                                 ik_mail_msg_t ***out, size_t *count);

// Mark message as read
res_t ik_db_mail_mark_read(ik_db_ctx_t *db, int64_t mail_id);

// Delete message (validates recipient ownership)
res_t ik_db_mail_delete(ik_db_ctx_t *db, int64_t mail_id,
                         const char *recipient_uuid);
```

## Migration System

Migrations live in `migrations/NNN-description.sql` and are applied in numerical order.

```c
// Apply all pending migrations
res_t ik_db_migrate(ik_db_ctx_t *db_ctx, const char *migrations_dir);
```

Algorithm:
1. Query current schema version from schema_metadata table (0 if missing)
2. Scan migrations directory for .sql files
3. Parse migration numbers from filenames (NNN-description.sql)
4. Sort by number and filter to pending (number > current_version)
5. Execute each migration SQL (must include own BEGIN/COMMIT)
6. Each migration updates schema_metadata version on success

Current migrations:
- 001-initial-schema.sql: sessions, messages, schema_metadata
- 002-agents-table.sql: agents table with agent_status enum
- 003-messages-agent-uuid.sql: agent_uuid column in messages
- 004-mail-table.sql: mail table for inter-agent messaging
- 005-multi-provider.sql: provider, model, thinking_level columns

## Testing Patterns

Per-file database isolation for parallel test execution:

```c
static const char *DB_NAME;
static ik_db_ctx_t *db;
static TALLOC_CTX *test_ctx;

// Suite setup (once per file)
DB_NAME = ik_test_db_name(NULL, __FILE__);  // e.g., "ikigai_test_session_test"
ik_test_db_create(DB_NAME);                  // DROP IF EXISTS, CREATE
ik_test_db_migrate(NULL, DB_NAME);           // Apply all migrations

// Per-test setup
test_ctx = talloc_new(NULL);
ik_test_db_connect(test_ctx, DB_NAME, &db);
ik_test_db_begin(db);

// ... run test ...

// Per-test teardown
ik_test_db_rollback(db);  // Discard test changes
talloc_free(test_ctx);

// Suite teardown (once per file)
ik_test_db_destroy(DB_NAME);
```

Test utility functions (from `tests/test_utils.h`):
- `ik_test_db_name(ctx, __FILE__)` - Derive unique DB name from source file
- `ik_test_db_create(db_name)` - Create test database (drops if exists)
- `ik_test_db_migrate(ctx, db_name)` - Run all migrations
- `ik_test_db_connect(ctx, db_name, &db)` - Connect without migrations
- `ik_test_db_begin(db)` - Begin transaction
- `ik_test_db_rollback(db)` - Rollback transaction
- `ik_test_db_truncate_all(db)` - Truncate all tables (when transactions not suitable)
- `ik_test_db_destroy(db_name)` - Drop test database

## Key Files

| Path | Purpose |
|------|---------|
| `src/db/connection.{h,c}` | Context, init, transactions |
| `src/db/session.{h,c}` | Session CRUD |
| `src/db/message.{h,c}` | Message insert, validation, tool result creation |
| `src/db/agent.{h,c}` | Agent registry operations |
| `src/db/agent_row.{h,c}` | Row parsing from PGresult |
| `src/db/agent_zero.{h,c}` | Agent 0 creation and orphan adoption |
| `src/db/mail.{h,c}` | Inter-agent mail operations |
| `src/db/replay.{h,c}` | Session-based replay (TEST-ONLY function) |
| `src/db/agent_replay.{h,c}` | Agent-based replay (production) |
| `src/db/migration.{h,c}` | Migration execution |
| `src/db/pg_result.{h,c}` | PGresult talloc wrapper (auto PQclear on free) |
| `src/msg.h` | ik_msg_t definition |
| `src/mail/msg.h` | ik_mail_msg_t definition |
| `migrations/*.sql` | Schema migrations |
| `tests/test_utils.{h,c}` | DB test helpers |
