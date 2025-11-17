# Logical Architecture Analysis

This document analyzes the logical construction and data flow of ikigai's main modules: REPL, buffer management, HTTP client (for LLM integration), and database persistence.

## Current State (v0.1.0)

### Implemented: REPL Foundation

**Main Components:**
- `ik_repl_ctx_t` - Top-level REPL context orchestrating all subsystems
- `ik_input_buffer_t` - Editable multi-line input below separator
- `ik_scrollback_t` - Immutable conversation history above separator
- `ik_term_ctx_t` - Terminal state (raw mode, dimensions)
- `ik_render_ctx_t` - Framebuffer for single-write rendering

**Data Flow (Current):**
```
Terminal Input → Input Parser → REPL Actions → Input Buffer Mutation → Render Frame → Terminal
                                     ↓
                              (on submit/Enter)
                                     ↓
                              Scrollback Append
```

**Current Entry Point (`src/client.c`):**
```c
main()
  └─> ik_repl_init(root_ctx, &repl)
       ├─> Creates terminal context
       ├─> Creates input buffer
       ├─> Creates scrollback
       └─> Creates render context
  └─> ik_repl_run(repl)  // Event loop
       └─> while (!repl->quit)
            ├─> Read raw bytes from terminal
            ├─> Parse to semantic actions
            ├─> Apply actions to input buffer
            └─> Render frame to terminal
```

**Characteristics:**
- **Single-threaded event loop** - Sequential processing
- **Talloc hierarchy** - Root context owns all subsystems
- **Direct terminal control** - Raw mode, ANSI escape sequences
- **O(1) viewport calculations** - Pre-computed display widths
- **Immutable scrollback** - Append-only history

---

## Future Integration: HTTP + Database

### Key Questions

Before analyzing the architecture, we need to consider several design decisions:

#### 1. Conversation State Management

**Decision:** Database is the permanent record. Scrollback defines the active LLM context.

**Architecture:**
```
Database (permanent history)
  ↑ persist
  |
Active Context (what's in scrollback right now)
  ↓ sent to LLM

LLM sees only messages currently in scrollback
```

**Critical Insight: Scrollback IS the Context Window**

- **On startup or `/clear`**: Scrollback is BLANK - fresh conversation context
- **Database stores everything**: All messages persist permanently
- **Only scrollback goes to LLM**: Messages in scrollback = conversation history sent to API
- **User controls context**: What you see is what the LLM sees

**Implications:**
- Scrollback boundary represents the current conversation context
- `/clear` creates fresh context WITHOUT deleting from database
- Can selectively load messages from DB into scrollback if needed
- Token usage controlled by scrollback size (not entire DB history)
- User has explicit control over what context the LLM receives
- Database enables search/recall but doesn't auto-inject into context

#### 2. Message Data Structures

**Question:** How do we represent messages (user/assistant/system)?

**Considerations:**
- Messages need to support both display (scrollback) and API format (JSON)
- Need to track: role, content, timestamp, tokens, model
- May need to store raw API responses for debugging
- Tools will add structured content (function calls, results)

**Potential Structure:**
```c
typedef struct {
    int64_t id;                    // Database primary key (0 if not persisted)
    char *role;                    // "user", "assistant", "system", "mark", "rewind"
    char *content;                 // Text content or label for marks
    char *timestamp;               // ISO 8601
    int32_t tokens;                // Token count (if available)
    char *model;                   // Model identifier
    int64_t rewind_to_message_id;  // For rewind messages: points to mark
    // Future: tool calls, attachments
} ik_message_t;

typedef struct {
    int64_t id;              // Database primary key
    char *title;             // Human-readable title
    char *created_at;        // ISO 8601
    ik_message_t **messages; // Array of message pointers
    size_t message_count;
} ik_conversation_t;
```

#### 3. HTTP Client Integration

**Question:** Where does the HTTP client fit in the architecture?

**Options:**
- **A) Part of REPL** - REPL calls `ik_llm_send_streaming()` directly
- **B) Separate LLM module** - `ik_llm_ctx_t` manages API state
- **C) Provider abstraction** - `ik_provider_t` interface with OpenAI/Anthropic impls

**Current Plan (from architecture.md):**
- Start with OpenAI-specific client (libcurl streaming)
- Later abstract to multi-provider interface

#### 4. Streaming Response Handling

**Question:** How do streaming chunks flow from HTTP to display?

**Current Event Loop Pattern:**
```c
while (!repl->quit) {
    read_input();
    parse_input();
    apply_actions();
    render_frame();
}
```

**With Streaming:**
```c
while (!repl->quit) {
    if (waiting_for_llm) {
        // Poll for HTTP chunks
        res_t chunk = ik_http_read_chunk(http_ctx);
        if (is_ok(&chunk)) {
            append_to_scrollback(chunk.data);
            render_frame();  // Show chunk immediately
        }
    } else {
        // Normal input handling
        read_input();
        parse_input();
        apply_actions();
        render_frame();
    }
}
```

**Alternative: Callback-based:**
```c
// On submit
ik_llm_send_streaming(llm_ctx, prompt, on_chunk_callback, repl);

// Callback
void on_chunk_callback(void *user_data, const char *chunk) {
    ik_repl_ctx_t *repl = user_data;
    ik_scrollback_append(repl->scrollback, chunk);
    ik_repl_render_frame(repl);
}
```

**Question:** Blocking vs. Non-blocking?
- **Blocking:** Simple but can't handle terminal input during stream
- **Non-blocking:** Can scroll/cancel during stream, more complex

#### 5. Database Persistence Points

**Decision:** Immediate persistence - messages are written to database as they are created.

**Rationale:**
- Database is source of truth, must be authoritative
- Single-threaded model makes synchronous writes simple
- PostgreSQL writes are fast enough for interactive use
- Eliminates data loss on crash
- No complex sync logic needed

**Error Handling:**
- If INSERT fails, message is not added to in-memory conversation
- Error displayed to user in scrollback
- User can retry or handle as needed
- Maintains invariant: database and memory always match

**Performance Considerations:**
- Modern PostgreSQL can handle thousands of INSERTs per second
- User messages are infrequent (human typing speed)
- LLM responses are one INSERT per complete message
- Acceptable latency for interactive terminal app
- Future: Could batch if profiling shows issues

#### 6. Slash Command Architecture

**Question:** How do slash commands integrate?

**Current Handling:**
- `/pp` command implemented in `repl_actions.c:handle_submit()`
- Hard-coded special case

**Options for Scalability:**
- **A) Command dispatcher** - Map `/command` → handler function
- **B) Plugin system** - Loadable command modules
- **C) Inline handling** - Keep growing the switch statement

**Considerations:**
- May need `/model`, `/provider` for LLM control
- Future: `/tool`, `/edit`, `/run` for tool execution

---

## Proposed Architecture: Conversation-Centric Model

### Core Concept

The REPL event loop orchestrates three main concerns:
1. **Terminal UI** (input buffer, scrollback, rendering)
2. **Conversation Management** (messages, state, persistence)
3. **LLM Communication** (HTTP client, streaming, parsing)

### Module Ownership

