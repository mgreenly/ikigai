# Phase 1 Implementation Details

This document provides detailed implementation specifications for Phase 1 modules, complementing the overview in phase-1.md.

## Design Decisions & Resolved Concerns

### Threading Model (VERIFIED)
**Decision**: libulfius creates one dedicated thread per WebSocket connection for protocol handling. Each connection spawns an additional worker thread for task processing.

**Status**: Verified through libulfius source code inspection and documentation review.

**Architecture**: Two threads per connection:
1. **WebSocket thread** (libulfius-managed): Protocol handling, envelope parsing, handshake
2. **Worker thread** (connection-managed): Task processing (OpenAI requests, etc.)

**Coordination**: WebSocket thread enqueues tasks and blocks waiting for completion. Worker thread processes tasks and signals completion via pthread condition variables.

**Implication**: Clean separation of concerns. WebSocket thread handles protocol, worker thread handles business logic. Worker can be immediately aborted on disconnect/shutdown.

### Worker Thread Architecture (FIRE-AND-FORGET PATTERN)

**Decision**: Each connection has a dedicated worker thread that processes tasks from a connection-local queue. WebSocket message callbacks enqueue tasks and return immediately ("fire-and-forget").

**Why fire-and-forget**: WebSocket callbacks never block, they just enqueue work and return immediately. This eliminates shutdown coordination complexity—the callback is done before shutdown even starts. Worker threads handle all processing and send responses directly to clients.

**Why worker threads are required**: Without worker threads, the WebSocket callback thread blocks during long-running operations (OpenAI streaming, future database queries). This creates two critical failures:

1. **Client disconnect ignored**: If client disconnects while callback is blocked in `curl_easy_perform()`, the close callback cannot execute until the HTTP request completes (10-30s). This wastes API quota, bandwidth, and server resources.

2. **Shutdown hangs**: Server shutdown (`ulfius_stop_framework()`) must wait for all callbacks to complete. Without abort capability, this means waiting for in-flight HTTP requests to finish naturally.

**How fire-and-forget solves this**:
- **WebSocket thread**: Handles protocol (framing, handshake). Enqueues task with copies of all data. Returns immediately.
- **Worker thread**: Processes tasks (OpenAI requests, future DB queries). Sends responses directly to client. Checks `abort_flag` during I/O.
- **Immediate abort**: Client disconnect → close callback sets `abort_flag` → worker sees flag within one poll interval → aborts curl request → worker exits → close callback joins worker → cleanup completes.

**Future-proofing**: Phase 3+ will add database operations and Phase 6 will add tool execution. Both require the same abort semantics. Building this in Phase 1 means the concurrency model is correct from the start.

**Performance**: One connection = two threads (WebSocket + worker). This is acceptable for a multi-user server handling 10-100 concurrent connections. Modern systems handle thousands of threads efficiently.

**Alignment with Single-Request-Per-Connection**: Phase 1 processes one request at a time per connection - the client sees synchronous behavior. The worker thread is an implementation detail that provides correct abort and shutdown semantics.

### Client Disconnect During Streaming (CORRECT)
**Decision**: Client disconnect immediately aborts in-flight requests.

**Implementation**:
1. Client disconnects → TCP FIN received
2. libulfius calls close callback immediately (WebSocket thread is not blocked)
3. Close callback sets `conn->abort_flag = 1`
4. Worker thread checks abort flag in curl multi loop (next poll iteration)
5. Worker aborts curl request, cleans up, exits thread
6. Close callback joins worker thread, frees connection context

**Benefit**: No wasted resources. OpenAI request stops immediately when client is gone.

### Fire-and-Forget Pattern Safety (EMPIRICALLY VERIFIED)

**Status**: ✅ VERIFIED - Comprehensive testing confirms the fire-and-forget pattern is safe for production use.

**Test Date**: 2025-11-06
**Test Location**: `tests/shutdown/`
**Test Results**: All safety criteria met

#### What Was Tested

A comprehensive test suite verified the safety of the fire-and-forget pattern under three scenarios:
1. **Normal completion** - Worker completes all tasks before disconnect
2. **Client disconnect** - Client drops connection while worker is sending
3. **Server shutdown** - Server receives SIGINT while worker is active

Each test scenario was executed with:
- Normal build (debug symbols)
- Valgrind (memory leak detection)
- AddressSanitizer (use-after-free detection)

#### Safety Verification Results

| Safety Criterion | Result | Evidence |
|------------------|--------|----------|
| **No deadlocks** | ✅ PASS | `pthread_join()` returned 0 in all tests |
| **No use-after-free** | ✅ PASS | Valgrind: 0 bytes definitely lost |
| **Worker detects disconnect** | ✅ PASS | `ulfius_websocket_send_message()` returns error code 3 |
| **Close callback can join worker** | ✅ PASS | No hangs, clean completion in all scenarios |
| **Memory safety** | ✅ PASS | AddressSanitizer clean, no memory errors |

**Key Finding**: Calling `pthread_join()` from the WebSocket close callback is **safe** - libulfius does not deadlock. The worker thread can safely use the WebSocket handle until the abort flag is checked.

#### Observed Behavior

**During disconnect:**
```
[time] Close callback: START
[time] Close callback: setting abort flag
[time] Close callback: calling pthread_join()
[time] Worker: ulfius_websocket_send_message() returned 3 (U_ERROR_PARAMS)
[time] Worker: send failed, aborting task
[time] Worker thread exiting
[time] Close callback: pthread_join() returned 0 (success)
```

**Timing characteristics:**
- Close callback can start while worker is active
- Worker detects abort flag before next send (cooperative cancellation)
- `ulfius_websocket_send_message()` returns error after disconnect (automatic detection)
- `pthread_join()` completes cleanly without deadlock
- Total shutdown time: < 200ms (meets shutdown response requirement)

#### Dual Protection Mechanism

The pattern provides **two independent safety mechanisms**:

1. **Primary - Cooperative cancellation**: Abort flag checked before each operation
2. **Secondary - Error detection**: `ulfius_websocket_send_message()` returns non-U_OK after disconnect

This redundancy ensures workers always stop promptly, even if one mechanism fails.

#### Memory Safety Details

**Valgrind results:**
```
definitely lost: 0 bytes in 0 blocks       ✅ No leaks
indirectly lost: 0 bytes in 0 blocks       ✅ No leaks
possibly lost: 3,984 bytes in 6 blocks     ℹ️ Library internals (TLS, thread pools)
still reachable: 117,627 bytes             ℹ️ libulfius/libmicrohttpd/GnuTLS state
```

The "possibly lost" memory comes from:
- Thread-local storage allocated by libulfius for WebSocket threads
- Dynamic linker internals for library loading
- GnuTLS crypto state initialization

These are **not real leaks** - they're internal library allocations that persist until process exit. This is normal and expected for server libraries.

**Most critical**: The server shutdown test shows **✅ NO MEMORY ERRORS** - completely clean.

#### Implementation Confidence

Based on these test results, the Phase 1 implementation can proceed with confidence:

```c
void websocket_onclose_callback(...) {
    connection_t *conn = user_data;

    // Signal abort - worker will see this within one poll interval
    conn->abort_flag = 1;

    // Close queue - prevents new tasks from being processed
    task_queue_close(&conn->queue);

    // Wait for worker - SAFE (verified by test suite)
    pthread_join(conn->worker_thread, NULL);

    // Cleanup - SAFE (worker has exited, no memory leaks)
    talloc_free(conn->ctx);
}
```

**No architectural changes needed.** The fire-and-forget pattern works as designed.

#### Answer to "Should we use a lower-level library?"

**No.** The test results prove that libulfius is suitable:
- WebSocket handles remain valid during close callback execution
- `pthread_join()` from close callback does not deadlock with libulfius/libmicrohttpd
- The fire-and-forget pattern is safe with libulfius's threading model
- Memory management is clean with no leaks or use-after-free errors

Any lower-level library would have similar threading concerns, but would require implementing WebSocket protocol details manually, adding complexity and potential security issues.

**Conclusion**: libulfius is the right choice. Proceed with Phase 1 implementation as designed.

### API Key Validation (DESIGN CHOICE)
**Decision**: Do NOT validate OpenAI API key on startup. Let the first request fail if key is invalid.

**Rationale**: Fail-fast at the boundary. Server starts quickly, invalid key produces clear 401 error on first use.

**Trade-off**: Server appears functional but isn't. Accepted because this is a single-user localhost tool in Phase 1.

### Error Propagation Macro Naming (RESOLVED)
**Decision**: Use `CHECK` instead of `TRY` for error propagation macro.

