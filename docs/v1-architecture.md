# v1.0 Architecture Overview

This document describes the core architecture for ikigai v1.0: a desktop terminal client with LLM integration, local tool execution, and PostgreSQL-backed conversation memory.

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

## Core Architectural Concept: Scrollback as Context Window

### Critical Insight

**Scrollback IS the Context Window** - The messages visible in the scrollback buffer are exactly what gets sent to the LLM. This provides explicit user control over context.

**Key Principles:**
- **On startup or `/clear`**: Scrollback is BLANK - fresh conversation context
- **Database stores everything**: All messages persist permanently
- **Only scrollback goes to LLM**: Messages in scrollback = conversation history sent to API
- **User controls context**: What you see is what the LLM sees (WYSIWYG)

**Implications:**
- Scrollback boundary represents the current conversation context
- `/clear` creates fresh context WITHOUT deleting from database
- Can selectively load messages from DB into scrollback if needed
- Token usage controlled by scrollback size (not entire DB history)
- User has explicit control over what context the LLM receives
- Database enables search/recall but doesn't auto-inject into context

---

## Three-Layer Architecture

The architecture separates concerns into three independent layers:

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

### Layer 1: Scrollback (Display)

**Decorations (applied at render time, not stored):**
- Formatted timestamps ("2 minutes ago" vs. ISO 8601)
- Visual separators (user/assistant labels, dividers)
- Syntax highlighting in code blocks
- Markdown rendering (bold, italic, headers)
- Status indicators (streaming..., error, retry)
- Token usage summaries
- Model information badges

### Layer 2: Session Messages (Active Context)

**Structured message array in REPL context:**
```c
struct ik_repl_ctx_t {
    ik_scrollback_t *scrollback;       // Display lines (formatted, decorated)
    ik_message_t **session_messages;   // Structured messages (for LLM API)
    size_t session_message_count;      // Count of messages in current session
    // ... other fields ...
};
```

**Message Structure:**
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
```

### Layer 3: Database (Permanent Archive)

**Purpose:**
- Permanent storage of all interactions
- Never automatically injected into context
- User searches and selectively loads relevant messages
- Enables long-term recall without context pollution

**Access Pattern:**
- Messages persisted immediately (synchronous INSERTs)
- Tool-based search and retrieval (LLM uses database tools)
- Session-based organization for context grouping

---

## Module Ownership

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
├─> char *streaming_buffer                 // Accumulates current LLM response
└─> int64_t current_session_id             // DB session for this run
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

---

## Initialization Sequence

With scrollback-as-context-window, the startup sequence is:

```
1. main()
   ├─> Load config (~/.ikigai/config.json)
   ├─> Connect to database (ik_db_init)
   ├─> Initialize LLM client (ik_llm_init)
   └─> Initialize REPL (ik_repl_init)

2. ik_repl_init()
   ├─> Initialize terminal (ik_term_init)
   ├─> Initialize scrollback buffer (ik_scrollback_init) → BLANK/EMPTY
   ├─> Initialize input buffer (ik_input_buffer_init)
   ├─> Create new session in database → store session_id
   └─> Set up LLM and DB contexts

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

---

## Data Flow: User Message → LLM Response

```
1. User types message and hits Enter
   ↓
2. ik_repl_handle_submit()
   ├─> Extract text from input_buffer
   ├─> Create ik_message_t (role="user", content=text)
   ├─> INSERT to database immediately → get message ID
   ├─> Add to session_messages[] (with DB id)
   └─> Render to scrollback (with decorations)
   ↓
3. Check for slash command
   ├─> If starts with '/', dispatch to command handler
   └─> Otherwise, continue to LLM
   ↓
4. Send to LLM
   ├─> Build API request from session_messages[]
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
   ├─> Add to session_messages[] (with DB id)
   └─> Replace streaming indicator with final decorated message in scrollback
```

**Key Points:**
- Database writes happen synchronously before updating in-memory state
- Message IDs from database are stored in `ik_message_t` objects
- Scrollback rendering happens after database persistence
- If database write fails, message is not added to conversation or displayed
- This ensures database and display stay in sync

---

## Benefits of This Architecture

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

## Configuration Management

**Explicit Dependency Injection:**

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

**Benefits:**
- Explicit dependencies
- Modules don't have hidden I/O (reading config files)
- Easy to test (pass mock config)
- Clear ownership (root_ctx owns config, passes to children)

---

## Error Handling Strategy: Graceful Degradation

**Core Principle:** Never crash the REPL for recoverable errors.

**Strategy:**
- Display errors inline in scrollback (user sees what happened)
- Continue accepting user input
- Try to save partial state when possible
- Log details for debugging
- DB failure doesn't prevent LLM interaction
- User stays in control and informed

**Examples:**

1. **Database Connection Lost:** Display error to user, continue session in memory-only mode
2. **LLM Request Fails:** Show error, save partial response if any, allow retry
3. **Malformed JSON:** Log error, display warning to user, continue operation

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

---

## Related Documentation

- **[v1-llm-integration.md](v1-llm-integration.md)** - HTTP client, streaming, event loop design
- **[v1-database-design.md](v1-database-design.md)** - Database schema, persistence, sessions
- **[v1-conversation-management.md](v1-conversation-management.md)** - Message lifecycle, slash commands, /mark and /rewind
- **[v1-implementation-roadmap.md](v1-implementation-roadmap.md)** - Phase-by-phase implementation plan