```
ik_repl_ctx_t (root context)
├─> ik_term_ctx_t *term                    // Terminal state
├─> ik_render_ctx_t *render                // Rendering
├─> ik_input_buffer_t *input_buffer        // User input
├─> ik_scrollback_t *scrollback            // Display layer (decorated text)
├─> ik_message_t **session_messages        // Active LLM context
├─> size_t session_message_count
├─> ik_mark_t **marks                      // Checkpoint stack for /rewind
├─> size_t mark_count
├─> ik_llm_client_t *llm                   // HTTP client (streaming)
├─> ik_db_ctx_t *db                        // PostgreSQL connection
└─> char *streaming_buffer                 // Accumulates current LLM response
```

**Note:** No `ik_conversation_t` needed. Session messages are tracked directly in REPL context.

### Data Flow: User Message → LLM Response

```
1. User types message and hits Enter
   ↓
2. ik_repl_handle_submit()
   ├─> Extract text from input_buffer
   ├─> Create ik_message_t (role="user", content=text)
   ├─> INSERT to database immediately → get message ID
   ├─> Add to current_conv->messages[] (with DB id)
   └─> Render to scrollback (with decorations)
   ↓
3. Check for slash command
   ├─> If starts with '/', dispatch to command handler
   └─> Otherwise, continue to LLM
   ↓
4. Send to LLM
   ├─> Build API request from current_conv->messages[]
   ├─> ik_llm_send_streaming(llm, messages, chunk_callback)
   └─> Add "streaming..." indicator to scrollback
   ↓
5. Process streaming chunks
   ├─> Callback: on_chunk(repl, chunk_text)
   ├─> Append chunk to scrollback immediately (decorated)
   ├─> Render frame (user sees response building)
   └─> Accumulate full response in buffer
   ↓
6. Stream complete
   ├─> Create ik_message_t (role="assistant", content=full_response)
   ├─> INSERT to database immediately → get message ID
   ├─> Add to current_conv->messages[] (with DB id)
   └─> Replace streaming indicator with final decorated message in scrollback
```

**Key Points:**
- Database writes happen synchronously before updating in-memory state
- Message IDs from database are stored in `ik_message_t` objects
- Scrollback rendering happens after database persistence
- If database write fails, message is not added to conversation or displayed
- This ensures database and display stay in sync

### Database Schema (Sketch)

```sql
CREATE TABLE conversations (
    id BIGSERIAL PRIMARY KEY,
    title TEXT NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE TABLE messages (
    id BIGSERIAL PRIMARY KEY,
    conversation_id BIGINT NOT NULL REFERENCES conversations(id),
    role TEXT NOT NULL CHECK (role IN ('user', 'assistant', 'system', 'mark', 'rewind')),
    content TEXT NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    tokens INTEGER,
    model TEXT,
    sequence_num INTEGER NOT NULL,  -- Order within conversation
    rewind_to_message_id BIGINT REFERENCES messages(id)  -- For rewind: points to mark
);

CREATE INDEX idx_messages_conversation ON messages(conversation_id, sequence_num);
```

### HTTP Client Module (Sketch)

```c
typedef struct ik_llm_client_t ik_llm_client_t;

// Callback for streaming chunks
typedef void (*ik_chunk_callback_t)(void *user_data, const char *chunk);

// Initialize LLM client
res_t ik_llm_init(void *parent, const char *api_key, ik_llm_client_t **out);

// Send streaming request
res_t ik_llm_send_streaming(
    ik_llm_client_t *client,
    ik_message_t **messages,
    size_t message_count,
    ik_chunk_callback_t on_chunk,
    void *user_data
);

// Poll for chunks (non-blocking variant)
res_t ik_llm_poll_chunk(ik_llm_client_t *client, char **chunk_out);

// Check if stream is complete
bool ik_llm_is_complete(ik_llm_client_t *client);
```

### Database Module (Sketch)

```c
typedef struct ik_db_ctx_t ik_db_ctx_t;

// Initialize database connection
res_t ik_db_init(void *parent, const char *conn_str, ik_db_ctx_t **out);

// Conversation operations
res_t ik_db_conversation_create(ik_db_ctx_t *db, const char *title, int64_t *id_out);
res_t ik_db_conversation_load(ik_db_ctx_t *db, int64_t id, ik_conversation_t **out);
res_t ik_db_conversation_list(ik_db_ctx_t *db, ik_conversation_t ***out, size_t *count);

// Message operations
res_t ik_db_message_insert(ik_db_ctx_t *db, int64_t conv_id, ik_message_t *msg);
res_t ik_db_messages_load(ik_db_ctx_t *db, int64_t conv_id, ik_message_t ***out, size_t *count);

// Search
res_t ik_db_search(ik_db_ctx_t *db, const char *query, ik_message_t ***out, size_t *count);
```

---

## Architectural Implications

### Initialization Sequence

With scrollback-as-context-window, the startup sequence is:

```
1. main()
   ├─> Load config (~/.ikigai/config.json)
   ├─> Connect to database (ik_db_init)
   └─> Initialize REPL (ik_repl_init)

2. ik_repl_init()
   ├─> Initialize terminal (ik_term_init)
   ├─> Initialize scrollback buffer (ik_scrollback_init) → BLANK/EMPTY
   ├─> Initialize input buffer (ik_input_buffer_init)
   └─> Initialize LLM client (ik_llm_init)

3. ik_repl_run()
   └─> Event loop begins with EMPTY scrollback - fresh context
```

**Key Insight:** Scrollback starts BLANK. Only messages added during the session (or explicitly loaded from DB) are part of the LLM context.

**Context Building:**
```
User sends message
  ↓
User message appears in scrollback
  ↓
LLM receives [user message] as context
  ↓
LLM response streams into scrollback
  ↓
Next user message
  ↓
LLM receives [previous user, previous assistant, new user] as context
  ↓
Context grows with each exchange (visible in scrollback)
```

### Loading Context from Database

Unlike traditional chat UIs, ikigai doesn't auto-load history. Scrollback starts blank and defines the LLM context.

**When to load from database:**
- **Never on startup** - Fresh context every session
- **Tool-based operations** - LLM can use tools to search and load relevant context when needed

**Key Principle:** Database is for recall and search, not automatic context injection. User decides what goes into the active context window (scrollback).

### Message Object Lifecycle

**Dual Tracking: Session Messages + Scrollback Display**

```c
struct ik_repl_ctx_t {
    ik_scrollback_t *scrollback;       // Display lines (formatted, decorated)
    ik_message_t **session_messages;   // Structured messages (for LLM API)
    size_t session_message_count;      // Count of messages in current session
    // ... other fields ...
};
```

**Creation Flow:**
```c
// User submits message
ik_message_t *msg = create_message(repl, "user", input_text);
  ↓
// Persist to database immediately
res_t result = ik_db_message_insert(db, msg);
  ↓
if (is_ok(&result)) {
    msg->id = result.data;  // Store DB-assigned ID

    // Add to session messages (source for LLM API)
    append_to_session_messages(repl, msg);

    // Render to scrollback (display layer)
    render_message_to_scrollback(repl->scrollback, msg);
} else {
    show_error("Failed to save message");
    talloc_free(msg);
}
```

