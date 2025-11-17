# Memory Management

## Philosophy

ikigai uses explicit memory management with clear ownership rules. We prefer arena allocators where they simplify lifecycle management.

## Ownership Rules

### General Principles

1. **Caller owns returned pointers** - Functions that allocate and return pointers transfer ownership to the caller
2. **Each allocation has one owner** - The owner is responsible for freeing
3. **Use arenas for request lifecycles** - Group related allocations together
4. **Document ownership in function comments** - Make it explicit who owns what

### Function Naming Conventions

- `*_create()` - Allocates and returns owned pointer, caller must call corresponding `*_free()`
- `*_load()` - Allocates and returns owned pointer (typically reading from file/network)
- `*_parse()` - Allocates and returns owned pointer (parsing input data)
- `*_serialize()` - Returns owned heap-allocated string, caller must `free()`
- `*_free()` - Deallocates object and all owned resources
- `*_init()` - Initializes pre-allocated memory (caller owns the allocation)

## Hierarchical Memory with talloc

### Why talloc?

We use **talloc** (from Samba project) for hierarchical memory management:
- **Tree structure** - Child allocations automatically freed when parent freed
- **Named contexts** - Memory leaks are easier to debug
- **Reference counting** - Share objects safely across boundaries
- **Battle-tested** - Used in Samba, LDB, and many other projects

### When to Use talloc

talloc contexts are ideal for:
- **WebSocket request/response cycles** - All message data hangs off request context
- **Configuration loading** - All config strings are children of config object
- **OpenAI streaming** - Buffer chunks and parsed JSON under exchange context
- **Per-connection state** - Session data hangs off connection context

### talloc Lifecycle

```c
// Create context for this request
TALLOC_CTX *req_ctx = talloc_new(NULL);

// All allocations are children of req_ctx
ik_protocol_msg_t *msg = talloc(req_ctx, ik_protocol_msg_t);
char *buffer = talloc_array(req_ctx, char, buffer_size);

// Free everything at once when done
talloc_free(req_ctx);  // Frees msg, buffer, and all children
```

### talloc API Subset for Phase 1

```c
// Context creation
TALLOC_CTX *talloc_new(const void *parent);

// Allocation
void *talloc(const void *ctx, type);
void *talloc_zero(const void *ctx, type);
void *talloc_array(const void *ctx, type, count);
char *talloc_strdup(const void *ctx, const char *str);
char *talloc_asprintf(const void *ctx, const char *fmt, ...);

// Deallocation
int talloc_free(void *ptr);

// Hierarchy manipulation
void *talloc_steal(const void *new_parent, const void *ptr);
void *talloc_reference(const void *ctx, const void *ptr);
```

## Module-Specific Ownership

### Configuration (`config.c/h`)

```c
// Caller owns returned config
// All strings are children of returned config
res_t ik_cfg_load(TALLOC_CTX *ctx, const char *path);
```

**talloc usage**: Config is allocated on provided context. All strings (`openai_api_key`, `listen_address`) are children. Single `talloc_free(ctx)` frees everything.

**OOM handling**: Memory allocation failures cause PANIC (abort), not recoverable errors.

### Protocol Module (REMOVED in Phase 2.5)

**Note:** The protocol module was removed in Phase 2.5 (2025-11-13) as part of simplifying to a desktop-only architecture. The patterns below are preserved as examples of talloc usage for future modules.

**Historical pattern:**
```c
// Caller owns returned message
// Pass parent context to make message a child
res_t ik_protocol_msg_parse(TALLOC_CTX *ctx, const char *json_str);

// Returns owned string (child of ctx)
res_t ik_protocol_msg_serialize(TALLOC_CTX *ctx, ik_protocol_msg_t *msg);
```

**talloc usage**: Handler creates per-request context, passes to parse function. All message data are children, freed when request context freed.

### Handler Module (REMOVED in Phase 2.5)

**Note:** The handler module (WebSocket connections) was removed in Phase 2.5 (2025-11-13). The patterns below are preserved as examples of talloc usage for event-driven code.

**Historical pattern:**
```c
// Per-connection state
typedef struct {
    TALLOC_CTX *ctx;          // Parent for entire connection
    char *sess_id;          // Child of ctx
    char *corr_id;      // Child of ctx
    ik_cfg_t *cfg_ref;   // Borrowed
    bool handshake_complete;
} ik_handler_ws_conn_t;

// Per-message callback
void ik_handler_websocket_message_callback(manager, message, user_data) {
    ik_handler_ws_conn_t *conn = user_data;
    TALLOC_CTX *msg_ctx = talloc_new(conn->ctx);

    res_t res = ik_protocol_msg_parse(msg_ctx, json);
    if (is_err(&res)) { /* handle error */ }
    ik_protocol_msg_t *msg = res.ok;
    // ... process ...

    talloc_free(msg_ctx);  // Free all message allocations
}
```

**talloc usage**:
- `ctx` - Lives for entire connection, freed on disconnect
- `msg_ctx` - Created per message as child of `ctx`, freed after response sent
- `sess_id` - Child of `ctx`, automatically freed with connection
- `corr_id` - Child of `ctx`, updated per request

### LLM Integration (Future - Phase 3+)

