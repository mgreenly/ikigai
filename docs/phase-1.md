# Phase 1 Implementation

## Overview

**Phase 1 is the architectural foundation** - it implements core patterns that all subsequent phases depend on. This is not a minimum viable product or a shippable feature. It's building the frame and foundation before adding the house.

**What we're building**: WebSocket server with streaming LLM proxy (OpenAI gpt-4o-mini) that establishes production-quality concurrency patterns, memory management, and error handling.

**Key architectural decisions implemented in Phase 1**:
- Worker threads with abort semantics (enables graceful shutdown, needed for DB/tools later)
- talloc hierarchical memory management (prevents entire classes of memory bugs)
- Result type error handling (systematic error propagation)
- libulfius HTTP/WebSocket patterns (connection lifecycle, shutdown coordination)

**Why these can't be added later**: You can't retrofit concurrency or memory management patterns into an existing codebase without rewriting everything. Get the foundation right in Phase 1, then build features on top.

**Scope**: No message history, no tools, no storage (those come in later phases). We implement just enough functionality to prove the architectural patterns work.

**Multi-client**: libulfius handles concurrent connections. Each WebSocket connection gets isolated state (session_id, talloc context).

**Architecture validation**: ✅ **COMPLETE** - The fire-and-forget pattern with worker threads has been empirically verified safe (see phase-1-details.md "Fire-and-Forget Pattern Safety"). Testing confirms no deadlocks, no memory leaks, and clean shutdown in all scenarios. libulfius is the right choice. Proceed with implementation as designed.

## Module Structure

### `src/error.c/h` - Error handling ✅ COMPLETE
Result type for systematic error propagation with talloc integration.

```c
typedef struct {
  void *ok;
  ik_error_t *err;
} ik_result_t;

#define OK(val) ((ik_result_t){.ok = (val), .err = NULL})
#define ERR(ctx, code, ...) ((ik_result_t){.ok = NULL, .err = ik_error_create(ctx, code, __VA_ARGS__)})
```

### `src/logger.c/h` - Logging ✅ COMPLETE
Thread-safe logging with mutex protection for atomic log line writes.

```c
void ik_log_info(const char *fmt, ...)
void ik_log_warn(const char *fmt, ...)
void ik_log_error(const char *fmt, ...)
void ik_log_fatal(const char *fmt, ...) __attribute__((noreturn))
```

### `src/config.c/h` - Configuration management ✅ COMPLETE
Loads and validates server configuration from `~/.ikigai/config.json`.

```c
ik_result_t ik_cfg_load(TALLOC_CTX *ctx, const char *path)
```

**Config structure:**
```c
typedef struct {
  char *openai_api_key;
  char *listen_address;
  int listen_port;
} ik_cfg_t;
```

All memory is talloc-managed. Caller frees with `talloc_free(ctx)`.

### `src/wrapper.c/h` - External library wrappers ✅ COMPLETE
Thread-safe wrappers for external library calls with test seams for OOM injection. Uses weak symbols in debug builds, inlined in release builds for zero overhead.

```c
MOCKABLE void *ik_talloc_zero_wrapper(TALLOC_CTX *ctx, size_t size);
MOCKABLE char *ik_talloc_strdup_wrapper(TALLOC_CTX *ctx, const char *str);
MOCKABLE json_t *ik_json_object_wrapper(void);
MOCKABLE int ik_json_is_string_wrapper(const json_t *json);
```

### `src/protocol.c/h` - Message parsing and serialization ✅ COMPLETE
Handles WebSocket message envelope and JSON serialization. UUID generation for session/correlation IDs. Includes talloc destructors for automatic JSON cleanup.

```c
typedef struct {
  char *sess_id;
  char *corr_id;
  char *type;
  json_t *payload;
} ik_protocol_msg_t;

ik_result_t ik_protocol_msg_parse(TALLOC_CTX *ctx, const char *json_str)
ik_result_t ik_protocol_msg_serialize(TALLOC_CTX *ctx, ik_protocol_msg_t *msg)
ik_result_t ik_protocol_generate_uuid(TALLOC_CTX *ctx)
ik_result_t ik_protocol_msg_create_err(TALLOC_CTX *ctx,
                                        const char *sess_id,
                                        const char *corr_id,
                                        const char *source,
                                        const char *err_msg)
ik_result_t ik_protocol_msg_create_assistant_resp(TALLOC_CTX *ctx,
                                                   const char *sess_id,
                                                   const char *corr_id,
                                                   json_t *payload)
```

Handshake messages (`hello`/`welcome`) are parsed inline in handler module.