**Memory Ownership:**
```
root_ctx
  └─> repl_ctx
       ├─> session_messages[] (messages in current context)
       │    ├─> msg[0] (owned by repl_ctx)
       │    ├─> msg[1]
       │    └─> msg[n]
       └─> scrollback
            └─> lines[] (display strings, derived from session_messages)
```

**Key Points:**
- `session_messages[]` is the source of truth for current LLM context
- Scrollback is a rendering of `session_messages[]` with decorations
- Both grow together during the session
- `/clear` empties both (but DB keeps all messages)
- When sending to LLM: build API request from `session_messages[]`

### /clear Command Behavior

The `/clear` command creates a fresh context without deleting anything from the database:

```c
// User types: /clear
res_t handle_clear_command(ik_repl_ctx_t *repl) {
    // Clear scrollback display
    ik_scrollback_clear(repl->scrollback);

    // Clear session messages (LLM context)
    for (size_t i = 0; i < repl->session_message_count; i++) {
        talloc_free(repl->session_messages[i]);
    }
    talloc_free(repl->session_messages);
    repl->session_messages = NULL;
    repl->session_message_count = 0;

    // Clear marks
    for (size_t i = 0; i < repl->mark_count; i++) {
        talloc_free(repl->marks[i]);
    }
    talloc_free(repl->marks);
    repl->marks = NULL;
    repl->mark_count = 0;

    // Database still has all messages - nothing deleted

    // Render fresh frame
    ik_repl_render_frame(repl);

    return OK(NULL);
}
```

**Effect:**
- User sees empty scrollback (fresh context)
- Next LLM request will have no history
- All previous messages remain searchable in database
- Can selectively load context from DB if needed

### /mark and /rewind Commands

The `/mark` and `/rewind` commands provide conversation checkpointing and rollback capabilities:

**`/mark [LABEL]`** - Creates a checkpoint at the current position
- Optional label for easy reference
- Persisted to database as a 'mark' message type
- Visible in scrollback as a separator
- Multiple marks can exist simultaneously

**`/rewind [LABEL]`** - Rolls back to a specific checkpoint
- Without label: rewinds to most recent mark (LIFO)
- With label: rewinds to most recent mark with matching label
- Removes messages after the mark from in-memory context
- Persists 'rewind' operation to database for replay
- Database retains all messages (complete audit trail)

#### Data Structures

```c
typedef struct {
    size_t message_index;          // Position in session_messages[] array
    int64_t db_message_id;         // ID of the mark message in database
    char *label;                   // Optional user label (or NULL)
    char *timestamp;               // ISO 8601
} ik_mark_t;

struct ik_repl_ctx_t {
    // ... existing fields ...
    ik_mark_t **marks;             // Stack of marks (LIFO)
    size_t mark_count;
};
```

#### Implementation

```c
// /mark [label]
res_t cmd_mark(ik_repl_ctx_t *repl, const char *args) {
    // Create mark message
    ik_message_t *mark_msg = talloc_zero(repl, ik_message_t);
    mark_msg->role = talloc_strdup(mark_msg, "mark");
    mark_msg->content = args && strlen(args) > 0 ?
                        talloc_strdup(mark_msg, args) :
                        talloc_strdup(mark_msg, "");
    mark_msg->timestamp = get_iso8601_timestamp(mark_msg);

    // Persist to database immediately
    TRY(ik_db_message_insert(repl->db, repl->current_session_id, mark_msg));

    // Create in-memory mark structure
    ik_mark_t *mark = talloc_zero(repl, ik_mark_t);
    mark->message_index = repl->session_message_count;
    mark->db_message_id = mark_msg->id;
    mark->label = mark_msg->content[0] != '\0' ?
                  talloc_strdup(mark, mark_msg->content) : NULL;

    // Add to marks array
    repl->marks = talloc_realloc(repl, repl->marks, ik_mark_t*, repl->mark_count + 1);
    repl->marks[repl->mark_count++] = mark;

    // Add mark message to session_messages
    repl->session_messages = talloc_realloc(repl, repl->session_messages,
                                           ik_message_t*, repl->session_message_count + 1);
    repl->session_messages[repl->session_message_count++] = mark_msg;

    // Display marker in scrollback
    render_mark_to_scrollback(repl->scrollback, mark);

    return OK(NULL);
}

// /rewind [label]
res_t cmd_rewind(ik_repl_ctx_t *repl, const char *args) {
    if (repl->mark_count == 0) {
        return ERR("No marks to rewind to. Use /mark to create a checkpoint.");
    }

    // Find target mark
    ik_mark_t *target_mark = NULL;
    ssize_t target_index = -1;

    if (args && strlen(args) > 0) {
        // Search for mark with matching label (most recent first)
        for (ssize_t i = repl->mark_count - 1; i >= 0; i--) {
            if (repl->marks[i]->label && strcmp(repl->marks[i]->label, args) == 0) {
                target_mark = repl->marks[i];
                target_index = i;
                break;
            }
        }
        if (!target_mark) {
            return ERR("No mark found with label: %s", args);
        }
    } else {
        // No label - use most recent mark
        target_mark = repl->marks[repl->mark_count - 1];
        target_index = repl->mark_count - 1;
    }

    // Create rewind message in database
    ik_message_t *rewind_msg = talloc_zero(repl, ik_message_t);
    rewind_msg->role = talloc_strdup(rewind_msg, "rewind");
    rewind_msg->content = talloc_asprintf(rewind_msg,
        "Rewind to mark%s%s",
        target_mark->label ? ": " : "",
        target_mark->label ? target_mark->label : "");
    rewind_msg->rewind_to_message_id = target_mark->db_message_id;

    // Persist rewind operation to database
    ik_db_message_insert(repl->db, repl->current_session_id, rewind_msg);

    // Free messages after the mark (already in DB)
    for (size_t i = target_mark->message_index; i < repl->session_message_count; i++) {
        talloc_free(repl->session_messages[i]);
    }
    repl->session_message_count = target_mark->message_index;

    // Remove marks at and after the target
    for (ssize_t i = repl->mark_count - 1; i >= target_index; i--) {
        talloc_free(repl->marks[i]);
    }
    repl->mark_count = target_index;

    // Rebuild scrollback from remaining messages
    rebuild_scrollback_from_messages(repl);
    render_rewind_indicator_to_scrollback(repl->scrollback, target_mark);

    return OK(NULL);
}
```

#### Database Replay

The database contains complete history including rewind operations, allowing reconstruction of exact context states:

```c
// Replay session from database to reconstruct context state
res_t ik_session_replay(
    ik_db_ctx_t *db,
    int64_t session_id,
    ik_message_t ***session_messages_out,
    size_t *count_out
) {
    // Load all messages in order
    ik_message_t **all_messages = NULL;
    size_t total_count = 0;
    TRY(ik_db_messages_load(db, session_id, &all_messages, &total_count));

    // Build context array (dynamically shrinks on rewind)
    ik_message_t **context = talloc_array(db, ik_message_t*, total_count);
    size_t context_count = 0;

    ik_mark_t **marks = NULL;
    size_t mark_count = 0;

    for (size_t i = 0; i < total_count; i++) {
        ik_message_t *msg = all_messages[i];

        if (strcmp(msg->role, "mark") == 0) {
            // Create mark at current position
            ik_mark_t *mark = talloc_zero(db, ik_mark_t);
            mark->message_index = context_count;
            mark->db_message_id = msg->id;
            mark->label = msg->content[0] != '\0' ?
                         talloc_strdup(mark, msg->content) : NULL;

            marks = talloc_realloc(db, marks, ik_mark_t*, mark_count + 1);
            marks[mark_count++] = mark;

            // Mark messages are part of context
            context[context_count++] = msg;

        } else if (strcmp(msg->role, "rewind") == 0) {
            // Find the mark being rewound to
            ik_mark_t *target = NULL;
            ssize_t target_idx = -1;
            for (ssize_t j = mark_count - 1; j >= 0; j--) {
                if (marks[j]->db_message_id == msg->rewind_to_message_id) {
                    target = marks[j];
                    target_idx = j;
                    break;
                }
            }

            if (target) {
                // Truncate context to mark position
                context_count = target->message_index;

                // Remove marks at and after target
                mark_count = target_idx;
            }

            // Rewind message itself is not part of context (it's a meta-operation)

        } else {
            // Regular message (user, assistant, system)
            context[context_count++] = msg;
        }
    }

    *session_messages_out = context;
    *count_out = context_count;

    talloc_free(marks);
    return OK(NULL);
}
```

#### Usage Examples

```bash
# Simple undo
/mark
User: "Try approach A"
LLM: [suggests A]
/rewind      # Goes back to unnamed mark

# Named checkpoints
/mark approach-a
User: "Use state machine"
LLM: [complex solution]
/mark approach-b
User: "Actually use regex"
LLM: [different solution]
/rewind approach-a    # Jump back to first mark

# Exploration with labels
/mark before-refactor
User: "Refactor parseInput to use state machine"
LLM: [complex refactor]
User: "Too complicated"
/rewind before-refactor
User: "Just extract a helper function instead"
LLM: [simpler refactor]
```

#### Benefits

1. **Complete audit trail** - Database shows full exploration including dead ends
2. **Reproducible context** - Can reconstruct exact LLM context at any point in time
3. **Flexible navigation** - Jump to specific points or use simple undo
4. **Non-destructive** - All messages preserved in database for analysis
5. **Explicit control** - User decides what context to keep or discard

### Streaming Response Assembly

During LLM streaming, we need to accumulate chunks into a complete message:

```c
// In REPL context
struct ik_repl_ctx_t {
    // ... existing fields ...
    char *streaming_buffer;      // Accumulates current LLM response
    size_t streaming_buffer_len;
    bool is_streaming;
};

// Chunk callback
void on_llm_chunk(void *user_data, const char *chunk) {
    ik_repl_ctx_t *repl = user_data;

    // Accumulate in buffer
    append_to_streaming_buffer(repl, chunk);

    // Also show immediately in scrollback (decorated)
    append_to_scrollback_with_decoration(repl->scrollback, chunk, "streaming");

    // Render frame
    ik_repl_render_frame(repl);
}

// On stream complete
void on_llm_complete(void *user_data) {
    ik_repl_ctx_t *repl = user_data;

    // Create message from accumulated buffer
    ik_message_t *msg = create_message(repl, "assistant", repl->streaming_buffer);

    // Persist to database
    res_t result = ik_db_message_insert(repl->db, repl->current_conv->id, msg);
    if (is_ok(&result)) {
        msg->id = result.data;
        append_to_conversation(repl->current_conv, msg);

        // Clear streaming indicator from scrollback
        // Re-render final message with proper decorations
        replace_streaming_content_in_scrollback(repl->scrollback, msg);
    }

    // Clean up streaming state
    talloc_free(repl->streaming_buffer);
    repl->streaming_buffer = NULL;
    repl->is_streaming = false;
}
```

---

## Open Design Questions

### 1. Scrollback vs. Session Messages vs. Database

**Three-Layer Architecture:**

```
┌─────────────────────────────────────┐
│ Scrollback (Display Layer)         │  ← What user sees
│ - Decorated, formatted text         │  ← ANSI codes, colors, syntax
│ - Render-time transformations       │  ← highlighting
└─────────────────────────────────────┘
           ↑ renders from
┌─────────────────────────────────────┐
│ Session Messages (Active Context)  │  ← What LLM receives
│ - ik_message_t[] array             │  ← Structured data for API
│ - Messages in current session      │  ← Cleared by /clear
└─────────────────────────────────────┘
           ↓ persists to
┌─────────────────────────────────────┐
│ Database (Permanent Archive)       │  ← All messages ever
│ - Complete history                 │  ← Never auto-loaded
│ - Full-text search                 │  ← Recall & research
└─────────────────────────────────────┘
```

**Decorations (applied at render time, not stored):**
- Formatted timestamps ("2 minutes ago" vs. ISO 8601)
- Visual separators (user/assistant labels, dividers)
- Syntax highlighting in code blocks
- Markdown rendering (bold, italic, headers)
- Status indicators (streaming..., error, retry)
- Token usage summaries
- Model information badges

**Key Relationships:**
- Session messages → rendered to scrollback (one-way)
- Session messages → persisted to database (one-way)
- Database → can be searched and selectively loaded into session (user-initiated)
- Scrollback and session messages cleared together (`/clear`)
- Database never cleared (permanent archive)

**Example Flow:**
```
User submits: "Explain talloc"
  ↓
Session message: {role: "user", content: "Explain talloc"}
  ↓                     ↓
  ↓                   Database INSERT → id: 42
  ↓
Scrollback: "You: Explain talloc" (with decoration)
  ↓
LLM receives: [{role: "user", content: "Explain talloc"}]
  ↓
LLM responds: "Talloc is a hierarchical..."
  ↓
Session message: {role: "assistant", content: "Talloc is..."}
  ↓                     ↓
  ↓                   Database INSERT → id: 43
  ↓
Scrollback: "Assistant: Talloc is..." (with syntax highlighting)
```

### 2. Conversation Lifecycle

**Question:** When do conversations start/end?

**Options:**
- **A) One conversation per session** - New conversation on each launch
- **B) Resume last conversation** - Continue where you left off

**Implications:**
- **A** keeps sessions isolated but loses context
- **B** provides continuity but may have stale context

### 3. Message Assembly During Streaming

**Question:** How do we build the complete assistant message from chunks?

**Current scrollback:** Appends display lines
**Future:** Need to accumulate structured message

**Option A - Buffer in LLM client:**
```c
struct ik_llm_client_t {
    char *response_buffer;  // Accumulates chunks
    size_t buffer_len;
};
```

**Option B - Buffer in REPL:**
```c
struct ik_repl_ctx_t {
    char *streaming_response;  // Current LLM response being built
    bool streaming_active;
};
```

**Option C - Buffer in conversation:**
```c
struct ik_conversation_t {
    ik_message_t *pending_message;  // Not yet complete
};
```

### 4. Error Recovery

