# Task: JSONL Logger Core Structure

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
- docs/memory.md
- docs/error_handling.md

### Pre-read Source (patterns)
- src/logger.h (existing logger interface)
- src/logger.c (existing logger implementation)
- src/config.h (struct patterns)

### Pre-read Tests (patterns)
- tests/unit/logger/basic_test.c (existing logger tests)

## Pre-conditions
- `make check` passes
- Existing logger.h/logger.c present

## Task
Replace the existing printf-style logger with a JSONL logger that accepts yyjson documents. The new logger will wrap each log entry with level and timestamp fields, then write as single-line JSON to a file.

Core API:
```c
// Create a log document (returns doc with empty root object)
yyjson_mut_doc *ik_log_create(void);

// Log function (takes ownership of doc, wraps it, writes, frees)
void ik_log_debug(yyjson_mut_doc *doc);
```

Expected output format (single line):
```json
{"level":"debug","timestamp":"2025-12-05T12:34:56.789-05:00","logline":{"event":"test","value":42}}
```

## TDD Cycle

### Red
1. Update `src/logger.h`:
   - Remove old printf-style declarations
   - Add `yyjson_mut_doc *ik_log_create(void);`
   - Add `void ik_log_debug(yyjson_mut_doc *doc);`
2. Create `tests/unit/logger/jsonl_basic_test.c`:
   - Test `ik_log_create()` returns non-NULL doc with empty root object
   - Test `ik_log_debug()` writes JSONL to stdout (use freopen to capture)
   - Test output has "level":"debug" field
   - Test output has "timestamp" field (basic check it exists)
   - Test output has "logline" field containing original doc
   - Test output is valid single-line JSON
3. Add stubs in `src/logger.c` that compile but don't work
4. Run `make check` - expect failures

### Green
1. Implement `ik_log_create()`:
   - Create new yyjson_mut_doc
   - Create empty root object
   - Return doc
2. Implement `ik_log_debug()`:
   - Get original root object from doc
   - Create new wrapper doc
   - Add "level" field with "debug"
   - Add "timestamp" field (use stub "2025-01-01T00:00:00.000Z" for now)
   - Add "logline" field with original root object
   - Serialize to JSON string
   - Write to stdout with newline
   - Free wrapper doc
   - Free original doc
3. Run `make check` - expect pass

### Refactor
1. Verify naming follows conventions
2. Ensure memory cleanup is correct (both docs freed)
3. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- `ik_log_create()` and `ik_log_debug()` exist
- JSONL output format verified
- 100% test coverage for new code
