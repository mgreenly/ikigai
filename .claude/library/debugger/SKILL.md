---
name: debugger
description: Debugger skill for the ikigai project
---

# Debugger

## Description
Debugging strategy for ikigai - a TUI app that requires a terminal.

## Build Modes and Assertions

**Default is debug build.** This matters for understanding crashes:

| Build | `assert()` | Crash on violation |
|-------|------------|-------------------|
| `debug` (default) | Active | `SIGABRT` |
| `release` (`-DNDEBUG`) | Compiled out | Undefined (segfault, corruption) |
| `sanitize` | Active | `SIGABRT` + sanitizer report |

**When debugging:** If you see `assert(x != NULL)` and expect it might fail, the result will be `SIGABRT` (abort), NOT a segfault. Segfaults only happen in release builds where asserts are removed.

## Critical Constraint

**ikigai requires `/dev/tty`** - it cannot run directly in a subshell or pipe. Direct execution fails:
```
Error: Failed to open /dev/tty [src/terminal.c:29]
```

This means:
- `./ikigai` in a Bash tool **will fail**
- Sanitizer builds run the same binary, **will also fail**
- You cannot capture stdout/stderr directly

## Debugging Strategy

### 1. gdbserver (Preferred for Live Debugging)

Use gdbserver to debug crashes, hangs, or runtime issues:
```bash
# Terminal 1: Start under gdbserver
gdbserver :1234 ./ikigai

# Terminal 2: Connect and debug
gdb ./ikigai -ex "target remote :1234"
```

gdbserver keeps crashed processes frozen for inspection. See `gdbserver.md`.

### 2. Core Dumps (For Crash Analysis)

For crashes that happen before gdbserver initializes:
```bash
ulimit -c unlimited
./ikigai  # Will crash but produce core
gdb ./ikigai core
```

See `coredump.md`.

### 3. Unit Tests (Preferred for Most Debugging)

Unit tests **do not require /dev/tty** - they test components in isolation:
```bash
# Run all tests with sanitizers
make BUILD=sanitize check

# Run specific test
make build/tests/unit/module/test && ./build/tests/unit/module/test

# Run with Valgrind
make check-valgrind
```

**This is often the best approach** - if the bug can be reproduced in a unit test, debug there.

### 4. Valgrind/Sanitizers on Tests

Since the app can't run directly, use these tools on **unit tests**:
```bash
make check-valgrind      # Memory errors
make check-sanitize      # ASan + UBSan
make check-tsan          # Thread sanitizer
make check-helgrind      # Thread errors via Valgrind
```

See `valgrind.md` and `sanitizers.md` for interpreting output.

## Decision Tree

```
Bug to investigate?
├── Can reproduce in unit test?
│   └── YES → Run test with sanitizers/valgrind
├── Crash on startup?
│   └── Use core dump analysis
├── Runtime crash/hang?
│   └── Use gdbserver
└── Memory/thread issue?
    └── Run tests with appropriate sanitizer
```

## What NOT To Do

- Don't run `./ikigai` directly expecting output
- Don't run `make BUILD=sanitize` then try to run the binary directly
- Don't pipe ikigai output - it needs a real terminal

## References

- `gdbserver.md` - Live remote debugging
- `coredump.md` - Post-mortem analysis
- `valgrind.md` - Memory debugging
- `sanitizers.md` - ASan/UBSan/TSan interpretation