Memory management uses talloc destructors to automatically decrement JSON reference counts when protocol messages are freed.

### `src/server.c` - Entry point ⚠️ STUB
Main function that coordinates startup and shutdown. Currently a hello-world stub.

```c
int main(int argc, char *argv[])
  - config_load()
  - httpd_run(config)
  - cleanup
```

### `src/openai.c/h` - OpenAI API client ❌ NOT IMPLEMENTED
HTTP client for OpenAI Chat Completions API with streaming SSE support. Uses libcurl multi interface for immediate shutdown.

```c
typedef void (*ik_openai_stream_cb_t)(const char *json_chunk, void *user_data);

ik_result_t ik_openai_stream_req(TALLOC_CTX *ctx,
                                  const ik_cfg_t *cfg,
                                  json_t *req_payload,
                                  ik_openai_stream_cb_t cb,
                                  void *cb_data,
                                  volatile sig_atomic_t *shutdown_flag)
```

**Note**: The callback receives `cb_data` which is typically `ik_task_t*` from the handler module. The task contains sess_id (borrowed from connection), corr_id (owned by task), and websocket handle for sending responses.

Parses Server-Sent Events (SSE) format, validates JSON chunks, calls callback for each complete chunk.

### `src/handler.c/h` - WebSocket connection handling ❌ NOT IMPLEMENTED
Manages WebSocket lifecycle and routes messages to worker thread for processing. Handles handshake protocol and message dispatch.

```c
typedef struct {
  TALLOC_CTX *ctx;
  struct _u_websocket *websocket;
  char *sess_id;
  ik_cfg_t *cfg_ref;
  bool handshake_complete;

  // Worker thread for processing requests
  pthread_t worker_thread;
  ik_task_queue_t queue;
  volatile sig_atomic_t abort_flag;
  bool closed;
} ik_handler_ws_conn_t;

typedef struct {
  TALLOC_CTX *ctx;                    // Task's talloc context for allocations
  char *type;                         // Task type: "user_query", etc.
  char *sess_id;                      // Borrowed from connection (connection outlives task)
  char *corr_id;                      // Owned by task, unique per request
  json_t *payload;                    // Request payload (owned by task, incref'd)
  struct _u_websocket *websocket;     // Borrowed from connection
  volatile sig_atomic_t *abort_flag;  // Points to conn->abort_flag
  ik_cfg_t *cfg_ref;                  // Borrowed from connection
} ik_task_t;

void ik_handler_websocket_manager(const struct _u_request *req,
                                   struct _u_websocket_manager *websocket_manager,
                                   void *user_data)
```

**Threading model**: Each WebSocket connection has a dedicated worker thread. WebSocket thread handles protocol (framing, handshake), enqueues tasks, and waits for completion. Worker thread processes tasks (OpenAI requests, etc.) and can be immediately aborted on disconnect/shutdown.

### `src/httpd.c/h` - HTTP/WebSocket server ❌ NOT IMPLEMENTED
Manages libulfius server lifecycle, signal handling, and graceful shutdown.

```c
ik_result_t ik_httpd_run(TALLOC_CTX *ctx, ik_cfg_t *cfg)

extern volatile sig_atomic_t g_httpd_shutdown;
```

Registers WebSocket endpoint (`/ws`), handles SIGINT/SIGTERM for graceful shutdown with < 200ms response time.

## Library Dependencies

### Required libraries (Debian packages)
- `libulfius-dev` - HTTP/WebSocket server framework
- `libjansson-dev` - JSON parsing and serialization
- `libcurl4-gnutls-dev` - HTTP client for OpenAI API
- `libtalloc-dev` - Hierarchical memory management
- `libuuid-dev` - RFC 4122 UUID generation
- `libb64-dev` - Base64 encoding for UUIDs
- `pthread` - POSIX threads (part of glibc)

### Linker flags
```makefile
SERVER_LIBS = -lulfius -ljansson -lcurl -ltalloc -luuid -lb64 -lpthread
```

### Test dependencies
- `check` - Unit testing framework
- Additional flags: `-lcheck -lm -lsubunit`

## Test Files

### Unit Tests (`tests/unit/`)
- `error_test.c` ✅ - Error creation, codes, messages, OOM handling
- `logger_test.c` ✅ - Log output routing, formatting, and thread safety
- `config_test.c` ✅ - Config file loading, validation, tilde expansion, defaults
- `protocol_test.c` ✅ - Message parsing/serialization, UUID generation, JSON lifecycle
- `openai_test.c` ❌ - OpenAI API streaming, SSE parsing, shutdown abort
- `handler_test.c` ❌ - WebSocket handshake, message routing, connection lifecycle
- `httpd_test.c` ❌ - Server initialization, signal handling, shutdown

