# v1.0 Database Design

PostgreSQL database schema, persistence strategy, and session management.

## Database Role in Architecture

**Key Principle:** Database is for permanent archival and search, NOT for automatic context injection.

**Purpose:**
- Permanent storage of all interactions
- Never automatically loaded into scrollback
- Tool-based search and retrieval (LLM uses database tools)
- Session-based organization
- Complete audit trail including rewind operations

**NOT Used For:**
- Auto-loading history on startup
- Automatic RAG context injection
- Defining current LLM context (scrollback does this)

---

## Database Schema

### Sessions Table

```sql
CREATE TABLE sessions (
    id BIGSERIAL PRIMARY KEY,
    started_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    ended_at TIMESTAMPTZ,          -- NULL if still active
    title TEXT                      -- Optional user-defined title
);

CREATE INDEX idx_sessions_started ON sessions(started_at DESC);
```

**Lifecycle:**
- Created when ikigai launches
- One session per app launch
- Multiple `/clear` commands = same session
- Marked as ended when app exits

### Messages Table

```sql
CREATE TABLE messages (
    id BIGSERIAL PRIMARY KEY,
    session_id BIGINT REFERENCES sessions(id),
    role TEXT NOT NULL CHECK (role IN ('user', 'assistant', 'system', 'mark', 'rewind')),
    content TEXT NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    tokens INTEGER,
    model TEXT,
    rewind_to_message_id BIGINT REFERENCES messages(id)  -- For rewind: points to mark
);

CREATE INDEX idx_messages_session ON messages(session_id, created_at);
CREATE INDEX idx_messages_search ON messages USING gin(to_tsvector('english', content));
```

**Message Roles:**
- `user` - User input
- `assistant` - LLM response
- `system` - System prompts
- `mark` - Checkpoint created by `/mark`
- `rewind` - Rollback operation created by `/rewind`

**Fields:**
- `id` - Auto-incrementing primary key
- `session_id` - Groups messages by launch session
- `role`, `content` - Message data
- `created_at` - Timestamp for ordering
- `tokens` - Token count from API (if available)
- `model` - Model identifier (e.g., "gpt-4")
- `rewind_to_message_id` - For rewind messages: references target mark

---

## Persistence Strategy: Immediate Writes

**Decision:** Messages written to database as they are created (synchronous).

**Rationale:**
- Database is source of truth
- Single-threaded → synchronous writes simple
- PostgreSQL fast enough for interactive use
- Eliminates data loss on crash
- No complex sync logic
- Invariant: database and memory always match

**Performance:** User messages are infrequent (human typing speed), acceptable latency

**Error Handling:**
- If INSERT fails → message NOT added to session_messages
- Display error to user in scrollback
- User can retry
- Maintains invariant: DB and memory match

---

## Database Module Interface

```c
typedef struct ik_db_ctx_t ik_db_ctx_t;

// Initialize database connection
res_t ik_db_init(void *parent, const char *conn_str, ik_db_ctx_t **out);

// Session operations
res_t ik_db_session_create(ik_db_ctx_t *db, int64_t *id_out);
res_t ik_db_session_end(ik_db_ctx_t *db, int64_t session_id);
res_t ik_db_session_load(ik_db_ctx_t *db, int64_t id, ik_session_t **out);
res_t ik_db_session_list(ik_db_ctx_t *db, ik_session_t ***out, size_t *count);

// Message operations
res_t ik_db_message_insert(ik_db_ctx_t *db, int64_t session_id, ik_message_t *msg);
res_t ik_db_messages_load(ik_db_ctx_t *db, int64_t session_id, ik_message_t ***out, size_t *count);

// Search operations (for tool use)
res_t ik_db_search(ik_db_ctx_t *db, const char *query, ik_message_t ***out, size_t *count);
res_t ik_db_search_in_session(ik_db_ctx_t *db, int64_t session_id, const char *query, ik_message_t ***out, size_t *count);
```

---

## Session Management

### Session Lifecycle

**On App Launch:**
```c
// Create new session, store ID in repl->current_session_id
int64_t session_id;
TRY(ik_db_session_create(db, &session_id));
```

**During Session:**
```c
// All messages reference current session
TRY(ik_db_message_insert(repl->db, repl->current_session_id, msg));
```

**On App Exit:**
```c
// Mark session as ended
ik_db_session_end(repl->db, repl->current_session_id);
```

### Session vs. Context

**Session (database concept):**
- All messages from one app launch
- Persists across `/clear` commands
- Chronological grouping
- Never auto-loaded

