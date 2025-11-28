# Why Mutex-Based Thread-Safe Logger?

**Decision**: Logger uses `pthread_mutex_t` to ensure atomic log line writes. Each log call acquires a global mutex before writing and releases it after flushing output.

**Rationale**:
- **Multi-threaded server**: Phase 1 uses multiple threads (libulfius WebSocket threads + worker threads per connection)
- **Prevent message interleaving**: Without synchronization, concurrent log calls produce garbled output (e.g., "INFO: ERR"Starting serverOR: Failed to connect")
- **Atomic writes required**: Each complete message (prefix + content + newline) must be written as a unit
- **Simple and effective**: Single global mutex is straightforward to implement and reason about
- **Acceptable performance**: Logging is not on critical path; mutex contention is negligible for typical log volumes

**Alternative considered**: `flockfile()`/`funlockfile()` (POSIX stream locking). Rejected because:
- Less explicit control over critical section
- Still requires pthread library
- No significant advantage over explicit mutex

**Implementation**:
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
    fflush(stdout);  // Ensure write completes before unlock
    pthread_mutex_unlock(&ik_log_mutex);
}
```

**Dependency impact**: Logger now depends on pthread (already required for worker threads, so no new external dependency).