### Integration Tests (`tests/integration/`)
- `config_integration_test.c` ✅ - Config creation and loading full flow
- `logger_integration_test.c` ✅ - Multi-level logging scenarios
- `oom_integration_test.c` ✅ - Out-of-memory error handling patterns
- `protocol_integration_test.c` ✅ - Full message flow: create → serialize → parse
- `websocket_openai_test.c` ❌ - Full flow: connect → handshake → query → stream → disconnect
- `httpd_lifecycle_test.c` ❌ - Server startup, client connections, graceful shutdown

### Test Utilities
- `tests/test_utils.c/h` ✅ - OOM injection helpers with strong symbol overrides

## Implementation Order

1. **Error module** (`error.c/h`) ✅ COMPLETE
   - Depends on: talloc
   - Result type and error handling
   - Foundation for all error propagation

2. **Logger module** (`logger.c/h`) ✅ COMPLETE
   - Depends on: pthread (for thread-safe mutex)
   - Used by all other modules
   - Foundation for observability

3. **Wrapper module** (`wrapper.c/h`) ✅ COMPLETE
   - Depends on: talloc, jansson
   - Test seams for OOM injection
   - Weak symbols in debug, inlined in release

4. **Config module** (`config.c/h`) ✅ COMPLETE
   - Depends on: error, logger, wrapper
   - Foundation for API credentials
   - Auto-creates config with defaults

5. **Protocol module** (`protocol.c/h`) ✅ COMPLETE
   - Depends on: error, logger, wrapper
   - Message types and serialization
   - UUID generation for session/correlation IDs
   - talloc destructors for JSON cleanup

6. **OpenAI client** (`openai.c/h`) ⏳ NEXT
   - Depends on: config, error, logger, wrapper
   - Test independently with real API calls
   - Handle streaming SSE response parsing
   - Implements curl multi for shutdown support

7. **Handler module** (`handler.c/h`) ❌ PLANNED
   - Depends on: protocol, openai, error, logger
   - WebSocket connection lifecycle
   - Handshake logic
   - Message routing between client and OpenAI

8. **HTTPD module** (`httpd.c/h`) ❌ PLANNED
   - Depends on: config, handler, error, logger
   - Initialize libulfius
   - Register WebSocket endpoint
   - Signal handling and graceful shutdown

9. **Server entry point** (`server.c`) ❌ STUB
   - Depends on: config, httpd, error, logger
   - Main entry point
   - Top-level error handling and cleanup

## Configuration File

Server loads `~/.ikigai/config.json`. If the file doesn't exist, it's auto-created with default values:

```json
{
  "openai_api_key": "YOUR_API_KEY_HERE",
  "listen_address": "127.0.0.1",
  "listen_port": 1984
}
```

**Config behavior:**
- Auto-creates `~/.ikigai/` directory if missing (mode 0755)
- Auto-creates config file with defaults if missing
- Tilde expansion using `getenv("HOME")`
- Validates JSON structure and required fields
- Port range validation (1024-65535, non-privileged only)
- **Does NOT validate API key content** - server starts successfully, OpenAI returns auth error on first request

## Notes

### Development Approach
- **Build order**: logger → config → protocol → openai → handler → httpd → server
- **TDD red/green cycle**: Write failing test → implement code to pass test → verify with `make check` and `make lint`
- **Test each module** independently before integration
- **Memory management**: All data structures are talloc-managed, no explicit free functions
- **Error handling**: All functions return `ik_result_t`, use `CHECK()` macro for propagation
- **Concurrency**: Worker thread pattern established in Phase 1 for use in all subsequent phases

### Threading Model
- libulfius creates one thread per WebSocket connection (protocol handling)
- Each connection spawns a dedicated worker thread (task processing)
- WebSocket thread: envelope parsing, handshake, enqueue tasks, wait for completion
- Worker thread: process tasks (OpenAI requests, etc.), check abort flag, signal completion
- Queue coordination uses pthread mutex/condition variables
- Worker can be immediately aborted on client disconnect or server shutdown
- One connection = two threads (websocket + worker), aligned with Single-Request-Per-Connection pattern

### Testing Strategy
- Use real OpenAI API during development (unit tests)
- Capture real API responses for mocked tests later
- Integration tests verify complete flow
- OOM injection via test utilities

### Client
- Phase 1 client is a stdio stub for basic functionality verification
- Python WebSocket test client used for development and integration testing
- Full terminal UI with libvterm comes in Phase 4