**Rationale**: More explicit about intent (checking for errors), familiar to systems programmers, avoids confusion with exception handling.

**Implementation**: Simple do-while macro that returns early on error. Value extraction is explicit.

---

## General Principles

### Memory Management
- **All data structures are talloc-managed**
- **No explicit `*_free()` functions** - use `talloc_free(ctx)` instead
- Caller provides context, receives allocated results on that context
- Single `talloc_free(ctx)` cleans up everything

### Error Handling

#### Error Codes for Phase 1

**Strategy**: Add error codes organically as implementation reveals actual error conditions.

**Current codes in error.h**:
- `IK_OK` - Success
- `IK_ERR_OOM` - Out of memory
- `IK_ERR_INVALID_ARG` - Invalid argument
- `IK_ERR_OUT_OF_RANGE` - Out of range

**Anticipated codes** (add when actually needed during implementation):
- `IK_ERR_IO` - File operations, config loading
- `IK_ERR_PARSE` - JSON/protocol parsing
- `IK_ERR_NETWORK` - HTTP/WebSocket failures
- `IK_ERR_AUTH` - OpenAI API key issues
- `IK_ERR_PROTOCOL` - Protocol violations

**Process**: When implementing a module and encountering an error condition, add the appropriate error code to `error.h` and update `ik_error_code_str()` at that time. Don't add codes speculatively.

#### Error Propagation Macro

Rename the `TRY` macro to `CHECK` for clarity. Keep the existing simple implementation from `error.h`:

```c
// CHECK macro - propagate error to caller (return early if error)
#define CHECK(expr) \
  do { \
    ik_result_t _result = (expr); \
    if (_result.is_err) { \
      return _result; \
    } \
  } while (0)
```

**Usage:**
```c
ik_result_t server_init(TALLOC_CTX *ctx) {
    ik_result_t res = ik_cfg_load(ctx, path);
    CHECK(res);  // Return early if error
    ik_cfg_t *cfg = res.ok;

    res = ik_cfg_validate(ctx, cfg);
    CHECK(res);  // Return early if error

    return OK(state);
}
```

**Rationale:** The macro does one thing well: propagates errors. For extracting values, use the explicit pattern shown above. This keeps the macro simple and the error flow visible.

#### API Design Rule

**All public API functions return `ik_result_t`**, including constructors and helper functions. This allows:
- Consistent error handling everywhere
- Use of `CHECK()` for clean error propagation
- Explicit handling of allocation failures (OOM)

Exceptions:
- Callbacks with fixed signatures (libulfius, etc.)
- Pure accessors/getters that cannot fail

---

## Config Module (`config.c/h`)

**Note**: Configuration is loaded once at startup. Changes require server restart (see decisions.md: "Why No Config Hot-Reload?").

### API

```c
typedef struct {
  char *openai_api_key;
  char *listen_address;
  int listen_port;
} ik_cfg_t;

ik_result_t ik_cfg_load(TALLOC_CTX *ctx, const char *path);
```

### Path Expansion

Use `getenv("HOME")` for tilde expansion:

```c
static char *expand_tilde(TALLOC_CTX *ctx, const char *path) {
    if (path[0] != '~') {
        return talloc_strdup(ctx, path);
    }

    const char *home = getenv("HOME");
    if (!home) {
        return NULL;  // Caller returns ERR(ctx, IO, "HOME environment variable not set")
    }

    return talloc_asprintf(ctx, "%s%s", home, path + 1);
}
```

**Error if HOME is not set** - return `IK_ERR_IO` with message "HOME environment variable not set"

### Auto-Creation Behavior

If `~/.ikigai/config.json` does not exist:

1. Create `~/.ikigai/` directory (mode 0755)
2. Create config file with default content:

```json
{
  "openai_api_key": "YOUR_API_KEY_HERE",
  "listen_address": "127.0.0.1",
  "listen_port": 1984
}
```

3. Continue loading the newly created file
4. **Do not validate api_key content** - let OpenAI API handle authentication errors naturally

### Validation Rules

- JSON structure is valid (return `IK_ERR_PARSE` on parse failure)
- Required fields exist: `openai_api_key`, `listen_address`, `listen_port`
- Field types: `openai_api_key` is string, `listen_address` is string, `listen_port` is number
- Port range: 1024-65535 (non-privileged ports only)
- **Do NOT check if api_key is placeholder** - server will start successfully, OpenAI will return auth error on first request

### Memory Management

- `ik_cfg_t` and all strings allocated on provided `ctx`
- Use libjansson's default allocator (malloc/free)
- Extract values with `talloc_strdup()` / `talloc_asprintf()`
- Call `json_decref()` when done with jansson objects
- Caller cleans up with `talloc_free(ctx)` - no `cfg_free()` function

### Test Coverage

`tests/unit/config_test.c`:
- Load valid config file
- Auto-create missing directory
- Auto-create missing config file
- Parse error on invalid JSON
- Error on missing fields
- Error on wrong field types
- Port validation (too low, too high, valid)
- HOME not set error
- Tilde expansion works correctly

---

## Protocol Module (`protocol.c/h`)

Handles post-handshake message parsing, serialization, and UUID generation. Handshake messages (`hello`/`welcome`) are parsed inline in `websocket.c`.

### API

```c
typedef struct {
  char *sess_id;
  char *corr_id;
  char *type;
  json_t *payload;  // Generic JSON, handler interprets based on type
} ik_protocol_msg_t;

// Parse envelope message from JSON string
ik_result_t ik_protocol_msg_parse(TALLOC_CTX *ctx, const char *json_str);

// Serialize envelope message to JSON string
ik_result_t ik_protocol_msg_serialize(TALLOC_CTX *ctx, ik_protocol_msg_t *msg);

// Generate base64url-encoded UUID (22 characters)
ik_result_t ik_protocol_generate_uuid(TALLOC_CTX *ctx);

// Constructors for server-created messages
ik_result_t ik_protocol_msg_create_err(TALLOC_CTX *ctx,
                                              const char *sess_id,
                                              const char *corr_id,
                                              const char *source,
                                              const char *err_msg);

ik_result_t ik_protocol_msg_create_assistant_resp(TALLOC_CTX *ctx,
                                                           const char *sess_id,
                                                           const char *corr_id,
                                                           json_t *payload);
```

### Message Format

All post-handshake messages use the envelope format.

**Client → Server:**
```json
{
  "sess_id": "VQ6EAOKbQdSnFkRmVUQAAA",
  "type": "user_query",
  "payload": {
    "model": "gpt-4o-mini",
    "messages": [...]
  }
}
```

**Server → Client:**
```json
{
  "sess_id": "VQ6EAOKbQdSnFkRmVUQAAA",
  "corr_id": "8fKm3pLxTdOqZ1YnHjW9Gg",
  "type": "assistant_response",
  "payload": {...}
}
```

**Fields:**
- `sess_id`: Base64URL-encoded UUID (22 chars) - identifies WebSocket connection (both directions)
- `corr_id`: Base64URL-encoded UUID (22 chars) - identifies exchange (server→client only, for logging/observability)
- `type`: String identifying message type ("user_query", "assistant_response", "error")
- `payload`: Generic JSON object, interpreted based on `type`

**Note:** Client messages do NOT include `corr_id`. Server generates it when processing requests and includes it in responses.

### UUID Generation

```c
ik_result_t ik_protocol_generate_uuid(TALLOC_CTX *ctx) {
    uuid_t uuid;
    uuid_generate_random(uuid);  // 16 bytes from libuuid

    // Base64 encode using libb64
    char *b64 = base64_encode(uuid, 16);
    if (!b64) {
        return ERR(ctx, OOM, "Failed to encode UUID");
    }

    // Convert to base64url: + → -, / → _, remove padding =
    char *b64url = talloc_array(ctx, char, 23);  // 22 + null
    if (!b64url) {
        free(b64);
        return ERR(ctx, OOM, "Failed to allocate UUID string");
    }

    int j = 0;
    for (int i = 0; b64[i] && b64[i] != '='; i++) {
        if (b64[i] == '+') b64url[j++] = '-';
        else if (b64[i] == '/') b64url[j++] = '_';
        else b64url[j++] = b64[i];
    }
    b64url[j] = '\0';

    free(b64);  // libb64 uses malloc
    return OK(b64url);
}
```

**UUID comparison:** IDs are always 22 characters, so can use `strcmp()` or `memcmp(id1, id2, 22)`.

### Message Parsing