**Question:** What happens if the LLM request fails mid-stream?

**Scenarios:**
- Network timeout
- API rate limit
- Malformed JSON response
- Connection drop

**Considerations:**
- Should we save partial responses?
- How do we signal error to user?
- Retry logic? User-initiated or automatic?

### 5. Configuration Management

**Current:** `ik_cfg_t` in `config.h` has basic fields
**Future:** Need API keys, model selection, DB connection string

**Question:** How does configuration flow to modules?

**Option A - Pass through REPL:**
```c
ik_repl_init(ctx, config, &repl);
  └─> ik_llm_init(ctx, config->api_key, &llm);
  └─> ik_db_init(ctx, config->db_conn, &db);
```

**Option B - Modules load directly:**
```c
ik_llm_init(ctx, &llm);  // Reads ~/.ikigai/config.json internally
```

**Option C - Global config singleton:**
```c
extern ik_cfg_t *g_config;  // Loaded in main()
```

---

## Benefits of Scrollback-as-Context Architecture

This three-layer design (scrollback → session messages → database) provides several key advantages:

### 1. Explicit Context Control

**User sees exactly what the LLM sees:**
- No hidden messages injected from database
- No automatic RAG that pollutes context
- Visual WYSIWYG: scrollback = LLM context
- `/clear` provides immediate fresh start

### 2. Token Management

**User controls token usage:**
- Only visible scrollback goes to LLM
- Can clear context to reduce tokens
- Can selectively load relevant history
- No accidental sending of entire conversation history

### 3. Database as Research Tool

**Permanent memory without automatic injection:**
- Full-text search across all past conversations
- User decides what's relevant to current task
- Can cherry-pick specific exchanges from history
- Enables "remember when we discussed X?" workflows
- No risk of outdated context poisoning fresh conversations

### 4. Clean Separation of Concerns

**Three independent responsibilities:**
- **Scrollback:** Presentation layer (colors, formatting, decorations)
- **Session messages:** Data layer (structured for LLM API)
- **Database:** Persistence layer (archive and search)

Each layer optimized for its purpose without coupling.

### 5. Performance

**Minimal memory footprint:**
- Session messages = only current context (tens of messages)
- Scrollback = display buffer (configurable limit)
- Database = disk-based, unlimited history
- No need to load entire conversation history into RAM

### 6. User Workflow Clarity

**Mental model matches behavior:**
- "Start fresh" → `/clear` → empty screen → LLM has no history
- "What does AI know?" → look at scrollback → that's it

No hidden state, no surprises.

---

## Summary of Architectural Decisions

### ✅ Decided

1. **Three-layer architecture:**
   - **Scrollback:** Display layer (what user sees, decorated text)
   - **Session messages:** Active context (what LLM receives, structured data)
   - **Database:** Permanent archive (all messages, tool-based access)

2. **Scrollback IS the context window:**
   - Starts blank on startup or `/clear`
   - Only messages in scrollback are sent to LLM
   - User has explicit control over context
   - What you see is what the LLM sees (WYSIWYG)

3. **Database for recall, not automatic injection:**
   - Messages persisted immediately (synchronous INSERTs)
   - Never auto-loaded into context
   - Tool-based search and selective loading (when database is implemented)
   - Permanent archive independent of active session

4. **Session messages tracked in REPL:**
   - `ik_message_t **session_messages` array in `ik_repl_ctx_t`
   - Source of truth for current LLM context
   - Scrollback renders from session messages
   - Both cleared together by `/clear`

5. **Memory ownership:**
   - REPL owns session messages (talloc parent)
   - Scrollback owns display strings (derived from messages)
   - Database owns permanent records (independent lifecycle)

6. **HTTP Streaming:**
   - Phase 1: Blocking streaming (simple, functional)
   - Phase 2: select()-based polling (interactive during stream)
   - User can scroll/cancel during response
   - Graceful error handling (partial responses saved)

7. **Slash Command Dispatch:**
   - Command registry with function pointers
   - New module: `src/commands.c`
   - Self-documenting for `/help` generation
   - Each command testable independently

8. **Database Schema:**
   - Session-based organization (sessions table)
   - Messages reference session_id
   - New session per app launch
   - `/clear` maintains current session
   - Full-text search with session context

9. **Error Recovery:**
   - Graceful degradation (never crash)
   - Display errors inline in scrollback
   - DB failure doesn't block LLM interaction
   - Save partial state when possible

10. **Configuration:**
    - Explicit dependency injection
    - Config passed to modules during init
    - No hidden I/O in modules

11. **Message Formatting:**
    - Separate module: `src/message_format.c`
    - Applies decorations before scrollback rendering
    - Colors, labels, metadata, future syntax highlighting

### 🔶 Next: HTTP Streaming Integration

#### Decision Required: Event Loop During LLM Response

**Current Event Loop (v0.1.0):**
```c
while (!repl->quit) {
    read_terminal_input();      // Blocking read
    parse_to_actions();
    apply_to_input_buffer();
    render_frame();
}
```

**With LLM Streaming - Three Approaches:**

**Option A: Blocking (Simplest)**
```c
while (!repl->quit) {
    if (waiting_for_llm) {
        // Block until LLM stream completes
        // User can't scroll, type, or interact
        ik_llm_receive_stream_blocking(llm, on_chunk_callback);
        waiting_for_llm = false;
    } else {
        read_terminal_input();      // Normal REPL
        parse_to_actions();
        apply_to_input_buffer();
        render_frame();
    }
}
```
- ✅ Simple implementation
- ✅ No concurrency complexity
- ❌ User frozen during response
- ❌ Can't scroll back during stream
- ❌ Can't cancel/interrupt

**Option B: Polling with select() (Recommended)**
```c
while (!repl->quit) {
    // Set up file descriptors
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(STDIN_FILENO, &read_fds);  // Terminal input

    int max_fd = STDIN_FILENO;
    if (waiting_for_llm) {
        int llm_fd = ik_llm_get_socket_fd(llm);
        FD_SET(llm_fd, &read_fds);
        max_fd = MAX(max_fd, llm_fd);
    }

    // Wait for activity (100ms timeout for responsive UI)
    struct timeval tv = {0, 100000};  // 100ms
    select(max_fd + 1, &read_fds, NULL, NULL, &tv);

    // Handle terminal input (even during streaming)
    if (FD_ISSET(STDIN_FILENO, &read_fds)) {
        read_terminal_input();
        parse_to_actions();
        apply_to_input_buffer();
        // User can scroll, type new input, or press Ctrl+C to cancel
    }

    // Handle LLM chunks
    if (waiting_for_llm && FD_ISSET(llm_fd, &read_fds)) {
        char *chunk = NULL;
        res_t result = ik_llm_read_chunk(llm, &chunk);
        if (is_ok(&result) && chunk != NULL) {
            append_to_streaming_buffer(repl, chunk);
            append_to_scrollback(repl->scrollback, chunk);
        } else if (result indicates stream complete) {
            finalize_assistant_message(repl);
            waiting_for_llm = false;
        }
    }

    render_frame();  // Always render (shows updates from either source)
}
```
- ✅ User can interact during streaming
- ✅ Can scroll through history while response arrives
- ✅ Can cancel with Ctrl+C or ESC
- ✅ Still single-threaded
- ✅ Standard POSIX approach
- 🔶 More complex than blocking
- 🔶 Need to handle partial reads

