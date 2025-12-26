# Database

PostgreSQL event stream architecture for persistent conversation storage with agent hierarchy and inter-agent messaging.

## Schema (Version 5)

### schema_metadata

| Column | Type | Constraints |
|--------|------|-------------|
| schema_version | INTEGER | PRIMARY KEY |

### sessions

| Column | Type | Constraints |
|--------|------|-------------|
| id | BIGSERIAL | PRIMARY KEY |
| started_at | TIMESTAMPTZ | NOT NULL DEFAULT NOW() |
| ended_at | TIMESTAMPTZ | NULL (active if NULL) |
| title | TEXT | NULL |

Index: `idx_sessions_started` on (started_at DESC)

### messages

| Column | Type | Constraints |
|--------|------|-------------|
| id | BIGSERIAL | PRIMARY KEY |
| session_id | BIGINT | NOT NULL, FK sessions(id) ON DELETE CASCADE |
| agent_uuid | TEXT | FK agents(uuid) |
| kind | TEXT | NOT NULL |
| content | TEXT | NULL |
| data | JSONB | NULL |
| created_at | TIMESTAMPTZ | NOT NULL DEFAULT NOW() |

Valid kinds: `clear`, `system`, `user`, `assistant`, `tool_call`, `tool_result`, `mark`, `rewind`, `agent_killed`, `command`, `fork`, `usage`

Indexes: `idx_messages_session`, `idx_messages_search` (GIN), `idx_messages_agent`

### agents

| Column | Type | Constraints |
|--------|------|-------------|
| uuid | TEXT | PRIMARY KEY (base64url, 22 chars) |
| name | TEXT | NULL |
| parent_uuid | TEXT | FK agents(uuid) ON DELETE RESTRICT |
| fork_message_id | BIGINT | NULL |
| status | agent_status | NOT NULL DEFAULT 'running' |
| created_at | BIGINT | NOT NULL |
| ended_at | BIGINT | NULL |
| provider | TEXT | NULL |
| model | TEXT | NULL |
| thinking_level | TEXT | NULL |

Indexes: `idx_agents_parent`, `idx_agents_status`

### mail

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
typedef struct { PGconn *conn; } ik_db_ctx_t;

typedef struct {
    int64_t id; char *kind; char *content; char *data_json;
} ik_msg_t;

typedef struct {
    char *uuid, *name, *parent_uuid, *fork_message_id, *status;
    int64_t created_at, ended_at;
    char *provider, *model, *thinking_level;
} ik_db_agent_row_t;

typedef struct ik_mail_msg {
    int64_t id; char *from_uuid, *to_uuid, *body;
    int64_t timestamp; bool read;
} ik_mail_msg_t;

typedef struct {
    char *agent_uuid; int64_t start_id, end_id;
} ik_replay_range_t;

