# Task: JSONL Logger All Log Levels

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
- src/logger.c (existing ik_log_debug implementation)

### Pre-read Tests (patterns)
- tests/unit/logger/jsonl_basic_test.c

## Pre-conditions
- `make check` passes
- Task `jsonl-logger-rotation.md` completed
- Only `ik_log_debug()` exists

## Task
Add remaining log level functions: `ik_log_info()`, `ik_log_warn()`, `ik_log_error()`, and `ik_log_fatal()`.

All functions have same signature as `ik_log_debug()`:
```c
void ik_log_info(yyjson_mut_doc *doc);
void ik_log_warn(yyjson_mut_doc *doc);
void ik_log_error(yyjson_mut_doc *doc);
void ik_log_fatal(yyjson_mut_doc *doc) __attribute__((noreturn));
```

The only difference is the "level" field value in output:
- `ik_log_debug()` → `"level":"debug"`
- `ik_log_info()` → `"level":"info"`
- `ik_log_warn()` → `"level":"warn"`
- `ik_log_error()` → `"level":"error"`
- `ik_log_fatal()` → `"level":"fatal"` then calls `exit(1)`

## TDD Cycle

### Red
1. Add to `src/logger.h`:
   - All four function declarations
   - `__attribute__((noreturn))` on fatal
2. Add tests in `tests/unit/logger/jsonl_levels_test.c`:
   - Test each level function writes correct "level" field
   - Test `ik_log_fatal()` calls exit(1) (use fork + waitpid)
3. Add stubs in `src/logger.c`
4. Run `make check` - expect failures

### Green
1. Refactor existing `ik_log_debug()`:
   - Extract common logic to `ik_log_write()` internal function
   - Pass level string as parameter
2. Implement each level function as thin wrapper:
   ```c
   void ik_log_info(yyjson_mut_doc *doc) {
       ik_log_write("info", doc);
   }
   ```
3. Implement `ik_log_fatal()`:
   - Call `ik_log_write("fatal", doc)`
   - Call `exit(1)`
4. Run `make check` - expect pass

### Refactor
1. Verify no code duplication
2. Ensure `ik_log_write()` is properly thread-safe
3. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- All five log level functions exist
- `ik_log_fatal()` terminates process
- 100% test coverage