**Pattern for future OpenAI/Anthropic integration:**
```c
// Streaming callback receives chunks
typedef void (*ik_llm_stream_cb_t)(const char *chunk, void *user_data);

// Caller provides context for buffering chunks
res_t ik_llm_stream_request(TALLOC_CTX *ctx,
                             const ik_cfg_t *config,
                             const char *provider,  // "openai", "anthropic", etc.
                             json_t *request_payload,
                             ik_llm_stream_cb_t callback,
                             void *user_data);
```

**talloc usage**: SSE line buffering and JSON parsing allocate under provided context. Caller frees context when stream completes.

## Error Handling Integration

Our `res_t` system works with talloc:

```c
res_t load_config_example(void) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    if (!ctx) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    res_t res = ik_cfg_load(ctx, "~/.ikigai/config.json");
    if (is_err(&res)) {
        error_fprintf(stderr, res.err);
        talloc_free(ctx);
        return res;
    }

    ik_cfg_t *config = res.ok;
    // ... use config ...

    talloc_free(ctx);  // Frees config and all children
    return OK(NULL);
}
```

**Note:** Out of memory conditions call `PANIC("Out of memory")` which immediately terminates the process. Memory allocation failures are not recoverable errors.

## When NOT to Use talloc

Rare cases where plain `malloc()` is appropriate:
- **FFI boundaries** - Passing to libraries that expect `free()`-able memory
- **Long-lived singletons** - Global state that lives for entire program

For most cases, use talloc and make allocations children of appropriate context.

## Common Patterns

### Pattern 1: Short-lived request processing

```c
void handle_request(const char *input) {
    TALLOC_CTX *req_ctx = talloc_new(NULL);

    // All processing allocations are children
    res_t res = ik_protocol_msg_parse(req_ctx, input);
    if (is_err(&res)) { /* handle error */ }
    ik_protocol_msg_t *msg = res.ok;

    // ... process message ...

    talloc_free(req_ctx);  // Everything freed at once
}
```

### Pattern 2: Allocating result on caller's context

```c
res_t ik_cfg_load(TALLOC_CTX *ctx, const char *path) {
    // Allocate config on caller's context
    ik_cfg_t *config = talloc_zero_(ctx, sizeof(ik_cfg_t));
    if (!config) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // All strings are children of config
    config->openai_api_key = talloc_strdup(config, key_from_file);
    if (!config->openai_api_key) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    config->listen_address = talloc_strdup(config, addr_from_file);
    if (!config->listen_address) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    config->listen_port = port_from_file;

    return OK(config);
}

// Caller frees context when done
// talloc_free(ctx);  // Frees config and all children
```

### Pattern 3: Hierarchical contexts

```c
typedef struct {
    TALLOC_CTX *ctx;      // Parent for entire connection
    char *sess_id;      // Child of ctx
    char *corr_id;  // Child of ctx
    // No need to track allocations - they're all children
} ik_handler_ws_conn_t;

ik_handler_ws_conn_t *connection_create(void) {
    TALLOC_CTX *ctx = talloc_new(NULL);
    ik_handler_ws_conn_t *conn = talloc_zero(ctx, ik_handler_ws_conn_t);
    conn->ctx = ctx;

    res_t res = ik_protocol_generate_uuid(conn);
    if (is_err(&res)) { /* handle error */ }
    conn->sess_id = res.ok;  // Child of conn

    return conn;
}

void connection_free(ik_handler_ws_conn_t *conn) {
    talloc_free(conn->ctx);  // Everything freed automatically
}
```

### Pattern 4: Using temporary contexts for intermediate work

```c
res_t ik_openai_stream_req(TALLOC_CTX *ctx, ...) {
    // Create temporary context for request processing
    TALLOC_CTX *tmp = talloc_new(ctx);

    // All intermediate allocations on tmp
    sse_parser_t *parser = talloc_zero(tmp, sse_parser_t);
    parser->buffer = talloc_array(tmp, char, 4096);
    // ... perform request ...

    // Result allocated on ctx if returning error
    res_t result;
    if (error_occurred) {
        result = ERR(ctx, NETWORK, "Request failed");
    } else {
        result = OK(NULL);
    }

    talloc_free(tmp);  // Clean up all intermediate allocations
    return result;
}
```

## Phase 1 Implementation Plan

1. Add talloc dependency to Makefile (`-ltalloc`)
2. Install libtalloc-dev on Debian: `apt-get install libtalloc-dev`
3. Update config module to use talloc
4. Update protocol module to accept `TALLOC_CTX*` parameter
5. WebSocket handler creates per-message contexts
6. Document ownership in each module's header file comments
7. Add talloc usage tests to relevant test files

## Debugging Memory Issues

talloc provides excellent debugging tools:

```c
// Enable memory tracking
talloc_enable_leak_report();
talloc_enable_leak_report_full();

// Dump memory tree
talloc_report_full(context, stdout);

// Check for leaks at program exit
atexit(talloc_report_full_on_exit);
```

## Future Considerations

- **Phase 2+**: Consider reference counting for shared objects
- **Phase 3+**: Named contexts for better leak debugging
- **Phase 4+**: talloc pools for performance-critical paths

---

**Rule**: When in doubt, use talloc. Individual `malloc/free` should be rare and well-justified.
