# Logger Module (`logger.c/h`)

[← Back to Phase 1 Details](phase-1-details.md)

Provides simple logging to stdout/stderr following systemd conventions. Thread-safe with atomic log line writes. No configuration, no file output.

## API

```c
void ik_log_debug(const char *fmt, ...);
void ik_log_info(const char *fmt, ...);
void ik_log_warn(const char *fmt, ...);
void ik_log_error(const char *fmt, ...);
void ik_log_fatal(const char *fmt, ...) __attribute__((noreturn));
```

## Behavior

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

## Implementation Notes

- Uses `vfprintf()` for printf-style formatting
- **Thread-safe**: Uses `pthread_mutex_t` to ensure atomic log line writes (no message interleaving between threads)
- Global mutex protects entire log operation (prefix + formatted message + newline + flush)
- No talloc dependency (uses stack and libc only)
- No configuration or runtime state
- `__attribute__((noreturn))` on `ik_log_fatal` helps compiler optimize

## Thread Safety

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

## Example Usage

```c
ik_log_info("Processing user_query session=%s correlation=%s", sess_id, corr_id);
ik_log_debug("Calling OpenAI API for correlation=%s", corr_id);
ik_log_warn("Failed to send message to client (may have disconnected)");
ik_log_error("OpenAI API returned 401: %s", error_message);
ik_log_fatal("Unexpected null pointer in critical path");  // abort()
```

## Test Coverage

`tests/unit/logger_test.c`:
- Verify debug/info/warn go to stdout
- Verify error/fatal go to stderr
- Verify format includes level prefix
- Verify printf-style formatting works
- Verify fatal calls abort() (use subprocess or signal catching)
- **Thread safety**: Verify no message interleaving with concurrent logging from multiple threads
