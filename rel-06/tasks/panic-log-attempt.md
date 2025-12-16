# Task: Panic Handler Logger Integration

## Overview

Modify `src/panic.c` to attempt writing a "panic" event to the JSONL logger before terminating. The panic handler currently writes to `STDERR_FILENO` using async-signal-safe `write()` calls. We want to also attempt a logger write, but this is best-effort - it may fail during a corrupted state.

## Execution Constraints

**CRITICAL: Do NOT use background tools (run_in_background parameter). All tool calls must run in the foreground so output is visible.**

## Why This Matters

1. **Alternate buffer mode**: stderr output is invisible in alternate buffer mode
2. **Diagnostics**: Panic events are critical for debugging; capturing them in logs is valuable
3. **Forensics**: If the logger write succeeds, we have a structured record of the panic

## Current State

`src/panic.c` uses `write(STDERR_FILENO, ...)` at lines 118-128 (inside `ik_panic_impl`) for panic output. This is async-signal-safe but invisible to users.

Current panic output format:
```
FATAL: <message>
  at <file>:<line>
```

**Existing helpers in panic.c** (reuse these):
- `safe_strlen()` - async-signal-safe string length (lines 14-23)
- `int_to_str()` - async-signal-safe integer to string (lines 34-65)
- `write_ignore()` - write wrapper that ignores result (lines 71-75)

## Constraints

Panic handlers have strict requirements:
1. **No malloc**: The heap may be corrupted
2. **Async-signal-safe**: May be called from signal handlers
3. **Minimal dependencies**: Cannot assume anything works

The logger write is **best-effort**:
- If global logger pointer is NULL, skip
- If logger file descriptor is invalid (< 0), skip
- If write fails, continue with STDERR_FILENO fallback
- Never block or retry

## Requirements

### 1. Global Logger Access
**IMPORTANT**: The `g_panic_logger` global is set up by the `logger-lifecycle.md` task. This task assumes it already exists.

- `extern ik_logger_t *volatile g_panic_logger` declared in `panic.h`
- `ik_logger_t *volatile g_panic_logger = NULL` defined in `panic.c`
- Set by client.c after logger creation
- Cleared before logger destruction

### 2. File Descriptor Access
The logger struct is opaque (`struct ik_logger` in logger.c). To get the file descriptor:
- **Add** `int ik_logger_get_fd(ik_logger_t *logger)` function to logger.c
- **Declare** it in logger.h
- Returns `fileno(logger->file)` or -1 if logger/file is NULL

```c
// In logger.c
int ik_logger_get_fd(ik_logger_t *logger)
{
    if (logger == NULL || logger->file == NULL) {
        return -1;
    }
    return fileno(logger->file);
}
```

### 3. Panic Handler Modification
- Before existing STDERR_FILENO output, attempt logger write
- Use direct `write()` to logger's file descriptor
- Format a minimal JSON line manually (no yyjson - may allocate)
- Fall through to existing STDERR output regardless of success

### 4. Manual JSON Formatting
Since we can't use yyjson (may allocate), format JSON manually using `snprintf`:

```c
// Pre-format in a stack buffer
char buf[512];
int len = snprintf(buf, sizeof(buf),
    "{\"level\":\"fatal\",\"event\":\"panic\",\"message\":\"%s\",\"file\":\"%s\",\"line\":%d}\n",
    msg, file, line);
if (len > 0 && (size_t)len < sizeof(buf)) {
    write_ignore(logger_fd, buf, (size_t)len);
}
```

**Notes on signal safety:**
- `snprintf` is NOT async-signal-safe per POSIX
- However, PANIC() is typically called from normal code paths, not signal handlers
- For a fully async-signal-safe approach, you could hand-roll JSON using existing `safe_strlen()` and `int_to_str()` helpers, but snprintf is acceptable here