**Context (scrollback concept):**
- Messages currently visible
- Cleared by `/clear`
- Defines what LLM sees
- Starts blank on launch

**Example Flow:**
```
Launch → Create session 42, scrollback empty
User: "Hello" → INSERT to session 42, scrollback shows ["Hello"]
LLM: "Hi!" → INSERT to session 42, scrollback shows ["Hello", "Hi!"]
/clear → No DB changes, scrollback empty, session 42 still has 2 messages
User: "New topic" → INSERT to session 42, scrollback shows ["New topic"]
```

---

## Database Replay: Reconstructing Context

Database contains complete history including mark/rewind operations. Replay reconstructs exact context states.

**Implementation Pattern:**
```c
res_t ik_session_replay(ik_db_ctx_t *db, int64_t session_id,
                        ik_message_t ***session_messages_out, size_t *count_out) {
    // Load all messages in order
    // Build context array (shrinks on rewind)
    // Process marks: add to marks stack
    // Process rewinds: truncate to mark position, remove marks
    // Regular messages: add to context
    // Return final context state
}
```

**Why Replay Matters:**
- Complete audit trail with dead ends
- Reconstruct exact LLM context at any point
- Enables analysis and debugging
- Supports research

**Example:**
```sql
-- DB messages:
1. [user] "Feature X"
2. [assistant] "Approach A..."
3. [mark] "approach-a"
4. [user] "Try B"
5. [assistant] "Approach B..."
6. [rewind] (to message 3)
7. [user] "Improve A"
8. [assistant] "Improved A..."

-- Context after replay:
["Feature X", "Approach A...", mark, "Improve A", "Improved A..."]
-- Messages 4-5 not in context (rewound past), but preserved in DB
```

---

## Full-Text Search

**PostgreSQL GIN Index:**
```sql
CREATE INDEX idx_messages_search ON messages
USING gin(to_tsvector('english', content));
```

**Search Implementation:**
```c
// Basic search across all messages
res_t ik_db_search(ik_db_ctx_t *db, const char *query, ik_message_t ***out, size_t *count) {
    const char *sql =
        "SELECT id, session_id, role, content, created_at, tokens, model, rewind_to_message_id "
        "FROM messages "
        "WHERE to_tsvector('english', content) @@ plainto_tsquery('english', $1) "
        "ORDER BY created_at DESC LIMIT 50";
    // Execute with parameter, parse results
}

// Search within specific session
res_t ik_db_search_in_session(ik_db_ctx_t *db, int64_t session_id, const char *query, ...);
```

---

## Database Error Handling

**Connection Lost:**
- Check connection before queries
- Display error to user
- Continue in memory-only mode
- Messages kept in session_messages array
- Lost on exit, but LLM interaction continues

**Insert Failures (connection lost, constraint violation, disk full, permissions):**
- Display specific error message
- Don't add message to session_messages
- Allow user to retry
- Maintain invariant: DB and memory match

---

## Database Initialization

**Schema Creation:**
```c
res_t ik_db_init_schema(ik_db_ctx_t *db) {
    // CREATE TABLE IF NOT EXISTS sessions (...)
    // CREATE TABLE IF NOT EXISTS messages (...)
    // CREATE INDEX IF NOT EXISTS (...)
}
```

**Connection String Format:**
`postgresql://[user[:password]@][host][:port][/dbname][?param=value]`

**Examples:**
- `postgresql://localhost/ikigai`
- `postgresql://user:pass@localhost:5432/ikigai`
- `postgresql:///ikigai?host=/var/run/postgresql`

**Configuration:**
```c
// In config.json
{"db_connection_string": "postgresql://localhost/ikigai"}
```

---

## Future Enhancements

**Conversation Titles:** Auto-generate from first user message

**Token Usage Tracking:** Aggregate per session for analysis

**Context Window Size Tracking:** Track message count per LLM call for analysis

---

## Testing Strategy

**Unit Tests:**
- Connection establishment
- Session create/end
- Message insert/load
- Full-text search
- Error handling (connection lost, constraints)
- Use test database: `postgresql://localhost/ikigai_test`

**Integration Tests:**
- Full workflow: create session, insert messages, load back, search, verify, cleanup
- Verify data matches
- Test replay functionality

---

## Related Documentation

- [v1-architecture.md](v1-architecture.md) - Overall v1.0 architecture
- [v1-llm-integration.md](v1-llm-integration.md) - HTTP client and streaming
- [v1-conversation-management.md](v1-conversation-management.md) - Message lifecycle
- [v1-implementation-roadmap.md](v1-implementation-roadmap.md) - Implementation phases