**Option C: Non-blocking with libcurl multi interface (Future)**
```c
// Uses libcurl's multi interface for fully async HTTP
CURLM *multi_handle = curl_multi_init();
// More complex, probably overkill for v1.0
```
- ✅ Most flexible
- ❌ Significant complexity
- ❌ Harder to debug
- 📅 Can be added later if needed

**Recommendation: Start with Option A, upgrade to Option B**

Phase 1 (OpenAI Integration): Implement Option A (blocking)
- Gets basic streaming working quickly
- User can see responses building in real-time
- Acceptable UX for initial version

Phase 2 (Polish): Upgrade to Option B (select polling)
- Add interactivity during streaming
- Enable cancel/scroll during response
- Better user experience
- Still single-threaded, maintainable

**Implementation Notes for Option B:**

1. **libcurl setup for non-blocking:**
```c
CURL *curl = curl_easy_init();
curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, chunk_callback);
curl_easy_setopt(curl, CURLOPT_WRITEDATA, repl);

// Get the socket for select()
long sockfd;
curl_easy_getinfo(curl, CURLINFO_ACTIVESOCKET, &sockfd);
```

2. **Chunk callback accumulates and displays:**
```c
size_t chunk_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    ik_repl_ctx_t *repl = userdata;
    size_t total_size = size * nmemb;

    // Append to streaming buffer
    append_to_streaming_buffer(repl, ptr, total_size);

    // Parse SSE format (Server-Sent Events)
    // Extract JSON chunks from "data: {...}" lines
    // Render to scrollback

    return total_size;  // Must return bytes processed
}
```

3. **Cancellation handling:**
```c
// In action handler
if (action == ACTION_CANCEL && repl->is_streaming) {
    ik_llm_cancel_stream(repl->llm);
    append_to_scrollback(repl->scrollback, "[Cancelled by user]");
    cleanup_streaming_state(repl);
}
```

### 🔶 Next: Slash Command Dispatch

#### Decision Required: Command Handling Architecture

**Current Implementation (`/pp` only):**
```c
// In repl_actions.c:handle_submit()
if (text[0] == '/') {
    if (strcmp(text, "/pp") == 0) {
        // Handle /pp inline
        return handle_pp_command(repl);
    }
    // ... more if/else as commands grow
}
```

**Future Commands Needed:**
- `/clear` - Clear scrollback and session context
- `/model <name>` - Switch LLM model
- `/help` - Show available commands
- `/quit` - Exit application
- (Future: `/tool`, `/edit`, `/run` for tool execution)

**Three Approaches:**

**Option A: Command Registry (Recommended)**
```c
// Command function signature
typedef res_t (*ik_cmd_handler_t)(ik_repl_ctx_t *repl, const char *args);

// Command registration
typedef struct {
    const char *name;           // "clear", "search", etc.
    const char *description;    // For /help
    ik_cmd_handler_t handler;   // Function pointer
} ik_command_t;

// Registry
static ik_command_t commands[] = {
    {"clear",   "Clear scrollback and start fresh context",    cmd_clear},
    {"help",    "Show available commands",                     cmd_help},
    {"quit",    "Exit ikigai",                                 cmd_quit},
    {NULL, NULL, NULL}  // Sentinel
};

// Dispatch
res_t dispatch_command(ik_repl_ctx_t *repl, const char *input) {
    // Parse: "/command args"
    char *cmd_name = parse_command_name(input);
    char *args = parse_command_args(input);

    // Look up in registry
    for (size_t i = 0; commands[i].name != NULL; i++) {
        if (strcmp(cmd_name, commands[i].name) == 0) {
            return commands[i].handler(repl, args);
        }
    }

    return ERR("Unknown command: %s", cmd_name);
}

// Individual handlers
res_t cmd_clear(ik_repl_ctx_t *repl, const char *args) {
    ik_scrollback_clear(repl->scrollback);
    clear_session_messages(repl);
    return OK(NULL);
}
```
- ✅ Easy to add new commands
- ✅ Self-documenting (registry has descriptions)
- ✅ `/help` auto-generated from registry
- ✅ Clean separation of concerns
- ✅ Testable (each handler independent)
- 🔶 Slightly more code than inline

**Option B: Inline switch/if-else**
```c
res_t handle_command(ik_repl_ctx_t *repl, const char *input) {
    if (strcmp(input, "/clear") == 0) {
        // Handle clear
    } else if (strcmp(input, "/help") == 0) {
        // Handle help
    } else {
        return ERR("Unknown command");
    }
}
```
- ✅ Very simple
- ❌ Gets unwieldy with many commands
- ❌ Hard to test individual commands
- ❌ `/help` must be manually maintained
- ❌ All logic in one function

**Option C: Plugin Architecture**
```c
// Dynamic loading of command modules
void *handle = dlopen("libcmd_search.so", RTLD_LAZY);
// Too complex for v1.0
```
- ✅ Ultimate flexibility
- ❌ Massive complexity
- ❌ Not needed for built-in commands
- 📅 Overkill for this use case

**Recommendation: Option A (Command Registry)**

Benefits:
- Scalable as we add more commands
- Each command is a focused function (TDD friendly)
- `/help` implementation is trivial (loop over registry)
- Easy to add metadata (aliases, argument specs, permissions)

**Example Command Implementations:**

```c
res_t cmd_help(ik_repl_ctx_t *repl, const char *args) {
    char *help_text = talloc_strdup(repl, "Available commands:\n\n");

    for (size_t i = 0; commands[i].name != NULL; i++) {
        help_text = talloc_asprintf_append(help_text,
            "  /%s - %s\n",
            commands[i].name,
            commands[i].description);
    }

    append_to_scrollback(repl->scrollback, help_text);
    talloc_free(help_text);
    return OK(NULL);
}

res_t cmd_load(ik_repl_ctx_t *repl, const char *args) {
    // Search database
    ik_message_t **results = NULL;
    size_t count = 0;
    TRY(ik_db_search(repl->db, args, &results, &count));

    if (count == 0) {
        append_to_scrollback(repl->scrollback, "No messages found.");
        return OK(NULL);
    }

    // Add to session context
    for (size_t i = 0; i < count; i++) {
        append_to_session_messages(repl, results[i]);
        render_message_to_scrollback(repl->scrollback, results[i]);
    }

    char *msg = talloc_asprintf(repl, "Loaded %zu messages into context.", count);
    append_to_scrollback(repl->scrollback, msg);
    talloc_free(msg);

    return OK(NULL);
}
```

**Module Location:**
- New file: `src/commands.c` and `src/commands.h`
- Registry and dispatch in commands.c
- Individual handlers as static functions
- Called from `repl_actions.c:handle_submit()`

### 🔶 Next: Database Schema and Message Organization

#### Decision Required: How to organize messages in the database

Given that scrollback defines context (not database), how should messages be organized in DB?