typedef struct {
    ik_msg_t **messages; size_t count, capacity;
    ik_replay_mark_stack_t mark_stack;
} ik_replay_context_t;
```

## API

### Connection
```c
res_t ik_db_init(TALLOC_CTX *ctx, const char *conn_str, ik_db_ctx_t **out_ctx);
res_t ik_db_init_with_migrations(TALLOC_CTX *ctx, const char *conn_str, const char *migrations_dir, ik_db_ctx_t **out_ctx);
res_t ik_db_begin(ik_db_ctx_t *db_ctx);
res_t ik_db_commit(ik_db_ctx_t *db_ctx);
res_t ik_db_rollback(ik_db_ctx_t *db_ctx);
```

### Session
```c
res_t ik_db_session_create(ik_db_ctx_t *db_ctx, int64_t *session_id_out);
res_t ik_db_session_get_active(ik_db_ctx_t *db_ctx, int64_t *session_id_out);
res_t ik_db_session_end(ik_db_ctx_t *db_ctx, int64_t session_id);
```

### Message
```c
res_t ik_db_message_insert(ik_db_ctx_t *db, int64_t session_id, const char *agent_uuid, const char *kind, const char *content, const char *data_json);
bool ik_db_message_is_valid_kind(const char *kind);
bool ik_msg_is_conversation_kind(const char *kind);
ik_msg_t *ik_msg_create_tool_result(void *parent, const char *tool_call_id, const char *name, const char *output, bool success, const char *content);
```

### Agent
```c
res_t ik_db_agent_insert(ik_db_ctx_t *db_ctx, const ik_agent_ctx_t *agent);
res_t ik_db_agent_mark_dead(ik_db_ctx_t *db_ctx, const char *uuid);
res_t ik_db_agent_get(ik_db_ctx_t *db_ctx, TALLOC_CTX *ctx, const char *uuid, ik_db_agent_row_t **out);
res_t ik_db_agent_list_running(ik_db_ctx_t *db_ctx, TALLOC_CTX *ctx, ik_db_agent_row_t ***out, size_t *count);
res_t ik_db_agent_get_children(ik_db_ctx_t *db_ctx, TALLOC_CTX *ctx, const char *parent_uuid, ik_db_agent_row_t ***out, size_t *count);
res_t ik_db_agent_get_parent(ik_db_ctx_t *db_ctx, TALLOC_CTX *ctx, const char *uuid, ik_db_agent_row_t **out);
res_t ik_db_agent_get_last_message_id(ik_db_ctx_t *db_ctx, const char *agent_uuid, int64_t *out_message_id);
res_t ik_db_agent_update_provider(ik_db_ctx_t *db_ctx, const char *uuid, const char *provider, const char *model, const char *thinking_level);
res_t ik_db_ensure_agent_zero(ik_db_ctx_t *db, char **out_uuid);
```

### Mail
```c
res_t ik_db_mail_insert(ik_db_ctx_t *db, int64_t session_id, ik_mail_msg_t *msg);
res_t ik_db_mail_inbox(ik_db_ctx_t *db, TALLOC_CTX *ctx, int64_t session_id, const char *to_uuid, ik_mail_msg_t ***out, size_t *count);
res_t ik_db_mail_inbox_filtered(ik_db_ctx_t *db, TALLOC_CTX *ctx, int64_t session_id, const char *to_uuid, const char *from_uuid, ik_mail_msg_t ***out, size_t *count);
res_t ik_db_mail_mark_read(ik_db_ctx_t *db, int64_t mail_id);
res_t ik_db_mail_delete(ik_db_ctx_t *db, int64_t mail_id, const char *recipient_uuid);
```

### Replay

Agent-based replay uses "walk backwards, play forwards": start at leaf, find most recent clear per agent, walk to parent if no clear, reverse for chronological order.

```c
res_t ik_agent_find_clear(ik_db_ctx_t *db_ctx, TALLOC_CTX *ctx, const char *agent_uuid, int64_t max_id, int64_t *clear_id_out);
res_t ik_agent_build_replay_ranges(ik_db_ctx_t *db_ctx, TALLOC_CTX *ctx, const char *agent_uuid, ik_replay_range_t **ranges_out, size_t *count_out);
res_t ik_agent_query_range(ik_db_ctx_t *db_ctx, TALLOC_CTX *ctx, const ik_replay_range_t *range, ik_msg_t ***messages_out, size_t *count_out);
res_t ik_agent_replay_history(ik_db_ctx_t *db_ctx, TALLOC_CTX *ctx, const char *agent_uuid, ik_replay_context_t **ctx_out);
res_t ik_db_messages_load(TALLOC_CTX *ctx, ik_db_ctx_t *db_ctx, int64_t session_id, ik_logger_t *logger);  // TEST-ONLY
```

### Migration
```c
res_t ik_db_migrate(ik_db_ctx_t *db_ctx, const char *migrations_dir);
```

Migrations in `migrations/NNN-description.sql`. Current: 001-initial, 002-agents, 003-agent-uuid, 004-mail, 005-multi-provider.

## Testing Patterns

Per-file database isolation for parallel execution:

```c
// Suite setup
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

// Suite teardown
ik_test_db_destroy(DB_NAME);
```

Test utilities: `ik_test_db_name`, `ik_test_db_create`, `ik_test_db_migrate`, `ik_test_db_connect`, `ik_test_db_begin`, `ik_test_db_rollback`, `ik_test_db_truncate_all`, `ik_test_db_destroy`

## Key Files

| Path | Purpose |
|------|---------|
| `src/db/connection.{h,c}` | Context, init, transactions |
| `src/db/session.{h,c}` | Session CRUD |
| `src/db/message.{h,c}` | Message insert, validation |
| `src/db/agent.{h,c}` | Agent registry, provider update |
| `src/db/agent_row.{h,c}` | Row parsing from PGresult |
| `src/db/agent_zero.{h,c}` | Agent 0 creation |
| `src/db/mail.{h,c}` | Inter-agent mail |
| `src/db/replay.{h,c}` | Session-based replay |
| `src/db/agent_replay.{h,c}` | Agent-based replay |
| `src/db/migration.{h,c}` | Migration execution |
| `src/db/pg_result.{h,c}` | PGresult talloc wrapper |
| `src/msg.h` | ik_msg_t definition |
| `src/mail/msg.h` | ik_mail_msg_t definition |
| `migrations/*.sql` | Schema migrations |
| `tests/test_utils.{h,c}` | DB test helpers |