```c
ik_result_t ik_protocol_msg_parse(TALLOC_CTX *ctx, const char *json_str) {
    // Parse JSON
    json_error_t jerr;
    json_t *root = json_loads(json_str, 0, &jerr);
    if (!root) {
        return ERR(ctx, PARSE, "JSON parse error: %s", jerr.text);
    }

    // Extract envelope fields
    // Validate required fields exist and have correct types
    // Extract strings with talloc_strdup()
    // payload remains as json_t* (don't incref - steal or copy as needed)

    json_decref(root);
    return OK(message);
}
```

**Key points:**
- Use jansson's default allocator (malloc/free)
- Extract strings to talloc with `talloc_strdup()`
- `payload` field: keep as `json_t*` pointer (caller decides whether to incref/copy)
- Validate envelope structure, but don't validate payload contents
- Call `json_decref()` on root when done

### Message Serialization

```c
ik_result_t ik_protocol_msg_serialize(TALLOC_CTX *ctx, ik_protocol_msg_t *msg) {
    // Build JSON object with jansson
    json_t *root = json_object();
    json_object_set_new(root, "sess_id", json_string(msg->sess_id));
    json_object_set_new(root, "corr_id", json_string(msg->corr_id));
    json_object_set_new(root, "type", json_string(msg->type));
    json_object_set(root, "payload", msg->payload);  // Reference, not copy

    // Serialize to string
    char *json_str = json_dumps(root, JSON_COMPACT);
    json_decref(root);

    if (!json_str) {
        return ERR(ctx, OOM, "Failed to serialize message");
    }

    // Copy to talloc context
    char *result = talloc_strdup(ctx, json_str);
    free(json_str);  // jansson uses malloc

    if (!result) {
        return ERR(ctx, OOM, "Failed to allocate serialized message");
    }

    return OK(result);
}
```

### Handshake Messages

**NOT handled by this module.** WebSocket handler parses handshake inline:

**Hello (client → server):**
```json
{"type": "hello", "identity": "hostname@username"}
```

**Welcome (server → client):**
```json
{"type": "welcome", "sess_id": "VQ6EAOKbQdSnFkRmVUQAAA"}
```

Rationale: Handshake is connection setup (WebSocket layer concern), not application protocol (Protocol layer concern). Keeping them separate allows different formats without complicating the message parser.

### Memory Management

- `ik_protocol_msg_t` and all strings allocated on provided `ctx`
- `payload` is a `json_t*` - caller responsible for reference counting if needed
- No `ik_protocol_msg_free()` - caller uses `talloc_free(ctx)`

### Dependencies

- `libuuid-dev` - UUID generation
- `libb64-dev` - Base64 encoding (add to Makefile)
- `libjansson-dev` - JSON parsing (already present)

Update Makefile:
```makefile
SERVER_LIBS = -lulfius -ljansson -lcurl -ltalloc -luuid -lb64
```

### Test Coverage

`tests/unit/protocol_test.c`:
- Parse valid envelope message
- Parse error on invalid JSON
- Parse error on missing envelope fields
- Parse error on wrong field types
- Serialize message round-trip (parse → serialize → parse)
- UUID generation produces 22-char base64url strings
- UUID generation produces unique values
- ik_protocol_msg_create_err constructs valid error message
- ik_protocol_msg_create_assistant_resp constructs valid response

---

## OpenAI Client (`openai.c/h`)

Handles HTTP streaming requests to OpenAI Chat Completions API. Uses libcurl for HTTP client and implements SSE (Server-Sent Events) parsing.

**IMPORTANT UPDATE:** Must use curl multi interface instead of `curl_easy_perform()` to support immediate abort on shutdown. See "Shutdown Support" section below.

### API

```c
// Stream callback - called for each parsed JSON chunk from OpenAI
// json_chunk is a complete JSON object string (without "data: " prefix)
// Will NOT be called for [DONE] marker or error responses
typedef void (*ik_openai_stream_cb_t)(const char *json_chunk, void *user_data);

// Make streaming request to OpenAI Chat Completions API
ik_result_t ik_openai_stream_req(TALLOC_CTX *ctx,
                                     const ik_cfg_t *config,
                                     json_t *req_payload,
                                     ik_openai_stream_cb_t callback,
                                     void *cb_data,
                                     volatile sig_atomic_t *abort_flag);
```

**Parameters:**
- `ctx` - talloc context for error allocation only
- `config` - server config (contains `openai_api_key`)
- `req_payload` - JSON object with OpenAI request (borrowed, caller retains ownership)
- `callback` - function called for each SSE chunk containing JSON
- `cb_data` - passed through to callback (typically `ik_task_t*` from handler module)
- `abort_flag` - pointer to abort flag (from task->abort_flag which points to conn->abort_flag)

**Returns:**
- `OK(NULL)` - stream completed successfully (received `[DONE]`)
- `ERR(..., NETWORK, ...)` - HTTP errors (connection failed, non-2xx status)
- `ERR(..., AUTH, ...)` - authentication failure (401, invalid API key)
- `ERR(..., PARSE, ...)` - malformed SSE or JSON from OpenAI
- `ERR(..., OOM, ...)` - memory allocation failure

### HTTP Request Details

**Endpoint:** `https://api.openai.com/v1/chat/completions`

**Headers:**
```
Authorization: Bearer <config->openai_api_key>
Content-Type: application/json
```

**Request Body:**
Serialized JSON from `req_payload` parameter. The WebSocket handler passes through the client's payload, which should have:
```json
{
  "model": "gpt-4o-mini",
  "messages": [...],
  "stream": true
}
```

The server does NOT modify the payload - it forwards exactly what the client sent (except for injecting credentials).

### SSE Stream Parsing

OpenAI responds with `Content-Type: text/event-stream`:

```
data: {"id":"chatcmpl-123","object":"chat.completion.chunk","created":1234567890,"model":"gpt-4o-mini","choices":[{"index":0,"delta":{"role":"assistant","content":""},"finish_reason":null}]}

data: {"id":"chatcmpl-123","object":"chat.completion.chunk","created":1234567890,"model":"gpt-4o-mini","choices":[{"index":0,"delta":{"content":"Hello"},"finish_reason":null}]}

data: {"id":"chatcmpl-123","object":"chat.completion.chunk","created":1234567890,"model":"gpt-4o-mini","choices":[{"index":0,"delta":{"content":" there"},"finish_reason":null}]}

data: {"id":"chatcmpl-123","object":"chat.completion.chunk","created":1234567890,"model":"gpt-4o-mini","choices":[{"index":0,"delta":{},"finish_reason":"stop"}]}

data: [DONE]

```

**SSE Format:**
- Each message starts with `data: `
- Message ends with `\n\n` (double newline)
- Final message is `data: [DONE]\n\n`

**Parsing Algorithm:**

1. libcurl write callback accumulates data in buffer
2. Scan buffer for complete SSE messages (delimited by `\n\n`)
3. For each complete message:
   - Strip `data: ` prefix
   - If content is `[DONE]`, set done flag and stop
   - Validate JSON (use `json_loads` to check)
   - Call `callback(json_chunk, cb_data)`
   - Remove processed message from buffer
4. Keep partial message in buffer for next callback
5. Continue until `[DONE]` or stream ends

**SSE Parser State:**

```c
typedef struct {
  TALLOC_CTX *ctx;                 // For buffer allocations
  char *buffer;                    // Accumulated SSE data (talloc)
  size_t buffer_len;               // Bytes currently in buffer
  size_t buffer_capacity;          // Allocated buffer size
  ik_openai_stream_cb_t callback;
  void *cb_data;
  bool done;                       // Received [DONE] marker
  ik_result_t error;               // Stores error if parsing fails
} sse_parser_t;
```

**Buffer Management:**
- Initial size: 4096 bytes
- Growth strategy: double capacity when full (`capacity * 2`)
- Maximum size: 1MB (1,048,576 bytes) - prevents unbounded growth from malicious/buggy responses
- If doubling would exceed 1MB, reject growth and abort transfer (buffer capacity never exceeds 1MB)
- This means SSE responses must fit in under 1MB of accumulated data before processing
- Automatic cleanup via talloc (allocated on temporary context)

**libcurl Write Callback:**

