# Task: JSONL Logger Thread Safety

## Target
Infrastructure: JSONL logging system

## Agent
model: haiku

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md

### Pre-read Docs
- docs/naming.md

### Pre-read Source (patterns)
- src/logger.c (existing pthread_mutex usage)

### Pre-read Tests (patterns)
- tests/unit/logger/basic_test.c

## Pre-conditions
- `make check` passes
- Task `jsonl-logger-file-output.md` completed
- Logger writes to file but is not thread-safe

## Task
Add pthread mutex protection to make logger thread-safe for concurrent writes from multiple threads.

The logger must:
- Use a global `pthread_mutex_t` for write serialization
- Lock mutex before any file write
- Unlock mutex after fflush
- Ensure atomic JSONL line writes (no interleaving)

## TDD Cycle

### Red
1. Add test in `tests/unit/logger/jsonl_thread_test.c`:
   - Create 10 threads that each log 100 entries
   - Verify output file has exactly 1000 lines
   - Verify each line is valid JSON (no corruption/interleaving)
   - Parse each JSONL line and verify structure
2. Run `make check` - expect race conditions/corruption (no mutex yet)

### Green
1. Add static global in `src/logger.c`:
   ```c
   static pthread_mutex_t ik_log_mutex = PTHREAD_MUTEX_INITIALIZER;
   ```
2. Update `ik_log_debug()`:
   - Add `pthread_mutex_lock(&ik_log_mutex)` before file operations
   - Add `pthread_mutex_unlock(&ik_log_mutex)` after fflush
3. Update `ik_log_init()`:
   - Lock mutex during file open
4. Update `ik_log_shutdown()`:
   - Lock mutex during file close
5. Run `make check` - expect pass

### Refactor
1. Verify all file operations are protected
2. Ensure no deadlock possibilities
3. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- Logger is thread-safe (tested with concurrent writes)
- 100% test coverage
