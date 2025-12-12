# Remove Legacy Logger Functions

## Target
Cleanup - remove stdout/stderr logging functions that break alternate buffer mode.

## Pre-read

### Skills
- `.agents/skills/default.md`
- `.agents/skills/tdd.md`
- `.agents/skills/scm.md`

### Docs
- `docs/error_handling.md`

### Source Patterns
- `src/logger.c` - Logger implementation
- `src/logger.h` - Logger API

### Test Patterns
- `tests/unit/logger/` - Existing logger tests

## Pre-conditions
- Working tree is clean
- `make check` passes

## Task

Remove the legacy printf-style logging functions that output to stdout/stderr. These break the terminal alternate buffer mode. Only the JSONL logging functions that write to file should remain.

**Functions to remove from logger.h and logger.c:**
- `ik_log_debug()` - writes to stdout
- `ik_log_info()` - writes to stdout
- `ik_log_warn()` - writes to stdout
- `ik_log_error()` - writes to stderr

**Functions to keep:**
- `ik_log_create()` - creates JSONL doc
- `ik_log_debug_json()` - writes to file
- `ik_log_info_json()` - writes to file
- `ik_log_warn_json()` - writes to file
- `ik_log_error_json()` - writes to file
- `ik_log_fatal_json()` - writes to file
- All logger initialization/shutdown functions

**Call sites to update or remove:**
- `src/history.c` - `ik_log_warn()` calls (3 instances)
- `src/input.c` - `ik_log_warn()` calls (3 instances)
- `src/repl.c` - `ik_log_warn()` call (1 instance)
- `src/shared.c` - `ik_log_warn()` call (1 instance)
- `src/db/replay.c` - `ik_log_error()` calls (4 instances)

For each call site, either:
1. Convert to JSONL API if the log message is important for debugging
2. Remove the call entirely if it's not needed

## TDD Cycle

### Red
1. Remove function declarations from `logger.h`
2. Build should fail with undefined reference errors at call sites

### Green
1. Update each call site - convert to JSONL or remove
2. Remove function implementations from `logger.c`
3. Update any tests that use the legacy functions
4. `make check` passes

### Refactor
1. Remove any helper functions in logger.c only used by legacy functions
2. Clean up includes in files that no longer need logger.h
3. `make check` passes

## Post-conditions
- No `ik_log_debug`, `ik_log_info`, `ik_log_warn`, `ik_log_error` functions exist
- No code writes directly to stdout/stderr for logging purposes
- All JSONL logging functions remain functional
- `make check` passes
- `make lint` passes