**Option A: Session-based (Recommended)**
```sql
CREATE TABLE sessions (
    id BIGSERIAL PRIMARY KEY,
    started_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    ended_at TIMESTAMPTZ,          -- NULL if still active
    title TEXT                      -- Optional user-defined title
);

CREATE TABLE messages (
    id BIGSERIAL PRIMARY KEY,
    session_id BIGINT REFERENCES sessions(id),  -- Can be NULL
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

**Characteristics:**
- Messages optionally belong to a session
- Session tracks when it started/ended
- `/clear` doesn't create new session (sessions are DB concept only)
- Multiple `/clear` commands in one app launch = same session
- Different app launches = different sessions (unless we persist session ID)

**Behavior:**
```
Launch ikigai
  → Create new session in DB (session_id = 42)
  → Scrollback empty

User sends message "Hello"
  → INSERT message with session_id = 42

User types /clear
  → Scrollback cleared
  → Session continues (still session_id = 42)

User sends message "New topic"
  → INSERT message with session_id = 42
  → (Same session, different context window)

Exit ikigai
  → UPDATE sessions SET ended_at = NOW() WHERE id = 42
```

**Option B: No sessions, just messages**
```sql
CREATE TABLE messages (
    id BIGSERIAL PRIMARY KEY,
    role TEXT NOT NULL,
    content TEXT NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    tokens INTEGER,
    model TEXT
);
```

- ✅ Simpler schema
- ❌ No way to group related messages
- ❌ Hard to answer "what conversations did I have?"
- ❌ Search results lack context

**Option C: Named conversations (like ChatGPT)**
```sql
CREATE TABLE conversations (
    id BIGSERIAL PRIMARY KEY,
    title TEXT NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE TABLE messages (
    id BIGSERIAL PRIMARY KEY,
    conversation_id BIGINT NOT NULL REFERENCES conversations(id),
    role TEXT NOT NULL,
    content TEXT NOT NULL,
    sequence_num INTEGER NOT NULL,  -- Order within conversation
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);
```

- ✅ Familiar model from ChatGPT
- ❌ Doesn't match our "scrollback is context" architecture
- ❌ Implies conversations are loaded/resumed (we don't do that)
- ❌ Confusing: "conversation" in DB vs. "session context" in scrollback

**Recommendation: Option A (Sessions)**

**Rationale:**
- Sessions track chronological work periods
- Messages can be grouped for search context
- Search results can include session context
- Doesn't conflict with scrollback-as-context model

**Session Management:**
```c
struct ik_repl_ctx_t {
    // ... existing fields ...
    int64_t current_session_id;  // DB session for this run
};

// On REPL init
res_t ik_repl_init(void *parent, ik_db_ctx_t *db, ik_repl_ctx_t **repl_out) {
    // Create new session
    int64_t session_id;
    TRY(ik_db_session_create(db, &session_id));

    repl->current_session_id = session_id;
    // ... rest of init
}

// On message insert
res_t create_and_persist_message(ik_repl_ctx_t *repl, const char *role, const char *content) {
    ik_message_t *msg = create_message(repl, role, content);

    // Insert with current session ID
    TRY(ik_db_message_insert(repl->db, repl->current_session_id, msg));

    msg->id = /* returned from INSERT */;
    append_to_session_messages(repl, msg);
    render_message_to_scrollback(repl->scrollback, msg);

    return OK(NULL);
}

// On exit
void ik_repl_cleanup(ik_repl_ctx_t *repl) {
    // Mark session as ended
    ik_db_session_end(repl->db, repl->current_session_id);

    // ... rest of cleanup
}
```

### 🔶 Remaining: Error Recovery

**Error Scenarios and Handling:**

**1. Database Connection Lost**
```c
res_t create_and_persist_message(ik_repl_ctx_t *repl, const char *role, const char *content) {
    ik_message_t *msg = create_message(repl, role, content);

    res_t result = ik_db_message_insert(repl->db, repl->current_session_id, msg);
    if (is_err(&result)) {
        // Display error to user
        append_to_scrollback(repl->scrollback,
            "⚠ Database error: Message not saved to permanent storage");
        append_to_scrollback(repl->scrollback,
            error_to_string(result.err));

        // Still add to session context (work continues)
        append_to_session_messages(repl, msg);
        render_message_to_scrollback(repl->scrollback, msg);

        // Return OK - don't block user workflow
        return OK(NULL);
    }

    msg->id = /* returned from INSERT */;
    append_to_session_messages(repl, msg);
    render_message_to_scrollback(repl->scrollback, msg);
    return OK(NULL);
}
```
- Session continues even if DB fails
- User notified but not blocked
- Messages kept in memory (lost on exit, but LLM interaction works)

**2. LLM Request Fails Mid-Stream**
```c
// During streaming
if (is_err(&chunk_result)) {
    // Stream failed
    append_to_scrollback(repl->scrollback,
        "\n⚠ Error: LLM stream interrupted");
    append_to_scrollback(repl->scrollback, error_to_string(chunk_result.err));

    // Save partial response if we got any
    if (repl->streaming_buffer_len > 0) {
        ik_message_t *msg = create_message(repl, "assistant", repl->streaming_buffer);
        msg->content = talloc_asprintf(msg, "[INCOMPLETE] %s", msg->content);

        // Try to persist (may also fail)
        ik_db_message_insert(repl->db, repl->current_session_id, msg);

        append_to_session_messages(repl, msg);
    }

    cleanup_streaming_state(repl);
    return OK(NULL);  // Don't crash, let user try again
}
```

**3. Malformed JSON in API Response**
```c
// In JSON parsing
res_t parse_llm_chunk(const char *json_text, char **content_out) {
    yyjson_doc *doc = yyjson_read(json_text, strlen(json_text), 0);
    if (!doc) {
        return ERR("Malformed JSON in LLM response");
    }

    // ... parse
}

// Caller handles error
if (is_err(&parse_result)) {
    append_to_scrollback(repl->scrollback,
        "⚠ Failed to parse LLM response (malformed JSON)");
    // Log raw response for debugging
    log_error("Raw response: %s", raw_chunk);
}
```

**Strategy: Graceful Degradation**
- Never crash the REPL for recoverable errors
- Display errors inline in scrollback (user sees what happened)
- Continue accepting user input
- Try to save partial state when possible
- Log details for debugging

### 🔶 Remaining: Configuration Management

**Recommendation: Pass config to modules during init**

```c
// In main()
int main(void) {
    void *root_ctx = talloc_new(NULL);

    // Load config
    ik_cfg_t *cfg = NULL;
    TRY(ik_cfg_load(root_ctx, "~/.ikigai/config.json", &cfg));

    // Initialize database with config
    ik_db_ctx_t *db = NULL;
    TRY(ik_db_init(root_ctx, cfg->db_connection_string, &db));

    // Initialize LLM client with config
    ik_llm_client_t *llm = NULL;
    TRY(ik_llm_init(root_ctx, cfg->openai_api_key, &llm));

    // Initialize REPL (gets access to db and llm)
    ik_repl_ctx_t *repl = NULL;
    TRY(ik_repl_init(root_ctx, cfg, db, llm, &repl));

    // Run
    ik_repl_run(repl);

    talloc_free(root_ctx);
    return 0;
}
```

**Config structure:**
```c
typedef struct {
    // Terminal
    size_t scrollback_lines;

    // Database
    char *db_connection_string;  // "postgresql://localhost/ikigai"

    // LLM
    char *openai_api_key;
    char *anthropic_api_key;
    char *default_model;

    // Future
    // char *google_api_key;
    // char *xai_api_key;
} ik_cfg_t;
```

- ✅ Explicit dependencies
- ✅ Modules don't have hidden I/O (reading config files)
- ✅ Easy to test (pass mock config)
- ✅ Clear ownership (root_ctx owns config, passes to children)

### 🔶 Remaining: Message Formatting/Decoration

**Recommendation: Formatting module called by REPL**

```c
// New module: src/message_format.c

// Format a message for display with decorations
res_t ik_format_message(
    void *ctx,
    const ik_message_t *msg,
    char **formatted_out
);

// Usage in REPL
res_t render_message_to_scrollback(
    ik_repl_ctx_t *repl,
    const ik_message_t *msg
) {
    char *formatted = NULL;
    TRY(ik_format_message(repl, msg, &formatted));

    ik_scrollback_append(repl->scrollback, formatted);
    talloc_free(formatted);

    return OK(NULL);
}
```

**Formatting logic:**
```c
res_t ik_format_message(void *ctx, const ik_message_t *msg, char **out) {
    char *result = talloc_strdup(ctx, "");

    // Role label with color
    if (strcmp(msg->role, "user") == 0) {
        result = talloc_asprintf_append(result, "\033[1;36mYou:\033[0m ");
    } else if (strcmp(msg->role, "assistant") == 0) {
        result = talloc_asprintf_append(result, "\033[1;32mAssistant:\033[0m ");
    }

    // Content (could add markdown rendering, syntax highlighting here)
    result = talloc_asprintf_append(result, "%s", msg->content);

    // Metadata footer (if desired)
    if (msg->tokens > 0) {
        result = talloc_asprintf_append(result,
            "\n\033[2m(%d tokens, %s)\033[0m",
            msg->tokens,
            msg->model);
    }

    *out = result;
    return OK(NULL);
}
```

**Separation of concerns:**
- `ik_message_t` - Pure data structure
- `ik_format_message()` - Presentation logic
- `ik_scrollback_append()` - Display management
- REPL orchestrates the flow

---

## Implementation Roadmap Summary

Based on dependency analysis, here's the recommended implementation sequence:

**Phase 1: LLM HTTP Client** (No dependencies)
1. Add libcurl dependency to Makefile
2. Implement `ik_llm_client_t` module (src/llm_client.c)
3. OpenAI API integration with blocking streaming
4. Parse SSE format (Server-Sent Events) from OpenAI
5. Extract JSON chunks with yyjson
6. Accumulate streaming response in buffer
7. Display chunks in scrollback as they arrive
8. Store session messages in `ik_repl_ctx_t` (memory only)
9. Build API requests from session messages array

**Deliverable:** Working AI chat with streaming responses. Messages lost on exit (no persistence).

**Phase 2: Basic Command Infrastructure** (No hard dependencies)
1. Create `src/commands.c` with command registry
2. Implement `/help` (auto-generated from registry)
3. Implement `/quit` (graceful exit)
4. Implement `/clear` (clear scrollback, session messages, and marks)
5. Implement `/mark [LABEL]` (create checkpoint for rollback)
6. Implement `/rewind [LABEL]` (rollback to checkpoint)
7. Move `/pp` into registry
8. Create `src/message_format.c` for message decorations

**Deliverable:** Structured command system with context management (/clear, /mark, /rewind) and /help.

**Phase 3: Database Integration** (Depends on LLM for usefulness)
1. Add libpq dependency to Makefile
2. Implement `ik_db_ctx_t` module (src/db.c)
3. Create PostgreSQL schema (sessions and messages tables)
4. Add session tracking to REPL (create session on init, end on cleanup)
5. Persist messages on send/receive (INSERT after LLM response)
6. Add `current_session_id` to `ik_repl_ctx_t`
7. Update config to include `db_connection_string`
8. Implement core database query functions (for tool use)
9. Add PostgreSQL full-text search index (GIN on messages.content)

**Deliverable:** Permanent message storage, sessions tracked, full conversation history preserved.

**Note on Database Access:** Database access will primarily be performed by the LLM through tool use (Phase 5). The `/clear` command remains essential for user control of context.

**Phase 4: Interactive Streaming** (Polish on LLM)
1. Upgrade event loop to select()-based polling
2. Enable terminal input during LLM streaming
3. Add cancellation support (ESC or Ctrl+C during stream)
4. Enable scrollback navigation during streaming (Page Up/Down)
5. Improve error recovery and partial response handling

**Deliverable:** Non-blocking UI, can scroll/cancel during AI responses.

**Phase 5: Tool Execution** (Depends on LLM + Database)
1. Tool interface design (follows provider pattern)
2. Implement database tools (search_messages, load_context)
3. Implement file operations (read, write, search)
4. Implement shell command execution
5. Parse tool calls from LLM responses (OpenAI format initially)
6. Execute tools and inject results into conversation
7. Tree-sitter integration for code analysis

**Deliverable:** AI can search conversation history, execute commands, read/write files, analyze code.

**Phase 6: Multi-LLM Support** (Depends on LLM HTTP client)
1. Abstract provider interface (ik_provider_t)
2. Refactor OpenAI client to use interface
3. Add tool format translation per provider
4. Implement Anthropic provider
5. Implement Google provider (Gemini)
6. Implement X.AI provider (Grok)
7. Add `/model <name>` command
8. Update config for multiple API keys

**Deliverable:** Support for multiple AI providers with tool calling, runtime switching.

This order ensures each phase delivers working functionality and all dependencies are satisfied.

---

## Key Architectural Insights

This analysis revealed several important design principles:

**1. Scrollback IS the Context Window**
- Unlike traditional chat apps, ikigai doesn't auto-load history
- The user sees exactly what the LLM sees (WYSIWYG)
- `/clear` provides instant fresh context without losing data
- Explicit user control over token usage

**2. Database as Research Archive**
- Permanent storage of all interactions
- Never automatically injected into context
- User searches and selectively loads relevant messages
- Enables long-term recall without context pollution

**3. Three-Layer Separation**
- Display (scrollback) - Decorated, formatted, ephemeral
- Active context (session messages) - Structured, sent to LLM
- Archive (database) - Permanent, searchable, never auto-loaded

**4. Progressive Enhancement**
- Start simple (blocking HTTP, basic commands)
- Add sophistication incrementally (select() loop, rich formatting)
- Each phase delivers working functionality
- TDD maintains quality throughout

**5. Graceful Degradation**
- Never crash the REPL for recoverable errors
- DB failure doesn't prevent LLM interaction
- User stays in control and informed
- Partial state better than no state

This architecture balances simplicity (single-threaded, direct control) with power (permanent memory, context control) while maintaining the transparency and predictability that developers need from their tools.
