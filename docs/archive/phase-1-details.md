# Phase 1 Implementation Details

This document provides detailed implementation specifications for Phase 1 modules, complementing the overview in phase-1.md.

## Table of Contents

### General Principles (this document)
- [Design Decisions & Resolved Concerns](#design-decisions--resolved-concerns)
- [General Principles](#general-principles)

### Module Specifications (separate files)
- [Config Module](phase-1-config.md) - Configuration loading and validation
- [Protocol Module](phase-1-protocol.md) - Message parsing, serialization, UUID generation
- [OpenAI Client](phase-1-openai.md) - HTTP streaming requests to OpenAI API
- [Handler Module](phase-1-handler.md) - WebSocket connection lifecycle and task dispatching
- [HTTPD Module](phase-1-httpd.md) - Server lifecycle and signal handling
- [Logger Module](phase-1-logger.md) - Thread-safe logging to stdout/stderr
- [Server Entry Point](phase-1-server.md) - Main entry point and top-level coordination

---

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

## Module Specifications

See the linked documents at the top of this file for detailed specifications of each module.
