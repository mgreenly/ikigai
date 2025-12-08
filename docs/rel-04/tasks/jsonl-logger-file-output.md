# Task: JSONL Logger File Output

## Target
Infrastructure: JSONL logging system

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/naming.md
- .agents/skills/errors.md

### Pre-read Docs
- docs/naming.md
- docs/error_handling.md

### Pre-read Source (patterns)
- src/config.c (file I/O patterns)
- src/wrapper.h (POSIX wrappers)

### Pre-read Tests (patterns)
- tests/unit/config/load_test.c (file I/O testing)

## Pre-conditions
- `make check` passes
- Task `jsonl-logger-timestamp.md` completed
- Logger writes to stdout

## Task
Change logger to write to `.ikigai/logs/current.log` relative to a working directory path. Add initialization function to set the log file path. PANIC if file operations fail.

API additions:
```c
void ik_log_init(const char *working_dir);
void ik_log_shutdown(void);
```

The logger will:
- Open `.ikigai/logs/current.log` in append mode during init
- Create parent directories if they don't exist
- Write all log entries to this file
- Close file during shutdown
- PANIC on any file operation failure

## TDD Cycle

### Red
1. Add to `src/logger.h`:
   - `void ik_log_init(const char *working_dir);`
   - `void ik_log_shutdown(void);`
2. Add test in `tests/unit/logger/jsonl_file_test.c`:
   - Test `ik_log_init()` with temp directory
   - Test log entries written to `.ikigai/logs/current.log`
   - Test multiple log entries append correctly
   - Test `ik_log_shutdown()` closes file
   - Test subsequent writes after shutdown PANIC (signal test)
3. Add stubs in `src/logger.c`
4. Run `make check` - expect failures

### Green
1. Add static global `FILE *ik_log_file = NULL` in `src/logger.c`
2. Implement `ik_log_init()`:
   - Construct path: `{working_dir}/.ikigai/logs/current.log`
   - Create `.ikigai/logs/` directory with `mkdir -p` logic
   - Open file in append mode (`"a"`)
   - PANIC if mkdir or fopen fails
   - Store FILE* in global
3. Update `ik_log_debug()`:
   - Assert `ik_log_file != NULL`
   - Write to `ik_log_file` instead of stdout
   - Call `fflush(ik_log_file)` after write
   - PANIC if write fails
4. Implement `ik_log_shutdown()`:
   - Close `ik_log_file`
   - Set to NULL
5. Run `make check` - expect pass

### Refactor
1. Verify error handling uses PANIC consistently
2. Ensure path construction is safe (buffer sizes)
3. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- Logger writes to `.ikigai/logs/current.log`
- Directory creation automatic
- 100% test coverage (excluding PANIC paths)
