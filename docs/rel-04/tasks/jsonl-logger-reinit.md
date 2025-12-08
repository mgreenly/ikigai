# Task: JSONL Logger Reinitialization

## Target
Infrastructure: JSONL logging system

## Agent
model: haiku

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/naming.md

### Pre-read Docs
- docs/naming.md

### Pre-read Source (patterns)
- src/logger.c (existing init/shutdown)

### Pre-read Tests (patterns)
- tests/unit/logger/jsonl_file_test.c

## Pre-conditions
- `make check` passes
- Task `jsonl-logger-levels.md` completed
- Logger can only be initialized once

## Task
Add `ik_log_reinit()` function to support /clear command. This allows switching to a new working directory and rotating the log file without restarting the process.

API:
```c
void ik_log_reinit(const char *working_dir);
```

Behavior:
1. Close current log file
2. Switch to new working directory path
3. Rotate existing current.log (if present) in new location
4. Open new current.log in new location

This is equivalent to `ik_log_shutdown()` + `ik_log_init()` but without the intermediate NULL state.

## TDD Cycle

### Red
1. Add to `src/logger.h`:
   - `void ik_log_reinit(const char *working_dir);`
2. Add test in `tests/unit/logger/jsonl_reinit_test.c`:
   - Initialize logger with dir1
   - Write log entries to dir1
   - Call `ik_log_reinit()` with dir2
   - Write log entries to dir2
   - Verify dir1 log file unchanged
   - Verify dir2 log file contains new entries
   - Test reinit rotates existing current.log in new dir
3. Add stub in `src/logger.c`
4. Run `make check` - expect failures

### Green
1. Implement `ik_log_reinit()`:
   - Lock mutex
   - Close current log file if open
   - Call rotation logic for new working_dir
   - Open new log file in new location
   - Unlock mutex
2. Run `make check` - expect pass

### Refactor
1. Extract common logic between init and reinit
2. Verify thread-safety
3. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `ik_log_reinit()` switches working directory and rotates log
- 100% test coverage
