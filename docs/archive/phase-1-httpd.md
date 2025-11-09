# HTTPD Module (`httpd.c/h`)

[← Back to Phase 1 Details](phase-1-details.md)

Manages the ulfius HTTP/WebSocket server lifecycle. Low-level network listener that initializes the server, registers routes, handles signals for graceful shutdown, and runs the event loop.

## API

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

## Implementation Flow

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

## Signal Handling

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

## libulfius Configuration

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

## Error Handling

**Initialization failures:**
- `ulfius_init_instance` fails → return `IK_ERR_NETWORK`
- `ulfius_add_endpoint_by_val` fails → clean up instance, return `IK_ERR_NETWORK`
- `ulfius_start_framework` fails → clean up instance, return `IK_ERR_NETWORK`

**Common failure reasons:**
- Port already in use → "Failed to start server on 127.0.0.1:1984"
- Invalid address → "Failed to initialize ulfius instance"
- Permission denied (privileged port) → "Failed to start server"

**No recovery attempts in Phase 1** - if server fails to start, return error to caller (server.c). Caller logs error and exits.

## Memory Management

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

## Logging

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

## Accessing Shutdown Flag from Other Modules

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

## Test Coverage

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

## Dependencies

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
