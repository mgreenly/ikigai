# v1.0 Conversation Management

Message lifecycle, slash command architecture, and conversation control commands (/clear, /mark, /rewind).

## Message Architecture

### Dual Tracking: Session Messages + Scrollback

```c
struct ik_repl_ctx_t {
    ik_scrollback_t *scrollback;          // Display lines (formatted, decorated)
    ik_message_t **session_messages;      // Structured messages (for LLM API)
    size_t session_message_count;
    ik_mark_t **marks;                    // Checkpoint stack (LIFO)
    size_t mark_count;
    char *streaming_buffer;               // Accumulates current LLM response
    bool is_streaming;
};
```

**Key Points:**
- `session_messages[]` is source of truth for LLM context
- Scrollback is rendered view with decorations
- Both grow together during session
- `/clear` empties both (DB retains all)
- Marks enable checkpoint/rollback within session

### Message Lifecycle

1. User submits → create message
2. Persist to database immediately (get DB-assigned ID)
3. Add to `session_messages[]` (LLM context)
4. Render to scrollback (display layer)
5. On error: display warning, continue in-memory only

**Memory Ownership:** talloc hierarchy under `repl_ctx`

---

## Slash Command Architecture

### Command Registry Pattern (Recommended)

```c
typedef res_t (*ik_cmd_handler_t)(ik_repl_ctx_t *repl, const char *args);

typedef struct {
    const char *name;
    const char *description;
    ik_cmd_handler_t handler;
} ik_command_t;

static ik_command_t commands[] = {
    {"clear",   "Clear scrollback and start fresh context",    cmd_clear},
    {"mark",    "Create checkpoint [LABEL]",                   cmd_mark},
    {"rewind",  "Rollback to checkpoint [LABEL]",              cmd_rewind},
    {"help",    "Show available commands",                     cmd_help},
    {"quit",    "Exit ikigai",                                 cmd_quit},
    {NULL, NULL, NULL}
};

res_t dispatch_command(ik_repl_ctx_t *repl, const char *input) {
    // Parse command name and args
    // Look up in registry
    // Call handler
}
```

**Benefits:** Easy to add commands, self-documenting, `/help` auto-generated from registry

**Location:** `src/commands.c` and `src/commands.h`

---

## Core Commands

### /clear

**Effect:**
- Clears scrollback display
- Clears `session_messages[]` (LLM context)
- Clears marks
- Database unchanged (same session, all messages preserved)

**Implementation:**
```c
res_t cmd_clear(ik_repl_ctx_t *repl, const char *args) {
    ik_scrollback_clear(repl->scrollback);

    // Free session messages and marks
    for (size_t i = 0; i < repl->session_message_count; i++)
        talloc_free(repl->session_messages[i]);
    talloc_free(repl->session_messages);

    for (size_t i = 0; i < repl->mark_count; i++)
        talloc_free(repl->marks[i]);
    talloc_free(repl->marks);

    repl->session_messages = NULL;
    repl->session_message_count = 0;
    repl->marks = NULL;
    repl->mark_count = 0;

    return OK(NULL);
}
```

### /mark and /rewind

**Mark Structure:**
```c
typedef struct {
    size_t message_index;          // Position in session_messages[]
    int64_t db_message_id;         // ID of mark message in DB
    char *label;                   // Optional user label (or NULL)
    char *timestamp;
} ik_mark_t;
```

**/mark [label]** - Creates checkpoint at current position
- Persisted as 'mark' message type
- Visible in scrollback as separator
- Multiple marks allowed (LIFO stack)

**/rewind [label]** - Rolls back to checkpoint
- Without label: rewind to most recent mark
- With label: rewind to matching label
- Truncates `session_messages[]` to mark position
- Removes marks at and after target
- Rebuilds scrollback from remaining messages
- Persists 'rewind' operation to DB (audit trail)

**Implementation Pattern:**
```c
// /mark: Create mark message, persist to DB, add to marks array
// /rewind: Find target mark, truncate messages/marks, rebuild scrollback
```

**Usage Examples:**
```bash
/mark                      # Simple undo point
/rewind                    # Go back

/mark approach-a           # Named checkpoint
/mark approach-b           # Another checkpoint
/rewind approach-a         # Jump back to first

/mark before-refactor      # Exploration
# ... complex changes ...
/rewind before-refactor    # Restore
```

**Benefits:**
- Complete audit trail in database
- Reproducible context states
- Flexible navigation (labels or simple undo)
- Non-destructive (all messages preserved)

### /help

Auto-generates from command registry:
```c
res_t cmd_help(ik_repl_ctx_t *repl, const char *args) {
    // Iterate commands[], format output, append to scrollback
}
```

---

## Message Formatting

**Recommendation:** Separate module `src/message_format.c`

```c
res_t ik_format_message(void *ctx, const ik_message_t *msg, char **formatted_out);
```

**Formatting Logic:**
- Role labels with ANSI colors (user: cyan, assistant: green, mark: yellow)
- Content rendering (future: markdown, syntax highlighting)
- Optional metadata footer (tokens, model)

**Separation of Concerns:**
- `ik_message_t` - Pure data
- `ik_format_message()` - Presentation logic
- `ik_scrollback_append()` - Display management
- REPL orchestrates flow

---

## Future Commands

- **/load** - Search DB and load context
- **/model** - Switch model (gpt-4, gpt-3.5-turbo, etc.)
- **/sessions** - List recent sessions

(Details in implementation roadmap)

---

## Testing Strategy

**Unit Tests:**
- Each command handler independently
- Mock database for persistence tests
- Verify state changes (message counts, marks, scrollback)
- OOM injection on all allocations

**Integration Tests:**
- Full workflows: create conversation, add marks, rewind
- Verify context state after operations
- Database persistence and replay

**Test Pattern:**
```c
// Create test REPL → execute command → verify state changes
```

---

## Related Documentation

- [v1-architecture.md](v1-architecture.md) - Overall v1.0 architecture
- [v1-llm-integration.md](v1-llm-integration.md) - HTTP client and streaming
- [v1-database-design.md](v1-database-design.md) - Database schema
- [v1-implementation-roadmap.md](v1-implementation-roadmap.md) - Implementation phases
