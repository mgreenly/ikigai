# Task: JSONL Logger File Rotation

## Target
Infrastructure: JSONL logging system

## Agent
model: sonnet

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/naming.md

### Pre-read Docs
- docs/naming.md
- docs/error_handling.md

### Pre-read Source (patterns)
- src/logger.c (existing file handling)
- src/wrapper.h (POSIX wrappers)

### Pre-read Tests (patterns)
- tests/unit/logger/jsonl_file_test.c

## Pre-conditions
- `make check` passes
- Task `jsonl-logger-thread-safety.md` completed
- Logger opens/writes to current.log

## Task
Add atomic file rotation during initialization. If `current.log` exists when `ik_log_init()` is called, rename it to a timestamped archive file before opening a new current.log.

Rotation logic:
1. Check if `current.log` exists
2. If exists, rename to `YYYY-MM-DDTHH-MM-SS.sss±HH-MM.log` (timestamp format with colons replaced by hyphens for filename safety)
3. Open new `current.log` in write mode (truncate)
4. PANIC if rename fails

The rotation must be atomic from the perspective of concurrent writes (happens within mutex lock).

## TDD Cycle

### Red
1. Add test in `tests/unit/logger/jsonl_rotation_test.c`:
   - Create `current.log` with existing content
   - Call `ik_log_init()`
   - Verify `current.log` exists and is empty
   - Verify archived file exists with timestamped name
   - Verify archived file contains original content
   - Test multiple rotations create multiple archives
2. Run `make check` - expect failure (no rotation yet)

### Green
1. Add `ik_log_rotate_if_exists()` internal function in `src/logger.c`:
   - Construct path to `current.log`
   - Check if exists with `access()`
   - If exists:
     - Generate timestamp-based filename
     - Use `rename()` to move current.log to archive
     - PANIC if rename fails
2. Update `ik_log_init()`:
   - Lock mutex
   - Call `ik_log_rotate_if_exists()` before opening file
   - Open `current.log` in write mode (creates new file)
   - Unlock mutex
3. Run `make check` - expect pass

### Refactor
1. Verify timestamp format is filesystem-safe (no colons in filename)
2. Ensure rotation is atomic (within mutex lock)
3. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `ik_log_init()` rotates existing current.log to timestamped archive
- Rotation is atomic and thread-safe
- 100% test coverage (excluding PANIC paths)
