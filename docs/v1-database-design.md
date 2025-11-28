# v1.0 Database Design

PostgreSQL database schema, persistence strategy, and session management.

**Implementation Status:** Schema implemented in `migrations/001-initial-schema.sql`

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

**Lifecycle (Model B - Continuous Sessions):**
- Created when no active session exists (first launch or after `/new-session`)
- Sessions persist across app launches
- Detected via `ended_at IS NULL` query
- Multiple `/clear` commands = same session
- Multiple app launches = same session (until explicitly ended)
- Marked as ended only by `/new-session` command

### Messages Table (Event Stream Model)

```sql
CREATE TABLE messages (
    id BIGSERIAL PRIMARY KEY,
    session_id BIGINT REFERENCES sessions(id),
    kind TEXT NOT NULL,                     -- Event type: clear, system, user, assistant, mark, rewind
    content TEXT,                           -- Message content (NULL for clear events)
    data JSONB,                             -- Event-specific metadata (LLM params, tokens, rewind targets)
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX idx_messages_session ON messages(session_id, created_at);
CREATE INDEX idx_messages_search ON messages USING gin(to_tsvector('english', content));
```

**Event Kinds:**
- `clear` - Context reset (implicit at session start, explicit via `/clear`)
- `system` - System prompt
- `user` - User message
- `assistant` - LLM response
- `mark` - Checkpoint created by `/mark`
- `rewind` - Rollback operation created by `/rewind`

**Fields:**
- `id` - Auto-incrementing primary key
- `session_id` - Groups messages by launch session
- `kind` - Event type discriminator
- `content` - Message text (NULL for clear events)
- `data` - JSONB metadata (see Event Data Structures below)
- `created_at` - Timestamp for ordering and replay

**Event Data Structures:**

Each event kind stores specific metadata in the `data` JSONB field:

| Event Kind | Content | Data (JSONB) | Example |
|------------|---------|--------------|---------|
| `clear` | NULL or empty | `{}` or NULL | Session start, `/clear` command |
| `system` | System prompt text | `{}` | System message configuration |
| `user` | User's message | `{"model": "gpt-4", "temperature": 1.0, "max_completion_tokens": 4096}` | LLM parameters at request time |
| `assistant` | LLM response | `{"model": "gpt-4", "tokens": 150, "finish_reason": "stop"}` | Response metadata |
| `mark` | Optional label | `{"label": "approach-a"}` or `{"label": null, "number": 1}` | Checkpoint metadata |
| `rewind` | Target mark label | `{"target_message_id": 42, "target_label": "approach-a"}` | References mark event ID |

**Why JSONB for Metadata:**
- Flexible schema per event kind
- No dedicated columns for rarely-used fields
- Rewind targets stored as `{"target_message_id": 42}` instead of dedicated column
- Easy to extend with new metadata without migrations

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

// Session operations (Model B)
res_t ik_db_session_create(ik_db_ctx_t *db, int64_t *id_out);
res_t ik_db_session_get_active(ik_db_ctx_t *db, int64_t *id_out);  // Returns 0 if none active
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

### Session Lifecycle (Model B - Continuous Sessions)

**On App Launch:**
```c
// Check for active session
int64_t session_id;
TRY(ik_db_session_get_active(db, &session_id));

if (session_id > 0) {
    // Continue existing session
    repl->current_session_id = session_id;

    // Replay messages to restore state
    ik_message_t **messages;
    size_t count;
    TRY(ik_db_messages_load(db, session_id, &messages, &count));

    // Populate scrollback with replayed messages (after last clear)
    for (size_t i = 0; i < count; i++) {
        ik_scrollback_append(repl->scrollback, messages[i]);
    }
} else {
    // No active session - create new one
    TRY(ik_db_session_create(db, &session_id));
    repl->current_session_id = session_id;

    // Write initial clear event
    TRY(ik_db_message_insert(db, session_id, &(ik_message_t){
        .kind = "clear", .content = NULL, .data = "{}"
    }));

    // Write system message if configured
    if (config->system_message) {
        TRY(ik_db_message_insert(db, session_id, &(ik_message_t){
            .kind = "system", .content = config->system_message, .data = "{}"
        }));
    }
}
```

**During Session:**
```c
// All messages reference current session
TRY(ik_db_message_insert(repl->db, repl->current_session_id, msg));
```

**On App Exit:**
```c
// Session remains active (ended_at stays NULL)
// Just close database connection
ik_db_cleanup(repl->db);
```

**On /new-session Command:**
```c
// Explicitly end current session and start fresh
TRY(ik_db_session_end(repl->db, repl->current_session_id));
TRY(ik_db_session_create(repl->db, &repl->current_session_id));
TRY(ik_db_message_insert(repl->db, repl->current_session_id,
    &(ik_message_t){.kind = "clear", .content = NULL, .data = "{}"}));
ik_scrollback_clear(repl->scrollback);
```

### Session vs. Context

**Session (database concept - Model B):**
- All messages from potentially multiple app launches
- Persists across `/clear` commands AND app restarts
- Chronological grouping over session lifetime
- Auto-restored on launch via replay

**Context (scrollback concept):**
- Messages currently visible (reconstructed from events after last clear)
- Cleared by `/clear` but persists in database
- Defines what LLM sees
- Restored from database on launch (Model B)

**Example Flow (Model B):**
```
Launch 1 → No active session, create session 42, INSERT clear event, scrollback empty
User: "Hello" → INSERT user event to session 42, scrollback shows ["Hello"]
LLM: "Hi!" → INSERT assistant event to session 42, scrollback shows ["Hello", "Hi!"]
Exit → Session 42 remains active (ended_at=NULL), 3 events in DB

Launch 2 → Detect active session 42, replay [clear, user, assistant] → scrollback shows ["Hello", "Hi!"]
User: "Continue" → INSERT user event to session 42, scrollback shows ["Hello", "Hi!", "Continue"]
/clear → INSERT clear event to session 42, scrollback empty, session 42 has 5 events
User: "New topic" → INSERT user event to session 42, scrollback shows ["New topic"]
Exit → Session 42 remains active, 6 events in DB

Launch 3 → Replay finds last clear, scrollback shows ["New topic"] (context after last clear)
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
-- DB events (messages table):
1. kind=user, content="Feature X"
2. kind=assistant, content="Approach A..."
3. kind=mark, content="approach-a", data={"label": "approach-a"}
4. kind=user, content="Try B"
5. kind=assistant, content="Approach B..."
6. kind=rewind, data={"target_message_id": 3, "target_label": "approach-a"}
7. kind=user, content="Improve A"
8. kind=assistant, content="Improved A..."

-- Context after replay:
["Feature X", "Approach A...", mark("approach-a"), "Improve A", "Improved A..."]
-- Events 4-5 not in context (rewound past), but preserved in DB for audit trail
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
        "SELECT id, session_id, kind, content, data, created_at "
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
