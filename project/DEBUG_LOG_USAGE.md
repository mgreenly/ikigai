# Debug Log Usage

The debug log module provides a way to log diagnostic information when running in alternate buffer mode where stdout is unavailable.

## Features

- **DEBUG builds only**: All code compiles away to nothing in release builds
- **Thread-safe**: Opened in append mode (O_APPEND) for atomic writes from multiple threads
- **Automatic initialization**: Called early in `main()` before other subsystems
- **Automatic cleanup**: Uses `atexit()` to ensure file is flushed/closed on any exit path
- **Timestamped entries**: Each log entry includes timestamp, file, line, and function
- **PANIC on failure**: If log file cannot be created, program will panic (fail fast)
- **Tail-friendly**: Each write is flushed immediately so you can `tail -f IKIGAI_DEBUG.LOG`
- **Automatic I/O tracing**: Every terminal read/write is automatically logged with formatted bytes

## Automatic Terminal I/O Tracing

All terminal I/O is automatically traced in DEBUG builds. You'll see every byte read from and written to the terminal, formatted for readability:

- Escape sequences: `\e` instead of `\x1b`
- Control chars: `\r`, `\n`, `\t`
- Non-printable bytes: `\x00`, `\xff`, etc.
- Printable characters: shown as-is

Example log output showing CSI_u initialization:
```
[2026-01-06 10:30:15] src/wrapper_posix.c:109:posix_write_: WRITE fd=3 count=4: \e[?u
[2026-01-06 10:30:15] src/wrapper_posix.c:113:posix_write_: WRITE fd=3 result=4
[2026-01-06 10:30:15] src/wrapper_posix.c:127:posix_read_: READ  fd=3 count=31
[2026-01-06 10:30:15] src/wrapper_posix.c:132:posix_read_: READ  fd=3 result=7: \e[?1u
```

This allows you to see exactly what's being sent/received without interfering with the terminal output.

## Manual Logging

You can also add manual log entries anywhere in the code:

```c
#include "debug_log.h"

// Then anywhere:
DEBUG_LOG("CSI_u initialization starting");
DEBUG_LOG("Sent %d bytes: %s", bytes_written, buffer);
DEBUG_LOG("Terminal state: flags=0x%x", term->flags);
```

## Example Output

```
=== IKIGAI DEBUG LOG ===
[2026-01-06 10:30:15] src/client.c:34:main: === Session starting, PID=12345 ===
[2026-01-06 10:30:15] src/terminal.c:123:ik_term_init: CSI_u initialization starting
[2026-01-06 10:30:15] src/wrapper_posix.c:109:posix_write_: WRITE fd=3 count=4: \e[?u
[2026-01-06 10:30:15] src/wrapper_posix.c:113:posix_write_: WRITE fd=3 result=4
[2026-01-06 10:30:15] src/wrapper_posix.c:127:posix_read_: READ  fd=3 count=31
[2026-01-06 10:30:15] src/wrapper_posix.c:132:posix_read_: READ  fd=3 result=7: \e[?1u
[2026-01-06 10:30:15] src/terminal.c:156:ik_term_init: Terminal state: flags=0x0f
[2026-01-06 10:32:45] src/client.c:198:main: === Session ending normally, exit_code=0 ===
```

## Watching the Log

In another terminal:
```bash
tail -f IKIGAI_DEBUG.LOG
```

## Notes

- Log file is created in the current working directory
- File is truncated on each program start
- **Session markers**: Every run automatically logs "Session starting" and "Session ending" to verify the log is working
- **I/O tracing is automatic**: No need to add DEBUG_LOG calls for terminal I/O - it's all captured
- In release builds (`BUILD=release`), all DEBUG_LOG calls compile to no-ops
- The file is automatically added to `.gitignore`