```c
size_t sse_write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
  sse_parser_t *parser = (sse_parser_t *)userdata;
  size_t bytes = nmemb;  // size is always 1

  // Grow buffer if needed
  if (parser->buffer_len + bytes > parser->buffer_capacity) {
    size_t new_cap = parser->buffer_capacity * 2;

    // Check 1MB limit - ensure new capacity doesn't exceed limit
    if (new_cap > 1048576) {
      parser->error = ERR(parser->ctx, PARSE, "SSE buffer growth would exceed 1MB limit");
      return 0;  // Abort transfer
    }

    char *new_buf = talloc_realloc(parser->ctx, parser->buffer, char, new_cap);
    if (!new_buf) {
      parser->error = ERR(parser->ctx, OOM, "Failed to grow SSE buffer");
      return 0;  // Abort transfer
    }
    parser->buffer = new_buf;
    parser->buffer_capacity = new_cap;
  }

  // Append data to buffer
  memcpy(parser->buffer + parser->buffer_len, ptr, bytes);
  parser->buffer_len += bytes;

  // Process complete SSE messages
  while (!parser->done) {
    char *delim = strstr(parser->buffer, "\n\n");
    if (!delim) break;  // No complete message yet

    size_t msg_len = delim - parser->buffer;

    // Extract message (skip "data: " prefix - 6 chars)
    if (msg_len < 6 || memcmp(parser->buffer, "data: ", 6) != 0) {
      parser->error = ERR(parser->ctx, PARSE, "Invalid SSE format");
      return 0;
    }

    char *content = parser->buffer + 6;
    size_t content_len = msg_len - 6;

    // Check for [DONE]
    if (content_len == 6 && memcmp(content, "[DONE]", 6) == 0) {
      parser->done = true;
      break;
    }

    // Validate JSON and call callback
    // (null-terminate temporarily, validate, restore, call callback)
    // Implementation details omitted for brevity

    // Remove processed message from buffer (shift remaining data)
    memmove(parser->buffer, delim + 2, parser->buffer_len - (msg_len + 2));
    parser->buffer_len -= (msg_len + 2);
  }

  return bytes;
}
```

### Error Handling

**OpenAI API Errors:**

When OpenAI returns an error (e.g., invalid API key, rate limit), the response is JSON instead of SSE:

```json
{
  "error": {
    "message": "Incorrect API key provided",
    "type": "invalid_request_error",
    "code": "invalid_api_key"
  }
}
```

**Detection:**
- Check HTTP status code (401 = auth error, 429 = rate limit, etc.)
- For non-2xx status, read response body
- Parse as JSON, extract error message
- Return appropriate error code:
  - 401 → `IK_ERR_AUTH`
  - 429, 500, 503 → `IK_ERR_NETWORK`
  - Other → `IK_ERR_NETWORK`

**Do NOT call the stream callback for error responses.** Return an error result and let the WebSocket handler construct an error message envelope.

**Parsing Errors:**

If SSE parsing fails:
- Invalid SSE format (missing `data: ` prefix) → `IK_ERR_PARSE`
- Invalid JSON in chunk → `IK_ERR_PARSE`
- Buffer growth failure → `IK_ERR_OOM`

### Abort Support

**Requirement:** Must abort in-flight requests immediately when abort flag is set (client disconnect or server shutdown).

**Implementation:** Use curl multi interface instead of blocking `curl_easy_perform()`. The abort flag parameter can be either:
- `&conn->abort_flag` - set by close callback on client disconnect
- `&g_httpd_shutdown` - set by signal handler on server shutdown

Handler module passes `&conn->abort_flag` for proper per-connection abort semantics.

```c
// Abort flag parameter:
ik_result_t ik_openai_stream_req(TALLOC_CTX *ctx,
                                     const ik_cfg_t *config,
                                     json_t *req_payload,
                                     ik_openai_stream_cb_t callback,
                                     void *cb_data,
                                     volatile sig_atomic_t *abort_flag);
```

**Event loop with abort check:**
```c
CURLM *multi = curl_multi_init();
curl_multi_add_handle(multi, curl);

int running;
while (running && !*abort_flag) {
    curl_multi_perform(multi, &running);

    // Poll with short timeout for responsive abort detection
    // Actual value chosen to meet shutdown requirements
    curl_multi_poll(multi, NULL, 0, 50, NULL);
}

if (*abort_flag) {
    // Abort: remove handle immediately
    curl_multi_remove_handle(multi, curl);
    // Return OK(NULL) - abort is not an error, just early termination
}
```

**Handler passes connection's abort flag:**
```c
// In worker thread:
task->result = ik_openai_stream_req(..., task->abort_flag);
// task->abort_flag points to conn->abort_flag
```

### Implementation Flow (Updated for curl multi)

```c
ik_result_t ik_openai_stream_req(TALLOC_CTX *ctx,
                                     const ik_cfg_t *config,
                                     json_t *req_payload,
                                     ik_openai_stream_cb_t callback,
                                     void *cb_data,
                                     volatile sig_atomic_t *shutdown_flag) {
  TALLOC_CTX *tmp_ctx = talloc_new(ctx);
  if (!tmp_ctx) {
    return ERR(ctx, OOM, "Failed to create temporary context");
  }

  // 1. Serialize request payload to JSON string
  char *json_body = json_dumps(req_payload, JSON_COMPACT);
  if (!json_body) {
    talloc_free(tmp_ctx);
    return ERR(ctx, OOM, "Failed to serialize request");
  }

  // 2. Initialize SSE parser
  sse_parser_t *parser = talloc_zero(tmp_ctx, sse_parser_t);
  parser->ctx = tmp_ctx;
  parser->buffer = talloc_array(tmp_ctx, char, 4096);
  parser->buffer_capacity = 4096;
  parser->callback = callback;
  parser->cb_data = cb_data;
  parser->error = OK(NULL);

  // 3. Setup libcurl easy handle
  CURL *curl = curl_easy_init();
  if (!curl) {
    free(json_body);
    talloc_free(tmp_ctx);
    return ERR(ctx, NETWORK, "Failed to initialize curl");
  }

  // Build Authorization header
  char *auth_header = talloc_asprintf(tmp_ctx, "Authorization: Bearer %s",
                                      config->openai_api_key);

  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, auth_header);
  headers = curl_slist_append(headers, "Content-Type: application/json");

  curl_easy_setopt(curl, CURLOPT_URL, "https://api.openai.com/v1/chat/completions");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, sse_write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, parser);

  // 4. Create multi handle and add easy handle
  CURLM *multi = curl_multi_init();
  if (!multi) {
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    free(json_body);
    talloc_free(tmp_ctx);
    return ERR(ctx, NETWORK, "Failed to initialize curl multi");
  }

  curl_multi_add_handle(multi, curl);

  // 5. Perform request with shutdown polling
  int running = 1;
  bool aborted = false;

  while (running && !*shutdown_flag) {
    curl_multi_perform(multi, &running);

    // Poll with 50ms timeout for quick abort detection
    curl_multi_poll(multi, NULL, 0, 50, NULL);
  }

  if (*shutdown_flag) {
    aborted = true;
  }

  // 6. Check HTTP status
  long http_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

  // 7. Cleanup
  curl_multi_remove_handle(multi, curl);
  curl_multi_cleanup(multi);
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  free(json_body);

  // 8. Handle errors
  ik_result_t result = OK(NULL);

  if (aborted) {
    result = OK(NULL);  // Shutdown is not an error, just early termination
  } else if (parser->error.is_err) {
    result = parser->error;  // Parsing error occurred
  } else if (http_code == 401) {
    result = ERR(ctx, AUTH, "OpenAI authentication failed (invalid API key)");
  } else if (http_code != 200) {
    result = ERR(ctx, NETWORK, "OpenAI API error (HTTP %ld)", http_code);
  }

  talloc_free(tmp_ctx);
  return result;
}
```

### Memory Management

