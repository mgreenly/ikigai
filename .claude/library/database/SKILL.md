---
name: database
description: PostgreSQL database skill for ikigai
---

# Database

PostgreSQL event stream with agents, sessions, messages, mail.

## Connection

```
DATABASE_URL=postgresql://user:pass@host:port/dbname
Test: postgresql://ikigai:ikigai@localhost/ikigai_test_<basename>
```

## Schema (v5)

```sql
-- sessions: conversation grouping
CREATE TABLE sessions (
  id BIGSERIAL PRIMARY KEY,
  started_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
  ended_at TIMESTAMPTZ,
  title TEXT
);

-- messages: event stream
CREATE TABLE messages (
  id BIGSERIAL PRIMARY KEY,
  session_id BIGINT NOT NULL REFERENCES sessions(id) ON DELETE CASCADE,
  kind TEXT NOT NULL,  -- clear|system|user|assistant|tool_call|tool_result|mark|rewind|agent_killed|command|fork
  content TEXT,
  data JSONB,
  created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
  agent_uuid TEXT REFERENCES agents(uuid)
);

-- agents: registry with parent-child
CREATE TYPE agent_status AS ENUM ('running', 'dead');
CREATE TABLE agents (
  uuid TEXT PRIMARY KEY,  -- base64url, 22 chars
  name TEXT,
  parent_uuid TEXT REFERENCES agents(uuid) ON DELETE RESTRICT,
  fork_message_id BIGINT,
  status agent_status NOT NULL DEFAULT 'running',
  created_at BIGINT NOT NULL,  -- unix epoch
  ended_at BIGINT,
  provider TEXT,        -- anthropic, openai, etc
  model TEXT,           -- claude-opus-4.5, etc
  thinking_level TEXT   -- thinking budget
);

-- mail: inter-agent messaging
CREATE TABLE mail (
  id BIGSERIAL PRIMARY KEY,
  session_id BIGINT NOT NULL REFERENCES sessions(id) ON DELETE CASCADE,
  from_uuid TEXT NOT NULL,
  to_uuid TEXT NOT NULL,
  body TEXT NOT NULL,
  timestamp BIGINT NOT NULL,  -- unix epoch
  read INTEGER NOT NULL DEFAULT 0
);
```

## Key Types

```c
typedef struct { PGconn *conn; } ik_db_ctx_t;
typedef struct { int64_t id; char *kind, *content, *data_json; } ik_msg_t;
typedef struct { char *uuid, *name, *parent_uuid, *status, *provider, *model, *thinking_level; int64_t created_at, ended_at; } ik_db_agent_row_t;
typedef struct { int64_t id; char *from_uuid, *to_uuid, *body; int64_t timestamp; bool read; } ik_mail_msg_t;
typedef struct { char *agent_uuid; int64_t start_id, end_id; } ik_replay_range_t;
typedef struct { ik_msg_t **messages; size_t count, capacity; } ik_replay_context_t;
```

## Core APIs

```c
// Connection
res_t ik_db_init(TALLOC_CTX *ctx, const char *conn_str, ik_db_ctx_t **out);
res_t ik_db_begin/commit/rollback(ik_db_ctx_t *db);

// Sessions
res_t ik_db_session_create(ik_db_ctx_t *db, int64_t *id_out);
res_t ik_db_session_get_active(ik_db_ctx_t *db, int64_t *id_out);  // 0 if none
res_t ik_db_session_end(ik_db_ctx_t *db, int64_t session_id);

// Messages
res_t ik_db_message_insert(ik_db_ctx_t *db, int64_t session_id, const char *agent_uuid, const char *kind, const char *content, const char *data_json);
bool ik_db_message_is_valid_kind(const char *kind);
bool ik_msg_is_conversation_kind(const char *kind);  // true: system,user,assistant,tool_call,tool_result,tool

// Agents
res_t ik_db_agent_insert(ik_db_ctx_t *db, const ik_agent_ctx_t *agent);
res_t ik_db_agent_mark_dead(ik_db_ctx_t *db, const char *uuid);
res_t ik_db_agent_get(ik_db_ctx_t *db, TALLOC_CTX *ctx, const char *uuid, ik_db_agent_row_t **out);
res_t ik_db_agent_list_running(ik_db_ctx_t *db, TALLOC_CTX *ctx, ik_db_agent_row_t ***out, size_t *count);
res_t ik_db_agent_get_children/parent(ik_db_ctx_t *db, TALLOC_CTX *ctx, const char *uuid, ...);
res_t ik_db_ensure_agent_zero(ik_db_ctx_t *db, char **out_uuid);

// Mail
res_t ik_db_mail_insert(ik_db_ctx_t *db, int64_t session_id, ik_mail_msg_t *msg);
res_t ik_db_mail_inbox(ik_db_ctx_t *db, TALLOC_CTX *ctx, int64_t session_id, const char *to_uuid, ik_mail_msg_t ***out, size_t *count);
res_t ik_db_mail_mark_read(ik_db_ctx_t *db, int64_t mail_id);

// Replay (agent-based)
res_t ik_agent_replay_history(ik_db_ctx_t *db, TALLOC_CTX *ctx, const char *agent_uuid, ik_replay_context_t **out);
res_t ik_agent_build_replay_ranges(ik_db_ctx_t *db, TALLOC_CTX *ctx, const char *agent_uuid, ik_replay_range_t **ranges, size_t *count);
```

## Testing

```c
// Per-file DB isolation
const char *DB_NAME = ik_test_db_name(NULL, __FILE__);  // → ikigai_test_<basename>
ik_test_db_create(DB_NAME);
ik_test_db_migrate(NULL, DB_NAME);

// Per-test transaction isolation
ik_test_db_connect(ctx, DB_NAME, &db);
ik_test_db_begin(db);
// ... test ...
ik_test_db_rollback(db);

// Cleanup
ik_test_db_destroy(DB_NAME);
```

## Files

| Path | Purpose |
|------|---------|
| `src/db/connection.c` | Init, transactions, migrations |
| `src/db/session.c` | Session CRUD |
| `src/db/message.c` | Message insert, kind validation |
| `src/db/agent.c` | Agent registry |
| `src/db/mail.c` | Inter-agent mail |
| `src/db/agent_replay.c` | Ancestry-based replay |
| `migrations/*.sql` | Schema migrations |
| `tests/test_utils.c` | DB test helpers |