**JSON escaping limitation:**
- The message and file path are NOT escaped for JSON special characters
- If they contain `"` or `\`, the JSON will be malformed
- This is acceptable for best-effort logging - panic messages are usually string literals

## Pre-read Skills
- .agents/skills/default.md
- .agents/skills/database.md
- .agents/skills/errors.md
- .agents/skills/log.md
- .agents/skills/makefile.md
- .agents/skills/naming.md
- .agents/skills/quality.md
- .agents/skills/scm.md
- .agents/skills/source-code.md
- .agents/skills/style.md
- .agents/skills/tdd.md

## Pre-read Docs
- project/error_handling.md (PANIC patterns and usage)

## Pre-read Source
- READ: src/panic.c (current panic handler implementation)
- READ: src/panic.h (PANIC macro definition)
- READ: src/logger.h (logger structure, file descriptor access)
- READ: src/logger.c (understand logger internals)
- READ: src/client.c (where logger is created, for global pointer setup)

## Implementation Notes

### Complete Implementation Example for ik_panic_impl

```c
void ik_panic_impl(const char *msg, const char *file, int32_t line)
{
    // Restore terminal state if available (existing code)
    if (g_term_ctx_for_panic != NULL) {
        // ... existing terminal restore code ...
    }

    // Best-effort logger write BEFORE stderr output
    ik_logger_t *logger = g_panic_logger;  // Read volatile once
    if (logger != NULL) {
        int fd = ik_logger_get_fd(logger);
        if (fd >= 0) {
            char buf[512];
            int len = snprintf(buf, sizeof(buf),
                "{\"level\":\"fatal\",\"event\":\"panic\","
                "\"message\":\"%s\",\"file\":\"%s\",\"line\":%d}\n",
                msg ? msg : "", file ? file : "", line);
            if (len > 0 && (size_t)len < sizeof(buf)) {
                write_ignore(fd, buf, (size_t)len);
            }
        }
    }

    // Format line number (existing code)
    char line_buf[16];
    int32_t line_len = int_to_str(line, line_buf, sizeof(line_buf));

    // Write error message to stderr (existing code)
    write_ignore(STDERR_FILENO, "FATAL: ", 7);
    // ... rest of existing stderr output ...

    abort();
}
```

### Logger Header Addition (logger.h)
```c
// Get file descriptor for low-level writes (panic handler)
// Returns -1 if logger or file is NULL
int ik_logger_get_fd(ik_logger_t *logger);
```

### Logger Implementation Addition (logger.c)
```c
int ik_logger_get_fd(ik_logger_t *logger)
{
    if (logger == NULL || logger->file == NULL) {
        return -1;
    }
    return fileno(logger->file);
}
```

## Dependencies

**PREREQUISITE**: This task MUST be done after `logger-lifecycle.md`.

The `logger-lifecycle.md` task:
1. Declares `extern ik_logger_t *volatile g_panic_logger` in `panic.h`
2. Defines `ik_logger_t *volatile g_panic_logger = NULL` in `panic.c`
3. Sets `g_panic_logger = logger` in `client.c` after logger creation
4. Clears `g_panic_logger = NULL` in `client.c` before logger destruction

This task (panic-log-attempt.md) then USES that global to attempt logging.

## Acceptance Criteria
- [ ] `ik_logger_get_fd()` function added to logger.c and declared in logger.h
- [ ] Panic handler includes `logger.h` for access to `ik_logger_get_fd()`
- [ ] Panic handler reads `g_panic_logger` volatile pointer once into local variable
- [ ] Panic handler attempts to write JSON to logger FD before STDERR output
- [ ] Logger write is best-effort (NULL pointer or invalid FD skipped silently)
- [ ] Existing STDERR output preserved as fallback (always executes)
- [ ] No heap allocations in panic path (stack buffer only)
- [ ] All tests pass: `make check`
- [ ] Lint passes: `make lint`

## Testing Notes

This code path is inherently difficult to test:
- The file is marked `LCOV_EXCL_START` / `LCOV_EXCL_STOP` (excluded from coverage)
- PANIC() aborts the process, so unit tests cannot verify output

**Manual verification approach:**
1. After completing logger-lifecycle.md, add a temporary `PANIC("test")` call
2. Run the client and verify the panic JSON appears in `.ikigai/logs/current.log`
3. Remove the temporary PANIC call

The primary verification is:
- Code compiles without warnings
- Existing tests still pass
- Lint passes

## Agent Configuration
- Model: sonnet
- Thinking: thinking