- **req_payload**: Borrowed (caller owns, we don't incref/decref)
- **shutdown_flag**: Borrowed pointer (owned by httpd module)
- **Temporary context**: Created with `talloc_new(ctx)`, freed before return
- **SSE parser**: Allocated on temporary context, auto-freed
- **JSON serialization**: Uses jansson's malloc, must call `free(json_body)`
- **curl easy handle**: Created with `curl_easy_init()`, freed with `curl_easy_cleanup()`
- **curl multi handle**: Created with `curl_multi_init()`, freed with `curl_multi_cleanup()`
- **curl headers**: Allocated by libcurl, freed with `curl_slist_free_all()`
- **Error results**: Allocated on `ctx` parameter (only if returning error)

### Test Coverage

`tests/unit/openai_test.c`:
- Successful streaming request (use real API during development)
- Parse multiple SSE chunks correctly
- Handle [DONE] terminator
- HTTP error (401 authentication)
- HTTP error (500 server error)
- Malformed SSE (missing `data:` prefix)
- Malformed JSON in chunk
- Buffer growth (send large chunks that exceed 4KB)
- OOM during buffer growth (inject allocation failure)
- Connection failure (network error)
- **Shutdown abort**: Set shutdown flag during request, verify abort within shutdown requirement
- **Shutdown abort**: Verify callback not called after shutdown flag set

**Note:** Tests will call real OpenAI API during development. Capture responses and create mocked tests for release builds.

---

## Handler Module (`handler.c/h`)

Manages WebSocket connection lifecycle, handshake protocol, and dispatches tasks to worker thread for processing. Uses libulfius for WebSocket handling and pthread for worker coordination.

### Threading Architecture

**Two threads per connection:**

1. **WebSocket thread** (libulfius-managed):
   - Handles WebSocket protocol (framing, ping/pong)
   - Parses message envelopes
   - Manages handshake protocol
   - Enqueues tasks to worker
   - Blocks waiting for task completion
   - Sends responses back to client

2. **Worker thread** (connection-managed):
   - Processes tasks from queue (OpenAI requests, etc.)
   - Checks abort flag during processing
   - Signals completion when done
   - Can be immediately aborted on disconnect/shutdown

**Coordination:** pthread mutex/condition variables for queue synchronization and completion signaling.

**Verified behavior:** libulfius creates one dedicated thread per WebSocket connection. All callbacks execute sequentially on that thread. WebSocket thread cannot block during task processing because it delegates to worker thread.

**Multi-connection concurrency:** Different WebSocket connections run on different threads with different workers, so the server handles multiple clients concurrently.

### Connection State

```c
typedef struct {
  TALLOC_CTX *ctx;                    // Connection's talloc context (parent for all allocations)
  struct _u_websocket *websocket;     // libulfius WebSocket handle (borrowed, owned by libulfius)
  char *sess_id;                      // 22-char base64url UUID (allocated on ctx)
  ik_cfg_t *cfg_ref;                  // Server config (borrowed, owned by server_main)
  bool handshake_complete;            // true after successful hello/welcome exchange

  // Worker thread and synchronization
  pthread_t worker_thread;            // Worker thread handle
  ik_task_queue_t queue;              // Task queue (single slot for Phase 1)
  volatile sig_atomic_t abort_flag;   // Set by close callback, checked by worker
  bool closed;                        // Connection closed flag
} ik_handler_ws_conn_t;

typedef struct {
  TALLOC_CTX *ctx;                    // Task's talloc context for allocations
  char *type;                         // Task type: "user_query", etc. (allocated on task->ctx)
  char *sess_id;                      // Session ID (borrowed from connection - conn outlives task)
  char *corr_id;                      // Correlation ID (owned by task, allocated on task->ctx, unique per request)
  json_t *payload;                    // Request payload (owned by task, incref'd from message)
  struct _u_websocket *websocket;     // WebSocket handle (borrowed, owned by libulfius)
  volatile sig_atomic_t *abort_flag;  // Points to conn->abort_flag
  ik_cfg_t *cfg_ref;                  // Server config (borrowed)
} ik_task_t;

typedef struct {
  ik_task_t *pending_task;            // Single task slot (Phase 1: one request at a time)
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  bool has_task;
  bool closed;
} ik_task_queue_t;
```

**Lifecycle:**
1. Client connects → libulfius calls `ik_handler_websocket_manager` → allocate `ik_handler_ws_conn_t`
2. Initialize queue and spawn worker thread
3. Handshake (`hello` → `welcome`) → set `handshake_complete = true`
4. Message exchange (`user_query` → `assistant_response` chunks) - **FIRE AND FORGET**
   - WebSocket thread: parse message, create task (with copies of all data), enqueue, **RETURN IMMEDIATELY**
   - Worker thread: dequeue task, call `ik_openai_stream_request()`, send responses directly to client
   - OpenAI streams chunks → worker sends each chunk via `ulfius_websocket_send_message()`
   - Worker completes or aborts → frees task context, loops back to wait for next task
5. Client disconnects or error → libulfius calls close callback:
   - Set `abort_flag = 1` and `closed = true`
   - Signal queue to wake worker
   - `pthread_join(worker_thread)` (wait for worker to exit cleanly)
   - `talloc_free(conn->ctx)`

**Memory management:**
- Connection context created with `talloc_new(NULL)` (top-level context for connection state)
- All connection data allocated on `conn->ctx` (including sess_id)
- **Each task gets its own context**: `talloc_new(NULL)` for task allocations
- Task owns: type (copied), corr_id (generated), payload (incref'd)
- Task borrows: sess_id (from conn), websocket (from conn), abort_flag (from conn), cfg_ref (from conn)
- Worker frees task context when done: `talloc_free(task->ctx)`
- Connection cleanup: `talloc_free(conn->ctx)` in close callback after joining worker

**Thread safety:**
- `abort_flag` is `sig_atomic_t` (safe for worker to read, close callback to write)
- Queue operations protected by mutex
- **No completion signaling needed** - fire and forget pattern
- `ulfius_websocket_send_message()` called from worker thread (safe due to libulfius internal locking)
- Task-owned data (type, corr_id, payload) not shared after enqueue
- Borrowed pointers (sess_id, websocket, abort_flag, cfg_ref) safe because connection outlives task (pthread_join guarantees this)

### API

```c
// Main WebSocket callback - registered with libulfius
void ik_handler_websocket_manager(const struct _u_request *request,
                                   struct _u_websocket_manager *websocket_manager,
                                   void *user_data);

// Message handler - called for each incoming WebSocket message
static void handler_message_callback(struct _u_websocket_manager *websocket_manager,
                                     const struct _u_websocket_message *message,
                                     void *user_data);

// Close handler - called when connection closes
static void handler_close_callback(struct _u_websocket_manager *websocket_manager,
                                   void *user_data);

// Worker thread entry point
static void* worker_thread_fn(void *arg);

// Stream callback for OpenAI responses (called from worker thread)
static void openai_stream_callback(const char *json_chunk, void *user_data);

// Task queue operations (simplified - no completion signaling)
static void task_queue_init(ik_task_queue_t *queue);
static void task_queue_push(ik_task_queue_t *queue, ik_task_t *task);  // Takes ownership of task
static ik_task_t* task_queue_pop(ik_task_queue_t *queue);  // Blocks until task available or closed
static void task_queue_close(ik_task_queue_t *queue);  // Wakes worker to exit
static void task_queue_destroy(ik_task_queue_t *queue);  // Cleanup mutex/cond
```

### Handshake Protocol

**Client → Server (hello):**
```json
{
  "type": "hello",
  "identity": "hostname@username"
}
```

**Server → Client (welcome):**
```json
{
  "type": "welcome",
  "sess_id": "VQ6EAOKbQdSnFkRmVUQAAA"
}
```

**Handshake validation:**
- Parse JSON, check for `type` field
- If `type == "hello"`, validate `identity` field exists and is a string
- Generate `sess_id` using `ik_protocol_generate_uuid()`
- Send `welcome` message
- Set `handshake_complete = true`
- **Note:** Identity value is validated but NOT stored (future phases may store it for authorization)

**Before handshake completes:**
- Reject any message other than `hello` with error
- Close connection after sending error

### Message Flow

**1. User Query (Client → Server):**

Client sends envelope message:
```json
{
  "sess_id": "VQ6EAOKbQdSnFkRmVUQAAA",
  "type": "user_query",
  "payload": {
    "model": "gpt-4o-mini",
    "messages": [{"role": "user", "content": "Hello"}],
    "stream": true
  }
}
```

**Note:** Client does NOT send `corr_id` field. Server generates it.

**WebSocket thread processing (fire-and-forget):**
1. Parse message with `ik_protocol_msg_parse()`
2. Validate `sess_id` matches `conn->sess_id`
3. Validate `type == "user_query"`
4. Create task:
   ```c
   TALLOC_CTX *task_ctx = talloc_new(NULL);
   ik_task_t *task = talloc_zero(task_ctx, ik_task_t);
   task->ctx = task_ctx;

   // Owned by task - allocated on task_ctx
   task->type = talloc_strdup(task_ctx, msg->type);
   ik_result_t res = ik_protocol_generate_uuid(task_ctx);
   if (ik_is_err(&res)) { /* error */ }
   task->corr_id = res.ok;  // Task owns this unique corr_id
   task->payload = msg->payload;
   json_incref(task->payload);  // Task owns a reference

   // Borrowed from connection (conn outlives task via pthread_join)
   task->sess_id = conn->sess_id;
   task->websocket = conn->websocket;
   task->abort_flag = &conn->abort_flag;
   task->cfg_ref = conn->cfg_ref;
   ```
5. Push task to queue: `task_queue_push(&conn->queue, task)` (transfers ownership)
6. **RETURN IMMEDIATELY** (callback completes, no waiting)

**Worker thread processing:**
1. Block on `task_queue_pop(&conn->queue)` (waits for task or queue close)
2. If queue closed, exit thread
3. Receive task (worker now owns task)
4. Call `ik_openai_stream_req()` with callback:
   ```c
   ik_result_t result = ik_openai_stream_req(
     task->ctx,
     task->cfg_ref,
     task->payload,
     openai_stream_callback,
     task,  // Pass task as callback context
     task->abort_flag
   );
   ```
5. If result is error, send error message to client
6. Free task: `talloc_free(task->ctx)` (frees task and all owned data)
7. Loop back to step 1 (wait for next task)

**2. Assistant Response (Server → Client, via OpenAI callback):**

For each SSE chunk from OpenAI, `openai_stream_callback` is called (from worker thread):

```c
static void openai_stream_callback(const char *json_chunk, void *user_data) {
  ik_task_t *task = (ik_task_t *)user_data;

  // Parse json_chunk to get the JSON object
  json_error_t jerr;
  json_t *chunk_json = json_loads(json_chunk, 0, &jerr);
  if (!chunk_json) {
    ik_log_error("Failed to parse OpenAI chunk: %s", jerr.text);
    return;  // Skip this chunk, continue processing stream
  }

  // Create assistant_response message
  TALLOC_CTX *tmp = talloc_new(NULL);
  ik_result_t res = ik_protocol_msg_create_assistant_resp(tmp,
                                                                   task->sess_id,
                                                                   task->corr_id,
                                                                   chunk_json);
  if (ik_is_err(&res)) {
    ik_log_error("Failed to create assistant_response: %s", ik_error_message(res.err));
    json_decref(chunk_json);
    talloc_free(tmp);
    return;  // Skip this chunk
  }

  ik_protocol_msg_t *msg = res.ok;
  res = ik_protocol_msg_serialize(tmp, msg);
  if (ik_is_err(&res)) {
    ik_log_error("Failed to serialize assistant_response: %s", ik_error_message(res.err));
    json_decref(chunk_json);
    talloc_free(tmp);
    return;
  }

  char *json_str = res.ok;

  // Send via WebSocket (from worker thread - libulfius handles thread safety)
  int ret = ulfius_websocket_send_message(task->websocket,
                                          U_WEBSOCKET_OPCODE_TEXT,
                                          strlen(json_str),
                                          json_str);

  if (ret != U_OK) {
    ik_log_warn("Failed to send message to client (may have disconnected)");
  }

  json_decref(chunk_json);
  talloc_free(tmp);
}
```

**Key points:**
- `sess_id` and `corr_id` owned by task (copied during task creation)
- Each chunk is wrapped in message envelope before sending
- Send errors are ignored (client may disconnect during streaming)
- Temporary talloc context for message construction, freed after send
- **Worker thread sends directly** using task's websocket handle (libulfius handles locking)

### Error Handling

**Protocol Errors (server-side):**

When errors occur during message processing, send error message and close connection:

```c
// Example: sess_id mismatch
if (strcmp(msg->sess_id, conn->sess_id) != 0) {
  TALLOC_CTX *tmp = talloc_new(NULL);
  ik_result_t res = ik_protocol_msg_create_err(tmp,
                                                      conn->sess_id,
                                                      conn->corr_id,
                                                      "protocol",
                                                      "Session ID mismatch");
  if (ik_is_ok(&res)) {
    ik_protocol_msg_t *err_msg = res.ok;
    ik_result_t ser_res = ik_protocol_msg_serialize(tmp, err_msg);
    if (ik_is_ok(&ser_res)) {
      char *json_str = ser_res.ok;
      ulfius_websocket_send_message(conn->websocket,
                                    U_WEBSOCKET_OPCODE_TEXT,
                                    strlen(json_str),
                                    json_str);
    }
  }
  talloc_free(tmp);
  ulfius_websocket_send_message(conn->websocket, U_WEBSOCKET_OPCODE_CLOSE, 0, NULL);
  return;
}
```

**Error categories:**
- `source: "protocol"` - Invalid envelope, session mismatch, unknown message type
- `source: "server"` - Internal server errors (OOM, etc.)
- `source: "openai"` - Errors from `openai_stream_request()` (network, auth, etc.)

**After sending error:** Always close the WebSocket connection.

**OpenAI Errors:**

If `ik_openai_stream_req()` returns an error:

```c
ik_result_t res = ik_openai_stream_req(...);
if (ik_is_err(&res)) {
  // Extract error message
  const char *err_msg = ik_error_message(res.err);

  // Send error to client
  TALLOC_CTX *tmp = talloc_new(NULL);
  ik_result_t res = ik_protocol_msg_create_err(tmp,
                                                      conn->sess_id,
                                                      conn->corr_id,
                                                      "openai",
                                                      err_msg);
  if (ik_is_err(&res)) {
    talloc_free(tmp);
    ulfius_websocket_send_message(conn->websocket, U_WEBSOCKET_OPCODE_CLOSE, 0, NULL);
    return;
  }
  ik_protocol_msg_t *error = res.ok;

  res = ik_protocol_msg_serialize(tmp, error);
  if (ik_is_err(&res)) {
    talloc_free(tmp);
    ulfius_websocket_send_message(conn->websocket, U_WEBSOCKET_OPCODE_CLOSE, 0, NULL);
    return;
  }
  char *json_str = res.ok;
  ulfius_websocket_send_message(conn->websocket,
                                U_WEBSOCKET_OPCODE_TEXT,
                                strlen(json_str),
                                json_str);
  talloc_free(tmp);

  // Close connection
  ulfius_websocket_send_message(conn->websocket, U_WEBSOCKET_OPCODE_CLOSE, 0, NULL);
  return;
}
```

### Connection Cleanup

**Normal disconnect:**
- Client closes WebSocket
- libulfius calls `websocket_close_callback` immediately (not blocked by worker)
- Close callback:
  1. Set `conn->abort_flag = 1` and `conn->closed = true`
  2. Call `task_queue_close(&conn->queue)` to wake worker
  3. `pthread_join(conn->worker_thread, NULL)` - wait for worker to exit
  4. Destroy queue mutexes/conds: `task_queue_destroy(&conn->queue)`
  5. Free connection context: `talloc_free(conn->ctx)`

**Disconnect during OpenAI streaming (immediate abort):**
- Client disconnects → TCP FIN received
- libulfius calls `websocket_close_callback` immediately
- Close callback sets `conn->abort_flag = 1`
- Worker thread is running `ik_openai_stream_req()`, which uses curl multi loop
- curl multi loop checks `abort_flag` each poll iteration
- When abort detected, curl request is aborted via `curl_multi_remove_handle()`
- Worker thread returns from `ik_openai_stream_req()` with `OK(NULL)` (abort is not error)
- Worker exits its event loop (sees `conn->closed == true`)
- Close callback's `pthread_join()` returns
- Connection context freed

**Benefit:** OpenAI request stops promptly on disconnect. No wasted API calls or bandwidth.

### Correlation ID Flow

**Complete flow diagram:**

```
1. Client sends user_query (no corr_id field - server generates it)
   ↓
2. websocket_message_callback receives message
   ↓
3. Create task, generate unique corr_id for this task:
   task->corr_id = ik_protocol_generate_uuid(task_ctx)
   ↓
4. Enqueue task to worker (task owns corr_id)
   ↓
5. Worker dequeues task, calls ik_openai_stream_req(..., openai_stream_callback, task)
   ↓
6. OpenAI sends chunk → sse_write_callback → openai_stream_callback(chunk, task)
   ↓
7. openai_stream_callback accesses task fields:
   - task->sess_id → "VQ6EAOKbQdSnFkRmVUQAAA" (borrowed from conn)
   - task->corr_id → "8fKm3pLxTdOqZ1YnHjW9Gg" (owned by task)
   ↓
8. Build assistant_response envelope with sess_id + corr_id
   ↓
9. Send to client via task->websocket
   ↓
10. Repeat steps 6-9 for each chunk until [DONE]
   ↓
11. Worker frees task: talloc_free(task->ctx) (frees corr_id and all task data)
```

**Key insight:** `corr_id` is task-scoped, not connection-scoped. Each task generates its own unique corr_id, uses it for all chunks in that exchange, then frees it when the task completes. Connection's sess_id is borrowed by all tasks and lives for the entire connection lifetime.

### libulfius Integration

**Registration (in server_main.c):**

```c
// Add WebSocket endpoint to ulfius instance
ulfius_add_endpoint_by_val(&instance, "GET", "/ws", NULL, 0,
                           &websocket_manager_callback,
                           (void *)config);  // Pass config as user_data
```

**Manager callback:**

```c
void ik_handler_websocket_manager(const struct _u_request *request,
                                   struct _u_websocket_manager *websocket_manager,
                                   void *user_data) {
  ik_cfg_t *config = (ik_cfg_t *)user_data;

  // Allocate connection state
  TALLOC_CTX *conn_ctx = talloc_new(NULL);
  ik_handler_ws_conn_t *conn = talloc_zero(conn_ctx, ik_handler_ws_conn_t);
  conn->ctx = conn_ctx;
  conn->cfg_ref = config;  // Borrowed
  conn->handshake_complete = false;
  conn->abort_flag = 0;
  conn->closed = false;

  // Initialize task queue
  task_queue_init(&conn->queue);

  // Spawn worker thread
  if (pthread_create(&conn->worker_thread, NULL, worker_thread_fn, conn) != 0) {
    ik_log_error("Failed to create worker thread");
    talloc_free(conn_ctx);
    // Return error response
    return;
  }

  // Set up WebSocket callbacks
  ulfius_set_websocket_response(response, NULL, NULL,
                               &websocket_message_callback,
                               conn,
                               &websocket_close_callback,
                               conn);
}
```

**Message callback:**

```c
void ik_handler_websocket_message_callback(struct _u_websocket_manager *websocket_manager,
                                           const struct _u_websocket_message *message,
                                           void *user_data) {
  ik_handler_ws_conn_t *conn = (ik_handler_ws_conn_t *)user_data;

  if (message->opcode != U_WEBSOCKET_OPCODE_TEXT) {
    return;  // Ignore non-text messages
  }

  // Parse message
  TALLOC_CTX *tmp = talloc_new(NULL);
  const char *msg_str = (const char *)message->data;

  if (!conn->handshake_complete) {
    // Handle hello message
    // ...
  } else {
    // Handle user_query
    // ...
  }

  talloc_free(tmp);
}
```

**Close callback:**

```c
void ik_handler_websocket_close_callback(struct _u_websocket_manager *websocket_manager,
                                         void *user_data) {
  ik_handler_ws_conn_t *conn = (ik_handler_ws_conn_t *)user_data;

  // Signal worker to abort and exit
  conn->abort_flag = 1;
  conn->closed = true;
  task_queue_close(&conn->queue);  // Wake up worker if blocked

  // Wait for worker thread to exit
  pthread_join(conn->worker_thread, NULL);

  // Clean up queue resources
  task_queue_destroy(&conn->queue);

  // Clean up connection state
  talloc_free(conn->ctx);  // Frees conn, sess_id, everything
}
```

### Memory Management

**Connection vs Task Scoping:**
- **Connection-scoped data** (lives for entire WebSocket connection):
  - `sess_id` - identifies the connection, allocated on `conn->ctx`
  - `websocket` - libulfius handle, borrowed
  - `abort_flag` - shared by all tasks on this connection
  - `cfg_ref` - server config, borrowed from main

- **Task-scoped data** (lives for one request-response exchange):
  - `corr_id` - unique per request, allocated on `task->ctx`
  - `type` - message type, allocated on `task->ctx`
  - `payload` - request JSON, owned by task via incref
  - All task data freed with `talloc_free(task->ctx)` when request completes

**Connection lifetime:**
- Created: `talloc_new(NULL)` in `websocket_manager_callback`
- Used: Throughout connection lifetime
- Freed: `talloc_free(conn->ctx)` in `websocket_close_callback`

**Task lifetime:**
- Created: `talloc_new(NULL)` when user_query received
- Used: During OpenAI request processing and response streaming
- Freed: `talloc_free(task->ctx)` by worker when request completes

**Borrowed pointers (task → connection):**
- `task->sess_id` - points to `conn->sess_id` (safe: conn outlives task via pthread_join)
- `task->websocket` - points to `conn->websocket` (safe: libulfius handle valid until close)
- `task->abort_flag` - points to `&conn->abort_flag` (safe: conn outlives task)
- `task->cfg_ref` - points to `conn->cfg_ref` (safe: config lives for entire server lifetime)

**Thread safety:**
- Tasks don't modify connection state (only read borrowed pointers)
- Each task has independent context (task->ctx) owned by worker thread
- Queue operations synchronized with mutex/cond
- No completion signaling needed - fire and forget pattern

### Test Coverage

`tests/unit/handler_test.c`:
- Handshake: hello → welcome exchange
- Session ID generation and validation
- Reject messages before handshake
- User query parsing and correlation ID generation
- Session ID mismatch error
- Unknown message type error
- Invalid JSON error
- Connection cleanup (verify talloc cleanup, pthread_join completes)
- Task queue operations (push, pop, close)
- Worker thread spawning and cleanup
- Abort flag handling during streaming

`tests/integration/websocket_openai_test.c`:
- Full flow: connect → handshake → query → stream response → disconnect
- Multiple queries on same connection (correlation IDs are different)
- Client disconnect during streaming (verify prompt abort)
- Worker thread processes task and signals completion correctly
- OpenAI error propagation (auth error, network error)
- Verify no resource leaks (all threads joined, all mutexes destroyed)

---

## HTTPD Module (`httpd.c/h`)

Manages the ulfius HTTP/WebSocket server lifecycle. Low-level network listener that initializes the server, registers routes, handles signals for graceful shutdown, and runs the event loop.

### API

```c
// Main server function - called from server.c
ik_result_t ik_httpd_run(TALLOC_CTX *ctx, ik_cfg_t *config);
```

**Parameters:**
- `ctx` - talloc context for error allocation only
- `config` - server configuration (borrowed, owned by caller)

**Returns:**
- `OK(NULL)` - server shut down cleanly
- `ERR(..., NETWORK, ...)` - failed to bind to port or start server
- `ERR(..., INVALID_ARG, ...)` - NULL config parameter

**Behavior:**
- Validates parameters
- Initializes ulfius instance (stack-allocated)
- Registers WebSocket endpoint (`/ws`) with handler module callback
- Sets up signal handlers (SIGINT/SIGTERM)
- Starts listening on configured address/port
- Blocks in event loop until shutdown signal received
- Aborts in-flight requests immediately on shutdown
- Cleans up ulfius instance and returns

### Implementation Flow

```c
ik_result_t ik_httpd_run(TALLOC_CTX *ctx, ik_cfg_t *config) {
  // 1. Validate parameters
  CHECK(ik_check_null(ctx, config, "config"));

  // 2. Initialize ulfius instance (stack-allocated)
  struct _u_instance instance;
  if (ulfius_init_instance(&instance, config->listen_port, NULL, NULL) != U_OK) {
    return ERR(ctx, NETWORK, "Failed to initialize ulfius instance");
  }

  // 3. Register WebSocket endpoint with handler module callback
  if (ulfius_add_endpoint_by_val(&instance, "GET", "/ws", NULL, 0,
                                 &ik_handler_websocket_manager,
                                 (void *)config) != U_OK) {
    ulfius_clean_instance(&instance);
    return ERR(ctx, NETWORK, "Failed to register WebSocket endpoint");
  }

  // 4. Set up signal handlers for graceful shutdown
  signal(SIGINT, httpd_signal_handler);
  signal(SIGTERM, httpd_signal_handler);

  // 5. Start server
  ik_log_info("Starting server on %s:%d", config->listen_address, config->listen_port);
  if (ulfius_start_framework(&instance) != U_OK) {
    ulfius_clean_instance(&instance);
    return ERR(ctx, NETWORK, "Failed to start server on %s:%d",
               config->listen_address, config->listen_port);
  }

  ik_log_info("Server running. Press Ctrl+C to stop.");

  // 6. Wait for shutdown signal (poll frequently to meet < 200ms requirement)
  while (!g_httpd_shutdown) {
    usleep(50000);  // 50ms = 50,000 microseconds
  }

  // 7. Graceful shutdown
  ik_log_info("Shutting down server...");
  ulfius_stop_framework(&instance);
  ulfius_clean_instance(&instance);

  return OK(NULL);
}
```

### Signal Handling

**Global shutdown flag:**
```c
static volatile sig_atomic_t g_httpd_shutdown = 0;

static void httpd_signal_handler(int signum) {
  (void)signum;
  g_httpd_shutdown = 1;
}
```

**Registered signals:**
- `SIGINT` (Ctrl+C in terminal)
- `SIGTERM` (kill command, systemd stop)

**Polling interval:**
- Main loop polls shutdown flag every 50ms (`usleep(50000)`)
- Ensures shutdown completes within 200ms of signal

**Shutdown sequence:**
1. Signal handler sets `g_httpd_shutdown = 1`
2. Main loop detects flag and exits
3. `ulfius_stop_framework()` stops accepting new connections
4. Existing WebSocket connections are closed immediately
5. In-flight OpenAI requests are aborted (worker threads check abort flag)
6. `ulfius_clean_instance()` frees ulfius resources
7. Function returns to caller (server.c)

**Immediate abort requirement:** Any in-flight OpenAI requests must be aborted immediately on shutdown. Handler module must check `g_httpd_shutdown` flag during OpenAI streaming and cancel requests. This requires using curl multi interface (not `curl_easy_perform()`) - see OpenAI Client module updates.

### libulfius Configuration

**Instance initialization:**
```c
ulfius_init_instance(&instance, config->listen_port, NULL, NULL);
```
- Port from config
- No default base URL (NULL)
- No default callback (NULL)

**Endpoint registration:**
```c
ulfius_add_endpoint_by_val(&instance,
                           "GET",                         // HTTP method (WebSocket upgrade uses GET)
                           "/ws",                         // URL path
                           NULL,                          // No additional prefix
                           0,                             // Priority (default)
                           &ik_handler_websocket_manager, // Callback from handler module
                           (void *)config);               // user_data passed to callback
```

**Dependency on handler module:**
- httpd.c includes handler.h to get `ik_handler_websocket_manager` declaration
- One-way dependency: httpd → handler
- Config is passed as user_data and will be available to handler callbacks

**Starting the server:**
```c
ulfius_start_framework(&instance);
```
- Binds to configured address and port
- Starts accepting connections
- Runs in background threads (non-blocking)

**Stopping the server:**
```c
ulfius_stop_framework(&instance);   // Stop accepting connections
ulfius_clean_instance(&instance);   // Free resources
```

### Error Handling

**Initialization failures:**
- `ulfius_init_instance` fails → return `IK_ERR_NETWORK`
- `ulfius_add_endpoint_by_val` fails → clean up instance, return `IK_ERR_NETWORK`
- `ulfius_start_framework` fails → clean up instance, return `IK_ERR_NETWORK`

**Common failure reasons:**
- Port already in use → "Failed to start server on 127.0.0.1:1984"
- Invalid address → "Failed to initialize ulfius instance"
- Permission denied (privileged port) → "Failed to start server"

**No recovery attempts in Phase 1** - if server fails to start, return error to caller (server.c). Caller logs error and exits.

### Memory Management

**ulfius instance:**
- Stack-allocated: `struct _u_instance instance;`
- libulfius manages its own heap allocations internally
- Cleanup via `ulfius_clean_instance()` before function returns

**Config:**
- Borrowed pointer (owned by caller in server.c)
- Must remain valid for entire duration of `ik_httpd_run()` call
- Passed to handler callbacks via ulfius user_data
- Caller cleans up after `ik_httpd_run()` returns

**Error results:**
- Only allocations made by this module
- Allocated on `ctx` parameter when returning errors

### Logging

**All output via logger module:**
```c
ik_log_info("Starting server on %s:%d", config->listen_address, config->listen_port);
ik_log_info("Server running. Press Ctrl+C to stop.");
ik_log_info("Shutting down server...");
```

**Error handling:**
- Errors returned as `ik_result_t`
- Caller (server.c) logs errors using `ik_error_message()` and logger module
- httpd module does NOT log its own errors

### Accessing Shutdown Flag from Other Modules

**Global flag visibility:**

Handler module needs access to `g_httpd_shutdown` to abort in-flight OpenAI requests. Options:

1. **Export via header** (recommended):
```c
// httpd.h
extern volatile sig_atomic_t g_httpd_shutdown;
```

Handler can then check:
```c
if (g_httpd_shutdown) {
    // Abort OpenAI request
}
```

2. **Provide accessor function**:
```c
// httpd.h
bool ik_httpd_is_shutdown_requested(void);

// httpd.c
bool ik_httpd_is_shutdown_requested(void) {
    return g_httpd_shutdown != 0;
}
```

**Recommendation:** Export the flag directly (option 1) for minimal overhead during tight polling loops.

### Test Coverage

`tests/unit/httpd_test.c`:
- Successful initialization and startup
- Failed to bind (port in use)
- Invalid configuration (NULL config)
- Graceful shutdown (send SIGINT to test process)
- Shutdown flag polling (verify meets 200ms requirement)

`tests/integration/httpd_lifecycle_test.c`:
- Start server → connect client → handshake → shutdown server
- Verify WebSocket endpoint is accessible (`/ws`)
- Verify server stops cleanly on signal (SIGINT, SIGTERM)
- Verify in-flight OpenAI requests are aborted on shutdown

**Note:** Integration tests may need to use dynamic port allocation to avoid conflicts.

### Dependencies

**Required headers:**
```c
#include <talloc.h>
#include <ulfius.h>
#include <signal.h>
#include <unistd.h>  // For usleep()
#include "config.h"
#include "handler.h"  // For ik_handler_websocket_manager
#include "logger.h"   // For ik_log_*
#include "error.h"
```

**Linker flags:**
- Already covered by existing SERVER_LIBS: `-lulfius`

---

## Logger Module (`logger.c/h`)

Provides simple logging to stdout/stderr following systemd conventions. Thread-safe with atomic log line writes. No configuration, no file output.

### API

```c
void ik_log_debug(const char *fmt, ...);
void ik_log_info(const char *fmt, ...);
void ik_log_warn(const char *fmt, ...);
void ik_log_error(const char *fmt, ...);
void ik_log_fatal(const char *fmt, ...) __attribute__((noreturn));
```

### Behavior

**Output routing (systemd conventions):**
- `ik_log_debug`, `ik_log_info`, `ik_log_warn` → stdout (systemd treats as info level)
- `ik_log_error`, `ik_log_fatal` → stderr (systemd treats as error level)

**Format:**
```
<LEVEL>: message\n
```

Examples:
```
INFO: Starting server on 127.0.0.1:1984
ERROR: Failed to connect to OpenAI API: connection timeout
FATAL: Out of memory allocating connection context
```

**No timestamps** - systemd-journald automatically adds timestamps to all logged messages

**Fatal behavior:**
```c
void ik_log_fatal(const char *fmt, ...) {
  // Print to stderr with "FATAL: " prefix
  // Call abort() to generate core dump and trigger debugger
  // Never returns
}
```

Use `ik_log_fatal()` for "should never happen" conditions (assertion-like failures, critical invariant violations). For expected runtime errors (config missing, port in use), use `ik_log_error()` and handle gracefully.

### Implementation Notes

- Uses `vfprintf()` for printf-style formatting
- **Thread-safe**: Uses `pthread_mutex_t` to ensure atomic log line writes (no message interleaving between threads)
- Global mutex protects entire log operation (prefix + formatted message + newline + flush)
- No talloc dependency (uses stack and libc only)
- No configuration or runtime state
- `__attribute__((noreturn))` on `ik_log_fatal` helps compiler optimize

### Thread Safety

Each log function call is atomic - the entire message (prefix + content + newline) is written without interleaving from other threads:

```c
static pthread_mutex_t ik_log_mutex = PTHREAD_MUTEX_INITIALIZER;

void ik_log_info(const char *fmt, ...) {
    pthread_mutex_lock(&ik_log_mutex);
    fprintf(stdout, "INFO: ");
    va_list args;
    va_start(args, fmt);
    vfprintf(stdout, fmt, args);
    va_end(args);
    fprintf(stdout, "\n");
    fflush(stdout);
    pthread_mutex_unlock(&ik_log_mutex);
}
```

**Rationale:** The server uses multiple threads (WebSocket threads, worker threads). Without synchronization, log messages from different threads could interleave, producing garbled output. A single global mutex ensures clean, readable logs.

### Example Usage

```c
ik_log_info("Processing user_query session=%s correlation=%s", sess_id, corr_id);
ik_log_debug("Calling OpenAI API for correlation=%s", corr_id);
ik_log_warn("Failed to send message to client (may have disconnected)");
ik_log_error("OpenAI API returned 401: %s", error_message);
ik_log_fatal("Unexpected null pointer in critical path");  // abort()
```

### Test Coverage

`tests/unit/logger_test.c`:
- Verify debug/info/warn go to stdout
- Verify error/fatal go to stderr
- Verify format includes level prefix
- Verify printf-style formatting works
- Verify fatal calls abort() (use subprocess or signal catching)
- **Thread safety**: Verify no message interleaving with concurrent logging from multiple threads

---

## Server Entry Point (`server.c`)

Main entry point for Phase 1. Coordinates module initialization, runs the server, and handles top-level cleanup.
